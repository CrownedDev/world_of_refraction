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
#include "InventoryData.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

#if WITH_EDITOR
EDataValidationResult UCharacterData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate name
    if (Name.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Character must have a name")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate Caster has element
    if (CharacterClass == ECharacterClass::Caster && InnateElement == ESpellElement::Generic)
    {
        Context.AddWarning(FText::FromString(TEXT("Caster should have an innate element set")));
    }

    // Inventory is the sole authoring path for loadouts.
    if (!Inventory)
    {
        Context.AddWarning(FText::FromString(TEXT("No Inventory assigned - character will have no equipment")));
    }

    // Validate Cosmetics asset assigned
    if (!Cosmetics)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("UCharacterData '%s' has no Cosmetics asset assigned. Assign a UCosmeticsData asset before using this character in combat."),
            *Name)));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif