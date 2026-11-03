// TrialGate.h
// B-183 (RunStructure §3, Q-183): what the trial door asks before a run. A variation changes
// the boss (and its story, no rule twists); the difficulty scales enemy strength and the payout
// through one named multiplier per level.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Encounter/EEncounterRank.h"
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

	/** Enemies' max HP and damage, × this. */
	float GetEnemyStrengthMultiplier(ETrialDifficulty Difficulty);

	/** The run's payout (D-006), × this. */
	float GetPayoutMultiplier(ETrialDifficulty Difficulty);

	FText GetDifficultyName(ETrialDifficulty Difficulty);

	/** The level after Difficulty, wrapping (the placeholder door cycles through them). */
	ETrialDifficulty NextDifficulty(ETrialDifficulty Difficulty);

	/** Gives Enemy the level's strength for this fight: permanent, uncleansable max-HP and damage
	 *  buffs (or debuffs below 1), then a full refill to the new max. No-op at 1. Returns false when Enemy
	 *  has no character data or the effect manager is missing. */
	bool ApplyEnemyStrength(AActor *Enemy, float StrengthMultiplier);

	/** The rank's max HP multiplier (B-224, BaselineNumbers.md): the EnemyRankHealth tuning ladder, else
	 *  Normal 1 / Elite 1.5 / Mini-boss 3 / Boss 6. */
	float GetRankHealthMultiplier(EEncounterRank Rank);

	/** B-224b: the enemy's tier power for its HP (UCharacterData::EnemyTier through the TierPower ladder; x1 at F). */
	float GetEnemyTierHealthMultiplier(const AActor *Enemy);

	/** Gives Enemy its rank's and its tier's HP on top of ApplyEnemyStrength (HP only, no damage): a permanent,
	 *  uncleansable max-HP buff, then a full refill. MaxHP percents add up, so the share is Strength x (rank x tier
	 *  power - 1), making the total Strength x rank x tier power. No-op when both are x1. False as ApplyEnemyStrength. */
	bool ApplyEnemyRankHealth(AActor *Enemy, float StrengthMultiplier, EEncounterRank Rank);
}
