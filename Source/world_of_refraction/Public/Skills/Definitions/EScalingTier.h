// EScalingTier.h
// Souls-style stat-scaling grades + the per-skill (stat, grade) pair.
// Pure-addition types — nothing consumes them yet (the skill array is stage b,
// the scaling formula is stage b). Distinct from EItemTier (rarity/budget).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStatModifiers.h" // ESubStat
#include "Combat/CombatConstants.h"             // bucket normalization constants
#include "EScalingTier.generated.h"

/** Souls-style stat-scaling grade: how STEEPLY a stat scales a skill's damage (S steepest -> F shallowest).
 *  F is the shallowest REAL scaling, not zero -- a stat simply ABSENT from a skill's scaling array is the
 *  true no-scaling case. Distinct from EItemTier (item rarity/budget) despite the shared letters.
 *  Append-only -- never reorder/remove. */
UENUM(BlueprintType)
enum class EScalingTier : uint8
{
    S = 0  UMETA(DisplayName = "S"),
    A = 1  UMETA(DisplayName = "A"),
    B = 2  UMETA(DisplayName = "B"),
    C = 3  UMETA(DisplayName = "C"),
    D = 4  UMETA(DisplayName = "D"),
    E = 5  UMETA(DisplayName = "E"),
    F = 6  UMETA(DisplayName = "F")
};

/** Linear scaling coefficient for a grade. PLACEHOLDER ladder (Crown tunes later): S 1.0 -> F 0.05.
 *  Used as: perPairBonus = base * GetScalingTierCoefficient(grade) * StatFraction(stat). */
inline float GetScalingTierCoefficient(EScalingTier Tier)
{
    switch (Tier)
    {
        case EScalingTier::S: return 1.0f;
        case EScalingTier::A: return 0.8f;
        case EScalingTier::B: return 0.65f;
        case EScalingTier::C: return 0.5f;
        case EScalingTier::D: return 0.35f;
        case EScalingTier::E: return 0.2f;
        case EScalingTier::F: return 0.05f;
        default:              return 0.0f;
    }
}

/** Debug/inspection string for a scaling grade. */
inline FString GetScalingTierString(EScalingTier Tier)
{
    switch (Tier)
    {
        case EScalingTier::S: return TEXT("S");
        case EScalingTier::A: return TEXT("A");
        case EScalingTier::B: return TEXT("B");
        case EScalingTier::C: return TEXT("C");
        case EScalingTier::D: return TEXT("D");
        case EScalingTier::E: return TEXT("E");
        case EScalingTier::F: return TEXT("F");
        default:              return TEXT("?");
    }
}

/** Shared upside-only stat surcharge fraction: max(0, stat - 1). The SINGLE definition used by BOTH the
 *  infusion cost/effect path (ActionExecutor) and the damage-scaling tier term (DamageCalculator) so the two
 *  never drift. Only multiplier-family stats (baseline >= 1.0: Raw/Spell damage, crit damage, the speeds,
 *  status multiplier) yield a nonzero fraction; %-family stats (Luck/Defense/Resistance/Efficiency, 0..0.5)
 *  return 0 here by design. */
inline float StatFraction(float Stat)
{
    return FMath::Max(0.0f, Stat - 1.0f);
}

/** Normalized scaling fraction for a stat, read in ITS OWN units (Model Y). Each numeric bucket maps to a
 *  ~[0..1] (→~2 with gear) fraction so a tier coefficient weights all stats consistently (S=1.0 adds
 *  ~base×1.0 at max investment). EffectiveValue is the attacker's composed/crystal-aware value for Stat.
 *  Replaces the naive all-stats StatFraction in the damage tier loop — it returned 0 for every non-
 *  multiplier stat (Luck/Defense/Resistance/Efficiency/Reflex). Any stat is authorable (no allowlist). */
inline float GetScalingFraction(ESubStat Stat, float EffectiveValue)
{
    switch (Stat)
    {
        // Bucket A — multiplier-family (1.0-centered): RawDamage, SpellDamage, CritDamage,
        // StatusMultiplier, SpellSpeed, ActionSpeed
        case ESubStat::RawDamage:
        case ESubStat::SpellDamage:
        case ESubStat::CritDamage:
        case ESubStat::StatusMultiplier:
        case ESubStat::SpellSpeed:
        case ESubStat::ActionSpeed:
            return StatFraction(EffectiveValue); // max(0, v - 1)

        // Bucket B — fraction-family [0..0.5]: Defense, Resistance (read via the capped accessor)
        case ESubStat::Defense:
        case ESubStat::Resistance:
            return FMath::Max(0.0f, EffectiveValue / CombatConstants::UNIVERSAL_STAT_CAP); // 0..0.5 -> 0..1

        // Bucket C — Luck (GetEquipmentModifiedLuck already normalized 0..1(..2))
        case ESubStat::Luck:
            return FMath::Max(0.0f, EffectiveValue);

        // Bucket D — Efficiency (inverted: lower multiplier = more investment)
        case ESubStat::Efficiency:
            return FMath::Max(0.0f, 1.0f - EffectiveValue);

        // Bucket E — turn-economy / non-1.0-centered
        case ESubStat::TurnSpeed:
            return FMath::Max(0.0f,
                (EffectiveValue - CombatConstants::TURN_SPEED_BASE) /
                (CombatConstants::TURN_SPEED_CAP - CombatConstants::TURN_SPEED_BASE));
        case ESubStat::Reflex:
            return FMath::Max(0.0f, EffectiveValue / CombatConstants::WINDOW_CAP_SECONDS);

        case ESubStat::None:
        default:
            return 0.0f;
    }
}

/** One stat-scaling grade on a skill: "this skill scales with <Stat> at <Tier> steepness." A skill's full
 *  scaling is the additive sum over a TArray<FStatScaling>. A stat absent from the array does not scale. */
USTRUCT(BlueprintType)
struct FStatScaling
{
    GENERATED_BODY()

    /** Which stat drives this scaling contribution. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
    ESubStat Stat = ESubStat::None;

    /** How steeply that stat scales (S steepest -> F shallowest). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
    EScalingTier Tier = EScalingTier::C;
};
