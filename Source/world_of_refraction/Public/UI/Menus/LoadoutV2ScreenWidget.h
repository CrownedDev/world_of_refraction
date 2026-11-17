// LoadoutV2ScreenWidget.h
// B-476b (D-206, D-209, D-210; UIScreens.md #2, mockups menus/loadout/v2), first part: the v2 loadout screen's Weapons
// tab. B-488 (Crown): this is the loadout screen; the first one (ULoadoutScreenWidget) is retired. LT / RT the character; the
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
// B-482: each character and each tab is a framed chip (WoRMenuStyle::FChipRow), the current one filled, Cosmetics
// greyed with "later"; the details box is hidden when it has nothing to say (the Add weapon / Add apparel
// frames). Mouse: a click on a chip goes there, on a line focuses it (again: A); hovering lights a chip or a line.
// B-485 (mockups menus/loadout/icons): icons from UWoRIconTable: a weapon-type icon at a row's start, its two sockets
// as badges after the name (instead of "(Garnet)"), a tier badge by a piece's name, an icon beside an effect line and
// a socket line in the box, element and tier badges on the crystal lines of Add gear. Each icon carries its name as a
// tooltip; where the table has no icon (or there is no table yet) the text of before stays.
// B-476d: the Skills tab between Weapons and Apparel (LB / RB turn the three in a ring): the entries (WoRLoadoutV2Skills)
// with filled / total; A opens an entry's slots, A on a slot the picker, A puts the pick in (today's Skills page
// change); X / V the full or short skill text; the box says what the highlighted skill does.
// B-488: on a skill's slot Y / R takes the skill out (the first screen's X). Not yet (B-476d): the apparel socket and Compare.
// B-531 (D-230): the Inventory tab, first in the row (the screen still opens on Equipment): what the character owns and
// isn't wearing, grouped (WoRLoadoutV2::BuildInventoryPage); the box shows the lit or hovered piece; A on a piece opens
// the Add equipment window with it already picked (the same window and Confirm as "+ Add equipment").

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/Menus/LoadoutSavedModel.h"
#include "UI/Menus/LoadoutV2InventoryModel.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "UI/Menus/MenuChipRow.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutV2ScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UEditableTextBox;
class UHorizontalBox;
class UImage;
class UTexture2D;
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
	WindowAsk,
	/** B-476d, the Skills tab: its entries (the tab's top level). */
	SkillEntries,
	/** The focused entry's skill slots. */
	SkillRows,
	/** The picker for a slot. */
	SkillPicker
};

/** B-440f: the tab LB / RB shows. B-476d: Weapons | Skills | Apparel, the chips' order (Cosmetics later). B-531: Inventory first. */
enum class ELoadoutV2Tab : uint8
{
	Inventory,
	Weapons,
	Skills,
	Apparel,
	/** The last tab: SwitchTab's ring ends here (move it when Cosmetics comes). */
	Last = Apparel
};

/** B-482: the tab chips, in the mockup's order (Cosmetics greyed until it is built; B-476d built Skills). */
enum ELoadoutV2TabChip : int32
{
	LoadoutV2Chip_Inventory,
	LoadoutV2Chip_Weapons,
	LoadoutV2Chip_Skills,
	LoadoutV2Chip_Apparel,
	LoadoutV2Chip_Cosmetics,
	LoadoutV2Chip_Count
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
	/** B-534 (D-231): the crystal socket's Restore line: A tops the gem up (Line says what it spends). */
	bool bRestore = false;
	FWeaponInventoryEntry Weapon;
	FRingInventoryEntry Adapter;
	FEvolutionInventoryEntry Evolution;
	/** B-531 (D-230): how many exactly-same pieces the line stands for ("Adapter Ring x5"); picking it takes the first. */
	int32 Count = 1;
	/** B-485: a socket line's tier as a letter, written after the name when there is no tier badge to show. */
	FText TierText;
	/** A socket line (today's socket picker's choice for the entry's row); B-440f: or an apparel line (the piece as
	 *  WoRLoadoutApparel::BuildApparelChoices lists it, or the worn one on the EQUIPPED line). */
	FLoadoutScreenChoice Socket;
	/** B-476e: the apparel picker's "Crystal socket" line: A opens the socket's choices. */
	bool bOpensSocket = false;
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
	/** B-440f, LB / RB: the next tab (Delta > 0) or the one before, from a tab's top level only. B-476d: Weapons |
	 *  Skills | Apparel in a ring. */
	void SwitchTab(int32 Delta = 1);
	bool IsOnSkillsTab() const { return Mode == ELoadoutV2Mode::SkillEntries || Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker; }
	const FLoadoutV2SkillsPage &GetSkillsPage() const { return SkillsPage; }
	const TArray<FLoadoutV2SkillRowRef> &GetSkillRows() const { return SkillRows; }
	const TArray<FLoadoutScreenChoice> &GetSkillChoices() const { return SkillChoices; }
	/** A: Edit an entry, Equip on the Add weapon frame; in Options, select; in the window, open a slot's list, Pick or
	 *  Remove; when asked, Attach. */
	void PressA();
	/** X: on the list, the full / simple view; in the window's list, Compare; in the saved list, Duplicate. */
	void PressCompare();
	/** Y: on the list, Options; in the window, Confirm; in the saved list, Rename (opens the name box). */
	void PressConfirm();
	/** L3 / 6: the focused weapon entry becomes the one in hand (D-207). */
	void PressSetActive();
	// B-482: what a mouse click does (the specs call these too).
	/** A tab chip: goes to that tab; the greyed one (Cosmetics) only says it comes later. From a tab's top level. */
	void ClickTabChip(int32 Chip);
	/** A character chip: shows that character. On the list only. */
	void ClickMemberChip(int32 Chip);
	/** A list line: focuses it; on the focused one it does A. */
	void ClickLine(int32 Line);
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
	const FLoadoutV2InventoryPage &GetInventoryPage() const { return InventoryPage; }
	const TArray<FLoadoutScreenRow> &GetApparelRows() const { return ApparelRows; }
	bool IsRenaming() const { return bRenaming; }
	static bool IsSimpleView() { return bSimpleView; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;
	/** B-553 (D-233): the wheel moves the lit line, as Up / Down do. */
	virtual FReply NativeOnMouseWheel(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
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
	/** B-485: a list line's icons: the weapon type at its start, its socket badges and a tier badge after its name. */
	struct FLineIcons
	{
		FLoadoutV2Icon Type;
		FLoadoutV2Icon Socket1;
		FLoadoutV2Icon Socket2;
		bool bHasTier = false;
		EItemTier Tier = EItemTier::F_Tier;
	};
	FLineIcons GetLineIcons(int32 Line) const;
	/** B-485: the icon table's texture for Icon; null when it has none (the text stays). */
	static UTexture2D *FindIcon(const FLoadoutV2Icon &Icon);
	/** B-484: the row's tag, in its own space at the right end: ACTIVE on the weapon in hand, EQUIPPED / PICKED in a
	 *  slot's list. */
	FText GetLineTag(int32 Line) const;
	/** B-484: the line the details box shows in a list of pieces: the one under the mouse, else the focused one.
	 *  B-476d: a skill's slots and the picker follow the mouse the same way. B-503u: the Edit window's slot rows too. */
	int32 GetBoxLine() const
	{
		const bool bFollowsMouse = Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::ApparelItems || Mode == ELoadoutV2Mode::SkillRows ||
								   Mode == ELoadoutV2Mode::SkillPicker || Mode == ELoadoutV2Mode::WindowSlots || IsOnInventoryList();
		return bFollowsMouse && HoveredLine >= 0 && HoveredLine < GetLineCount() ? HoveredLine : Selected;
	}
	/** B-484: one line of a slot's list as the box's details: that piece alone (WoRLoadoutV2::Build...Details). */
	TArray<FLoadoutV2DetailLine> BuildItemDetails(const FLoadoutV2WindowItem &Item) const;
	/** B-503u: the Edit window's box on a slot row: that slot's piece, or a line saying what it takes. */
	TArray<FLoadoutV2DetailLine> BuildSlotDetails(ELoadoutV2Slot GearSlot) const;
	bool IsLineRefused(int32 Line) const;
	FText GetTitle() const;
	FText GetHints() const;
	bool IsOnAddFrame() const { return Mode == ELoadoutV2Mode::List && Tab == ELoadoutV2Tab::Weapons && Page.bCanAdd && Selected == Page.Entries.Num(); }
	const FLoadoutV2Entry *GetFocusedEntry() const;

	// The window.
	void OpenWindowForNew();
	/** A on an entry (B-531, D-230): the Add window's first question; its own kind edits it, another replaces it on Y. */
	void OpenWindowForEntry(const FLoadoutV2Entry &Entry);
	/** B-531: the window edits an entry with a piece of another kind (it replaces the entry on Y Confirm). */
	bool IsReplacingKind() const;
	void StartDraft(ELoadoutV2DraftKind Kind);
	void BuildItems();
	void PickItem();
	/** A piece slot's pick (PickItem, and B-531's Inventory tab): Item's piece into the draft; false for a socket slot. */
	bool PutPieceInDraft(ELoadoutV2Slot GearSlot, const FLoadoutV2WindowItem &Item);
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

	// B-482: the mouse.
	/** The shown list line under ScreenPosition; INDEX_NONE for none. */
	int32 FindLineUnder(const FVector2D &ScreenPosition) const;
	/** What the mouse is over, shown lit; true when it changed. */
	bool SetHovered(int32 MemberChip, int32 TabChip, int32 Line);

	/** B-476e: X Compare for any piece list (adapters, evolutions, sockets, apparel): the EQUIPPED line beside the
	 *  highlighted one, each in its own words. */
	FText BuildEquippedCompare() const;

	/** B-476e: the apparel picker shows the worn piece's socket choices (today's socket picker) instead of pieces. */
	bool bApparelSocket = false;
	bool IsApparelSocketOpen() const { return Mode == ELoadoutV2Mode::ApparelItems && bApparelSocket; }

	/** B-476d: shows NewTab at its top level (the Skills tab's entries, else the list). */
	void ShowTab(ELoadoutV2Tab NewTab);

	// B-476d, the Skills tab.
	/** The Weapons tab entry a list line shows (the Weapons list's own, or the one a Skills entry stands for): its
	 *  icons, socket badges and ACTIVE are drawn the same on both tabs. Null on any other line. */
	const FLoadoutV2Entry *GetRowEntry(int32 Line) const;
	void RefreshSkills();
	void PutPickedSkill();
	/** Debug: the Skills tab as shown (WoRLoadoutV2Skills::GetSkillsString), logged when the tab opens. */
	void LogSkillsTab() const;
	FLoadoutV2SkillsPage SkillsPage;
	/** The entry whose slots are open (SkillRows, SkillPicker). */
	int32 SkillEntry = 0;
	TArray<FLoadoutV2SkillRowRef> SkillRows;
	/** The slot the picker is for. */
	int32 SkillRowSelected = 0;
	TArray<FLoadoutScreenChoice> SkillChoices;

	// B-531 (D-230): the Inventory tab.
	bool IsOnInventoryList() const { return Mode == ELoadoutV2Mode::List && Tab == ELoadoutV2Tab::Inventory; }
	const FLoadoutV2InventoryRow *GetInventoryRow(int32 Line) const { return IsOnInventoryList() && InventoryPage.Rows.IsValidIndex(Line) ? &InventoryPage.Rows[Line] : nullptr; }
	/** The tab's page from the member's inventory, evolutions and pouch (the real store), logged when it changes. */
	void RefreshInventory(const UInventoryComponent &Inventory, const FCombatLoadout &Active);
	/** A on a piece: the Add equipment window for its kind, the piece already picked (the kind's refusal when it has no place). */
	void WearFromInventory();
	FLoadoutV2InventoryPage InventoryPage;

	// B-440f: the Apparel tab.
	int32 GetWornApparelCount() const;
	/** The apparel place the list's line stands for: a worn piece, or on the Add apparel frame the first empty place. */
	const FLoadoutScreenRow *GetFocusedApparelRow() const;
	void OpenApparelItems();
	void BuildApparelItems();
	/** A in the apparel list: wears the lit piece in the place, or takes the EQUIPPED one off. */
	void PickApparel();
	/** B-476e: the socket line opens the socket's choices; in them, A sockets the pick (ApplySocketChoice). */
	void PickApparelSocket(const FLoadoutScreenRow &Place, const FLoadoutV2WindowItem &Item);

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

	/** B-482: the character chips and the tab chips, and what the mouse is over (INDEX_NONE: nothing). */
	WoRMenuStyle::FChipRow MemberChips;
	WoRMenuStyle::FChipRow TabChips;
	int32 HoveredMemberChip = INDEX_NONE;
	int32 HoveredTabChip = INDEX_NONE;
	int32 HoveredLine = INDEX_NONE;
	/** When the mouse last did A on the focused line: a double click must not confirm what its first click only asked. */
	double LastClickActionSeconds = 0.0;

	/** The loadout in use, top right ("Bruiser (2 of 5)"). */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutNameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CellsText;
	/** B-482: the details box's frame, hidden when the box has nothing to say. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailsBox;
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
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineTags;
	// B-485: a row's icons (made with the row, hidden until the icon table has a texture for them).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> LineTypeIcons;
	/** B-530 (D-229): on an adapter's type icon (its socket frame), its crystal's badge. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> LineTypeCentres;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> LineSocket1Icons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> LineSocket2Icons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> LineTierIcons;
	/** The details box (or, in the window, the slots) on the right. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoxTitleText;
	// B-487b: the header: a quiet row above the title (the piece's icon and type), and the tier badge after the title.
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> BoxTypeRow;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoxTypeText;
	UPROPERTY(Transient)
	TObjectPtr<UImage> BoxTitleIcon;
	UPROPERTY(Transient)
	TObjectPtr<UImage> BoxTitleTier;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BoxLines;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BoxTexts;
	// B-485: a box line is a row: its icon, its text, a tier badge.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHorizontalBox>> BoxRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BoxIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BoxTierIcons;
};
