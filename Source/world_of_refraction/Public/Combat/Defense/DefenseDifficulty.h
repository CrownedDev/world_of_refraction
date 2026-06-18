// DefenseDifficulty.h
// Per-impact defense difficulty (melee per-impact rework). An authored-per-impact tier
// that scales the defender's input window for a single impact — harder impacts shrink
// the reaction window. Cluster 1: enum + multiplier helper only; nothing reads it until
// the per-impact window read is wired (cluster 4).

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"
#include "DefenseDifficulty.generated.h"

/**
 * Difficulty tier for a single impact's defense window.
 *
 * Inherit = "not authored on this impact — fall back" (to the skill-level default, then
 * ultimately to Easy). It is the natural default for unauthored impacts, so it is listed
 * FIRST (value 0). This is a brand-new enum with no serialized data yet, so ordering is
 * free; keep it append-only from here on.
 */
UENUM(BlueprintType)
enum class EDefenseDifficulty : uint8
{
	Inherit UMETA(DisplayName = "Inherit (fall back)"),
	Easy UMETA(DisplayName = "Easy"),
	Medium UMETA(DisplayName = "Medium"),
	Hard UMETA(DisplayName = "Hard"),
	Impossible UMETA(DisplayName = "Impossible")
};

/**
 * Per-defense-type difficulty for one impact: an independent tier for each of the three
 * defense actions (a skill can make Parry hard but Block easy). All default Inherit —
 * unauthored falls back (to the skill-level DefaultDifficulty, then Easy). Authored on
 * USkillDataBase (per-impact overrides + a skill-level default).
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseDifficultyTriple
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense Difficulty")
	EDefenseDifficulty Parry = EDefenseDifficulty::Inherit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense Difficulty")
	EDefenseDifficulty Dodge = EDefenseDifficulty::Inherit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense Difficulty")
	EDefenseDifficulty Block = EDefenseDifficulty::Inherit;
};

/**
 * Window multiplier for a difficulty tier. Easy = x1.0 (the no-change anchor), Medium/Hard
 * shrink the window per CombatConstants. Inherit resolves to Easy (x1.0) as the TERMINAL
 * fallback — if an Inherit ever reaches the multiplier (i.e. the caller didn't resolve it
 * against a skill-level default first), it must not shrink the window. Pure type->float.
 */
inline float DefenseDifficultyMultiplier(EDefenseDifficulty D)
{
	switch (D)
	{
	case EDefenseDifficulty::Easy:
		return CombatConstants::DEFENSE_DIFFICULTY_EASY;
	case EDefenseDifficulty::Medium:
		return CombatConstants::DEFENSE_DIFFICULTY_MEDIUM;
	case EDefenseDifficulty::Hard:
		return CombatConstants::DEFENSE_DIFFICULTY_HARD;
	case EDefenseDifficulty::Impossible:
		return CombatConstants::DEFENSE_DIFFICULTY_IMPOSSIBLE;
	case EDefenseDifficulty::Inherit:
	default:
		return CombatConstants::DEFENSE_DIFFICULTY_EASY; // terminal fallback — never shrinks
	}
}

/**
 * Resolve one impact field against the skill-level default: the impact's own tier wins if
 * authored; otherwise the default; if BOTH are Inherit, Easy (the terminal fallback). The
 * return value is ALWAYS concrete (never Inherit) — so a resolved table carries no Inherit.
 */
inline EDefenseDifficulty ResolveInheritedDifficulty(EDefenseDifficulty Impact, EDefenseDifficulty Default)
{
	if (Impact != EDefenseDifficulty::Inherit)
	{
		return Impact;
	}
	if (Default != EDefenseDifficulty::Inherit)
	{
		return Default;
	}
	return EDefenseDifficulty::Easy;
}
