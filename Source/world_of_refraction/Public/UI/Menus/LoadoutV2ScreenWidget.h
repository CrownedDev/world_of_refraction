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
// B-633 (Q-633): the window opens straight on its first slot's grid with the ‹ Weapon · Adapter · Evolution › selector
// (mockup v5); A equips a piece at once (no Y Confirm), a new evolution still asks first, a refused pick says why.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "UI/Menus/LoadoutSavedModel.h"
#include "UI/Menus/LoadoutV2GridModel.h"
#include "UI/Menus/LoadoutV2InventoryModel.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "Containers/Ticker.h"
#include "UI/Menus/MenuChipRow.h"
#include "UI/Menus/MenuFeel.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutV2ScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UEditableTextBox;
class UHorizontalBox;
class UImage;
class UScrollBox;
class USoundBase;
class USizeBox;
class UTexture2D;
class UInventoryComponent;
class ULoadoutComponent;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidget;

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
	/** OBSOLETE (B-633): the Add gear window's old first question, Weapon / Adapter / Evolution; the grid's kind
	 *  selector asks it now. Nothing opens it; removed after Crown's PIE pass. */
	WindowKind,
	/** The window's slots (Main hand, Off hand, sockets...). */
	WindowSlots,
	/** The focused slot's list of pieces. */
	WindowItems,
	/** "Attach ... to ...? Uses 2 cells": A Attach, B Not now. */
	WindowAsk,
	/** B-476d, the Skills tab. B-629 (mockup menus/skills/v1): its top level, the lit entry's skill slots under the
	 *  ‹ entry › selector (B-476d's list of entries is gone). */
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

/** B-614 (mockup menus/loadout/v5): a word of the kind selector over a slot's grid. */
enum class ELoadoutV2PieceKind : uint8
{
	Weapon,
	Adapter,
	Evolution,
	Crystal,
	Stone
};

/** B-476c: Y Options' lines. */
enum class ELoadoutV2Option : uint8
{
	SetActive,
	/** B-610: the focused entry's skill slots (JumpToEntrySkills). */
	Skills,
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
	/** D-248: worn (in this slot, or in another entry of the loadout): the tick on its tile. */
	bool bWorn = false;
	/** B-584 (D-256): in a socket now, so its badge keeps the silver frame. */
	bool IsSlotted() const { return bEquipped || bWorn; }
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
};

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API ULoadoutV2ScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	ULoadoutV2ScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout; null without one. */
	static ULoadoutV2ScreenWidget *Open(APlayerController *PC);

	// What the keys do (the specs call these too).
	void MoveSelection(int32 Delta);
	/** A direction key, wherever the keys are: the inventory's tiles, a slot's grid, the loadout's list (Up / Down, and
	 *  Right to the inventory's side). False when the direction has nothing to do here. */
	bool MoveInDirection(int32 DeltaX, int32 DeltaY);
	void MoveMember(int32 Delta);
	/** B-628: the shown character, an index of WoRLoadoutScreen::BuildMembers (the party screen's Y opens on its member). */
	int32 GetMemberIndex() const { return MemberIndex; }
	/** B-440f, LB / RB: the next tab (Delta > 0) or the one before, from a tab's top level only. B-476d: Weapons |
	 *  Skills | Apparel in a ring. */
	void SwitchTab(int32 Delta = 1);
	bool IsOnSkillsTab() const { return Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker; }
	/** B-629, Left / Right on the Skills tab's slots (and a click on its selector): the next entry (Delta > 0) or the one
	 *  before, in a ring; its first slot lit. */
	void TurnSkillEntry(int32 Delta);
	int32 GetSkillEntry() const { return SkillEntry; }
	/** B-629, Left / Right in the skill picker (and a click on its selector): Fits this slot, or All learned (the ones
	 *  that can't go here too, greyed with the reason). The picker opens on Fits this slot. */
	void SetSkillsAllLearned(bool bAll);
	bool IsSkillsAllLearned() const { return bSkillsAllLearned; }
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
	/** B-610, R3 / K (and Options' Skills): from the Equipment tab, the focused weapon's or adapter's skill slots open on
	 *  the Skills tab; Back from there returns to Equipment on the same line. False (with the reason shown) when the
	 *  focused line has no slots. */
	bool JumpToEntrySkills();
	/** B-610: the Equipment line Back returns to from slots opened by JumpToEntrySkills; INDEX_NONE otherwise. */
	int32 GetSkillsReturnLine() const { return SkillsReturnLine; }
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

	// D-249 piece 1: Inventory and Loadout are one screen. B-598c (mockups menus/loadout/v5, menus/inventory/v4, Crown
	// 2026-11-25): it is pages, Inventory | Equipment | Skills | Apparel (Q / E, the tab chips), each in three fixed
	// columns: the list or the picture grid on the left, the lit piece's details in the middle (no pop-up card), and
	// Details on the right. The Inventory page is "the loadout out" (bLoadoutOpen false): its grid shows the bag, with
	// the Gear / Pouch selector above the filter row (Up onto it, Left / Right turn it). The specs call these too.
	/** B-598c: the page shown: 0 Inventory, then Equipment, Skills, Apparel. */
	int32 GetPageIndex() const;
	static constexpr int32 INVENTORY_PAGE = 0;
	static constexpr int32 EQUIPMENT_PAGE = 1;
	static constexpr int32 PAGE_COUNT = 4;
	bool IsOnInventoryPage() const { return !bLoadoutOpen; }
	/** B-598c: the keys are on the Gear / Pouch selector above the inventory's filter row. */
	bool IsOnInventoryKinds() const { return bOnInvKinds; }
	/** Opens the screen with the loadout already in and the keys on its side (the game menu's Loadout page, the Gate). */
	static ULoadoutV2ScreenWidget *OpenWithLoadout(APlayerController *PC);
	/** B-615: a line that stays under the screen while it is open (the Gate: a trial starts you on the class kit). */
	void SetStandingNote(const FText &Note);
	/** B-615: the message line: this step's message, else the standing note. */
	static FText PickMessageLine(const FText &StepMessage, const FText &Note) { return StepMessage.IsEmpty() ? Note : StepMessage; }
	bool IsLoadoutOpen() const { return bLoadoutOpen; }
	/** The keys are on the inventory's side (its tiles, or its filter row). B-598c: the Inventory page. */
	bool IsOnInventorySide() const { return !bOnLoadoutSide; }
	/** B-598c: the Inventory page or the Equipment page, the other way. Refused while the loadout is in the middle of
	 *  something (Options, a window, a slot's list): finish or leave that first. */
	void ToggleLoadout();
	/** OBSOLETE (B-598c: pages replace D-249's two sides; delete after Crown's PIE pass): always false now, the keys are
	 *  on the page shown. */
	bool FocusLoadoutSide();
	bool FocusInventorySide();
	/** The arrows on the Inventory page: tile to tile; Up off the top row reaches the filter row (Left / Right pick,
	 *  Down returns), Up again the Gear / Pouch selector (Left / Right turn it, Down returns). */
	void MoveInventoryTile(int32 DeltaX, int32 DeltaY);
	/** The Gear / Pouch selector: the next kind (Delta > 0) or the one before, in a ring. */
	void MoveInventoryTab(int32 Delta);
	/** The View button / H on the inventory's side: the next sort. */
	void CycleInventorySort();
	// B-598c piece 2 (mockup menus/inventory/v4): the Details column's facts and buttons for the lit tile.
	/** A, Equip on…: to the page where the piece goes (Equipment; Apparel for apparel), with the piece lit once a
	 *  slot's grid opens. A stack is refused (it goes in a socket, picked from the slot). */
	void PressEquipOn();
	/** X, Give to…: the other members as the Details column's buttons (A gives, B back); refused with the reason. */
	void PressGive();
	/** B-598c piece 4 (D-250): the Inventory page's Pouch, where A picks the lit stack as a fight item or unpicks it. */
	bool IsOnPouch() const { return !bLoadoutOpen && InvTab == EInventoryScreenTab::Pouch; }
	/** A on the Pouch: picks or unpicks the lit stack (WoRInventoryScreen::ToggleFightPick); refused with the reason. */
	void PressFightPick();
	bool IsGiving() const { return bGiving; }
	/** The lit tile's facts (WoRInventoryScreen::BuildFacts). */
	const TArray<FLoadoutScreenDetail> &GetInventoryFacts() const { return InvFacts; }
	/** The Details column's buttons: label (with its key), and the reason when greyed. */
	struct FInventoryAction
	{
		FText Label;
		FText Refusal;
		/** Giving: the member it goes to. */
		FName Receiver;
	};
	const TArray<FInventoryAction> &GetInventoryActions() const { return InvActions; }
	/** The buttons' places (not giving): Equip on… or Pick / unpick, Give to…, Sell. */
	static constexpr int32 ACTION_PRIMARY = 0;
	static constexpr int32 ACTION_GIVE = 1;
	static constexpr int32 ACTION_SELL = 2;
	EInventoryScreenTab GetInventoryTab() const { return InvTab; }
	EInventoryScreenSort GetInventorySort() const { return InvSort; }
	int32 GetInventoryTile() const { return InvSelected; }
	bool IsOnInventoryFilters() const { return bOnInvFilters; }
	const FInventoryScreenPage &GetInventorySide() const { return InvPage; }
	/** Tiles to a row: B-598c, the left column's five. */
	int32 GetGridColumns() const { return WoRLoadoutV2Grid::COLUMNS; }

	// D-248 step 1: the picture grid (WoRLoadoutV2Grid), shown while a weapon slot (Main hand, Off hand) is being filled:
	// the entry's slots stay in the list on the left, the choices are tiles beside it. The specs call these too.
	bool IsGridOpen() const;
	/** B-612: the slots whose choices are the picture grid: the two hands (D-248) and the two sockets (the crystals,
	 *  stones and evolutions that fit, not the inventory's tab). B-614 (mockup v5: the owned pieces as a grid): the
	 *  adapter and the evolution too, so every slot has the kind selector. */
	static bool HasSlotGrid(ELoadoutV2Slot GearSlot);
	/** The tiles as drawn, one per line of GetItems(). */
	TArray<FLoadoutV2GridTile> GetGridTiles() const;
	/** The filter tabs after All: the weapon types among the slot's pieces. */
	const TArray<EWeaponType> &GetGridTypes() const { return GridTypes; }
	/** The arrows in the grid: one tile left / right, one row up / down. */
	void MoveInGrid(int32 DeltaX, int32 DeltaY);
	/** LB / RB in the grid: the next filter tab (All, then each type present), in a ring. */
	void CycleGridFilter(int32 Delta);
	/** A click on a filter tab: 0 is All, then GetGridTypes() in order. */
	void ClickGridTab(int32 GridTab);
	// B-614 (Q-614, mockup v5): the kind selector over a slot's grid, reached by Up from the top row (Left / Right turn
	// it, Down or A back to the tiles, a click picks). A hand, the adapter and the evolution turn the entry's kind, as
	// the window's first question does (the draft of each kind is kept while the window is open); a socket shows its
	// choices of one kind (Crystal, Stone, Evolution), opening on its own (Crystal; the stone socket Stone).
	static TArray<ELoadoutV2PieceKind> GetSlotKinds(ELoadoutV2Slot GearSlot);
	static FText GetPieceKindName(ELoadoutV2PieceKind Kind);
	/** The open grid's kinds; empty with no grid open. */
	TArray<ELoadoutV2PieceKind> GetGridKinds() const;
	ELoadoutV2PieceKind GetGridKind() const;
	/** Why Kind can't be turned to here (the entry kind's refusal; a socket: nothing of it fits); empty when it can. */
	FText GetGridKindRefusal(ELoadoutV2PieceKind Kind) const;
	bool IsOnGridKinds() const { return bOnGridKinds && IsGridOpen(); }
	// B-629b (mockup menus/skills/v1 2): the skill picker's choices are the same grid's tiles (the skill's piece type,
	// its tier, the tick when slotted, the lock when it can't go here); the tabs filter by the weapon type a skill needs.
	// Up from the top row reaches the ‹ Fits this slot  All learned › selector (Left / Right turn it, Down or A back).
	bool IsSkillGrid() const { return Mode == ELoadoutV2Mode::SkillPicker; }
	/** A grid whose tiles are the lit line (Selected): a slot's or the skill picker's; not the inventory's. */
	bool IsPickGrid() const { return IsGridOpen() || IsSkillGrid(); }
	int32 GetPickTileCount() const { return IsSkillGrid() ? SkillChoices.Num() : Items.Num(); }
	bool IsOnSkillFits() const { return bOnGridKinds && IsSkillGrid(); }
	/** Left / Right on the selector: the next kind that can be turned to, in a ring (the others are skipped). */
	void TurnGridKind(int32 Delta);
	/** A click on a word of the selector (an index of GetGridKinds()); a refused one says why. */
	void PickGridKind(int32 Index);

	const FLoadoutV2WeaponsPage &GetPage() const { return Page; }
	ELoadoutV2Mode GetMode() const { return Mode; }
	bool IsAdding() const { return Mode == ELoadoutV2Mode::WindowKind || Mode == ELoadoutV2Mode::WindowSlots || Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk; }
	const TArray<FLoadoutV2WindowItem> &GetItems() const { return Items; }
	const TArray<ELoadoutV2Slot> &GetSlots() const { return Slots; }
	const TArray<FLoadoutV2DetailLine> &GetDetails() const { return Details; }
	int32 GetSelected() const { return Selected; }
	const FText &GetMessage() const { return Message; }
	/** The window's draft (the specs read it). */
	const FLoadoutV2Draft &GetDraft() const { return Draft; }
	const FText &GetStandingNote() const { return StandingNote; }
	const FLoadoutSavedList &GetSavedList() const { return Saved; }
	ELoadoutV2Tab GetTab() const { return Tab; }
	const FLoadoutV2InventoryPage &GetInventoryPage() const { return InventoryPage; }
	const TArray<FLoadoutScreenRow> &GetApparelRows() const { return ApparelRows; }
	bool IsRenaming() const { return bRenaming; }
	static bool IsSimpleView() { return bSimpleView; }
	/** UI_Theme 5a: the last sound cue asked for (noted even when no sound is set, so the specs read it). */
	EWoRMenuCue GetLastCue() const { return LastCue; }
	/** UI_Theme 5a: how many times the whole view was redrawn, and how many times only the selection (the specs read these). */
	int32 GetFullRefreshCount() const { return FullRefreshCount; }
	int32 GetLightRefreshCount() const { return LightRefreshCount; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent &InFocusEvent) override;
	virtual void NativeOnDeactivated() override;
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
	// UI_Theme 5a: our own hold-to-repeat. A direction key acts once when pressed (the system's repeats are ignored);
	// while it stays down it repeats after WoRMenuFeel::HOLD_REPEAT_FIRST_SECONDS, then every HOLD_REPEAT_NEXT_SECONDS.
	void StartHold(const FKey &Key, int32 DeltaX, int32 DeltaY);
	void StopHold();
	/** The core ticker's step while a direction is held; false ends it (the key was let go, or the screen left). */
	bool TickHold(float DeltaSeconds);
	FKey HeldKey;
	FIntPoint HeldDirection = FIntPoint::ZeroValue;
	FWoRHoldRepeat HoldRepeat;
	FTSTicker::FDelegateHandle HoldTicker;

	// UI_Theme 5a: a sound on every action: a move, a confirm, a back, a refusal. Soft references, set in
	// DefaultGame.ini under [/Script/world_of_refraction.LoadoutV2ScreenWidget] (MoveSound=/Game/...); unset: silent.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> MoveSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> ConfirmSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> BackSound;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> RefusedSound;
	/** The cues loaded so far, kept so they stay loaded. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> LoadedCueSounds;
	EWoRMenuCue LastCue = EWoRMenuCue::None;
	void PlayCue(EWoRMenuCue Cue);
	/** What the keys are on, as one number: a move is heard only when it changed. */
	uint32 GetFocusKey() const;
	/** A on the lit item would only be refused (a greyed line, a locked tile): the Refused cue, not Confirm. */
	bool IsLitRefused() const;

	// UI_Theme 5a: motion. The highlight fades to the new item, a pressed item dips, the grid glides to its lit row.
	// One core ticker runs them, only while something is moving (TickFeel); the next input cuts any of them short.
	struct FItemFade
	{
		TWeakObjectPtr<UBorder> Item;
		FLinearColor From;
		FLinearColor To;
		float Elapsed = 0.0f;
	};
	struct FItemPress
	{
		TWeakObjectPtr<UBorder> Item;
		/** The colour it had, and the darker one it wears while down. */
		FLinearColor Normal;
		FLinearColor Darkened;
		float Elapsed = 0.0f;
	};
	/** The lit line or tile: what A is pressed on. Null with nothing lit, or headless. */
	UBorder *GetLitItem() const;
	/** The item goes darker and a little lower for WoRMenuFeel::PRESS_SECONDS, then comes back. */
	void PressItem(UBorder *Item);
	void StartFeelTicker();
	bool TickFeel(float DeltaSeconds);
	/** Where the grid's scroll is going (a glide's end), else where it is: the card is placed against this. */
	float GetGridScrollTarget() const;
	TArray<FItemFade> ItemFades;
	TArray<FItemPress> ItemPresses;
	static constexpr float GRID_GLIDE_IDLE = -1.0f;
	float GridGlideFrom = 0.0f;
	float GridGlideTo = 0.0f;
	/** How long the grid has been gliding; GRID_GLIDE_IDLE when it isn't. */
	float GridGlideElapsed = GRID_GLIDE_IDLE;
	FTSTicker::FDelegateHandle FeelTicker;

	// UI_Theme 5a: a move redraws only what changed.
	/** The light view, for when only the selection or the hover moved: the lines and tiles are recoloured (their text
	 *  and pictures stay), the lit one is scrolled into view, and the card is filled again. */
	void RefreshSelection();
	/** After the mouse moved: the full view when a chip or a filter tab lit or went out, else the light one. */
	void RefreshHover();
	/** The card (the details box), the compare line, the message and the hints. */
	void RefreshBox();
	/** The lit line and the lit tile, brought into view when they changed. */
	void ScrollToSelection();
	FLinearColor GetLineColor(int32 Index, bool bSlotGrid) const;
	FLinearColor GetTileColor(int32 Index, bool bLocked, bool bSlotGrid) const;
	/** Gives a line or a tile its colour, only when it differs from what it has: it fades to it (HIGHLIGHT_FADE_SECONDS). */
	void SetItemColor(UBorder *Item, const FLinearColor &Color);
	int32 FullRefreshCount = 0;
	int32 LightRefreshCount = 0;
	/** The chips and filter tabs the mouse is over, as one number, and the one the full view last drew (RefreshHover). */
	uint32 GetHoverChipKey() const;
	uint32 HoverChipKey = 0;
	/** The shown member's loadout component, inventory and active loadout; false when the member isn't here. */
	bool FindActive(ULoadoutComponent *&OutLoadout, UInventoryComponent *&OutInventory, const FCombatLoadout *&OutActive) const;

	/** The list's lines in the current mode. */
	int32 GetLineCount() const;
	FText GetLineName(int32 Line) const;
	/** D-248 (Crown): the quiet line under a piece's name: "Sword (F)". Empty where the line is not a weapon or adapter. */
	FText GetLineSubName(int32 Line) const;
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
	/** The line the details box shows: the selected one. B-598 (Crown): it no longer follows the mouse (no hover pop-ups);
	 *  a click selects a line, so the box still shows what is picked. */
	int32 GetBoxLine() const { return Selected; }
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
	/** B-633: the window's state for a new entry (Entry null) or an edit, with the kinds read for the grid's selector. */
	bool PrepareWindow(const FLoadoutV2Entry *Entry);
	/** B-633 (Q-633, mockup v5): the first fitting kind's first slot grid, the ‹ Weapon · Adapter · Evolution › selector above. */
	void OpenWindowForNew();
	/** A on an entry (B-531, D-230; B-633): its own kind's first slot grid; the selector turns it to another kind. */
	void OpenWindowForEntry(const FLoadoutV2Entry &Entry);
	/** B-531: the window edits an entry with a piece of another kind (it replaces the entry once a piece is picked). */
	bool IsReplacingKind() const;
	/** B-633 (Q-633): a picked piece goes into the loadout at once (a new evolution asks first; a refusal says why). */
	void CommitPick();
	/** B-633: after a pick went in, the window stays open on that entry as the loadout now holds it. */
	void StayOnAppliedEntry(const FLoadoutV2Draft &Applied);
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

	// D-249 piece 1: the inventory's side.
	/** The loadout is at a tab's top level (its entries, the Skills tab's entries): the only place the keys may leave it. */
	bool IsLoadoutAtTopLevel() const { return Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillRows; }
	/** The grid shows the inventory's tab. B-598c: on the Inventory page only (the loadout's pages show their list, or a
	 *  slot's choices while one is being filled). */
	bool IsInventoryGrid() const { return !bLoadoutOpen; }
	/** B-612: the grid is a socket's choices (a crystal, stone or evolution goes in on A). */
	bool IsSocketGrid() const;
	/** The inventory's page for the shown member, tab, filter and sort, read fresh. */
	void RefreshInventorySide();
	/** The inventory's tiles as grid tiles (FInventoryScreenTile to FLoadoutV2GridTile). */
	TArray<FLoadoutV2GridTile> GetInventoryTiles() const;
	/** The tile the card shows on the inventory's side: the selected one (B-598: not the one under the mouse). */
	int32 GetInventoryBoxTile() const { return InvSelected; }
	/** The inventory's side has the card: the keys are there and no slot is being filled. */
	bool IsInventoryBox() const { return !bOnLoadoutSide && IsInventoryGrid(); }
	void SelectInventoryTab(EInventoryScreenTab NewTab);
	void SelectInventoryFilter(int32 Index);
	/** B-598c: shows page NewPage (0 Inventory, then the loadout's tabs) at its top level. */
	void ShowPage(int32 NewPage);
	/** B-598c: the keys are above the inventory's tiles (the filter row or the selector): no tile is lit. */
	bool IsAboveInventoryTiles() const { return bOnInvFilters || bOnInvKinds; }
	/** B-598c: the Details column on the right: what the page is about (the character and loadout, or the lit piece). */
	void RefreshRightPanel();
	/** The lit weapon of a hand's grid against the one it would replace (Compare, and the Details column); false when
	 *  there is none to set beside it. */
	bool BuildEquipCompare(FLoadoutV2Compare &OutResult, FText &OutEquippedName) const;
	bool bLoadoutOpen = false;
	bool bOnLoadoutSide = false;
	bool bOnInvFilters = false;
	bool bOnInvKinds = false;
	EInventoryScreenTab InvTab = EInventoryScreenTab::Gear;
	EInventoryScreenFilter InvFilter = EInventoryScreenFilter::All;
	EInventoryScreenSort InvSort = EInventoryScreenSort::Kind;
	TArray<EInventoryScreenFilter> InvFilters;
	FInventoryScreenPage InvPage;
	int32 InvSelected = 0;
	int32 HoveredInvTile = INDEX_NONE;
	int32 HoveredInvTab = INDEX_NONE;
	/** The columns the tiles were last laid out in (their rows and columns are set again when it changes). */
	int32 GridColumnsLaidOut = 0;
	/** B-598c: the Gear / Pouch selector's words (one chip each, in one plate). */
	WoRMenuStyle::FChipRow InvTabChips;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> InvKindsPlate;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> InvKindUnderlines;
	// B-598c: the Details column, and the middle panel's picture of the lit piece.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RightSubText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RightNoteText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RightLines;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RightLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RightValues;
	/** B-598c piece 2: the Details column's buttons (a plate each: its key and verb, a reason under a greyed one). */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RightButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RightButtonPlates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RightButtonTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RightButtonReasons;
	TArray<FLoadoutScreenDetail> InvFacts;
	TArray<FInventoryAction> InvActions;
	/** Give to…: the buttons are the receivers; GiveSelected is the lit one. */
	bool bGiving = false;
	int32 GiveSelected = 0;
	/** Equip on…: the piece to light when a slot's grid opens (cleared on the Inventory page and when the window closes). */
	FGuid PendingEquipGear;
	/** The lit tile's facts and buttons, read fresh (RefreshDetails). */
	void RefreshInventoryActions();
	/** A on the lit button: Equip on…, or give to the lit receiver. */
	void PressInventoryButton(int32 Button);
	int32 FindRightButtonUnder(const FVector2D &ScreenPosition) const;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> BoxPictureFrame;
	UPROPERTY(Transient)
	TObjectPtr<UImage> BoxPicture;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LoadoutTabsPanel;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> LoadoutButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutButtonText;

	// D-248 step 1: the grid.
	/** After the slot's pieces are listed: the types present, the piece count, then the filter (only its type stays). */
	void ApplyGridFilter();
	/** Shows or hides the grid and gives the list and the grid their widths; fills the tabs and tiles when shown. */
	void RefreshGrid(bool bShown);
	/** D-248 step 2: where the details box is drawn. While the grid shows it is the card beside the lit (or hovered)
	 *  tile (WoRLoadoutV2Grid::PlaceCard), in a light frame; otherwise it sits right of the list, as before. Called
	 *  once the box's lines are written, so the card's size is the one it will have.
	 *  OBSOLETE (B-598c: the box is the fixed middle column, not on a canvas, so this returns at once; delete with
	 *  WoRLoadoutV2Grid::PlaceCard after Crown's PIE pass). */
	void PlaceBox(bool bGrid);
	/** One more RefreshView on the next frame (once; not a Tick), asked for only before the screen's first draw, when
	 *  its own height is not known yet (UI_Theme 5a: otherwise the card is measured and placed the same frame). The
	 *  settling refresh asks for no further one. */
	void SettleViewNextFrame();
	bool bSettlePending = false;
	bool bSettlingView = false;
	/** The grid was scrolled by the wheel or its bar: the card is placed again beside its tile. */
	UFUNCTION()
	void HandleGridScrolled(float CurrentOffset);
	int32 FindTileUnder(const FVector2D &ScreenPosition) const;
	int32 FindGridTabUnder(const FVector2D &ScreenPosition) const;
	/** Debug: the grid as shown (WoRLoadoutV2Grid::GetGridString), logged when it opens or its filter changes. */
	void LogGrid() const;
	/** B-629b: the skill picker's filter tabs from its choices, and the choices cut to the lit tab's weapon type. */
	void FilterSkillGrid();
	TArray<EWeaponType> GridTypes;
	bool bGridFiltered = false;
	EWeaponType GridFilter = EWeaponType::Sword;
	/** Every piece the slot lists, stacks counted in full, before the filter ("14 weapons"). */
	int32 GridPieceCount = 0;
	/** The tile the grid was last scrolled to, so only a change scrolls it. */
	uint32 GridScrolledKey = 0;
	int32 HoveredGridTab = INDEX_NONE;
	// B-614: the kind selector over a slot's grid.
	/** WindowSlots' A: the slot at Index opens (its grid, or its list), its choices read fresh. */
	void OpenSlot(int32 Index);
	/** Turns the entry to Kind (the window's first question's answer), keeping the draft it had, and opens its first slot. */
	void TurnEntryKind(ELoadoutV2DraftKind Kind);
	bool IsSocketSlot() const;
	bool bOnGridKinds = false;
	/** A socket's kind shown; set when the socket opens. */
	ELoadoutV2PieceKind SocketKind = ELoadoutV2PieceKind::Crystal;
	/** The kinds among the socket's choices before the kind is applied (the others are refused on the selector). */
	TArray<ELoadoutV2PieceKind> SocketKindsPresent;
	/** Each entry kind's draft as it was left by a turn of the selector (cleared when the window opens). */
	TMap<ELoadoutV2DraftKind, FLoadoutV2Draft> KindDrafts;
	WoRMenuStyle::FChipRow GridKindChips;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> GridKindsPlate;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> GridKindUnderlines;

	// B-482: the mouse.
	/** The shown list line under ScreenPosition; INDEX_NONE for none. */
	int32 FindLineUnder(const FVector2D &ScreenPosition) const;
	/** What the mouse is over, shown lit; true when it changed. */
	bool SetHovered(int32 MemberChip, int32 TabChip, int32 Line);

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
	/** The entry whose slots are shown (SkillRows, SkillPicker). */
	int32 SkillEntry = 0;
	// B-629: the ‹ entry › selector over the slots (one word: the lit entry; the title says which of how many).
	WoRMenuStyle::FChipRow SkillEntryChips;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> SkillEntryPlate;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> SkillEntryUnderlines;
	// B-629: the picker's ‹ Fits this slot  All learned › selector.
	bool bSkillsAllLearned = false;
	WoRMenuStyle::FChipRow SkillFitsChips;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> SkillFitsPlate;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> SkillFitsUnderlines;
	/** The Details column's buttons on the Skills tab's slots: Change this slot, Take it out, Short / full. */
	TArray<FInventoryAction> SkillButtons;
	/** The buttons the Details column shows now (the Inventory page's, or the Skills tab's). */
	const TArray<FInventoryAction> &GetRightActions() const;
	/** B-610: set while slots opened from the Equipment tab are open (JumpToEntrySkills). */
	int32 SkillsReturnLine = INDEX_NONE;
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
	FText StandingNote;
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
	/** Crown (2026-11-18): a list longer than its panel scrolls inside it. ListScroll holds ListBox; ListSizer caps the
	 *  panel at ListArea's height (the space between the header and the footer), read once the screen has been drawn. */
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ListScroll;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ListSizer;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ListArea;
	/** The selection and list the view was last scrolled to (RefreshView), so only a change scrolls it. */
	uint32 ScrolledViewKey = 0;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LineRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineNames;
	/** D-248: the quiet line under a row's name ("Sword (F)"), and the name column's box (narrower beside the grid). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineSubNames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> LineNameColumns;
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

	// D-248 step 1: the grid's panel between the list and the box (hidden outside the grid), and its parts. Tabs and
	// tiles are made the first time they are needed and refilled after, never rebuilt (as the list's rows).
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> GridSizer;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BoxSizer;
	/** D-248 step 2: the thin frame around the details box; light while the box is the card beside a tile. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BoxFrame;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridTitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridCountText;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> GridTabsRow;
	/** The grid's header line (the slot or tab, the count): with the tabs row, what stands above the tiles (PlaceBox). */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> GridHeaderRow;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> GridTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> GridTabIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GridTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> GridScroll;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> GridTilePanel;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> GridTiles;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> GridTileIcons;
	/** The type in a word, shown on a tile whose type has no picture in the icon table. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GridTileWords;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> GridTileTicks;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> GridTileLocks;
	/** B-598c piece 4: each tile's F badge (a picked fight item). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> GridTileFights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GridTileCounts;
	/** B-629b: each tile's tier badge (a skill's; a piece's tier stays in its text, D-248). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> GridTileTiers;
	/** The legend's words: a slot's "worn", the skill picker's "slotted already". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridLegendWorn;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridLegendLock;
};
