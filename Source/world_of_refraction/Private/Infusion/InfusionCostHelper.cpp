// InfusionCostHelper.cpp
// Implementations of pure cost calculations for the infusion system.

#include "Infusion/InfusionCostHelper.h"
#include "Infusion/InfusionConstants.h"
#include "Equipment/Durability/BreakCalculator.h"
#include "Character/CharacterDataComponent.h"

namespace
{
    /** Map a level (0/1/2) to its HP cost percent. Anything outside the range returns 0. */
    float GetHPCostPercentForLevel(int32 Level)
    {
        switch (Level)
        {
        case 1:
            return InfusionConstants::CHARGE_L1_HP_COST_PERCENT;
        case 2:
            return InfusionConstants::CHARGE_L2_HP_COST_PERCENT;
        default:
            return 0.0f;
        }
    }
}

int32 UInfusionCostHelper::CalculateHPCost(AActor *Actor, int32 Level)
{
    if (!Actor || Level <= 0)
    {
        return 0;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return 0;
    }

    const float CostPercent = GetHPCostPercentForLevel(Level);
    const float RawCost = CharComp->CurrentHP * CostPercent;

    // Design changed: infusion CAN kill the caster (1-HP floor removed). The full
    // cost is deducted at FinalizeAsyncAction — AFTER the infused effect resolves —
    // so a lethal cost lands only once the action has fired.
    return FMath::RoundToInt(RawCost);
}

bool UInfusionCostHelper::WouldKill(AActor *Actor, int32 Level)
{
    if (!Actor || Level <= 0)
    {
        return false;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return false;
    }

    // Unrounded check — reports the underlying threat regardless of the 1-HP floor
    // applied in CalculateHPCost. Used by the HP-kill confirmation modal (Phase 5).
    const float CostPercent = GetHPCostPercentForLevel(Level);
    const float RawCost = CharComp->CurrentHP * CostPercent;
    return RawCost >= static_cast<float>(CharComp->CurrentHP);
}

int32 UInfusionCostHelper::CalculateDurabilityCost(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 Level,
    bool bIsSpell)
{
    return UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, Level, bIsSpell);
}