// WeaponData.cpp
// Weapon data implementation

#include "WeaponData.h"
#include "AbilityData.h"
#include "StanceData.h"
#include "LoadoutConstants.h"
#include "WeaponInfusionDisplayData.h"
#include "WeaponAttackData.h"

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

#if WITH_EDITOR
EDataValidationResult UWeaponData::IsDataValid(TArray<FText> &ValidationErrors)
{
    EDataValidationResult Result = EDataValidationResult::Valid;

    // Name validation
    if (WeaponName.IsEmpty() || WeaponName == TEXT("Unnamed Weapon"))
    {
        ValidationErrors.Add(FText::FromString(TEXT("Weapon must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Attack validation
    if (WeaponAttack == nullptr)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Warning: No weapon attack assigned")));
    }

    // Abilities validation (max 6)
    if (PresetAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
    {
        ValidationErrors.Add(FText::FromString(FString::Printf(
            TEXT("Weapon cannot have more than %d abilities"),
            LoadoutConstants::MAX_WEAPON_ABILITIES)));
        Result = EDataValidationResult::Invalid;
    }

    // Stance validation
    if (WeaponStance == nullptr)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Warning: No weapon stance assigned")));
    }

    return Result;
}
#endif