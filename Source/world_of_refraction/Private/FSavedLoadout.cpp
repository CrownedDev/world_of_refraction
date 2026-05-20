// FSavedLoadout.cpp
// Designer-authored loadout configuration — validation and accessors.
// Logic mirrors ULoadoutData (LoadoutData.cpp) to keep behaviour identical
// once UInventoryData replaces ULoadoutData as the asset source.

#include "FSavedLoadout.h"

#include "AbilityData.h"
#include "FCombatLoadout.h"
#include "InventoryConstants.h"
#include "ItemData.h"
#include "LoadoutConstants.h"
#include "RingData.h"
#include "SpellData.h"
#include "WeaponData.h"

// ==================== VALIDATION ====================

bool FSavedLoadout::IsValidForClass(ECharacterClass CharacterClass) const
{
    return RequiredClass == CharacterClass;
}

TArray<FString> FSavedLoadout::GetValidationErrors() const
{
    TArray<FString> Errors;

    // ==================== PRIMARY SLOT VALIDATION (All Classes) ====================

    switch (PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (!PrimaryWeapon)
        {
            Errors.Add(TEXT("Primary slot set to Weapon but no weapon assigned"));
        }
        break;

    case EPrimarySlotType::Ring:
        if (RequiredClass == ECharacterClass::Resonator)
        {
            Errors.Add(TEXT("Resonator cannot use Ring as primary slot"));
        }
        else if (!PrimaryRing)
        {
            Errors.Add(TEXT("Primary slot set to Ring but no ring assigned"));
        }
        break;

    case EPrimarySlotType::Evolution:
        if (!PrimaryEvolution)
        {
            Errors.Add(TEXT("Primary slot set to Evolution but no evolution crystal assigned"));
        }
        break;
    }

    // ==================== GENERIC VALIDATION ====================

    if (RequiredClass == ECharacterClass::Generic)
    {
        if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon)
        {
            Errors.Add(TEXT("Secondary slot set to Weapon but no weapon assigned"));
        }

        if (SecondarySlotType == ESecondarySlotType::Weapon)
        {
            if (SecondaryWeaponAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
            {
                Errors.Add(FString::Printf(TEXT("Secondary weapon has too many abilities (%d/%d)"),
                                           SecondaryWeaponAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES));
            }
        }
    }

    // ==================== CASTER VALIDATION ====================

    if (RequiredClass == ECharacterClass::Caster)
    {
        if (BDSpellPools.Num() > 0)
        {
            Errors.Append(FCombatLoadout::ValidateBDSpellLoadout(InnateSpells, BDSpellPools));
        }
        else if (InnateSpells.Num() > InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
        {
            Errors.Add(FString::Printf(TEXT("Too many innate spells (%d/%d)"),
                                       InnateSpells.Num(), InventoryConstants::MAX_INNATE_SPELLS_TOTAL));
        }
    }

    // ==================== RESONATOR VALIDATION ====================

    if (RequiredClass == ECharacterClass::Resonator)
    {
        const bool bIsEvolved = (PrimarySlotType == EPrimarySlotType::Evolution);
        const int32 MaxRings = bIsEvolved ? LoadoutConstants::RESONATOR_RING_SLOTS_EVOLVED
                                          : LoadoutConstants::RESONATOR_RING_SLOTS_NORMAL;
        const int32 MaxEvolvedRings = bIsEvolved ? LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_EVOLVED
                                                 : LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_NORMAL;

        if (EquippedRings.Num() > MaxRings)
        {
            Errors.Add(FString::Printf(TEXT("Too many equipped rings (%d/%d)"),
                                       EquippedRings.Num(), MaxRings));
        }

        int32 EvolvedCount = 0;
        for (const FResonatorRingSlot &Slot : EquippedRings)
        {
            if (Slot.Ring && Slot.Ring->IsEvolved())
            {
                EvolvedCount++;
            }
        }
        if (EvolvedCount > MaxEvolvedRings)
        {
            Errors.Add(FString::Printf(TEXT("Too many evolved rings (%d/%d)"),
                                       EvolvedCount, MaxEvolvedRings));
        }
    }

    // ==================== PRIMARY WEAPON VALIDATION ====================

    if (PrimarySlotType == EPrimarySlotType::Weapon)
    {
        if (PrimaryWeaponAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
        {
            Errors.Add(FString::Printf(TEXT("Primary weapon has too many abilities (%d/%d)"),
                                       PrimaryWeaponAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES));
        }
    }

    if (PrimarySlotType == EPrimarySlotType::Evolution &&
        EvolutionSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
    {
        Errors.Add(FString::Printf(TEXT("Too many evolution spells (%d/%d)"),
                                   EvolutionSpells.Num(), LoadoutConstants::MAX_SPELL_SLOTS));
    }

    // ==================== ITEM VALIDATION ====================

    if (EquippedItems.Num() > InventoryConstants::MAX_ITEM_LOADOUT_SLOTS)
    {
        Errors.Add(FString::Printf(TEXT("Too many equipped items (%d/%d)"),
                                   EquippedItems.Num(), InventoryConstants::MAX_ITEM_LOADOUT_SLOTS));
    }

    for (int32 i = 0; i < EquippedItems.Num(); ++i)
    {
        if (!EquippedItems[i])
        {
            Errors.Add(FString::Printf(TEXT("Null item in slot %d"), i));
        }
    }

    for (int32 i = 0; i < EquippedRings.Num(); ++i)
    {
        if (!EquippedRings[i].Ring)
        {
            Errors.Add(FString::Printf(TEXT("Null ring in slot %d"), i));
        }
    }

    return Errors;
}

bool FSavedLoadout::HasValidationErrors() const
{
    return GetValidationErrors().Num() > 0;
}

// ==================== ACCESSORS ====================

TArray<USpellData *> FSavedLoadout::GetAllSpells() const
{
    TArray<USpellData *> Result;

    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(EvolutionSpells);
    }

    if (RequiredClass == ECharacterClass::Caster)
    {
        Result.Append(InnateSpells);
    }

    if (RequiredClass == ECharacterClass::Resonator)
    {
        for (const FResonatorRingSlot &Slot : EquippedRings)
        {
            if (Slot.Ring)
            {
                Result.Append(Slot.Ring->DefaultSpells);
                Result.Append(Slot.AssignedSpells);
            }
        }
    }

    return Result;
}

TArray<UAbilityData *> FSavedLoadout::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    if (PrimarySlotType == EPrimarySlotType::Weapon)
    {
        Result.Append(PrimaryWeaponAbilities);
    }

    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponAbilities);
    }

    return Result;
}

UWeaponData *FSavedLoadout::GetPrimaryWeapon() const
{
    return (PrimarySlotType == EPrimarySlotType::Weapon) ? PrimaryWeapon : nullptr;
}

URingData *FSavedLoadout::GetPrimaryRing() const
{
    return (PrimarySlotType == EPrimarySlotType::Ring) ? PrimaryRing : nullptr;
}

UItemData *FSavedLoadout::GetPrimaryEvolution() const
{
    return (PrimarySlotType == EPrimarySlotType::Evolution) ? PrimaryEvolution : nullptr;
}
