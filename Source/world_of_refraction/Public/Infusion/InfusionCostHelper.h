// InfusionCostHelper.h
// Pure cost calculations for the infusion system.
// All queries — what HP would I lose, what wear would the crystal take,
// would this kill me — flow through this single helper. No mutation.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/ItemTier.h"
#include "InfusionCostHelper.generated.h"

/**
 * Pure cost calculations for infusion actions.
 *
 * All functions are stateless and side-effect-free. Used by:
 *  - ActionExecutor::ApplyCommitCosts at action commit time
 *  - UI cost preview rendering (Phase 3)
 *  - HP-kill confirmation modal (Phase 5)
 *
 * Source-cost routing (which sources pay HP vs. durability vs. status) is
 * the caller's concern — this helper just answers "what would the cost be?"
 * for each cost type independently.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UInfusionCostHelper : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * HP integer that would be deducted for an infused action (rework 6-2-3c).
     * Derived from the infused EP cost: (InfusedEnergyCost / MaxEP) × MaxHP, then
     * reduced by Resistance. InfusedEnergyCost MUST be the PRE-Efficiency, stat-
     * scaled EP (Base × charge-mult × (1 + stat surcharge), WITHOUT the Efficiency
     * multiplier) so Efficiency mitigates EP only and Resistance mitigates HP only.
     *
     * NOT floored — infusion CAN kill the caster. The full cost is returned here and
     * deducted at FinalizeAsyncAction (after the infused effect resolves). Use
     * WouldKill to check whether the cost is lethal and require the kill-confirmation
     * modal before committing.
     *
     * @param Actor             Actor that would pay the cost. Must have UCharacterDataComponent.
     * @param InfusedEnergyCost Pre-Efficiency stat-scaled infused EP. <= 0 returns 0.
     * @return                  HP integer to deduct. 0 if Actor/component null or MaxEP <= 0.
     */
    UFUNCTION(BlueprintPure, Category = "Infusion|Cost")
    static int32 CalculateHPCost(AActor *Actor, int32 InfusedEnergyCost);

    /**
     * Would the EP-derived HP cost reduce the actor's CURRENT HP to 0 or below?
     *
     * Uses the SAME unrounded basis as CalculateHPCost (so it cannot mis-predict the
     * kill). CalculateHPCost is not floored, so a cost reported lethal here WILL kill
     * the caster at finalize. Used by the HP-kill confirmation modal.
     *
     * @param Actor             Actor that would pay the cost.
     * @param InfusedEnergyCost Pre-Efficiency stat-scaled infused EP (same as CalculateHPCost).
     * @return                  True if the unrounded cost would reduce CURRENT HP to <= 0.
     */
    UFUNCTION(BlueprintPure, Category = "Infusion|Cost")
    static bool WouldKill(AActor *Actor, int32 InfusedEnergyCost);

    /**
     * Durability wear that would be applied to a crystal for an infused action.
     *
     * Thin wrapper over UBreakCalculator::CalculateDurabilityWear, kept here so
     * all infusion cost queries flow through one helper. Centralises the formula
     * lookup if it ever needs to evolve (e.g. re-adding the dropped custom-spell
     * wear modifier).
     *
     * @param CrystalTier   Tier of the slotted crystal taking the wear.
     * @param ActionTier    Tier of the action being committed.
     * @param Level         Infusion level (0/1/2).
     * @param bIsSpell      True if the action is a spell, false for ability/attack.
     * @return              Wear to apply. Identical to BreakCalculator's result.
     */
    UFUNCTION(BlueprintPure, Category = "Infusion|Cost")
    static int32 CalculateDurabilityCost(
        EItemTier CrystalTier,
        EItemTier ActionTier,
        int32 Level,
        bool bIsSpell);
};