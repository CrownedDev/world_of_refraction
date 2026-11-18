// LoadoutV2Model.h
// B-476 (D-206, D-209, D-210, D-211; UIScreens.md #2-3, mockups menus/loadout/v2), first part: the model the v2 loadout
// screen reads, headless and testable. The Weapons tab is a list of entries (a weapon or a same-type pair, an adapter,
// the character's own evolution as one line) with an "Add weapon" frame at the bottom showing the cells left; only the
// cells used show at the top. The Add gear window builds one entry (a draft: Main hand, Off hand, Augment socket 1 and 2)
// whose refusals are said in the game's words, and X Compare sets the equipped piece beside the highlighted one.
// The widget that draws this is ULoadoutV2ScreenWidget (B-476b); the first screen's widget was retired by B-488, its
// model (LoadoutScreenModel) stays: the v2 screen reads it.
// B-476c: what the Add gear window asks first (BuildAddKinds), the Adapter and Evolution lists, an entry edited in its
// own place (DraftFromEntry, CheckEntryEdit, ReplaceEntry) or taken off (RemoveEntry, RemoveAllGear), and the details
// box's lines (BuildEntryDetails: full or simple view, UIScreens B-479c).

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "UI/WoRIcon.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"

struct FCombatLoadout;
class UAbilityData;

/** A socket as the v2 screen draws it: an icon only (UIScreens: crystals a hexagon, stones a pebble, evolutions a star). */
enum class ELoadoutV2SocketIcon : uint8
{
	Empty,
	Crystal,
	Stone,
	Evolution
};

enum class ELoadoutV2EntryKind : uint8
{
	/** A weapon, or a same-type pair (dual wield). */
	Weapon,
	Adapter,
	/** The character's own evolution: one line, its name only. */
	Evolution
};

/** Where an entry lives in today's loadout shape (the legacy fields the slots mirror). */
enum class ELoadoutV2Place : uint8
{
	Primary,
	Secondary,
	/** The adapter list (AdapterIndex): a Resonator's adapters, a Generic's adapter entry. */
	Adapter,
	/** The character's own evolution (the primary slot as an evolution). */
	Seat
};

/** B-485: what an icon stands for (B-487: the shared FWoRIcon, UI/WoRIcon.h). The widget asks the icon table for it;
 *  where the table has no such icon, the text the icon replaces stays. */
using ELoadoutV2IconKind = EWoRIconKind;
using FLoadoutV2Icon = FWoRIcon;

struct FLoadoutV2Entry
{
	ELoadoutV2EntryKind Kind = ELoadoutV2EntryKind::Weapon;
	ELoadoutV2Place Place = ELoadoutV2Place::Primary;
	int32 AdapterIndex = INDEX_NONE;
	/** Names only: "Ashen Blade + Cinder Edge", "Quarry Maul", a ring's name. */
	FText Name;
	/** Augment socket 1 and 2 (none for the evolution line). */
	TArray<ELoadoutV2SocketIcon> Sockets;
	/** B-485: the weapon-type icon at the row's start (a weapon entry), and the two sockets as badges. */
	FLoadoutV2Icon TypeIcon;
	TArray<FLoadoutV2Icon> SocketBadges;
	/** B-482b: what augment socket 1 holds, by name ("Garnet", an evolution's name); empty when the socket is. The row
	 *  shows it in brackets after the name; it is the same name the details box prints (one source: the entry's socket). */
	FText SocketName;
	/** B-516b (D-223): the piece's tier, the badge beside the name (a pair: its main hand's); it tells two same-named
	 *  pieces apart before a number does. */
	EItemTier Tier = EItemTier::F_Tier;
	/** The weapon in hand (marked only when there are two weapon entries). */
	bool bActive = false;
	/** A weapon entry's skill slots (D-211 for a pair), 0 otherwise. */
	int32 SkillSlots = 0;
};

struct FLoadoutV2WeaponsPage
{
	TArray<FLoadoutV2Entry> Entries;
	int32 CellsUsed = 0;
	int32 CellsBudget = 0;
	/** "Cells used 4 of 5". */
	FText CellsLine;
	/** The Add weapon frame: "1/5 left". */
	FText AddFrameLine;
	/** False once the class's entries are all taken (D-210: Generic 2 weapons + 1 adapter, Resonator 1 + 5, Refractor 1). */
	bool bCanAdd = false;
};

/** What the Add gear window asks first (D-210 default: weapon or adapter), or the character's own evolution. */
enum class ELoadoutV2DraftKind : uint8
{
	Weapon,
	Adapter,
	Evolution
};

/** One entry being built in the Add gear window. */
struct FLoadoutV2Draft
{
	ELoadoutV2DraftKind Kind = ELoadoutV2DraftKind::Weapon;
	/** Main hand (sets the stance); a Weapon draft. */
	FWeaponInventoryEntry MainHand;
	/** Off hand: a weapon of the same type that can dual wield (D-209); empty for a weapon alone. */
	FWeaponInventoryEntry OffHand;
	/** An Adapter draft's adapter. */
	FRingInventoryEntry Adapter;
	/** Augment socket 1: a crystal, an evolution or a stone. Socket 2: a stone (D-210). Also an Evolution draft's
	 *  evolution sits in Socket1. */
	FRuntimeAttachedItem Socket1;
	FRuntimeAttachedItem Socket2;
	/** An Evolution draft: the owned evolution's id (the character's slot keeps it, as today's screen does). */
	FGuid EvolutionInstance;
};

/** A greyed or open line in an Add gear list: the piece and why it can't go in (empty: it fits). */
struct FLoadoutV2OffHandChoice
{
	FWeaponInventoryEntry Weapon;
	FText Refusal;
};

/** B-476b: a line of the Add gear window's Main hand list for a new weapon entry: the piece, why it can't be confirmed
 *  (empty: Y Confirm works) and the cells used once it is in. */
struct FLoadoutV2MainHandChoice
{
	FWeaponInventoryEntry Weapon;
	FText Refusal;
	int32 CellsAfter = 0;
};

/** X Compare: the equipped piece beside the highlighted one (only their own numbers and built-ins, never the player's
 *  slotted picks). */
struct FLoadoutV2Compare
{
	int32 EquippedPower = 0;
	int32 SelectedPower = 0;
	/** Always 0 for a weapon swap: the gear is free (D-206) and the sockets stay with the entry. */
	int32 CellsChange = 0;
	TArray<FText> BuiltInsGained;
	TArray<FText> BuiltInsLost;
};

/** B-476c: one of the Add gear window's first choices (Weapon, Adapter, Evolution), greyed with the reason when the
 *  class has no free place for it now. */
struct FLoadoutV2KindChoice
{
	ELoadoutV2DraftKind Kind = ELoadoutV2DraftKind::Weapon;
	FText Name;
	FText Refusal;
};

/** B-476c: a line of the Add gear window's Adapter or Evolution list: the piece, a short line saying what it is, why it
 *  can't be confirmed (empty: Y Confirm works) and the cells used once it is in. */
struct FLoadoutV2PieceChoice
{
	ELoadoutV2DraftKind Kind = ELoadoutV2DraftKind::Adapter;
	FRingInventoryEntry Adapter;
	FEvolutionInventoryEntry Evolution;
	FText Name;
	FText Line;
	FText Refusal;
	int32 CellsAfter = 0;
};

/** B-476c: what a line of the details box is (the widget styles each kind). */
enum class ELoadoutV2DetailKind : uint8
{
	/** A piece's name and tier ("Quarry Maul  C"). */
	Piece,
	/** Label / value ("Power" / "50"). */
	Stat,
	/** One line of the effect tree (Depth indents it) or, in the simple view, one short form. */
	Effect,
	/** A socket: Label the socket, Value what is in it. */
	Socket,
	/** "Skill slots" / n. */
	SkillSlots
};

struct FLoadoutV2DetailLine
{
	ELoadoutV2DetailKind Kind = ELoadoutV2DetailKind::Stat;
	int32 Depth = 0;
	FText Label;
	FText Value;
	/** B-485: the icon beside the line (a piece's weapon type, a socket's badge, an effect's kind), and a Piece line's
	 *  tier for its badge (Value is the tier's letter, the text kept when there is no badge). */
	FLoadoutV2Icon Icon;
	bool bHasTier = false;
	EItemTier Tier = EItemTier::F_Tier;
	/** B-487b: a Piece line's type in a word ("Sword", "Ring", "Crystal"): the quiet line above its name, in place of
	 *  a "Type" row. A filled Socket line also carries its tier (bHasTier), for "Garnet (F)" beside its badge. */
	FText TypeName;
};

namespace WoRLoadoutV2
{
	/** B-485: a socket as an icon: its crystal's or evolution's element badge, a stone, or the empty socket. */
	WORLD_OF_REFRACTION_API FLoadoutV2Icon MakeSocketIcon(const FRuntimeAttachedItem &Item);

	/** B-530 (D-229): an adapter's icon: the socket frame with its crystal's or evolution's badge inside; empty inside
	 *  with nothing (or a stone) socketed. */
	WORLD_OF_REFRACTION_API FLoadoutV2Icon MakeAdapterIcon(const FRingInventoryEntry &Adapter);

	/** B-485: the tier of what a socket holds; false for an empty socket. */
	WORLD_OF_REFRACTION_API bool GetAttachmentTier(const FRuntimeAttachedItem &Item, EItemTier &OutTier);

	/** The Weapons tab for the class (only the gear the class fights with). */
	WORLD_OF_REFRACTION_API FLoadoutV2WeaponsPage BuildWeaponsPage(const FCombatLoadout &Loadout, ECharacterClass Class);

	/** Puts the draft into the loadout's next free place for its kind (a weapon: the primary, then a Generic's second
	 *  weapon; an adapter: a Refractor's primary, else the adapter list; an evolution: the character's own slot, when the
	 *  primary is free). False with the reason when there is no free place for the class (D-210's entry limits). The
	 *  entry's sockets replace the pieces' own. CheckDraft runs this on a copy and adds the other refusals. */
	WORLD_OF_REFRACTION_API bool ApplyDraft(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, FText &OutRefusal);

	/** Why the draft can't be confirmed (empty: Y Confirm works): no free place, a pair the rules refuse, a dual-only
	 *  weapon alone, an evolution where the class can't put one, a non-stone in socket 2, the cells over the budget.
	 *  OutCellsAfter is the cells used once it is in. */
	WORLD_OF_REFRACTION_API FText CheckDraft(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, int32 &OutCellsAfter);

	/** The Off hand list for MainHand: every owned weapon but MainHand, greyed with the reason it can't pair. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2OffHandChoice> BuildOffHandChoices(const FWeaponInventoryEntry &MainHand, const TArray<FWeaponInventoryEntry> &OwnedWeapons,
																		 ECharacterClass Class);

	/** B-476b: the Main hand list for a new weapon entry: every owned weapon the loadout doesn't hold, each checked as a
	 *  draft (CheckDraft), so the ones that can't go in stay listed with the reason. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2MainHandChoice> BuildMainHandChoices(const FCombatLoadout &Loadout, ECharacterClass Class,
																			   const TArray<FWeaponInventoryEntry> &OwnedWeapons);

	/** Crown's PIE pass (2026-11-18): MainHand goes in the draft's main hand with its own sockets (Q-476c), and an off
	 *  hand that no longer pairs with it comes off (the new one can't be dual wielded, or is another type). A shield as
	 *  the main hand keeps the off hand, so it stays refused. True when the off hand came off. */
	WORLD_OF_REFRACTION_API bool SetMainHand(FLoadoutV2Draft &Draft, const FWeaponInventoryEntry &MainHand);

	/** A weapon row's name with its type after it: "Simple Sword · Sword". */
	WORLD_OF_REFRACTION_API FText GetWeaponRowName(const FText &Name, const UWeaponData *Weapon);

	/** X Compare of two weapons: power, the cells (a weapon swap changes none, D-206), and the built-in abilities gained
	 *  and lost. */
	WORLD_OF_REFRACTION_API FLoadoutV2Compare BuildCompare(const FWeaponInventoryEntry &Equipped, const FWeaponInventoryEntry &Selected);

	/** B-476c: the Add gear window's first question, Weapon / Adapter / Evolution, each refused with the reason when the
	 *  class can't add one now (D-210's entry limits; one evolution per character). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2KindChoice> BuildAddKinds(const FCombatLoadout &Loadout, ECharacterClass Class);

	/** B-476c: the Adapter list for a new entry: every owned adapter the loadout doesn't hold, each checked as a draft
	 *  (its own sockets come with it). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2PieceChoice> BuildAdapterChoices(const FCombatLoadout &Loadout, ECharacterClass Class,
																		   const TArray<FRingInventoryEntry> &OwnedAdapters);

	/** B-476c: the Evolution list for the character's own evolution: every owned evolution, those InUse (on gear or in a
	 *  loadout) refused, the rest checked as a draft (2 cells). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2PieceChoice> BuildEvolutionChoices(const FCombatLoadout &Loadout, ECharacterClass Class,
																			 const TArray<FEvolutionInventoryEntry> &Evolutions, const TSet<FGuid> &InUse);

	/** B-476c: an owned evolution as a socket item (its tier, wear and roll go with it). */
	WORLD_OF_REFRACTION_API FRuntimeAttachedItem MakeEvolutionAttachment(const FEvolutionInventoryEntry &Owned);

	/** B-476c: the entry as a draft (A Edit): its pieces and the sockets they carry. */
	WORLD_OF_REFRACTION_API FLoadoutV2Draft DraftFromEntry(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry);

	/** B-531 (D-230): the draft kind an entry of this kind is built as. */
	WORLD_OF_REFRACTION_API ELoadoutV2DraftKind GetDraftKind(ELoadoutV2EntryKind Kind);

	/** B-531 (D-230): Edit on a worn entry asks the same first question as Add (BuildAddKinds): the entry's own kind is
	 *  always open (edit or replace the piece); another kind is judged as if the entry were out, and greyed with why. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2KindChoice> BuildEditKinds(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry);

	/** B-476c: writes the draft in Entry's own place (a weapon entry's hands, an adapter, the character's evolution).
	 *  B-531: a draft of another kind replaces the entry: it comes out and the draft goes in, all or nothing (the first
	 *  entry is never left empty, D-116). False with the reason, Loadout unchanged. */
	WORLD_OF_REFRACTION_API bool ReplaceEntry(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, const FLoadoutV2Draft &Draft,
											  FText &OutRefusal);

	/** B-476c: CheckDraft for an edit of Entry: why the draft can't replace it (empty: Y Confirm works), and the cells
	 *  used after. */
	WORLD_OF_REFRACTION_API FText CheckEntryEdit(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry,
												 const FLoadoutV2Draft &Draft, int32 &OutCellsAfter);

	/** B-476c: A Remove on an entry's main piece takes the entry out. The first entry never empties (D-116): a
	 *  Generic's second weapon moves up into it, otherwise it is refused with the reason. The pieces keep their
	 *  sockets (nothing is destroyed). */
	WORLD_OF_REFRACTION_API bool RemoveEntry(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, FText &OutRefusal);

	/** B-476c, Y Options' Remove all gear: every entry but the first (D-116) comes out. The count taken out. */
	WORLD_OF_REFRACTION_API int32 RemoveAllGear(FCombatLoadout &Loadout, ECharacterClass Class);

	/** B-476c: the details box for Entry: each piece (name, tier, type, power, scaling), its effects (the ET-50 tree, or
	 *  one short form a line in the simple view), the built-ins, the sockets and the skill slots. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildEntryDetails(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry, bool bSimple);

	/** B-484: one piece's details on its own, as the box shows them while a slot's list is open. A weapon reads as the
	 *  Weapons tab shows a single-weapon entry: its stats, effects, built-ins, its own sockets and its skill slots. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildWeaponDetails(const FWeaponInventoryEntry &Weapon, bool bSimple);
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildAdapterDetails(const FRingInventoryEntry &Adapter, bool bSimple);
	/** A crystal, a stone or an evolution: its name and tier, its element; in the full view what it is and its cells. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildAttachmentDetails(const FRuntimeAttachedItem &Item, bool bSimple);

	/** B-503u (Crown): the Edit window's box on an Augment socket row: what is in it, or (empty) one line saying what
	 *  it takes: socket 1 a crystal, an evolution or a stone; socket 2 a stone (D-210). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildSocketDetails(const FRuntimeAttachedItem &Item, bool bFirstSocket, bool bSimple);

	/** B-503u: the box on the Off hand row: the weapon's own details, or (empty) one line saying what can go there. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2DetailLine> BuildOffHandDetails(const FWeaponInventoryEntry &OffHand, bool bSimple);

	/** B-503u: one plain line for the box (an empty slot's note). */
	WORLD_OF_REFRACTION_API FLoadoutV2DetailLine MakeNoteLine(const FText &Text);

	/** Debug: the details box as text. */
	WORLD_OF_REFRACTION_API FString GetDetailsString(const TArray<FLoadoutV2DetailLine> &Lines);

	/** Debug: the page as text (wor.Loadout.PrintV2). */
	WORLD_OF_REFRACTION_API FString GetWeaponsPageString(const FLoadoutV2WeaponsPage &Page);
}
