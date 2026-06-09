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
     * HP integer that would be deducted at the given infusion level.
     * Computed as a percent of the actor's CURRENT HP (not max HP) — matches
     * the original DeductHPCost formula.
     *
     * NOT floored — infusion CAN kill the caster (design changed). The full cost
     * is returned here and deducted at FinalizeAsyncAction (after the infused
     * effect resolves). Use WouldKill to check whether the cost is lethal and
     * require the kill confirmation modal (Phase 5) before committing.
     *
     * @param Actor  Actor that would pay the cost. Must have UCharacterDataComponent.
     * @param Level  Infusion level (0/1/2). Level 0 returns 0.
     * @return       HP integer to deduct. 0 if Actor or its CharacterDataComponent is null.
     */
    UFUNCTION(BlueprintPure, Category = "Infusion|Cost")
    static int32 CalculateHPCost(AActor *Actor, int32 Level);

    /**
     * Would applying the HP cost at this level reduce the actor's HP to 0 or below?
     *
     * CalculateHPCost is no longer floored, so a cost reported here as lethal WILL
     * kill the caster (at finalize). This function reports that threat up-front —
     * i.e. "this cost is large enough that we should warn the player." Used by the
     * HP-kill confirmation modal in Phase 5.
     *
     * @param Actor  Actor that would pay the cost.
     * @param Level  Infusion level (0/1/2).
     * @return       True if the unrounded cost would reduce HP to <= 0.
     */
    UFUNCTION(BlueprintPure, Category = "Infusion|Cost")
    static bool WouldKill(AActor *Actor, int32 Level);

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