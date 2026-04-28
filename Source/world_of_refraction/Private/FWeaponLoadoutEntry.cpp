// FWeaponLoadoutEntry.cpp
// Weapon loadout entry implementation

#include "FWeaponLoadoutEntry.h"
#include "WeaponData.h"
#include "AbilityData.h"
#include "SpellData.h"
#include "FAbilityCollection.h"
#include "FSpellCollection.h"

int32 FWeaponLoadoutEntry::GetLockedAbilityCount() const
{
    if (!WeaponEntry.Weapon)
    {
        return 0;
    }

    // Use weapon's locked status - if abilities are locked, all preset abilities are locked
    if (WeaponEntry.Weapon->bAbilitiesLocked)
    {
        return WeaponEntry.Weapon->PresetAbilities.Num();
    }

    // Otherwise no abilities are locked (all customizable)
    // TODO: Add LockedAbilityCount to WeaponData if partial locking is desired
    return 0;
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    // Start with locked/preset abilities
    if (WeaponEntry.Weapon)
    {
        int32 LockedCount = GetLockedAbilityCount();
        for (int32 i = 0; i < LockedCount && i < WeaponEntry.Weapon->PresetAbilities.Num(); ++i)
        {
            Result.Add(WeaponEntry.Weapon->PresetAbilities[i]);
        }
    }

    // Add customizable abilities
    for (UAbilityData *Ability : AssignedAbilities)
    {
        if (Result.Num() < LoadoutConstants::MAX_WEAPON_ABILITIES)
        {
            Result.Add(Ability);
        }
    }

    return Result;
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetLockedAbilities() const
{
    TArray<UAbilityData *> Result;

    if (!WeaponEntry.Weapon)
    {
        return Result;
    }

    int32 LockedCount = GetLockedAbilityCount();
    for (int32 i = 0; i < LockedCount && i < WeaponEntry.Weapon->PresetAbilities.Num(); ++i)
    {
        Result.Add(WeaponEntry.Weapon->PresetAbilities[i]);
    }

    return Result;
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetCustomizableAbilities() const
{
    return AssignedAbilities;
}

bool FWeaponLoadoutEntry::ValidateAbilities(const FAbilityCollection &OwnedAbilities) const
{
    if (!WeaponEntry.Weapon)
    {
        return true; // No weapon = nothing to validate
    }

    EWeaponType WeaponType = WeaponEntry.Weapon->WeaponType;

    for (UAbilityData *Ability : AssignedAbilities)
    {
        if (!Ability)
        {
            continue; // Empty slots are OK
        }

        // Check ownership
        if (!OwnedAbilities.HasAbility(Ability))
        {
            return false;
        }

        // Check weapon type match
        if (Ability->RequiredWeaponType != WeaponType)
        {
            return false;
        }
    }

    // Check count
    if (AssignedAbilities.Num() > GetCustomizableAbilityCount())
    {
        return false;
    }

    return true;
}

bool FWeaponLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    if (!CanHaveSpells())
    {
        return WeaponEntry.AssignedSpells.Num() == 0;
    }

    ESpellElement WeaponElement = WeaponEntry.GetElement();

    for (USpellData *Spell : WeaponEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (Spell->Element != WeaponElement)
            return false;
    }

    if (WeaponEntry.AssignedSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
        return false;
    return true;
}

void FWeaponLoadoutEntry::InitializeFromWeapon()
{
    AssignedAbilities.Empty();

    if (!WeaponEntry.Weapon)
    {
        return;
    }

    // Copy non-locked preset abilities as starting point
    // (Locked abilities are accessed via GetLockedAbilities(), not stored in AssignedAbilities)
    int32 LockedCount = GetLockedAbilityCount();
    for (int32 i = LockedCount; i < WeaponEntry.Weapon->PresetAbilities.Num(); ++i)
    {
        if (AssignedAbilities.Num() < GetCustomizableAbilityCount())
        {
            AssignedAbilities.Add(WeaponEntry.Weapon->PresetAbilities[i]);
        }
    }
}
