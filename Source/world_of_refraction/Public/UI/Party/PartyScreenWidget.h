// PartyScreenWidget.h
// B-458 (D-200, D-181; UIScreens #7, mockup screen_mockups/menus/party/1_party.png): the party screen. A CommonUI
// screen on the root layout's menu layer, opened from the game menu's Party page (or wor.UI.Party). Left: one row per
// member (name, class, who plays it, start row, field skill), then "Invite". Right: the 3 x 3 starting grid and the
// lit member's guard and seat notes. A changes the lit member's seat (you / AI) or opens Invite, X moves the lit
// member on the grid, B goes back. What it shows comes from WoRPartyScreen (PartyScreenModel.h), which the specs read.
// Replaces the placeholder UPartyControlWidget (still wor.UI.PartyControl, removed after Crown's PIE pass).

#pragma once

#include "CoreMinimal.h"
#include "UI/Party/PartyScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "PartyScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UScrollBox;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UPartyScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UPartyScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout. */
	static UPartyScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	/** A: the lit member's seat (you / AI), or Invite. */
	void PressChange();
	/** X: the lit member's next start cell. */
	void PressMoveOnGrid();

	const FPartyScreenPage &GetPage() const { return Page; }
	/** 0..Members-1 a member; Members.Num() the Invite row. */
	int32 GetSelectedRow() const { return SelectedRow; }
	bool IsInviteRow(int32 Index) const { return Index == Page.Members.Num(); }
	/** The last refusal; empty after a change goes through. */
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One member row's widgets, made once and refilled (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Line;
		TWeakObjectPtr<UTextBlock> Value;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	void BuildGrid(class UVerticalBox *Column);
	/** The page, read fresh from the model. */
	void RefreshData();
	/** The lit row's colour, the grid and the side panel. */
	void RefreshSelection();
	int32 RowCount() const { return Page.Members.Num() + 1; }
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	FPartyScreenPage Page;
	int32 SelectedRow = 0;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelGuard;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelSeat;

	/** One per party seat (MAX_PARTY_MEMBERS), built before the Invite row so it stays last. */
	TArray<FRowWidgets> RowWidgets;
	FRowWidgets InviteRow;
	/** The grid's cells, [Column * GRID_ROWS + Row]: a drawn column is a combat row (Back, Middle, Front). */
	TArray<TWeakObjectPtr<UBorder>> GridCells;
	TArray<TWeakObjectPtr<UTextBlock>> GridLabels;
};
