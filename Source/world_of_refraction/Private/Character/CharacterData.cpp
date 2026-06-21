// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"

#include "Character/StanceData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Character/StatConstants.h"
#include "Loadout/LoadoutConstants.h"
#include "Inventory/InventoryData.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "UObject/UnrealType.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

void UCharacterData::RefreshResistanceProfileDisplay()
{
	// Single source of truth: ResolveRow. Design-time BD signal is the asset's
	// bBrokenDarknessInnate toggle (no runtime transform state on the asset).
	const bool bAssetBrokenDarkness = bBrokenDarknessInnate;
	const ClassInnateResistanceTable::FResistanceRow Row =
		ClassInnateResistanceTable::ResolveRow(CharacterClass, GetElement(), bAssetBrokenDarkness);

	ResistanceProfile.Fire = Row.Fire;
	ResistanceProfile.Water = Row.Water;
	ResistanceProfile.Earth = Row.Earth;
	ResistanceProfile.Wind = Row.Wind;
	ResistanceProfile.Light = Row.Light;
	ResistanceProfile.Darkness = Row.Darkness;
	ResistanceProfile.Lightning = Row.Lightning;
	ResistanceProfile.Void = Row.Void;
	ResistanceProfile.Reality = Row.Reality;
	ResistanceProfile.Slash = Row.Slash;
	ResistanceProfile.Pierce = Row.Pierce;
	ResistanceProfile.Impact = Row.Impact;
}

void UCharacterData::PostInitProperties()
{
	Super::PostInitProperties();
	RefreshResistanceProfileDisplay();
}

void UCharacterData::PostLoad()
{
	Super::PostLoad();

	RefreshResistanceProfileDisplay();
}

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

void UCharacterData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Live-refresh the greyed display only when the row-selecting fields change.
    const FName ChangedProp = PropertyChangedEvent.GetPropertyName();
    if (ChangedProp == GET_MEMBER_NAME_CHECKED(UCharacterData, CharacterClass) ||
        ChangedProp == GET_MEMBER_NAME_CHECKED(UCharacterData, InnateElement))
    {
        RefreshResistanceProfileDisplay();
    }
}
#endif