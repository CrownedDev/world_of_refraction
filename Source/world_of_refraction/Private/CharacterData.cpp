// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterData.h"
#include "EvolutionData.h"
#include "RingData.h"
#include "EvolutionData.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

ESpellElement UCharacterData::GetSecondaryElement() const
{
    return (bIsEvolved && ActiveEvolution) ? ActiveEvolution->Element : ESpellElement::Generic;
}

bool UCharacterData::HasSecondaryElement() const
{
    return bIsEvolved && GetSecondaryElement() != ESpellElement::Generic;
}

bool UCharacterData::IsDualElementCaster() const
{
    return IsCaster() && bIsEvolved && ActiveEvolution && ActiveEvolution->Element != InnateElement;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UCharacterData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate stat budget
    if (!IsValidInitialDistribution())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Initial sub-stat distribution (%d) doesn't match budget (%d)"),
            GetInitialSubStatSum(), InitialStatBudget)));
        Result = EDataValidationResult::Invalid;
    }

    if (!IsValidWorldDistribution())
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("World sub-stat distribution (%d) doesn't match expected (%d)"),
            GetWorldSubStatSum(), GetExpectedWorldPoints())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate class-specific requirements
    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        if (!PrimaryWeapon)
        {
            Context.AddWarning(FText::FromString(TEXT("Generic character has no primary weapon")));
        }
        if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon)
        {
            Context.AddWarning(FText::FromString(TEXT("Secondary slot set to Weapon but no weapon assigned")));
        }
        if (SecondarySlotType == ESecondarySlotType::Ring && !SecondaryRing)
        {
            Context.AddWarning(FText::FromString(TEXT("Secondary slot set to Ring but no ring assigned")));
        }
        if (InnateSpells.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Generic characters cannot have innate spells")));
            Result = EDataValidationResult::Invalid;
        }
        if (EquippedRings.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Generic characters use SecondaryRing, not EquippedRings")));
            Result = EDataValidationResult::Invalid;
        }
        break;

    case ECharacterClass::Caster:
        if (InnateSpells.Num() == 0)
        {
            Context.AddWarning(FText::FromString(TEXT("Caster has no innate spells")));
        }
        if (EquippedRings.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Casters use PrimaryRing, not EquippedRings")));
            Result = EDataValidationResult::Invalid;
        }
        if (SecondarySlotType != ESecondarySlotType::None)
        {
            Context.AddError(FText::FromString(TEXT("Casters cannot have secondary slots")));
            Result = EDataValidationResult::Invalid;
        }
        break;

    case ECharacterClass::Resonator:
        if (EquippedRings.Num() == 0)
        {
            Context.AddWarning(FText::FromString(TEXT("Resonator has no rings equipped")));
        }
        if (EquippedRings.Num() > 6)
        {
            Context.AddError(FText::FromString(TEXT("Resonator can only equip 6 rings")));
            Result = EDataValidationResult::Invalid;
        }
        // Count evolved rings (max 2)
        {
            int32 EvolvedRingCount = 0;
            for (URingData *Ring : EquippedRings)
            {
                if (Ring && Ring->IsEvolved())
                {
                    EvolvedRingCount++;
                }
            }
            if (EvolvedRingCount > 2)
            {
                Context.AddError(FText::FromString(TEXT("Resonator can only equip 2 evolved rings")));
                Result = EDataValidationResult::Invalid;
            }
        }
        if (InnateSpells.Num() > 0)
        {
            Context.AddError(FText::FromString(TEXT("Resonators get spells from rings, not innate")));
            Result = EDataValidationResult::Invalid;
        }
        if (SecondarySlotType != ESecondarySlotType::None)
        {
            Context.AddError(FText::FromString(TEXT("Resonators use EquippedRings, not SecondarySlot")));
            Result = EDataValidationResult::Invalid;
        }
        break;
    }

    return Result;
}
void UCharacterData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Update evolution element display
    if (bIsEvolved && ActiveEvolution)
    {
        EvolutionElementDisplay = UEnum::GetValueAsString(ActiveEvolution->Element);
        EvolutionElementDisplay.RemoveFromStart(TEXT("ESpellElement::"));
    }
    else
    {
        EvolutionElementDisplay = TEXT("None");
    }
}
#endif