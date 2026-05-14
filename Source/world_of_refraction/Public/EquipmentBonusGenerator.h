// EquipmentBonusGenerator.h
// Static factory for FEquipmentStatBonus rolls. Drives the two SpendPending*
// algorithms on the struct by populating the matching pending pools then
// invoking them. Used by:
//  - Loot drop pipelines (when a generated item rolls a stat bonus)
//  - Blueprint callers needing a one-shot full roll without touching the
//    asset's pending/savings pools.
//
// UEquipmentDataBase's CallInEditor debug buttons no longer go through this
// generator — they drive the per-struct pending/savings flow directly.
//
// All methods are static — no per-instance state. Pure functions of the
// input tier + (optional) explicit pillar weights + the underlying RNG.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemTier.h"
#include "CombatConstants.h"
#include "FEquipmentStatBonus.h"
#include "FPillarWeights.h"
#include "EquipmentBonusGenerator.generated.h"

namespace EquipmentBonusGen
{
    /** Tier → substat capacity budget. Reads constexpr values from CombatConstants.
     *  Kept here (not in CombatConstants.h) so CombatConstants stays free of
     *  EItemTier and can remain a leaf-level constants header. */
    inline int32 GetSubstatBudget(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return CombatConstants::SUBSTAT_BUDGET_F;
        case EItemTier::E_Tier: return CombatConstants::SUBSTAT_BUDGET_E;
        case EItemTier::D_Tier: return CombatConstants::SUBSTAT_BUDGET_D;
        case EItemTier::C_Tier: return CombatConstants::SUBSTAT_BUDGET_C;
        case EItemTier::B_Tier: return CombatConstants::SUBSTAT_BUDGET_B;
        case EItemTier::A_Tier: return CombatConstants::SUBSTAT_BUDGET_A;
        case EItemTier::S_Tier: return CombatConstants::SUBSTAT_BUDGET_S;
        default:                return 0;
        }
    }

    /** Tier → pillar magnitude budget. */
    inline float GetPillarBudget(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return CombatConstants::PILLAR_BUDGET_F;
        case EItemTier::E_Tier: return CombatConstants::PILLAR_BUDGET_E;
        case EItemTier::D_Tier: return CombatConstants::PILLAR_BUDGET_D;
        case EItemTier::C_Tier: return CombatConstants::PILLAR_BUDGET_C;
        case EItemTier::B_Tier: return CombatConstants::PILLAR_BUDGET_B;
        case EItemTier::A_Tier: return CombatConstants::PILLAR_BUDGET_A;
        case EItemTier::S_Tier: return CombatConstants::PILLAR_BUDGET_S;
        default:                return 0.0f;
        }
    }
}

UCLASS()
class WORLD_OF_REFRACTION_API UEquipmentBonusGenerator : public UObject
{
    GENERATED_BODY()

public:
    /** Roll a fresh FEquipmentStatBonus for the given tier — distributes one
     *  full substat budget AND one full pillar budget. Equal pillar weights. */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Generator")
    static FEquipmentStatBonus Generate(EItemTier Tier);

    /** Generate with explicit pillar weights driving substat distribution.
     *  Pillar modifier distribution is unaffected by weights (signs random). */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Generator")
    static FEquipmentStatBonus GenerateWeighted(EItemTier Tier, FPillarWeights Weights);

    /** Wipe and reroll the substat fields only — preserves pillar modifiers
     *  and lock flags. */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Generator")
    static void RerollSubstats(UPARAM(ref) FEquipmentStatBonus &Bonus, EItemTier Tier);

    /** Wipe and reroll the pillar modifier fields only — preserves substats
     *  and lock flags. */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Generator")
    static void RerollPillars(UPARAM(ref) FEquipmentStatBonus &Bonus, EItemTier Tier);

    /** Reroll everything — substat + pillar — preserving lock flags. */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Generator")
    static FEquipmentStatBonus Reroll(const FEquipmentStatBonus &Current, EItemTier Tier);

    /** Tier → substat budget delegate. */
    UFUNCTION(BlueprintPure, Category = "Equipment|Generator")
    static int32 GetSubstatBudgetForTier(EItemTier Tier);

    /** Tier → pillar budget delegate. */
    UFUNCTION(BlueprintPure, Category = "Equipment|Generator")
    static float GetPillarBudgetForTier(EItemTier Tier);
};
