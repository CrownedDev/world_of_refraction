// TrialGate.h
// B-183 (RunStructure §3, Q-183): what the trial door asks before a run. A variation changes
// the boss (and its story, no rule twists); the difficulty scales enemy strength and the payout
// through one named multiplier per level.

#pragma once

#include "CoreMinimal.h"
#include "AI/EAIDifficulty.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Inventory/ItemTier.h"
#include "TrialGate.generated.h"

class AActor;
class UCharacterData;

/** Q-183: the deckbuilder's three levels. */
UENUM(BlueprintType)
enum class ETrialDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FTrialVariation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation", meta = (MultiLine = true))
	FText Description;

	/** The boss this variation fights: a variant of the lord or someone related to them.
	 *  TODO: read by the boss arena trigger (B-188). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation")
	TObjectPtr<UCharacterData> Boss = nullptr;
};

namespace TrialGate
{
	constexpr int32 DIFFICULTY_COUNT = 3;
	constexpr ETrialDifficulty DEFAULT_DIFFICULTY = ETrialDifficulty::Medium;

	/** Enemies' damage, × this (B-293c: no longer their max HP, see GetEnemyHealthMultiplier). */
	float GetEnemyStrengthMultiplier(ETrialDifficulty Difficulty);

	/** B-293c (Q-293c, Expedition 33): enemies' max HP for a difficulty. 1 at every level: the difficulty scales how
	 *  hard enemies hit (GetEnemyStrengthMultiplier), not how long they last. */
	float GetEnemyHealthMultiplier(ETrialDifficulty Difficulty);

	/** B-344 (D-136): the enemy AI tier a trial difficulty sets: Easy, Medium, Hard (Expert stays for bosses' later
	 *  phases, B-193). */
	EAIDifficulty GetEnemyAIDifficulty(ETrialDifficulty Difficulty);

	/** B-344 (D-136, D-044): a fight's enemy AI tier: the trial's, unless the encounter overrides it. */
	EAIDifficulty ResolveEncounterAIDifficulty(ETrialDifficulty TrialDifficulty, bool bOverride, EAIDifficulty Override);

	/** The run's payout (D-006), × this. */
	float GetPayoutMultiplier(ETrialDifficulty Difficulty);

	FText GetDifficultyName(ETrialDifficulty Difficulty);

	/** The level after Difficulty, wrapping (the placeholder door cycles through them). */
	ETrialDifficulty NextDifficulty(ETrialDifficulty Difficulty);

	/** Gives Enemy the level's strength for this fight: permanent, uncleansable max-HP and damage
	 *  buffs (or debuffs below 1), then a full refill to the new max. No-op at 1. Returns false when Enemy
	 *  has no character data or the effect manager is missing. */
	bool ApplyEnemyStrength(AActor *Enemy, float StrengthMultiplier);

	/** B-293c: ApplyEnemyStrength with max HP and damage scaled apart (each a no-op at 1). The single-value form
	 *  calls this with the same value for both. */
	bool ApplyEnemyStrength(AActor *Enemy, float HealthMultiplier, float DamageMultiplier);

	/** The rank's max HP multiplier (B-224, BaselineNumbers.md): the EnemyRankHealth tuning ladder, else
	 *  Normal 1 / Elite 1.5 / Mini-boss 3 / Boss 6. */
	float GetRankHealthMultiplier(EEncounterRank Rank);

	/** B-224b: the enemy's tier power for its HP: its Combat Tier (B-325, WoRCombatTier) through the TierPower ladder; x1 at F. */
	float GetEnemyTierHealthMultiplier(const AActor *Enemy);

	/** Gives Enemy its rank's and its tier's HP on top of ApplyEnemyStrength (HP only, no damage): a permanent,
	 *  uncleansable max-HP buff, then a full refill. MaxHP percents add up, so the share is Strength x (rank x tier
	 *  power - 1), making the total Strength x rank x tier power. No-op when both are x1. False as ApplyEnemyStrength. */
	bool ApplyEnemyRankHealth(AActor *Enemy, float StrengthMultiplier, EEncounterRank Rank);

	/** B-223 (D-102): a normal or elite enemy's chance to drop a pillar level: the LevelDropChance ladder at its
	 *  tier, doubled for an elite, at most 1. 0 for a mini-boss or boss (they always give, GetGuaranteedLevels). */
	float GetLevelDropChance(EItemTier EnemyTier, EEncounterRank Rank);

	/** B-223 (D-102): levels a mini-boss or boss always gives, by its tier (MiniBossLevels ladder); 0 below. */
	int32 GetGuaranteedLevels(EItemTier EnemyTier, EEncounterRank Rank);

	/** B-223: the pillar levels one defeated enemy gives: its guaranteed levels, else one on a battle-random
	 *  roll under its drop chance. */
	int32 RollLevelDrops(const UObject *WorldContext, EItemTier EnemyTier, EEncounterRank Rank);
}
