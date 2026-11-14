// LoadoutV2ScreenWidget.h
// B-476b (D-206, D-209, D-210; UIScreens.md #2, mockups menus/loadout/v2), first part: the v2 loadout screen's Weapons
// tab, built beside today's ULoadoutScreenWidget (which stays until Crown's PIE pass). LT / RT the character; the
// list of entries (names only, the two sockets, ACTIVE on the weapon in hand) with the Add weapon frame at the bottom
// ("1/5 left"); "Cells used 4 of 5" at the top. Opened by wor.UI.LoadoutV2.
//
// B-476c (UIScreens B-479c layout): the list on the left, the focused entry's details in a box on the right (always
// shown; X switches the full and simple views, remembered for the session). The Add gear window: on the Add weapon
// frame it asks Weapon / Adapter / Evolution first; A on an entry opens it for that entry (Edit). Its slots (Main hand,
// Off hand, Augment socket 1 and 2; an Adapter; the Evolution) each open a list: A Pick, A Remove on the EQUIPPED line,
// X Compare (weapons), Y Confirm, B back. An evolution asks "Attach ... ? Uses 2 cells" first. L3 / 6 sets the focused
// weapon entry active; Y Options (Set active, Saved loadouts, Remove all gear, Help, Cosmetics shortcuts).
// B-476d: LB / RB switch the tabs (Weapons | Skills; Apparel and Cosmetics greyed, B-476e). The Skills tab: the entries
// on the left (WoRLoadoutV2Skills), the focused entry's slots under "Name · 4/9 skills" in the box; A opens its slots,
// A on a slot the picker (A Put in slot), X the full / short skill text, B back. The box shows what the highlighted
// skill does. Not yet (B-476e): the Apparel tab, Compare for adapters / evolutions / sockets, Saved loadouts, then
// replacing ULoadoutScreenWidget on the game menu.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutV2ScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UInventoryComponent;
class ULoadoutComponent;
class UTextBlock;
class UVerticalBox;

/** B-476c: which part of the screen takes the keys. */
enum class ELoadoutV2Mode : uint8
{
	/** The entries list and the details box. */
	List,
	/** Y Options. */
	Options,
	/** The Add gear window's first question: Weapon / Adapter / Evolution. */
	WindowKind,
	/** The window's slots (Main hand, Off hand, sockets...). */
	WindowSlots,
	/** The focused slot's list of pieces. */
	WindowItems,
	/** "Attach ... to ...? Uses 2 cells": A Attach, B Not now. */
	WindowAsk,
	/** B-476d, the Skills tab: the entries. */
	SkillEntries,
	/** The focused entry's skill slots. */
	SkillRows,
	/** The picker for a slot. */
	SkillPicker
};

/** B-476c: a slot of the Add gear window. */
enum class ELoadoutV2Slot : uint8
{
	MainHand,
	OffHand,
	Adapter,
	Evolution,
	Socket1,
	Socket2
};

/** B-476c: Y Options' lines. */
enum class ELoadoutV2Option : uint8
{
	SetActive,
	SavedLoadouts,
	RemoveAllGear,
	Help,
	CosmeticsShortcuts
};

/** B-476c: one line of the window's piece list. */
struct FLoadoutV2WindowItem
{
	FText Name;
	/** What it is, or why it can't go in (Refusal set). */
	FText Line;
	FText Refusal;
	/** The piece already in the slot: A Removes it. */
	bool bEquipped = false;
	FWeaponInventoryEntry Weapon;
	FRingInventoryEntry Adapter;
	FEvolutionInventoryEntry Evolution;
	/** A socket line (today's socket picker's choice for the entry's row). */
	FLoadoutScreenChoice Socket;
};

UCLASS()
class WORLD_OF_REFRACTION_API ULoadoutV2ScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	ULoadoutV2ScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout; null without one. */
	static ULoadoutV2ScreenWidget *Open(APlayerController *PC);

	// What the keys do (the specs call these too).
	void MoveSelection(int32 Delta);
	void MoveMember(int32 Delta);
	/** A: Edit an entry, Equip on the Add weapon frame; in Options, select; in the window, open a slot's list, Pick or
	 *  Remove; when asked, Attach. */
	void PressA();
	/** X: on the list, the full / simple view; in the window's list, Compare. */
	void PressCompare();
	/** Y: on the list, Options; in the window, Confirm. */
	void PressConfirm();
	/** L3 / 6: the focused weapon entry becomes the one in hand (D-207). */
	void PressSetActive();
	/** B, Esc: one step back (the ask, the list, the window, Options), else closes the screen. */
	void PressBack() { HandleBackAction(); }
	/** B-476d, LB / RB: the next built tab (Weapons, Skills), from the tab's top level. */
	void PressTab(int32 Delta);
	bool IsOnSkillsTab() const { return Mode == ELoadoutV2Mode::SkillEntries || Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker; }
	const FLoadoutV2SkillsPage &GetSkillsPage() const { return SkillsPage; }
	const TArray<FLoadoutV2SkillRowRef> &GetSkillRows() const { return SkillRows; }
	const TArray<FLoadoutScreenChoice> &GetSkillChoices() const { return SkillChoices; }

	const FLoadoutV2WeaponsPage &GetPage() const { return Page; }
	ELoadoutV2Mode GetMode() const { return Mode; }
	bool IsAdding() const { return Mode == ELoadoutV2Mode::WindowKind || Mode == ELoadoutV2Mode::WindowSlots || Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk; }
	const TArray<FLoadoutV2WindowItem> &GetItems() const { return Items; }
	const TArray<ELoadoutV2Slot> &GetSlots() const { return Slots; }
	const TArray<FLoadoutV2DetailLine> &GetDetails() const { return Details; }
	int32 GetSelected() const { return Selected; }
	const FText &GetMessage() const { return Message; }
	static bool IsSimpleView() { return bSimpleView; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual bool NativeOnHandleBackAction() override;

private:
	/** The members and the shown member's page, read fresh; the window's lists too while it is open. */
	void RefreshData();
	void RefreshView();
	void RefreshDetails();
	/** The shown member's loadout component, inventory and active loadout; false when the member isn't here. */
	bool FindActive(ULoadoutComponent *&OutLoadout, UInventoryComponent *&OutInventory, const FCombatLoadout *&OutActive) const;

	/** The list's lines in the current mode. */
	int32 GetLineCount() const;
	FText GetLineName(int32 Line) const;
	FText GetLineDetail(int32 Line) const;
	bool IsLineRefused(int32 Line) const;
	FText GetTitle() const;
	FText GetHints() const;
	bool IsOnAddFrame() const { return Mode == ELoadoutV2Mode::List && Page.bCanAdd && Selected == Page.Entries.Num(); }
	const FLoadoutV2Entry *GetFocusedEntry() const;

	// The window.
	void OpenWindowForNew();
	void OpenWindowForEntry(const FLoadoutV2Entry &Entry);
	void StartDraft(ELoadoutV2DraftKind Kind);
	void BuildItems();
	void PickItem();
	void RemoveEquipped();
	void ConfirmWindow();
	/** Puts the draft in (a new entry, or the edited one) through ULoadoutComponent::TryChangeGear. */
	void ApplyWindowDraft();
	void CloseWindow();
	/** Today's screen's row for the edited entry (its sockets, its place), from the member's gear page. */
	bool FindScreenRow(FLoadoutScreenRow &OutRow) const;
	FText GetSlotName(ELoadoutV2Slot Slot) const;
	FText GetSlotValue(ELoadoutV2Slot Slot) const;

	// Options.
	void SelectOption();

	// B-476d, the Skills tab.
	void RefreshSkills();
	/** The focused slot's Skills page row (rows and picker); null elsewhere. */
	const FLoadoutScreenRow *GetFocusedSkillRow() const;
	void PutPickedSkill();
	FLoadoutV2SkillsPage SkillsPage;
	/** The entry whose slots are open (SkillRows, SkillPicker). */
	int32 SkillEntry = 0;
	TArray<FLoadoutV2SkillRowRef> SkillRows;
	/** The slot the picker is for. */
	int32 SkillRowSelected = 0;
	TArray<FLoadoutScreenChoice> SkillChoices;

	// The drafts and lists below hold raw pointers to the gear's data assets (weapons, adapters, evolutions), as today's
	// screen does: those are loaded assets the inventory keeps, read again on every RefreshData.
	TArray<FLoadoutScreenMember> Members;
	int32 MemberIndex = 0;
	FLoadoutV2WeaponsPage Page;
	TArray<FLoadoutV2DetailLine> Details;
	ELoadoutV2Mode Mode = ELoadoutV2Mode::List;
	int32 Selected = 0;
	/** The list's line when Options or the window opened (back to it after). */
	int32 ListSelected = 0;
	FText Message;

	TArray<FLoadoutV2KindChoice> Kinds;
	/** The window edits this entry (A Edit); a new entry when false. */
	bool bEditing = false;
	FLoadoutV2Entry EditEntry;
	FLoadoutV2Draft Draft;
	TArray<ELoadoutV2Slot> Slots;
	int32 SlotIndex = 0;
	TArray<FLoadoutV2WindowItem> Items;
	/** The picked socket line, applied on Y Confirm (a socket changes the owned piece at once). */
	int32 PickedSocket = INDEX_NONE;
	bool bComparing = false;
	/** A Remove on a filled socket said what comes off; A again does it. */
	bool bRemoveArmed = false;
	/** Options' Remove all gear asked; A again does it. */
	bool bRemoveAllArmed = false;
	/** Remembered while the game runs (UIScreens B-479c: the screen remembers the view). */
	static bool bSimpleView;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MemberText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TabsText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CellsText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CompareText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LineRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineNames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineDetails;
	/** The details box (or, in the window, the slots) on the right. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoxTitleText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BoxLines;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BoxTexts;
};
