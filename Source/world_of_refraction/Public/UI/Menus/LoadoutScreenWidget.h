// LoadoutScreenWidget.h
// B-440a (D-181, D-180, UIScreens #2, mockups screen_mockups/menus/loadout/): the loadout screen, gear page,
// read only. A CommonUI screen on the root layout's menu layer, opened from the game menu's Loadout page (Back
// returns there) or wor.UI.Loadout.
//
// Top left: the character tabs (LT / RT), the page tabs (Gear; Skills comes with B-440d) and the budget pips.
// Top right: the saved loadout's name. Left: one row per weapon, adapter, evolution seat and apparel piece, with
// its sockets and cost. Right: the focused row's details and its cost sum.
// What it shows comes from WoRLoadoutScreen (LoadoutScreenModel.h), which the specs read.
// B-440b: A on a gear row opens a picker of owned gear for its place (the budget after each in amber, refused lines
// greyed with the reason), A again puts the lit one in; X empties the place. B-440c: Left / Right lights a socket;
// A there picks a carried gem or stone or an owned evolution, X (twice) takes it off. B-440d: LB / RB switches to the
// Skills page (WoRLoadoutSkills): Left / Right picks the list (a school, a weapon, an adapter), A puts a learned skill
// in the lit slot, X takes it out. B-440e: Y (R) opens the saved loadouts (WoRLoadoutSaved): A uses the lit one (or makes a
// new one on the last line), X duplicates it, Y renames it in a name box (Enter keeps, Esc drops); B closes the list.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/Menus/LoadoutSavedModel.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutSkillsModel.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API ULoadoutScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	ULoadoutScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static ULoadoutScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index, bool bScrollIntoView = true);
	/** The next / previous character; wraps. */
	void MoveMember(int32 Delta);
	/** B-440b, A: on a gear row, opens the picker for its place; in the picker, puts the lit line in. */
	void PressChange();
	/** B-440b, X: empties the lit gear row's place. B-440c: on a socket, the first X says what comes off and the
	 *  second takes it off (Q-440c). */
	void PressUnequip();
	/** B-440c, Left / Right: the lit row itself, its crystal socket, its stone socket (a weapon or adapter row). */
	void MoveSocket(int32 Delta);
	/** The lit socket (LoadoutScreenSockets), INDEX_NONE for the row itself. */
	int32 GetSelectedSocket() const { return SelectedSocket; }
	/** B-440d, LB / RB: the Gear or the Skills page. */
	void SwitchPage();
	/** B-440d, Left / Right on the Skills page: the previous / next list; wraps. */
	void MoveSkillTab(int32 Delta);
	bool IsSkillsPage() const { return bSkillsPage; }
	const FLoadoutSkillsPage &GetSkills() const { return Skills; }
	int32 GetSkillTab() const { return SkillTab; }
	/** B-440e, Y (R): opens the saved loadouts list; in it, renames the lit loadout. */
	void PressSavedLoadouts();
	bool IsSavedList() const { return bSavedList; }
	const FLoadoutSavedList &GetSavedList() const { return Saved; }
	int32 GetSelectedSaved() const { return SelectedSaved; }
	/** B-440e: the name box is open for the lit saved loadout. */
	bool IsRenaming() const { return bRenaming; }
	/** B-440e: what Enter in the name box does (the specs call it too). */
	void CommitRename(const FText &NewName);
	/** B, Esc: closes the name box, the picker or the saved list, else the screen. */
	void PressBack() { HandleBackAction(); }

	int32 GetSelectedRow() const { return SelectedRow; }
	int32 GetMemberIndex() const { return MemberIndex; }
	const FLoadoutScreenPage &GetPage() const { return Page; }
	bool IsPicking() const { return bPicking; }
	const TArray<FLoadoutScreenChoice> &GetChoices() const { return Choices; }
	int32 GetSelectedChoice() const { return SelectedChoice; }
	/** The last refusal (a change or an unequip), shown over the list; empty after a change goes through. */
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual bool NativeOnHandleBackAction() override;

private:
	/** One gear row's widgets. Rows are made as a page first needs them and refilled after (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UTextBlock> Heading;
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UWidget> ActiveChip;
		TWeakObjectPtr<UTextBlock> KindLine;
		TArray<TWeakObjectPtr<UWidget>> SocketBoxes;
		TArray<TWeakObjectPtr<UTextBlock>> SocketNames;
		TArray<TWeakObjectPtr<UTextBlock>> SocketLabels;
		TWeakObjectPtr<UTextBlock> Cost;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	/** The members and the shown member's page, read fresh from the model. */
	void RefreshData();
	/** The focused row's colour and the detail panel. */
	void RefreshSelection(bool bScrollIntoView = true);
	void FillRow(const FRowWidgets &Widgets, const FLoadoutScreenRow &Row, bool bHeading) const;
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;
	int32 FindMemberTabUnder(const FPointerEvent &MouseEvent) const;

	/** The rows the list shows now: the saved loadouts, the picker's lines or the gear rows. */
	int32 GetListCount() const { return bSavedList ? Saved.Rows.Num() : bPicking ? Choices.Num() : Page.Rows.Num(); }
	/** The list's lit index: the saved loadout, the picker's line or the gear row. */
	int32 GetListSelection() const { return bSavedList ? SelectedSaved : bPicking ? SelectedChoice : SelectedRow; }
	/** Fills the row list from the gear page or the picker, and the budget pips (the lit line's budget in amber). */
	void RefreshList();
	void RefreshBudget();
	void ClosePicker();
	/** B-440e: the saved list read fresh, the lit line kept in range. */
	void RefreshSaved();
	void CloseSavedList();
	void CancelRename();
	/** B-440e, A / X in the saved list: use (or make new), duplicate. */
	void PressUseSaved();
	void PressDuplicateSaved();
	UFUNCTION()
	void HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod);

	TArray<FLoadoutScreenMember> Members;
	FLoadoutScreenPage Page;
	int32 MemberIndex = 0;
	int32 SelectedRow = 0;
	bool bPicking = false;
	/** The picker is for SelectedSocket (B-440c), not the row's place. */
	bool bPickingSocket = false;
	int32 SelectedSocket = INDEX_NONE;
	/** The first X on a socket said what comes off; the next X does it. */
	bool bTakeOffArmed = false;
	TArray<FLoadoutScreenChoice> Choices;
	int32 SelectedChoice = 0;
	FText Message;
	/** B-440d: the Skills page is shown; Page.Rows then holds its lit list's rows. */
	bool bSkillsPage = false;
	FLoadoutSkillsPage Skills;
	int32 SkillTab = 0;
	/** B-440e: the saved loadouts list is shown over the page. */
	bool bSavedList = false;
	FLoadoutSavedList Saved;
	int32 SelectedSaved = 0;
	bool bRenaming = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> BudgetPips;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BudgetLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BudgetText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageTabLabels;
	/** B-440d: the Skills page's lists, the lit one marked. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkillTabsText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutTitleText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefusalText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PickerTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;
	/** B-440e: the saved loadout's new name (placeholder until the CommonUI screen, D-024). */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailKindLine;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailValues;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailCostLine;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNote;

	TArray<FRowWidgets> RowWidgets;
};
