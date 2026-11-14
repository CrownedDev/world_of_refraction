// InviteScreenWidget.h
// B-459b (D-200, D-181; UIScreens #31, mockup screen_mockups/coop/session/1_invite.png): "Play together". A CommonUI
// screen on the root layout's menu layer, opened by the party screen's Invite row, the rest menu's Invite row, or
// wor.UI.PlayTogether. Left: friends (A invites). Right: the seats, then Host a session / Stop hosting (A). X moves
// between the two lists, B goes back. What it shows comes from WoRInviteScreen (InviteScreenModel.h), which the specs
// read. Replaces the placeholder UInviteWidget (still wor.UI.Invite, removed after Crown's PIE pass).

#pragma once

#include "CoreMinimal.h"
#include "UI/Coop/InviteScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "InviteScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API UInviteScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UInviteScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout; null without one. */
	static UInviteScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	/** X: the friends list or the seats. */
	void SwitchSide();
	/** A: invite the lit friend, or host / stop hosting on the session row. */
	void PressAction();

	const FInviteScreenPage &GetPage() const { return Page; }
	bool IsOnSeats() const { return bSeatsSide; }
	int32 GetSelectedRow() const { return bSeatsSide ? SeatRow : FriendRow; }
	/** On the seats side, the last row is the session row. */
	bool IsSessionRow(int32 Index) const { return Index == Page.Seats.Num(); }
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One row's widgets, made once and refilled (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Value;
	};

	void BuildTree();
	FRowWidgets BuildRow(UPanelWidget *List, float Width);
	void RefreshData();
	void RefreshSelection();
	void HandleFriendsRead();
	/** Which side and row the mouse is over; false when none. */
	bool FindRowUnder(const FPointerEvent &MouseEvent, bool &bOutSeats, int32 &OutRow) const;

	FInviteScreenPage Page;
	/** Friends invited while this screen was open, by name. */
	TSet<FString> Invited;
	bool bSeatsSide = false;
	int32 FriendRow = 0;
	int32 SeatRow = 0;
	FText Message;
	FDelegateHandle FriendsReadHandle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> FriendList;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> SeatList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FriendsNoteText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	TArray<FRowWidgets> FriendWidgets;
	/** One per party seat, then the session row last. */
	TArray<FRowWidgets> SeatWidgets;
};
