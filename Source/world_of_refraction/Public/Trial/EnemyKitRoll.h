// EnemyKitRoll.h
// B-568 (D-225, TrialPoolDesign §5 and §9 stage 4, the roll): an enemy's kit for the run, from its template
// (FEnemyKit: always has + limits) and the trial's pool, seeded so a retried fight, a resumed run and a co-op partner
// meet the same kit.
//   Order (§5): 1 the always-has pieces; 2 more pieces from the pool, inside the limits and the class cap; 3 a crystal
//   for each adapter (an adapter with no crystal does nothing; weapons stay empty, as §8's examples count their slots);
//   4 the always-has skills, then skills the gear can use, up to the slots and the skill limit.
//   The always-has part is the same on every enemy of the type; the open part is rolled per enemy (its own seed).
// shortcut: evolutions and apparel are not rolled or worn yet (always-has ones are skipped with a log); add them when
// a fight can wear an enemy's evolution or apparel without the pool.
// Debug: wor.Trial.PrintEnemyKits (every encounter on the map, each as a fight on its own), GetRolledKitString.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Trial/TrialPool.h"

class UAbilityData;
class UCharacterData;
class UEquipmentDataBase;
class USpellData;
class UTrialData;
struct FEncounterZoneInfo;
enum class ESpellElement : uint8;

/** One worn piece: a weapon or an adapter asset at its tier, with its socket. */
struct FRolledKitPiece
{
	TObjectPtr<UEquipmentDataBase> Piece = nullptr;
	EItemTier Tier = EItemTier::F_Tier;
	/** Type None: an empty socket. */
	FCrystalId Crystal;
	/** From the always-has list (the template's part), not the pool. */
	bool bAlwaysHas = false;
};

struct FRolledEnemyKit
{
	TArray<FRolledKitPiece> Pieces;
	/** In pick order: the always-has skills first. */
	TArray<TObjectPtr<UAbilityData>> Abilities;
	TArray<TObjectPtr<USpellData>> Spells;

	bool HasGear() const { return Pieces.Num() > 0; }
};

/** What the roll reads besides the template. */
struct FEnemyKitRollInput
{
	/** TrialLoot::GatherPool of the fight's pool tag (TrialPool::GetPoolTag). */
	TArray<UObject *> Pool;
	ESpellElement TrialElement;
	FTrialTierRange ZoneRange;
	int32 Seed = 0;
};

namespace EnemyKitRoll
{
	/** The fight's encounters, sorted and joined (TrialRunSubsystem's pending encounter sources). */
	WORLD_OF_REFRACTION_API FString MakeEncounterKey(TArray<FName> EncounterIds);
	/** Run seed, encounter key, the template and which of that template in the team it is (0, 1, 2 for three
	 *  bandits): stable across retries and resumes, different per enemy. */
	WORLD_OF_REFRACTION_API int32 MakeSeed(int32 RunSeed, const FString &EncounterKey, const UCharacterData *Template, int32 Occurrence);

	WORLD_OF_REFRACTION_API FRolledEnemyKit Roll(const UCharacterData &Template, const FEnemyKitRollInput &Input);

	/** The roll's input for a fight in Zone of Trial (no seed): its pool, empty when there is none. */
	WORLD_OF_REFRACTION_API FEnemyKitRollInput MakeInput(const UTrialData *Trial, const FEncounterZoneInfo &Zone);
	/** Each member's kit, in team order, each with its own seed. All empty (the fight keeps the authored inventories)
	 *  when the pool is empty: a fight outside a trial, or a trial with no pool. */
	WORLD_OF_REFRACTION_API TArray<FRolledEnemyKit> RollTeam(TConstArrayView<UCharacterData *> Team, FEnemyKitRollInput Input, int32 RunSeed,
															 const FString &EncounterKey);

	/** Skill slots the pieces give: a weapon by its tier, an adapter by its crystal's tier (else its own). */
	WORLD_OF_REFRACTION_API int32 GetSlotCount(const FRolledEnemyKit &Kit);

	/** "Arming Sword D; Adapter Ring F [Garnet C] | Lunge, Heavy Strike" with asset names, so same-named skills differ. */
	WORLD_OF_REFRACTION_API FString GetRolledKitString(const FRolledEnemyKit &Kit);
}
