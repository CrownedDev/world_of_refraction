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
// B-440e (D-111): Options' Saved loadouts opens the saved list (WoRLoadoutSaved): A uses the lit one (or makes a new one
// on the last line), X duplicates it, Y renames it in a name box (Enter keeps, Esc drops); B goes back to Options.
// B-440f (D-164): LB / RB turns to the Apparel tab, which works like Weapons: the worn pieces, an Add apparel frame
// ("1/2 left"), the focused piece in the box. A opens the owned pieces for that place (WoRLoadoutApparel): A Pick, or
// A Remove on the EQUIPPED line. Apparel is outside the cells.
// Not yet (B-476d): the Skills tab, the apparel socket and Compare, then replacing ULoadoutScreenWidget on the game menu.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/Menus/LoadoutSavedModel.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutV2ScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UEditableTextBox;
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
	/** B-440e: Options' Saved loadouts list. */
	Saved,
	/** B-440f: the owned apparel pieces for the focused apparel place. */
	ApparelItems,
	/** The Add gear window's first question: Weapon / Adapter / Evolution. */
	WindowKind,
	/** The window's slots (Main hand, Off hand, sockets...). */
	WindowSlots,
	/** The focused slot's list of pieces. */
	WindowItems,
	/** "Attach ... to ...? Uses 2 cells": A Attach, B Not now. */
	WindowAsk
};

/** B-440f: the tab LB / RB shows (Skills comes with B-476d). */
enum class ELoadoutV2Tab : uint8
{
	Weapons,
	Apparel
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
	/** A socket line (today's socket picker's choice for the entry's row); B-440f: or an apparel line (the piece as
	 *  WoRLoadoutApparel::BuildApparelChoices lists it, or the worn one on the EQUIPPED line). */
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
	/** B-440f, LB / RB: the other tab (Weapons, Apparel); on the list only. */
	void SwitchTab();
	/** A: Edit an entry, Equip on the Add weapon frame; in Options, select; in the window, open a slot's list, Pick or
	 *  Remove; when asked, Attach. */
	void PressA();
	/** X: on the list, the full / simple view; in the window's list, Compare; in the saved list, Duplicate. */
	void PressCompare();
	/** Y: on the list, Options; in the window, Confirm; in the saved list, Rename (opens the name box). */
	void PressConfirm();
	/** L3 / 6: the focused weapon entry becomes the one in hand (D-207). */
	void PressSetActive();
	/** B-440e: what Enter in the name box does (the specs call it too). */
	void CommitRename(const FText &NewName);
	/** B, Esc: one step back (the name box, the saved list, the ask, the list, the window, Options), else closes the screen. */
	void PressBack() { HandleBackAction(); }

	const FLoadoutV2WeaponsPage &GetPage() const { return Page; }
	ELoadoutV2Mode GetMode() const { return Mode; }
	bool IsAdding() const { return Mode == ELoadoutV2Mode::WindowKind || Mode == ELoadoutV2Mode::WindowSlots || Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk; }
	const TArray<FLoadoutV2WindowItem> &GetItems() const { return Items; }
	const TArray<ELoadoutV2Slot> &GetSlots() const { return Slots; }
	const TArray<FLoadoutV2DetailLine> &GetDetails() const { return Details; }
	int32 GetSelected() const { return Selected; }
	const FText &GetMessage() const { return Message; }
	const FLoadoutSavedList &GetSavedList() const { return Saved; }
	ELoadoutV2Tab GetTab() const { return Tab; }
	const TArray<FLoadoutScreenRow> &GetApparelRows() const { return ApparelRows; }
	bool IsRenaming() const { return bRenaming; }
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
	bool IsOnAddFrame() const { return Mode == ELoadoutV2Mode::List && Tab == ELoadoutV2Tab::Weapons && Page.bCanAdd && Selected == Page.Entries.Num(); }
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

	// B-440f: the Apparel tab.
	int32 GetWornApparelCount() const;
	/** The apparel place the list's line stands for: a worn piece, or on the Add apparel frame the first empty place. */
	const FLoadoutScreenRow *GetFocusedApparelRow() const;
	void OpenApparelItems();
	void BuildApparelItems();
	/** A in the apparel list: wears the lit piece in the place, or takes the EQUIPPED one off. */
	void PickApparel();

	// B-440e: the saved loadouts list.
	void OpenSaved();
	/** A: uses the lit loadout (back to the list, now of that loadout), or makes a new one on the last line. */
	void UseSaved();
	void DuplicateSaved();
	void StartRename();
	void CancelRename();
	UFUNCTION()
	void HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod);

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
	/** B-440e: the loadout in use, shown top right before the cells (UIScreens #2). */
	FText LoadoutName;

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
	FLoadoutSavedList Saved;
	ELoadoutV2Tab Tab = ELoadoutV2Tab::Weapons;
	/** B-440f: the member's apparel places (the worn ones first, then the empty ones), from today's gear page. */
	TArray<FLoadoutScreenRow> ApparelRows;
	/** B-440e: the name box is open for the lit saved loadout. */
	bool bRenaming = false;
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
	/** B-440e: the saved loadout's new name; shown only while renaming (placeholder until the CommonUI screen, D-024). */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;
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
