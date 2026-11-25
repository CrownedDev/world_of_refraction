// JournalScreenWidget.h
// B-463 (D-181, D-196, D-198, D-199, UIScreens #6, mockups screen_mockups/menus/journal/): the journal screen. A
// CommonUI screen on the root layout's menu layer, opened from the game menu's Journal page (Back returns there) or
// wor.UI.Journal.
//
// Page tabs on top (LB / RB): Quests, Memories, Lore, Conversations. On the left the page's sub-tabs (LT / RT) and
// its list; on the right the reader for the focused row. On Quests, X shows or hides the focused secret quest and
// Y hides them all; every secret quest is hidden again when the journal closes (Q-397b).
// What it shows comes from WoRJournalScreen (JournalScreenModel.h), which the specs read.
// The placeholder UMemoryJournalWidget stays for the hub's journal station (it has V's lines) until B-471.
// B-630 (mockup menus/journal/v2, Q-630): the loadout screen's three columns (v5). Left: the page's name and count, a
// ‹ kind › plate (Left / Right or a click turn it; LT / RT still do) and the list (a tracked quest says so). Middle:
// the focused entry, always open (its steps and text). Right: Details, its facts, the buttons for what A, X and Y do
// there (each clickable), and its note. A click on a row only lights it.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/JournalScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "JournalScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UStoryLinesData;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UJournalScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UJournalScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static UJournalScreenWidget *Open(APlayerController *PC);

	/** The story assets conversations come from; unset, every story asset is gathered when the screen opens. */
	void SetStory(const TArray<UStoryLinesData *> &InStory);

	// What the keys and the mouse do (the specs call these too).
	void MovePage(int32 Delta);
	void MoveFilter(int32 Delta);
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	/** A: the focused row's Select (a ready conversation's next line). */
	void PressSelect();
	/** X: shows or hides the focused secret quest. */
	void PressToggle();
	/** Y: hides every shown secret quest (the Quests page only). */
	void PressHideAll();
	void PressBack() { HandleBackAction(); }

	/** B-630: a Details button, in its key's job. */
	enum class EJournalButton : uint8
	{
		Select,
		Toggle,
		HideAll
	};

	/** B-630: one Details button: its key and verb, and why it can't be pressed (empty when it can). */
	struct FJournalButton
	{
		EJournalButton Kind = EJournalButton::Select;
		FText Label;
		FText Refusal;
	};

	/** B-630: the focused row's Details buttons, in order. */
	TArray<FJournalButton> GetButtons() const;
	void PressButton(EJournalButton Button);

	EJournalPage GetPage() const { return Page; }
	int32 GetFilter() const { return Filter; }
	int32 GetSelectedRow() const { return SelectedRow; }
	const FJournalView &GetView() const { return View; }
	const FJournalRow *GetFocusedRow() const { return View.Rows.IsValidIndex(SelectedRow) ? &View.Rows[SelectedRow] : nullptr; }
	/** The bottom line: what each key does on the focused row. */
	FText GetHints() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UBorder> KindStrip;
		TWeakObjectPtr<UTextBlock> Title;
		TWeakObjectPtr<UTextBlock> Subtitle;
		TWeakObjectPtr<UTextBlock> Tag;
		TWeakObjectPtr<UTextBlock> Tracked;
	};
	struct FFactWidgets
	{
		TWeakObjectPtr<UWidget> Line;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Value;
	};
	struct FButtonWidgets
	{
		TWeakObjectPtr<UBorder> Plate;
		TWeakObjectPtr<UTextBlock> Verb;
		TWeakObjectPtr<UTextBlock> Reason;
	};
	struct FStepWidgets
	{
		TWeakObjectPtr<UWidget> Line;
		TWeakObjectPtr<UTextBlock> Mark;
		TWeakObjectPtr<UTextBlock> Text;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	FStepWidgets BuildStep();
	UBorder *MakePanel(UVerticalBox *&OutColumn) const;
	/** B-630: one word of the kind plate, the lit one with a line under it. */
	void AddFilterWord();
	/** The page read fresh from the model, the list and the focused row's reader. */
	void Refresh(bool bScrollIntoView = true);
	void RefreshReader();
	/** B-630: the Details column for the focused row. */
	void RefreshDetails();
	int32 FindButtonUnder(const FPointerEvent &MouseEvent) const;
	int32 FindUnder(const TArray<TObjectPtr<UBorder>> &Boxes, const FPointerEvent &MouseEvent) const;
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	EJournalPage Page = EJournalPage::Quests;
	int32 Filter = 0;
	int32 SelectedRow = 0;
	FJournalState State;
	FJournalView View;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStoryLinesData>> Story;
	bool bStoryGiven = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ListTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountText;
	/** B-630: the ‹ kind › plate (was the LT / RT sub-tab row). */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> FilterRow;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> FilterTabBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> FilterTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FilterTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> FilterUnderlines;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsName;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FactBox;
	TArray<FFactWidgets> FactWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonBox;
	TArray<FButtonWidgets> ButtonWidgets;
	/** The buttons as last drawn, so a click presses the one under it. */
	TArray<FJournalButton> ShownButtons;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowScroll;
	TArray<FRowWidgets> RowWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ListNote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReaderTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReaderKicker;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StepBox;
	TArray<FStepWidgets> StepWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ReaderScroll;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReaderBody;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReaderNote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;
};
