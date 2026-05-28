// EquipmentBonusGenerator.cpp

#include "Equipment/EquipmentBonusGenerator.h"
#include "Combat/CombatConstants.h"

FEquipmentStatBonus UEquipmentBonusGenerator::Generate(EItemTier Tier)
{
    FPillarWeights EqualWeights;
    return GenerateWeighted(Tier, EqualWeights);
}

FEquipmentStatBonus UEquipmentBonusGenerator::GenerateWeighted(EItemTier Tier, FPillarWeights Weights)
{
    FEquipmentStatBonus Result;
    Result.RerollSubstats(Tier, Weights);
    Result.RerollPillars(Tier);
    return Result;
}

void UEquipmentBonusGenerator::RerollSubstats(FEquipmentStatBonus &Bonus, EItemTier Tier)
{
    FPillarWeights EqualWeights;
    Bonus.RerollSubstats(Tier, EqualWeights);
}

void UEquipmentBonusGenerator::RerollPillars(FEquipmentStatBonus &Bonus, EItemTier Tier)
{
    Bonus.RerollPillars(Tier);
}

FEquipmentStatBonus UEquipmentBonusGenerator::Reroll(const FEquipmentStatBonus &Current, EItemTier Tier)
{
    (void)Current; // No carry-over state remains on the struct — bonus fields are fully regenerated.
    FEquipmentStatBonus Result;
    FPillarWeights EqualWeights;
    Result.RerollSubstats(Tier, EqualWeights);
    Result.RerollPillars(Tier);
    return Result;
}

int32 UEquipmentBonusGenerator::GetSubstatBudgetForTier(EItemTier Tier)
{
    return EquipmentBonusGen::GetSubstatBudget(Tier);
}

float UEquipmentBonusGenerator::GetPillarBudgetForTier(EItemTier Tier)
{
    return EquipmentBonusGen::GetPillarBudget(Tier);
}
