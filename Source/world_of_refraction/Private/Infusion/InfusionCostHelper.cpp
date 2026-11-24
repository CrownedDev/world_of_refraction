// InfusionCostHelper.cpp
// Implementations of pure cost calculations for the infusion system.

#include "Infusion/InfusionCostHelper.h"
#include "Infusion/InfusionConstants.h"
#include "Equipment/Durability/BreakCalculator.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"

namespace
{
    /** Unrounded EP-derived infusion HP cost (rework 6-2-3c, D-271): the infused EP cost as a
     *  fraction of max EP, applied to max HP, then ScaleHPBacklash. Returns 0 when the component
     *  is null, has no EP pool, or the cost is non-positive. Shared by CalculateHPCost (rounds it)
     *  and WouldKill (compares). */
    float ComputeRawInfusionHPCost(const UCharacterDataComponent *CharComp, int32 InfusedEnergyCost)
    {
        if (!CharComp || InfusedEnergyCost <= 0 || CharComp->MaxEP <= 0)
        {
            return 0.0f;
        }

        const float EPFraction = static_cast<float>(InfusedEnergyCost) / static_cast<float>(CharComp->MaxEP);
        const float BaseHP = EPFraction * static_cast<float>(CharComp->MaxHP);
        return UInfusionCostHelper::ScaleHPBacklash(BaseHP, CharComp->GetEffectiveEfficiencyReduction(),
                                                    CharComp->GetEffectiveStats().Resistance);
    }
}

float UInfusionCostHelper::ScaleHPBacklash(float BaseHP, float EfficiencyReduction, float Resistance)
{
    // D-271: Efficiency's cut on HP stops at the stats-only max even with gear (EP keeps its gear ceiling).
    const float EfficiencyCut = FMath::Clamp(EfficiencyReduction, 0.0f, CombatConstants::EFFICIENCY_MAX);
    // Max(0, ...) guards against inversion (a resistance >= 1 zeroes the cost rather than healing); a negative
    // ("cursed") resistance amplifies it, matching how negative stats behave elsewhere.
    return BaseHP * InfusionConstants::HP_BACKLASH_MULTIPLIER * (1.0f - EfficiencyCut) * FMath::Max(0.0f, 1.0f - Resistance);
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