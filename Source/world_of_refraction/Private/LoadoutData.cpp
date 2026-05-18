// LoadoutData.cpp
// Pre-configured combat loadout asset implementation

#include "LoadoutData.h"
#include "WeaponData.h"
#include "RingData.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "LoadoutConstants.h"
#include "InventoryConstants.h"
#include "FCombatLoadout.h"

// ==================== VALIDATION ====================

bool ULoadoutData::IsValidForClass(ECharacterClass CharacterClass) const
{
    return RequiredClass == CharacterClass;
}

TArray<FString> ULoadoutData::GetValidationErrors() const
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
        // Validate secondary based on type
        if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon)
        {
            Errors.Add(TEXT("Secondary slot set to Weapon but no weapon assigned"));
        }

        // Validate secondary weapon abilities/spells counts
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
            // Broken Darkness template — InnateSpells is the Darkness pool,
            // BDSpellPools the per-element pools. Same rules as FCombatLoadout.
            Errors.Append(FCombatLoadout::ValidateBDSpellLoadout(InnateSpells, BDSpellPools));
        }
        else if (InnateSpells.Num() > InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
        {
            // Normal Caster — innate spells limit.
            Errors.Add(FString::Printf(TEXT("Too many innate spells (%d/%d)"),
                                       InnateSpells.Num(), InventoryConstants::MAX_INNATE_SPELLS_TOTAL));
        }

        // Note: Element matching validation requires CharacterData reference
        // This is validated at runtime in LoadoutComponent
    }

    // ==================== RESONATOR VALIDATION ====================

    if (RequiredClass == ECharacterClass::Resonator)
    {
        // Ring count limit depends on evolution state
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

        // Evolved ring limit
        int32 EvolvedCount = 0;
        for (const URingData *Ring : EquippedRings)
        {
            if (Ring && Ring->IsEvolved())
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

    // Validate primary equipment spells (applies to weapon/ring/evolution)
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

    // Check for null entries in arrays
    for (int32 i = 0; i < EquippedItems.Num(); ++i)
    {
        if (!EquippedItems[i])
        {
            Errors.Add(FString::Printf(TEXT("Null item in slot %d"), i));
        }
    }

    for (int32 i = 0; i < EquippedRings.Num(); ++i)
    {
        if (!EquippedRings[i])
        {
            Errors.Add(FString::Printf(TEXT("Null ring in slot %d"), i));
        }
    }

    return Errors;
}

bool ULoadoutData::HasValidationErrors() const
{
    return GetValidationErrors().Num() > 0;
}

// ==================== ACCESSORS ====================

TArray<USpellData *> ULoadoutData::GetAllSpells() const
{
    TArray<USpellData *> Result;

    // Primary equipment spells (weapon crystal / ring / evolution)
    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(EvolutionSpells);
    }

    // Innate spells (Caster only)
    if (RequiredClass == ECharacterClass::Caster)
    {
        Result.Append(InnateSpells);
    }

    // Ring loadout spells (Resonator only) - read from each ring's DefaultSpells
    if (RequiredClass == ECharacterClass::Resonator)
    {
        for (URingData *Ring : EquippedRings)
        {
            if (Ring)
            {
                Result.Append(Ring->DefaultSpells);
            }
        }
    }

    return Result;
}

TArray<UAbilityData *> ULoadoutData::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    // Primary weapon abilities
    if (PrimarySlotType == EPrimarySlotType::Weapon)
    {
        Result.Append(PrimaryWeaponAbilities);
    }

    // Secondary weapon abilities (Generic only)
    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponAbilities);
    }

    // Note: Evolution abilities would come from PrimaryEvolution->GetAbilities()
    // but we don't store them separately in LoadoutData

    return Result;
}

UWeaponData *ULoadoutData::GetPrimaryWeapon() const
{
    if (PrimarySlotType == EPrimarySlotType::Weapon)
    {
        return PrimaryWeapon;
    }
    return nullptr;
}

URingData *ULoadoutData::GetPrimaryRing() const
{
    if (PrimarySlotType == EPrimarySlotType::Ring)
    {
        return PrimaryRing;
    }
    return nullptr;
}

UItemData *ULoadoutData::GetPrimaryEvolution() const
{
    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        return PrimaryEvolution;
    }
    return nullptr;
}

// ==================== EDITOR ====================

#if WITH_EDITOR
EDataValidationResult ULoadoutData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    TArray<FString> Errors = GetValidationErrors();

    for (const FString &Error : Errors)
    {
        Context.AddError(FText::FromString(Error));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void ULoadoutData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = PropertyChangedEvent.GetPropertyName();

    // Clear irrelevant data when class changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ULoadoutData, RequiredClass))
    {
        // Clear class-specific data that doesn't apply
        if (RequiredClass != ECharacterClass::Generic)
        {
            SecondarySlotType = ESecondarySlotType::None;
            SecondaryWeapon = nullptr;
            SecondaryWeaponAbilities.Empty();
            SecondaryWeaponStanceOverride = nullptr;
        }

        if (RequiredClass != ECharacterClass::Caster)
        {
            InnateSpells.Empty();
            BDSpellPools.Empty();
        }

        if (RequiredClass != ECharacterClass::Resonator)
        {
            EquippedRings.Empty();
        }

        // Resonator cannot use Ring primary
        if (RequiredClass == ECharacterClass::Resonator && PrimarySlotType == EPrimarySlotType::Ring)
        {
            PrimarySlotType = EPrimarySlotType::Weapon;
            PrimaryRing = nullptr;
        }
    }

    // Clear irrelevant data when primary slot type changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ULoadoutData, PrimarySlotType))
    {
        // Clear weapon data if not using weapon
        if (PrimarySlotType != EPrimarySlotType::Weapon)
        {
            PrimaryWeapon = nullptr;
            PrimaryWeaponAbilities.Empty();
            PrimaryWeaponStanceOverride = nullptr;
        }

        // Clear ring data if not using ring
        if (PrimarySlotType != EPrimarySlotType::Ring)
        {
            PrimaryRing = nullptr;
        }

        // Clear evolution data if not using evolution
        if (PrimarySlotType != EPrimarySlotType::Evolution)
        {
            PrimaryEvolution = nullptr;
        }
    }

    // Clear secondary weapon data when secondary slot type changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ULoadoutData, SecondarySlotType))
    {
        if (SecondarySlotType != ESecondarySlotType::Weapon)
        {
            SecondaryWeapon = nullptr;
            SecondaryWeaponAbilities.Empty();

            SecondaryWeaponStanceOverride = nullptr;
        }
    }
}
#endif

// ==================== DATA ASSET ====================

FPrimaryAssetId ULoadoutData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("LoadoutData"), GetFName());
}