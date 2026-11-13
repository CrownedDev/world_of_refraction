// LoadoutScreenModel.h
// B-440a (D-181, D-180, UIScreens #2, mockups screen_mockups/menus/loadout/): what the loadout screen's gear page
// shows, with no widget in it, so the specs read it headless. ULoadoutScreenWidget draws it.
//
// Read only for now: changing gear, sockets, skills and saved loadouts are B-440b to B-440e.
// Debug: wor.UI.PrintLoadout logs GetLoadoutScreenString (every party member's gear page).

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"

class UWorld;
struct FCombatLoadout;
struct FLoadoutSlot;

enum class ELoadoutScreenRowKind : uint8
{
	Weapon,
	Adapter,
	/** An evolution on its own in the character's slot. */
	EvolutionSeat,
	Apparel,
	EmptyApparel
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
};

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

	/** "Bruiser (2 of 5)". */
	WORLD_OF_REFRACTION_API FText FormatLoadoutTitle(const FString &LoadoutName, int32 Index, int32 Count);

	WORLD_OF_REFRACTION_API FString GetLoadoutScreenString(UWorld *World);
}
