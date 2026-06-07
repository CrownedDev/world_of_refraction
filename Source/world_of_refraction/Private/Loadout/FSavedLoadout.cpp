// FSavedLoadout.cpp
// Designer-authored loadout configuration — validation and accessors.
// Logic mirrors ULoadoutData (LoadoutData.cpp) to keep behaviour identical
// once UInventoryData replaces ULoadoutData as the asset source.

#include "Loadout/FSavedLoadout.h"

#include "Skills/Definitions/AbilityData.h"
#include "Loadout/FCombatLoadout.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/LoadoutConstants.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Weapons/WeaponData.h"

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

    case EPrimarySlotType::None:
        // Empty primary is valid — no primary equipment to validate.
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

            if (SecondaryWeaponStoneAbilities.Num() > LoadoutConstants::MAX_WEAPONSTONE_ABILITIES)
            {
                Errors.Add(FString::Printf(TEXT("Secondary weapon has too many weapon stone abilities (%d/%d)"),
                                           SecondaryWeaponStoneAbilities.Num(), LoadoutConstants::MAX_WEAPONSTONE_ABILITIES));
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
        // Unified budget: primary slot cost + ring slot costs must fit one budget.
        // Subsumes the old count cap and the separate max-evolved-rings cap —
        // evolved rings cost 2 each, so overflow is caught by the cost arithmetic.
        int32 PrimaryCost = 0;
        switch (PrimarySlotType)
        {
        case EPrimarySlotType::Weapon:
            // Saved/design-time path: cost reflects the asset's default crystal.
            if (PrimaryWeapon)
            {
                PrimaryCost = InventoryConstants::GetWeaponSlotCost(
                    PrimaryWeapon->HasCrystal(), PrimaryWeapon->IsEvolved());
            }
            break;
        case EPrimarySlotType::Evolution:
            PrimaryCost = LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST;
            break;
        case EPrimarySlotType::None:
        case EPrimarySlotType::Ring:
            PrimaryCost = 0;
            break;
        }
        int32 RingCost = 0;
        for (const FResonatorRingSlot &Slot : EquippedRings)
        {
            if (Slot.Ring)
            {
                RingCost += InventoryConstants::GetRingSlotCost(Slot.Ring->IsEvolved());
            }
        }
        const int32 TotalCost = PrimaryCost + RingCost;
        if (TotalCost > LoadoutConstants::LOADOUT_TOTAL_BUDGET)
        {
            Errors.Add(FString::Printf(
                TEXT("Loadout exceeds budget: primary (cost %d) + rings (cost %d) = %d, max is %d"),
                PrimaryCost, RingCost, TotalCost, LoadoutConstants::LOADOUT_TOTAL_BUDGET));
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

        if (PrimaryWeaponStoneAbilities.Num() > LoadoutConstants::MAX_WEAPONSTONE_ABILITIES)
        {
            Errors.Add(FString::Printf(TEXT("Primary weapon has too many weapon stone abilities (%d/%d)"),
                                       PrimaryWeaponStoneAbilities.Num(), LoadoutConstants::MAX_WEAPONSTONE_ABILITIES));
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

    TSet<ECrystalType> SeenTypes;
    for (int32 i = 0; i < EquippedItems.Num(); ++i)
    {
        const FEquippedItemSlot &Slot = EquippedItems[i];

        if (Slot.Quantity < 0 || Slot.Quantity > InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT)
        {
            Errors.Add(FString::Printf(
                TEXT("Equipped item slot %d Quantity %d out of range [0, %d]"),
                i, Slot.Quantity, InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT));
        }

        bool bAlreadyIn = false;
        SeenTypes.Add(Slot.CrystalId.Type, &bAlreadyIn);
        if (bAlreadyIn)
        {
            Errors.Add(FString::Printf(
                TEXT("Equipped item slot %d duplicates ECrystalType %s already in another slot"),
                i, *UEnum::GetValueAsString(Slot.CrystalId.Type)));
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
        Result.Append(PrimaryWeaponStoneAbilities);
    }

    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponAbilities);
        Result.Append(SecondaryWeaponStoneAbilities);
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

UEvolutionItemData *FSavedLoadout::GetPrimaryEvolution() const
{
    return (PrimarySlotType == EPrimarySlotType::Evolution) ? PrimaryEvolution : nullptr;
}
