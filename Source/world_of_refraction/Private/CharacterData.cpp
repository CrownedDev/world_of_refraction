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

#if WITH_EDITOR
EDataValidationResult UCharacterData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate name
    if (CharacterName.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Character must have a name")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate Caster has element
    if (CharacterClass == ECharacterClass::Caster && InnateElement == ESpellElement::Generic)
    {
        Context.AddWarning(FText::FromString(TEXT("Caster should have an innate element set")));
    }

    // Validate DefaultLoadout exists
    if (!DefaultLoadout)
    {
        Context.AddWarning(FText::FromString(TEXT("No DefaultLoadout assigned - character will have no equipment")));
    }

    return Result;
}

void UCharacterData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Add any property change handling here if needed
}
#endif