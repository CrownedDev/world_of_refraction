// WeaponData.cpp
// Weapon data implementation
#include "WeaponData.h"
#include "AbilityData.h"
#include "StanceData.h"
#include "LoadoutConstants.h"
#include "WeaponInfusionDisplayData.h"
#include "WeaponAttackData.h"
#include "CharacterData.h"
#include "ItemData.h"
#include "CrystalType.h"

FString UWeaponData::GetWeaponTypeName() const
{
    switch (WeaponType)
    {
    case EWeaponType::Sword:
        return TEXT("Sword");
    case EWeaponType::Greatsword:
        return TEXT("Greatsword");
    case EWeaponType::Spear:
        return TEXT("Spear");
    case EWeaponType::Staff:
        return TEXT("Staff");
    case EWeaponType::Dagger:
        return TEXT("Dagger");
    case EWeaponType::Axe:
        return TEXT("Axe");
    case EWeaponType::Hammer:
        return TEXT("Hammer");
    case EWeaponType::Bow:
        return TEXT("Bow");
    case EWeaponType::Fists:
        return TEXT("Fists");
    case EWeaponType::Scythe:
        return TEXT("Scythe");
    default:
        return TEXT("Unknown");
    }
}

float UWeaponData::GetDurabilityPercent() const
{
    if (MaxDurability <= 0)
        return 0.0f;
    return static_cast<float>(CurrentDurability) / static_cast<float>(MaxDurability);
}

bool UWeaponData::IsCrystalFunctional() const
{
    return SlottedCrystal != nullptr && !bCrystalDisabled;
}

void UWeaponData::ApplyDurabilityDamage(int32 Damage)
{
    CurrentDurability = FMath::Max(0, CurrentDurability - Damage);

    if (CurrentDurability <= 0 && SlottedCrystal != nullptr)
    {
        bCrystalDisabled = true;
    }
}

void UWeaponData::RepairDurability(float Percent)
{
    int32 Repair = FMath::CeilToInt(MaxDurability * Percent);
    CurrentDurability = FMath::Min(MaxDurability, CurrentDurability + Repair);

    // Re-enable crystal if repaired above 0
    if (CurrentDurability > 0)
    {
        bCrystalDisabled = false;
    }
}

void UWeaponData::ResetDurability()
{
    CurrentDurability = MaxDurability;
    bCrystalDisabled = false;
}

int32 UWeaponData::GetCrystalBreakDamage(EItemTier CrystalTier)
{
    switch (CrystalTier)
    {
    case EItemTier::F_Tier:
        return 5;
    case EItemTier::E_Tier:
        return 10;
    case EItemTier::D_Tier:
        return 15;
    case EItemTier::C_Tier:
        return 20;
    case EItemTier::B_Tier:
        return 30;
    case EItemTier::A_Tier:
        return 40;
    case EItemTier::S_Tier:
        return 50;
    default:
        return 10;
    }
}

ESpellElement UWeaponData::GetWeaponElement() const
{
    // Crystal disabled = no element
    if (!IsCrystalFunctional())
    {
        return ESpellElement::Generic;
    }
    return SlottedCrystal->GetAssociatedElement();
}

bool UWeaponData::IsEvolved() const
{
    // Evolution crystals don't get disabled
    if (!SlottedCrystal)
        return false;

    return SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal;
}

#if WITH_EDITOR
EDataValidationResult UWeaponData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name validation
    if (WeaponName.IsEmpty() || WeaponName == TEXT("Unnamed Weapon"))
    {
        Context.AddError(FText::FromString(TEXT("Weapon must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Attack validation
    if (WeaponAttack == nullptr)
    {
        Context.AddWarning(FText::FromString(TEXT("No weapon attack assigned")));
    }

    // Abilities validation (max 6)
    if (PresetAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Weapon cannot have more than %d abilities"),
            LoadoutConstants::MAX_WEAPON_ABILITIES)));
        Result = EDataValidationResult::Invalid;
    }

    // Stance validation
    if (WeaponStance == nullptr)
    {
        Context.AddWarning(FText::FromString(TEXT("No weapon stance assigned")));
    }

    return Result;
}
#endif