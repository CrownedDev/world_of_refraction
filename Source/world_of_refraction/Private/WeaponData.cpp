// WeaponData.cpp
// Weapon data implementation
#include "WeaponData.h"
#include "AbilityData.h"
#include "StanceData.h"
#include "LoadoutConstants.h"
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
    case EWeaponType::DualBlades:
        return TEXT("Dual Blades");
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
    case EWeaponType::Gun:
        return TEXT("Gun");
    default:
        return TEXT("Unknown");
    }
}

int32 UWeaponData::GetMaxSpells() const
{
    return LoadoutConstants::MAX_WEAPON_ABILITIES;
}

#if WITH_EDITOR
EDataValidationResult UWeaponData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

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

    if (PhysicalDamageType == EPhysicalDamageType::None)
    {
        Context.AddError(FText::FromString(TEXT("PhysicalDamageType must be set — None is not allowed.")));
        Result = EDataValidationResult::Invalid;
    }
    return Result;
}
#endif
