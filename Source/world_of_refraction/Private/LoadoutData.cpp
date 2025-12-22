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

// ==================== VALIDATION ====================

bool ULoadoutData::IsValidForClass(ECharacterClass CharacterClass) const
{
    return RequiredClass == CharacterClass;
}

TArray<FString> ULoadoutData::GetValidationErrors() const
{
    TArray<FString> Errors;

    // ==================== GENERIC VALIDATION ====================

    if (RequiredClass == ECharacterClass::Generic)
    {
        // Generic must have primary weapon
        if (!PrimaryWeapon)
        {
            Errors.Add(TEXT("Generic class requires a primary weapon"));
        }

        // Validate secondary based on type
        if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon)
        {
            Errors.Add(TEXT("Secondary slot set to Weapon but no weapon assigned"));
        }
        if (SecondarySlotType == ESecondarySlotType::Ring && !SecondaryRing)
        {
            Errors.Add(TEXT("Secondary slot set to Ring but no ring assigned"));
        }

        // Validate secondary weapon abilities/spells counts
        if (SecondarySlotType == ESecondarySlotType::Weapon)
        {
            if (SecondaryWeaponAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
            {
                Errors.Add(FString::Printf(TEXT("Secondary weapon has too many abilities (%d/%d)"),
                                           SecondaryWeaponAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES));
            }
            if (SecondaryWeaponSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
            {
                Errors.Add(FString::Printf(TEXT("Secondary weapon has too many spells (%d/%d)"),
                                           SecondaryWeaponSpells.Num(), LoadoutConstants::MAX_SPELL_SLOTS));
            }
        }
    }

    // ==================== CASTER VALIDATION ====================

    if (RequiredClass == ECharacterClass::Caster)
    {
        // Caster must have either weapon or ring
        if (PrimarySlotType == EPrimarySlotType::Weapon && !PrimaryWeapon)
        {
            Errors.Add(TEXT("Caster with Weapon slot type requires a primary weapon"));
        }
        if (PrimarySlotType == EPrimarySlotType::Ring && !PrimaryRing)
        {
            Errors.Add(TEXT("Caster with Ring slot type requires a primary ring"));
        }

        // Innate spells limit
        if (InnateSpells.Num() > InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
        {
            Errors.Add(FString::Printf(TEXT("Too many innate spells (%d/%d)"),
                                       InnateSpells.Num(), InventoryConstants::MAX_INNATE_SPELLS_TOTAL));
        }

        // Note: Element matching validation requires CharacterData reference
        // This is validated at runtime in LoadoutComponent
    }

    // ==================== RESONATOR VALIDATION ====================

    if (RequiredClass == ECharacterClass::Resonator)
    {
        // Resonator must have primary weapon
        if (!PrimaryWeapon)
        {
            Errors.Add(TEXT("Resonator class requires a primary weapon"));
        }

        // Ring count limit
        if (EquippedRings.Num() > 5)
        {
            Errors.Add(FString::Printf(TEXT("Too many equipped rings (%d/5)"), EquippedRings.Num()));
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
        if (EvolvedCount > 2)
        {
            Errors.Add(FString::Printf(TEXT("Too many evolved rings (%d/2)"), EvolvedCount));
        }
    }

    // ==================== COMMON VALIDATION ====================

    // Primary weapon ability/spell limits
    if (PrimaryWeaponAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
    {
        Errors.Add(FString::Printf(TEXT("Primary weapon has too many abilities (%d/%d)"),
                                   PrimaryWeaponAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES));
    }
    if (PrimaryWeaponSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
    {
        Errors.Add(FString::Printf(TEXT("Primary weapon has too many spells (%d/%d)"),
                                   PrimaryWeaponSpells.Num(), LoadoutConstants::MAX_SPELL_SLOTS));
    }

    // Item limit
    if (EquippedItems.Num() > InventoryConstants::MAX_ITEM_LOADOUT_SLOTS)
    {
        Errors.Add(FString::Printf(TEXT("Too many equipped items (%d/%d)"),
                                   EquippedItems.Num(), InventoryConstants::MAX_ITEM_LOADOUT_SLOTS));
    }

    // Check for null entries in arrays
    for (int32 i = 0; i < InnateSpells.Num(); i++)
    {
        if (!InnateSpells[i])
        {
            Errors.Add(FString::Printf(TEXT("Null innate spell at index %d"), i));
        }
    }

    for (int32 i = 0; i < EquippedRings.Num(); i++)
    {
        if (!EquippedRings[i])
        {
            Errors.Add(FString::Printf(TEXT("Null ring at index %d"), i));
        }
    }

    for (int32 i = 0; i < EquippedItems.Num(); i++)
    {
        if (!EquippedItems[i])
        {
            Errors.Add(FString::Printf(TEXT("Null item at index %d"), i));
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

    // Innate spells (Caster)
    Result.Append(InnateSpells);

    // Primary weapon spells
    Result.Append(PrimaryWeaponSpells);

    // Secondary weapon spells (Generic)
    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponSpells);
    }

    // Ring spells (Resonator - from equipped rings)
    if (RequiredClass == ECharacterClass::Resonator)
    {
        for (const URingData *Ring : EquippedRings)
        {
            if (Ring)
            {
                // Note: Ring spells would come from ring's crystal
                // This requires RingData to expose GetSpells()
            }
        }
    }

    // Primary ring spells (Caster with ring)
    if (RequiredClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing)
    {
        // Ring spells from primary ring
    }

    // Secondary ring spells (Generic with ring)
    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Ring && SecondaryRing)
    {
        // Ring spells from secondary ring
    }

    return Result;
}

TArray<UAbilityData *> ULoadoutData::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    // Primary weapon abilities
    Result.Append(PrimaryWeaponAbilities);

    // Secondary weapon abilities (Generic)
    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponAbilities);
    }

    return Result;
}

UWeaponData *ULoadoutData::GetPrimaryWeapon() const
{
    // Caster with ring has no weapon
    if (RequiredClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring)
    {
        return nullptr;
    }

    return PrimaryWeapon;
}

URingData *ULoadoutData::GetPrimaryRing() const
{
    // Only Caster can have primary ring
    if (RequiredClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring)
    {
        return PrimaryRing;
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
            SecondaryRing = nullptr;
            SecondaryWeaponAbilities.Empty();
            SecondaryWeaponSpells.Empty();
        }

        if (RequiredClass != ECharacterClass::Caster)
        {
            PrimarySlotType = EPrimarySlotType::Weapon;
            PrimaryRing = nullptr;
            InnateSpells.Empty();
        }

        if (RequiredClass != ECharacterClass::Resonator)
        {
            EquippedRings.Empty();
        }
    }

    // Clear ring when switching to weapon
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ULoadoutData, PrimarySlotType))
    {
        if (PrimarySlotType == EPrimarySlotType::Weapon)
        {
            PrimaryRing = nullptr;
        }
        else
        {
            PrimaryWeapon = nullptr;
            PrimaryWeaponAbilities.Empty();
            PrimaryWeaponSpells.Empty();
        }
    }

    // Clear based on secondary slot type
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ULoadoutData, SecondarySlotType))
    {
        if (SecondarySlotType != ESecondarySlotType::Weapon)
        {
            SecondaryWeapon = nullptr;
            SecondaryWeaponAbilities.Empty();
            SecondaryWeaponSpells.Empty();
        }
        if (SecondarySlotType != ESecondarySlotType::Ring)
        {
            SecondaryRing = nullptr;
        }
    }
}
#endif

// ==================== DATA ASSET ====================

FPrimaryAssetId ULoadoutData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("LoadoutData"), GetFName());
}
