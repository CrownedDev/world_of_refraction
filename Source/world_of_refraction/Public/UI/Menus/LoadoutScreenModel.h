// LoadoutScreenModel.h
// B-440a (D-181, D-180, UIScreens #2, mockups screen_mockups/menus/loadout/): what the loadout screen's gear page
// shows, with no widget in it, so the specs read it headless. ULoadoutScreenWidget draws it.
//
// B-440b: a gear row's place takes another owned weapon or adapter (BuildGearChoices, ApplyGearChoice), or is emptied
// (UnequipRow). B-440c: a socket takes a carried gem or stone or an owned evolution (BuildSocketChoices,
// ApplySocketChoice), or is emptied (TakeOffSocket). Skills and saved loadouts are B-440d, B-440e.
// Debug: wor.UI.PrintLoadout logs GetLoadoutScreenString (every party member's gear page).

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Skills/Definitions/ESpellElement.h"

class UInventoryComponent;
class ULoadoutComponent;
class USkillDataBase;
class UWorld;
struct FApparelEntry;
struct FCombatLoadout;
struct FLoadoutSlot;
struct FEvolutionInventoryEntry;
struct FRingInventoryEntry;
struct FWeaponInventoryEntry;

enum class ELoadoutScreenRowKind : uint8
{
	Weapon,
	Adapter,
	/** An evolution on its own in the character's slot. */
	EvolutionSeat,
	Apparel,
	EmptyApparel,
	/** A free weapon or adapter place (B-440b): A fills it. */
	EmptyGear,
	/** B-440d, the Skills page: a filled skill or spell slot, and an empty one. */
	Skill,
	EmptySkill
};

/** The loadout place a gear row fills (B-440b): A Change and X Unequip act on it. */
enum class ELoadoutScreenPlace : uint8
{
	None,
	/** The weapon, adapter or evolution every loadout keeps (D-116). */
	Primary,
	/** A Generic's second weapon. */
	Secondary,
	/** One of a Resonator's adapters (AdapterIndex), or a new one (INDEX_NONE). */
	Adapter
};

/** One socket on a gear row: what it takes, and what sits in it. */
struct FLoadoutScreenSocket
{
	/** "Crystal", "Evolution", "Stone". */
	FText Label;
	/** The thing in the socket; empty when bEmpty. */
	FText Name;
	ESpellElement Element = ESpellElement::None;
	bool bEmpty = true;
	/** An evolution sits in it (it goes back to the owner when taken off; a gem or stone is destroyed). */
	bool bEvolution = false;
};

/** B-440c: a gear row's two sockets, as indices into FLoadoutScreenRow::Sockets. */
namespace LoadoutScreenSockets
{
	constexpr int32 CRYSTAL = 0;
	constexpr int32 STONE = 1;
}

/** One line of the detail panel: "Power" / "42". */
struct FLoadoutScreenDetail
{
	FText Label;
	FText Value;
};

struct FLoadoutScreenRow
{
	ELoadoutScreenRowKind Kind = ELoadoutScreenRowKind::Weapon;
	FText Name;
	/** "Weapon · Sword · with shield", "Adapter · Ring", "Apparel". */
	FText KindLine;
	FText Tier;
	/** The weapon in hand (or the adapter in use), shown only when the loadout has more than one. */
	bool bActive = false;
	TArray<FLoadoutScreenSocket> Sockets;
	/** Apparel never takes budget (D-164). */
	bool bCountsToBudget = true;
	int32 Cost = 0;
	/** "Weapon 1 + Crystal 1 = 2", or "Outside budget". */
	FText CostLine;
	TArray<FLoadoutScreenDetail> Details;
	/** A rule worth saying on this row; may be empty. */
	FText Note;
	ELoadoutScreenPlace Place = ELoadoutScreenPlace::None;
	int32 AdapterIndex = INDEX_NONE;
	/** The owned piece in the place; invalid for an evolution seat or an empty place. */
	FGuid GearID;
	/** B-440d, a Skills page row: its slot in the list (an innate spell's index, a weapon's or adapter's slot), the skill
	 *  in it (null for an empty slot), and whether the player put it there (X brings back what it replaced). */
	int32 SkillSlot = INDEX_NONE;
	USkillDataBase *Skill = nullptr;
	bool bPick = false;
};

/** One line of the gear picker (B-440b): an owned piece for the row's place, or "put in hand". */
struct FLoadoutScreenChoice
{
	/** The piece as a gear row describes it. */
	FLoadoutScreenRow Row;
	/** Hold the row's own weapon instead of the other (a Generic's two weapons). */
	bool bMakeActive = false;
	/** The piece to put in: one of the two is valid. */
	FWeaponLoadoutEntry Weapon;
	FRingLoadoutEntry Adapter;
	/** B-440c, a socket line: the carried gem or stone (Crystal), or the owned evolution (EvolutionID), and the
	 *  attachment it makes (for the budget after it). */
	FCrystalId Crystal;
	FGuid EvolutionID;
	FRuntimeAttachedItem Attachment;
	/** The budget spent after the change, shown before it is confirmed. */
	int32 NewSpent = 0;
	/** Why it can't go in; empty when it can. The line stays in the picker. */
	FText Refusal;
};

struct FLoadoutScreenPage
{
	/** "Bruiser (2 of 5)". */
	FText LoadoutTitle;
	int32 Budget = 0;
	int32 Spent = 0;
	TArray<FLoadoutScreenRow> Rows;
	/** The first apparel row (the "Apparel" heading sits above it). */
	int32 FirstApparelRow = INDEX_NONE;
	/** Why there are no rows (the member isn't in this level); empty otherwise. */
	FText Refusal;
};

struct FLoadoutScreenMember
{
	FName Key;
	FText Name;
	ECharacterClass Class = ECharacterClass::Generic;
};

namespace WoRLoadoutScreen
{
	/** The local party, in order: the character tabs. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenMember> BuildMembers(const UWorld *World);

	/** The gear page of one loadout: the evolution seat, each weapon and adapter the fight reads, then apparel.
	 *  The title is left empty (the caller knows the loadout's place among the saved ones). */
	WORLD_OF_REFRACTION_API FLoadoutScreenPage BuildGearPage(const FCombatLoadout &Loadout, ECharacterClass Class);

	/** The gear page of a party member's active loadout, read from its spawned inventory. */
	WORLD_OF_REFRACTION_API FLoadoutScreenPage BuildGearPage(UWorld *World, const FLoadoutScreenMember &Member);

	/** One weapon's or adapter's row (name, tier, kind line, sockets, details), as the gear page lists it. The
	 *  inventory screen describes an owned piece with it (B-441). */
	WORLD_OF_REFRACTION_API FLoadoutScreenRow BuildGearRow(const FLoadoutSlot &Slot);

	/** B-440f: an apparel piece as a gear row (its socket, tier, element; outside the budget); GearID is its owned id. */
	WORLD_OF_REFRACTION_API FLoadoutScreenRow BuildApparelRow(const FApparelEntry &Entry);

	/** B-440b: what can take Row's place: "put in hand" first when Row is the weapon not in hand, then every owned
	 *  weapon or adapter the place takes that isn't in this loadout, each with the budget after it. Refused lines stay:
	 *  over the budget, or WornElsewhere (another member wears it). */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildGearChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row,
																	   const TArray<FWeaponInventoryEntry> &OwnedWeapons, const TArray<FRingInventoryEntry> &OwnedAdapters,
																	   const TSet<FGuid> &WornElsewhere);

	/** Puts Choice in Row's place of Loadout (or Row's weapon in hand). False when the place can't take it. */
	WORLD_OF_REFRACTION_API bool PutGearChoice(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, const FLoadoutScreenChoice &Choice);

	/** Empties Row's place of Loadout. The primary can't be empty (D-116): a Generic's second weapon moves up into it,
	 *  otherwise it is refused with the reason. */
	WORLD_OF_REFRACTION_API bool TakeOffGear(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row, FText &OutRefusal);

	/** The member's choices for Row, read from its spawned inventory. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildGearChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row);

	/** Applies Choice to the member's active loadout through ULoadoutComponent::TryChangeGear. Empty on success; else the
	 *  reason, for the player. */
	WORLD_OF_REFRACTION_API FText ApplyGearChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row,
												 const FLoadoutScreenChoice &Choice);

	/** B-474 (D-207), L3 / 6: makes Row's weapon the one in hand (the entry a fight starts with). Empty on success; else
	 *  the reason, for the player. */
	WORLD_OF_REFRACTION_API FText SetRowActive(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row);

	/** X Unequip on the member's active loadout. Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText UnequipRow(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row);

	/** B-440c: what can go in Row's empty Socket (LoadoutScreenSockets): carried gems (the crystal socket) or stones
	 *  (the stone socket), and owned evolutions (the crystal socket), each with the budget after it. Refused lines
	 *  stay: over the budget, a second S-tier (MAX_EQUIPPED_S_TIER_CRYSTALS), an evolution in use. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildSocketChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row,
																		 int32 Socket, const TMap<FCrystalId, int32> &Carried,
																		 const TArray<FEvolutionInventoryEntry> &Evolutions, const TSet<FGuid> &EvolutionsInUse);

	/** Writes Choice's attachment into Row's Socket of Loadout (the preview; the owned piece is changed by
	 *  ApplySocketChoice). False when the row has no such socket. */
	WORLD_OF_REFRACTION_API bool PutSocketChoice(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, int32 Socket, const FLoadoutScreenChoice &Choice);

	/** The member's socket choices, from its pouch and its evolutions. */
	WORLD_OF_REFRACTION_API TArray<FLoadoutScreenChoice> BuildSocketChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row,
																		 int32 Socket);

	/** Sockets Choice into the owned piece (UInventoryComponent::AttachCrystalTo* / AttachEvolutionTo*). Empty on
	 *  success; else the reason. */
	WORLD_OF_REFRACTION_API FText ApplySocketChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, int32 Socket,
												   const FLoadoutScreenChoice &Choice);

	/** What X on Row's Socket would do, said before it is done (Q-440c): a gem and the stone are destroyed together
	 *  (CL-28b), an evolution goes back with wear. Empty when the socket can't be emptied; then OutRefusal says why. */
	WORLD_OF_REFRACTION_API FText GetTakeOffSocketWarning(const FLoadoutScreenRow &Row, int32 Socket, FText &OutRefusal);

	/** Empties Row's Socket on the owned piece. Empty on success; else the reason. */
	WORLD_OF_REFRACTION_API FText TakeOffSocket(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, int32 Socket);

	/** The weapon or adapter in Row's place of Loadout (at most one is set; neither for an empty place). */
	WORLD_OF_REFRACTION_API void FindRowGear(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, FWeaponLoadoutEntry *&OutWeapon,
											 FRingLoadoutEntry *&OutAdapter);

	/** The member's spawned loadout component, with its inventory; null when the member isn't in this level. */
	WORLD_OF_REFRACTION_API ULoadoutComponent *FindMemberLoadout(UWorld *World, const FLoadoutScreenMember &Member, UInventoryComponent *&OutInventory);

	/** "Bruiser (2 of 5)". */
	WORLD_OF_REFRACTION_API FText FormatLoadoutTitle(const FString &LoadoutName, int32 Index, int32 Count);

	WORLD_OF_REFRACTION_API FString GetLoadoutScreenString(UWorld *World);
}
