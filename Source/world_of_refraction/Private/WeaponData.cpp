// WeaponData.cpp
// Weapon data implementation
#include "WeaponData.h"
#include "AbilityData.h"
#include "StanceData.h"
#include "LoadoutConstants.h"
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

ESpellElement UWeaponData::GetWeaponElement() const
{
    // No crystal or crystal broken = no element.
    // TODO(Phase B commit 5): the SlottedCrystal->IsBroken() check is stale
    // post-commit-2 — asset-side CurrentDurability is no longer written.
    // Callers pre-filter broken crystals via FCrystalInventoryEntry::CanProvideSpells
    // before reaching this path, so the dead check is invariant-protected.
    // Remove this check when UItemData::IsBroken / CurrentDurability are deleted.
    if (!SlottedCrystal || SlottedCrystal->IsBroken())
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

    return SlottedCrystal->bIsEvolutionCrystal;
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

    if (PhysicalDamageType == EPhysicalDamageType::None)
    {
        Context.AddError(FText::FromString(TEXT("PhysicalDamageType must be set — None is not allowed.")));
        Result = EDataValidationResult::Invalid;
    }
    return Result;
}
#endif