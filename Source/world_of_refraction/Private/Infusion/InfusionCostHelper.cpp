// InfusionCostHelper.cpp
// Implementations of pure cost calculations for the infusion system.

#include "Infusion/InfusionCostHelper.h"
#include "Infusion/InfusionConstants.h"
#include "Equipment/Durability/BreakCalculator.h"
#include "Character/CharacterDataComponent.h"

namespace
{
    /** Unrounded EP-derived infusion HP cost (rework 6-2-3c). The infused EP cost as a
     *  fraction of max EP, applied to max HP, then reduced by Resistance — the sole HP
     *  mitigation (Efficiency is already excluded because InfusedEnergyCost is PRE-
     *  Efficiency). Returns 0 when the component is null, has no EP pool, or the cost
     *  is non-positive. Shared by CalculateHPCost (rounds it) and WouldKill (compares). */
    float ComputeRawInfusionHPCost(const UCharacterDataComponent *CharComp, int32 InfusedEnergyCost)
    {
        if (!CharComp || InfusedEnergyCost <= 0 || CharComp->MaxEP <= 0)
        {
            return 0.0f;
        }

        const float EPFraction = static_cast<float>(InfusedEnergyCost) / static_cast<float>(CharComp->MaxEP);
        const float BaseHP = EPFraction * static_cast<float>(CharComp->MaxHP);

        // Resistance reduces the HP cost. Max(0, ...) guards against inversion (a
        // resistance >= 1 zeroes the cost rather than healing); a negative ("cursed")
        // resistance amplifies it, matching how negative stats behave elsewhere.
        const float ResistanceFraction = CharComp->GetEffectiveStats().Resistance;
        return BaseHP * FMath::Max(0.0f, 1.0f - ResistanceFraction);
    }
}

int32 UInfusionCostHelper::CalculateHPCost(AActor *Actor, int32 InfusedEnergyCost)
{
    if (!Actor || InfusedEnergyCost <= 0)
    {
        return 0;
    }

    const UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();

    // NOT floored — infusion CAN kill the caster. The full cost is deducted at
    // FinalizeAsyncAction (AFTER the infused effect resolves), so a lethal cost
    // lands only once the action has fired.
    return FMath::RoundToInt(ComputeRawInfusionHPCost(CharComp, InfusedEnergyCost));
}

bool UInfusionCostHelper::WouldKill(AActor *Actor, int32 InfusedEnergyCost)
{
    if (!Actor || InfusedEnergyCost <= 0)
    {
        return false;
    }

    const UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return false;
    }

    // Same unrounded basis as CalculateHPCost — would the cost reduce CURRENT HP to <= 0?
    return ComputeRawInfusionHPCost(CharComp, InfusedEnergyCost) >= static_cast<float>(CharComp->CurrentHP);
}

int32 UInfusionCostHelper::CalculateDurabilityCost(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 Level,
    bool bIsSpell)
{
    return UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, Level, bIsSpell);
}