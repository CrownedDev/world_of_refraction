// InventoryScreenWidget.h
// B-441 (D-181, D-177, D-195, UIScreens #4, mockups screen_mockups/menus/inventory/): the inventory screen. A
// CommonUI screen on the root layout's menu layer, opened from the game menu's Inventory page (Back returns there)
// or wor.UI.InventoryScreen. Replaces the placeholder UInventoryWidget (wor.UI.Inventory), kept until the PIE pass.
//
// Top: the character tabs (LT / RT) and the Gear / Pouch / Chest tabs (LB / RB). Left: the tab's things as a grid
// of tiles (the pouch also draws its empty and its Jeweler-locked slots). Right: the focused tile's details and
// its verb menu; Give opens "Give to" in the menu's place. X switches a stack verb between one and the whole stack.
// What it shows and does comes from WoRInventoryScreen (InventoryScreenModel.h), which the specs read.
// B-461 (Q-461): a filter row over the Gear and Chest tiles (Up from the top row of tiles reaches it, Left / Right
// picks, Down returns), Sort on Y / R, evolution and apparel tiles on the Gear tab, the place name top right.
// Not built yet: icons (a letter stands in).

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "InventoryScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UWidget;

/** Where the keys act: the tile grid, the filter row above it, the focused tile's verb menu, or "Give to". */
enum class EInventoryScreenFocus : uint8
{
	Grid,
	Filters,
	Verbs,
	Receivers
};

UCLASS()
class WORLD_OF_REFRACTION_API UInventoryScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static UInventoryScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	/** Moves the focused tile by columns and rows; stays put at an edge. Up off the top row lands on the filter
	 *  row, when the tab has one. */
	void MoveTile(int32 DeltaColumn, int32 DeltaRow);
	/** The next / previous filter of the row; stays put at an edge. */
	void MoveFilter(int32 Delta);
	/** Shows the tab's Index-th filter, from the first tile. */
	void SelectFilter(int32 Index);
	/** Y: the next sort (as found, by tier, by name). */
	void CycleSort();
	void SelectTile(int32 Index, bool bScrollIntoView = true);
	/** The next / previous tab or character; wraps, and returns the keys to the grid. */
	void MoveTab(int32 Delta);
	void MoveMember(int32 Delta);
	/** Up / down the verb menu or "Give to". */
	void MoveChoice(int32 Delta);
	/** Grid: opens the tile's verb menu. Verbs: runs the verb (Give opens "Give to"). Receivers: gives. A greyed
	 *  line says its reason instead. */
	void Confirm();
	void ToggleWholeStack();
	/** One step back ("Give to", the verb menu), then closes. */
	void PressBack() { HandleBackAction(); }

	EInventoryScreenTab GetTab() const { return Tab; }
	EInventoryScreenFilter GetFilter() const { return Filter; }
	EInventoryScreenSort GetSort() const { return Sort; }
	int32 GetMemberIndex() const { return MemberIndex; }
	int32 GetSelectedTile() const { return SelectedTile; }
	EInventoryScreenFocus GetFocus() const { return Focus; }
	int32 GetChoice() const { return Choice; }
	bool IsWholeStack() const { return bWholeStack; }
	const FInventoryScreenPage &GetPage() const { return Page; }
	const TArray<FInventoryScreenVerb> &GetVerbs() const { return Verbs; }
	const TArray<FInventoryScreenReceiver> &GetReceivers() const { return Receivers; }
	const FText &GetStatus() const { return Status; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One tile's widgets. Tiles are made as a page first needs them and refilled after (never rebuilt). */
	struct FTileWidgets
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Mark;
		TWeakObjectPtr<UTextBlock> Worn;
		TWeakObjectPtr<UTextBlock> Count;
	};

	/** One line of the verb menu or of "Give to". */
	struct FChoiceWidgets
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Note;
		TWeakObjectPtr<UTextBlock> Reason;
	};

	void BuildTree();
	FTileWidgets BuildTile(int32 Index);
	FChoiceWidgets BuildChoice(class UVerticalBox *Column);
	/** The members and the shown tab, read fresh from the model; the tabs, the heading and the tiles. */
	void RefreshData();
	/** The focused tile's colour, the detail panel and the verb menu (or "Give to"). */
	void RefreshSelection(bool bScrollIntoView = true);
	void RunVerb(EInventoryVerb Verb, const FInventoryScreenReceiver *Receiver);
	FName MemberKey() const;
	const FInventoryScreenTile *FocusedTile() const;
	/** The keys are on the verb menu or on "Give to". */
	bool IsChoosing() const { return Focus == EInventoryScreenFocus::Verbs || Focus == EInventoryScreenFocus::Receivers; }
	int32 ChoiceCount() const;
	int32 FindTileUnder(const FPointerEvent &MouseEvent) const;
	int32 FindChoiceUnder(const FPointerEvent &MouseEvent) const;

	TArray<FLoadoutScreenMember> Members;
	FInventoryScreenPage Page;
	TArray<FInventoryScreenVerb> Verbs;
	TArray<FInventoryScreenReceiver> Receivers;
	/** The shown tab's filter row; empty when it has none (the Pouch). */
	TArray<EInventoryScreenFilter> Filters;
	EInventoryScreenTab Tab = EInventoryScreenTab::Gear;
	EInventoryScreenFilter Filter = EInventoryScreenFilter::All;
	EInventoryScreenSort Sort = EInventoryScreenSort::Kind;
	EInventoryScreenFocus Focus = EInventoryScreenFocus::Grid;
	int32 MemberIndex = 0;
	int32 SelectedTile = 0;
	int32 Choice = 0;
	bool bWholeStack = false;
	FText Status;
	bool bStatusIsRefusal = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> FilterTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FilterTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> FilterRow;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SortText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> TileScroll;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> TileGrid;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailKindLine;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailValues;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChoiceHeading;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WholeStackText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	TArray<FTileWidgets> TileWidgets;
	TArray<FChoiceWidgets> ChoiceWidgets;
};
