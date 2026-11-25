// PartyScreenWidget.h
// B-458 (D-200, D-181; UIScreens #7): the party screen. A CommonUI screen on the root layout's menu layer, opened from
// the game menu's Party page (or wor.UI.Party). What it shows comes from WoRPartyScreen (PartyScreenModel.h), which the
// specs read.
// B-628 (mockup menus/party/v2, Q-628): three columns as the loadout screen (v5). Left: a card per seat (name, class,
// who plays it, "Seat N"); no Invite row (the rest spot still opens Invite). Middle: the lit member's active loadout
// only. Right: Details, who plays it (‹ › turns it), where it starts and the 3 x 3 grid, then the buttons A Move start
// place, Y Open loadout, X Dismiss (twice). B goes back.
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
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API UPartyScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UPartyScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout. */
	static UPartyScreenWidget *Open(APlayerController *PC);

	/** B-628: the Details buttons, in their places. */
	enum class EPartyButton : uint8
	{
		MoveStart,
		OpenLoadout,
		Dismiss,
		Count
	};

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	/** Left / Right: the lit member's seat (you / AI). */
	void PressToggleSeat();
	/** A: the lit member's next start cell. */
	void PressMoveOnGrid();
	/** Y: the loadout screen on the lit member. */
	void PressOpenLoadout();
	/** X: arms, X again dismisses the lit member. */
	void PressDismiss();
	void PressButton(EPartyButton Button);

	const FPartyScreenPage &GetPage() const { return Page; }
	int32 GetSelectedRow() const { return SelectedRow; }
	bool IsDismissArmed() const { return bDismissArmed; }
	/** The last refusal or prompt; empty after a change goes through. */
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One seat card's widgets, made once and refilled (never rebuilt). */
	struct FSeatWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> ClassLine;
		TWeakObjectPtr<UTextBlock> PlayedBy;
		TWeakObjectPtr<UTextBlock> Seat;
	};

	/** One loadout entry's plate in the middle column. */
	struct FEntryWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> TypeLine;
		TWeakObjectPtr<UTextBlock> Skills;
	};

	/** One Details button: its key and verb, and the reason under a greyed one. */
	struct FButtonWidgets
	{
		TWeakObjectPtr<UBorder> Plate;
		TWeakObjectPtr<UTextBlock> Verb;
		TWeakObjectPtr<UTextBlock> Reason;
	};

	void BuildTree();
	UBorder *MakePanel(UVerticalBox *&OutColumn) const;
	FSeatWidgets BuildSeat();
	void BuildMiddle(UVerticalBox *Column);
	void BuildRight(UVerticalBox *Column);
	void BuildGrid(UVerticalBox *Column);
	/** The page, read fresh from the model. */
	void RefreshData();
	/** The lit card, the middle column, the grid and Details. */
	void RefreshSelection();
	void RefreshLoadout();
	void RefreshButtons();
	/** Why the lit member's button can't be pressed; empty when it can. */
	FText GetButtonRefusal(EPartyButton Button) const;
	void SetMessage(const FText &Text);
	void DisarmDismiss();
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;
	int32 FindButtonUnder(const FPointerEvent &MouseEvent) const;

	FPartyScreenPage Page;
	/** The lit member's active loadout. */
	FPartyScreenLoadout Loadout;
	int32 SelectedRow = 0;
	FText Message;
	bool bDismissArmed = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> SeatList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MiddleName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MiddleClass;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutInUse;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> EntryList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RightName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayedByValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartsValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RightNote;

	/** One per party seat (MAX_PARTY_MEMBERS). */
	TArray<FSeatWidgets> SeatWidgets;
	/** Grows to the longest loadout shown; spare plates are hidden. */
	TArray<FEntryWidgets> EntryWidgets;
	FButtonWidgets Buttons[static_cast<int32>(EPartyButton::Count)];
	/** The grid's cells, [Column * GRID_ROWS + Row]: a drawn column is a combat row (Back, Middle, Front). */
	TArray<TWeakObjectPtr<UBorder>> GridCells;
	TArray<TWeakObjectPtr<UTextBlock>> GridLabels;
};
