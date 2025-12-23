// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterData.h"
#include "RingData.h"

#include "StanceData.h"
#include "WeaponData.h"
#include "EWeaponSlotType.h"
#include "WeaponAttackData.h"
#include "ItemData.h"
#include "StatConstants.h"
#include "LoadoutConstants.h"
#include "LoadoutData.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

// ==================== EVOLUTION COST FUNCTIONS ====================

bool UCharacterData::CanApplyEvolution(UItemData *EvolutionCrystal) const
{
    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        return false;
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    // Special elements may have restrictions
    if (EvolutionElement == ESpellElement::BrokenDarkness || EvolutionElement == ESpellElement::Reality)
    {
        return false;
    }

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        return true;

    case ECharacterClass::Caster:
        return true;

    case ECharacterClass::Resonator:
        // Ring element validation happens at runtime via LoadoutComponent
        // CharacterData no longer stores equipment
        return true;
    }

    return false;
}

FEvolutionCostResult UCharacterData::CalculateEvolutionCost(UItemData *EvolutionCrystal) const
{
    FEvolutionCostResult Result;
    Result.bCanEvolve = CanApplyEvolution(EvolutionCrystal);

    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        Result.bCanEvolve = false;
        Result.CostDescription = TEXT("Invalid evolution crystal");
        return Result;
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        Result.CostDescription = TEXT("Lose secondary weapon slot");
        Result.GainDescription = TEXT("Gain evolution abilities and spells");

    case ECharacterClass::Caster:
        if (EvolutionElement == InnateElement)
        {
            Result.CostDescription = TEXT("No cost (same element)");
            Result.GainDescription = TEXT("Gain evolution abilities");
        }
        else
        {
            Result.CostDescription = TEXT("Lose weapon slot");
            Result.GainDescription = FString::Printf(
                TEXT("Gain %s as second element, can switch between elements"),
                *EvolutionCrystal->GetCrystalName());
        }
        break;

    case ECharacterClass::Resonator:
        Result.CostDescription = FString::Printf(
            TEXT("Locked to %s element only"), *EvolutionCrystal->GetCrystalName());
        Result.GainDescription = TEXT("Gain innate spells (no ring required), can equip 2nd weapon OR 2nd ring");
        Result.Warnings.Add(TEXT("Will lose access to other elements"));
        Result.Warnings.Add(TEXT("Multi-element builds no longer possible"));
        break;
    }

    return Result;
}

FString UCharacterData::GetEvolutionCostDescription(UItemData *EvolutionCrystal) const
{
    if (!EvolutionCrystal || !EvolutionCrystal->GrantsEvolution())
    {
        return TEXT("Invalid evolution crystal");
    }

    ESpellElement EvolutionElement = EvolutionCrystal->GetAssociatedElement();

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        return TEXT("Loses secondary weapon, gains abilities + spells");

    case ECharacterClass::Caster:
        return TEXT("Same element: No cost. Different element: Loses weapon, gains dual-element");

    case ECharacterClass::Resonator:
        return FString::Printf(TEXT("Locked to %s, gains innate spells"), *EvolutionCrystal->GetCrystalName());

    default:
        return TEXT("Unknown");
    }
}
