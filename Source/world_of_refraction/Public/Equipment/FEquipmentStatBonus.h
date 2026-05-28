// FEquipmentStatBonus.h
// Shared stat-bonus struct used by weapons and rings.
//
// Lives on:
//  - UEquipmentDataBase::BaseStatBonus       (designer-authored baseline; weapons + rings)
//  - UEquipmentDataBase::GeneratedStatBonus  (generator-rolled layer; weapons + rings)
//  - FWeaponInventoryEntry::StatBonus        (per-instance runtime copy)
//  - FRingInventoryEntry::StatBonus          (per-instance runtime copy)
//
// At inventory creation (FWeaponInventoryEntry::CreateFromWeapon /
// FRingInventoryEntry::CreateFromRing), the asset's field-wise sum of
// BaseStatBonus + GeneratedStatBonus (see UEquipmentDataBase::GetCombinedStatBonus)
// is copied into the entry's StatBonus. The per-instance copy is what should
// drive runtime stat queries; the asset-side layers are the starting state
// for new entries.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "FEquipmentStatBonus.generated.h"

/**
 * FEquipmentStatBonus
 * Bag of bonus fields populated by tier-budgeted reroll.
 *
 * Substat budgets (capacity points): F=6, E=10, D=15, C=21, B=28, A=36, S=45.
 * Pillar budgets (percent magnitude): F=3, E=5, D=8, C=12, B=18, A=25, S=33.
 *
 * Roll model:
 *  - RerollSubstats wipes the 13 substat fields and redistributes one full
 *    tier budget using zero-sum broken-stick: net signed sum of changes
 *    equals the budget; some fields land negative.
 *  - RerollPillars mirrors the above for the 3 pillar percent fields.
 *
 * The pending-pool / cap-gate lives on the caller (UEquipmentDataBase) — the
 * struct itself just wipes and redistributes when asked.
 *
 * Per-stat caps and field clamps live in CombatConstants — see
 * SUBSTAT_CAP_FRACTION / PILLAR_CAP_FRACTION / CRYSTAL_BONUS_MIN/MAX.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEquipmentStatBonus
{
    GENERATED_BODY()

    // ==================== BONUS FIELDS (16) ====================
    // 13 substat fields filled by zero-sum distribution of one full tier
    // substat budget, plus 3 pillar percent fields filled from one full
    // tier pillar budget. Per-field clamps below.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusRawDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusSpellDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusEfficiency = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusStatusMultiplier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21.0", ClampMax = "21.0"))
    float BonusCritChance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusSpellSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusDefense = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusActionSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusMaxHP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusMaxEnergy = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusResistance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusTurnSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "-21", ClampMax = "21"))
    int32 BonusLuck = 0;

    // ==================== PILLAR PERCENT BONUSES (3) ====================
    // Multiplicative percent layered on top of crystal pillar modifiers via
    // UCharacterDataComponent::GetEvolutionModified{Mind,Body,Spirit}.
    // Per-slot range: -15% to +15%. Negative values supported (cursed gear /
    // set-bonus tradeoffs). Multi-slot summing applies — see
    // ULoadoutComponent::GetActiveStatBonus for class-specific stacking rules.
    // NOT counted toward tier substat capacity — designer-tuned per-asset.
    // See CombatConstants::PILLAR_MODIFIER_MIN/MAX (literals must mirror).

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses|Pillar", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
    float BonusMindModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses|Pillar", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
    float BonusBodyModifierPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses|Pillar", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
    float BonusSpiritModifierPercent = 0.0f;

    // ==================== BUDGET / QUERIES ====================

    /** Tier → substat capacity budget. Delegates to EquipmentBonusGen::GetSubstatBudget
     *  (defined in EquipmentBonusGenerator.h to keep CombatConstants.h free of
     *  EItemTier dependencies). */
    static int32 GetSubstatBudget(EItemTier Tier);

    // ==================== MUTATORS ====================

    /** Wipe the 13 substat fields and redistribute one full tier budget
     *  using zero-sum broken-stick:
     *    1. Split the budget across Mind/Body/Spirit using FPillarWeights.
     *    2. Within each pillar, distribute positives AND negatives so that
     *       net signed change per pillar equals the pillar's share.
     *    3. Each field's |delta| capped at (pillar_share × SUBSTAT_CAP_FRACTION).
     *       Resulting field value clamped to CRYSTAL_BONUS_MIN/MAX (±21).
     *
     *  Returns the budget redistributed (0 on no-op). */
    int32 RerollSubstats(EItemTier Tier, const struct FPillarWeights &Weights);

    /** Wipe the 3 pillar fields and redistribute one full tier pillar budget
     *  using zero-sum broken-stick:
     *    - Random magnitudes (broken-stick across 3 pillars), random per-pillar
     *      negative offset, net signed sum = budget.
     *    - Per-pillar |delta| capped at (budget × PILLAR_CAP_FRACTION).
     *    - Resulting field clamped to PILLAR_MODIFIER_MIN/MAX (±15%).
     *
     *  Returns the budget redistributed (0.0f on no-op). */
    float RerollPillars(EItemTier Tier);
};
