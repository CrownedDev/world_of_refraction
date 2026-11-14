// LoadoutV2Model.h
// B-476 (D-206, D-209, D-210, D-211; UIScreens.md #2-3, mockups menus/loadout/v2), first part: the model the v2 loadout
// screen reads, headless and testable. The Weapons tab is a list of entries (a weapon or a same-type pair, an adapter,
// the character's own evolution as one line) with an "Add weapon" frame at the bottom showing the cells left; only the
// cells used show at the top. The Add gear window builds one entry (a draft: Main hand, Off hand, Augment socket 1 and 2)
// whose refusals are said in the game's words, and X Compare sets the equipped piece beside the highlighted one.
// The widget that draws this is B-476b; today's screen (LoadoutScreenModel / ULoadoutScreenWidget) stays until then.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Equipment/FRuntimeAttachedItem.h"
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

struct FLoadoutV2Entry
{
	ELoadoutV2EntryKind Kind = ELoadoutV2EntryKind::Weapon;
	ELoadoutV2Place Place = ELoadoutV2Place::Primary;
	int32 AdapterIndex = INDEX_NONE;
	/** Names only: "Ashen Blade + Cinder Edge", "Quarry Maul", a ring's name. */
	FText Name;
	/** Augment socket 1 and 2 (none for the evolution line). */
	TArray<ELoadoutV2SocketIcon> Sockets;
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
};

/** A greyed or open line in an Add gear list: the piece and why it can't go in (empty: it fits). */
struct FLoadoutV2OffHandChoice
{
	FWeaponInventoryEntry Weapon;
	FText Refusal;
};

/** X Compare: the equipped piece beside the highlighted one (only their own numbers and built-ins, never the player's
 *  slotted picks). */
struct FLoadoutV2Compare
{
	int32 EquippedPower = 0;
	int32 SelectedPower = 0;
	int32 CellsChange = 0;
	TArray<FText> BuiltInsGained;
	TArray<FText> BuiltInsLost;
};

namespace WoRLoadoutV2
{
	/** The Weapons tab for the class (only the gear the class fights with). */
	WORLD_OF_REFRACTION_API FLoadoutV2WeaponsPage BuildWeaponsPage(const FCombatLoadout &Loadout, ECharacterClass Class);

	/** Puts the draft into the loadout's next free place for its kind (a weapon: the primary, then a Generic's second
	 *  weapon; an adapter: a Refractor's primary, else the adapter list; an evolution: the character's own slot, when the
	 *  primary is free). False with the reason when there is no free place. */
	WORLD_OF_REFRACTION_API bool ApplyDraft(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, FText &OutRefusal);

	/** Why the draft can't be confirmed (empty: Y Confirm works): no free place, a pair the rules refuse, a dual-only
	 *  weapon alone, an evolution where the class can't put one, a non-stone in socket 2, the cells over the budget.
	 *  OutCellsAfter is the cells used once it is in. */
	WORLD_OF_REFRACTION_API FText CheckDraft(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, int32 &OutCellsAfter);

	/** The Off hand list for MainHand: every owned weapon but MainHand, greyed with the reason it can't pair. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutV2OffHandChoice> BuildOffHandChoices(const FWeaponInventoryEntry &MainHand, const TArray<FWeaponInventoryEntry> &OwnedWeapons,
																		 ECharacterClass Class);

	/** X Compare of two weapons: power, the cells the draft's sockets don't change (weapons are free, D-206), and the
	 *  built-in abilities gained and lost. */
	WORLD_OF_REFRACTION_API FLoadoutV2Compare BuildCompare(const FWeaponInventoryEntry &Equipped, const FWeaponInventoryEntry &Selected);

	/** Debug: the page as text (wor.Loadout.PrintV2). */
	WORLD_OF_REFRACTION_API FString GetWeaponsPageString(const FLoadoutV2WeaponsPage &Page);
}
