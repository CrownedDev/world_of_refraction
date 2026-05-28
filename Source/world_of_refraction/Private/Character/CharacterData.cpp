// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"

#include "Character/StanceData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Equipment/Weapons/WeaponAttackData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Character/StatConstants.h"
#include "Loadout/LoadoutConstants.h"
#include "Inventory/InventoryData.h"

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