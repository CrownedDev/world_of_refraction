// FWeaponLoadoutEntry.cpp
// Weapon loadout entry implementation

#include "FWeaponLoadoutEntry.h"
#include "WeaponData.h"
#include "AbilityData.h"
#include "SpellData.h"
#include "FAbilityCollection.h"
#include "FSpellCollection.h"
#include "ElementHelpers.h"

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
    // Preset abilities from the weapon asset.
    const TArray<UAbilityData *> Presets = WeaponEntry.Weapon
                                               ? WeaponEntry.Weapon->PresetAbilities
                                               : TArray<UAbilityData *>();

    // Locked weapons (conjured) ignore overrides entirely — presets only.
    if (WeaponEntry.Weapon && WeaponEntry.Weapon->bAbilitiesLocked)
    {
        return Presets;
    }

    // Sequential override merge: AssignedAbilities replace preset slots in
    // order; non-null entries beyond the preset count are appended.
    TArray<UAbilityData *> Result;
    int32 OverrideIndex = 0;

    for (int32 i = 0; i < Presets.Num(); ++i)
    {
        if (OverrideIndex < AssignedAbilities.Num() && AssignedAbilities[OverrideIndex])
        {
            Result.Add(AssignedAbilities[OverrideIndex]);
            ++OverrideIndex;
        }
        else
        {
            Result.Add(Presets[i]);
        }
    }

    while (OverrideIndex < AssignedAbilities.Num())
    {
        if (AssignedAbilities[OverrideIndex])
        {
            Result.Add(AssignedAbilities[OverrideIndex]);
        }
        ++OverrideIndex;
    }

    if (Result.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
    {
        Result.SetNum(LoadoutConstants::MAX_WEAPON_ABILITIES);
    }

    return Result;
}

TArray<USpellData *> FWeaponLoadoutEntry::GetAllSpells() const
{
    // Inventory-entry spells form the base list; AssignedSpells override them.
    const TArray<USpellData *> Presets = WeaponEntry.GetSpells();

    // Sequential override merge: AssignedSpells replace base slots in order;
    // non-null entries beyond the base count are appended.
    TArray<USpellData *> Result;
    int32 OverrideIndex = 0;

    for (int32 i = 0; i < Presets.Num(); ++i)
    {
        if (OverrideIndex < AssignedSpells.Num() && AssignedSpells[OverrideIndex])
        {
            Result.Add(AssignedSpells[OverrideIndex]);
            ++OverrideIndex;
        }
        else
        {
            Result.Add(Presets[i]);
        }
    }

    while (OverrideIndex < AssignedSpells.Num())
    {
        if (AssignedSpells[OverrideIndex])
        {
            Result.Add(AssignedSpells[OverrideIndex]);
        }
        ++OverrideIndex;
    }

    if (Result.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
    {
        Result.SetNum(LoadoutConstants::MAX_SPELL_SLOTS);
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

        // Dual-weapon gate: dual-only abilities reject single-wield weapons.
        if (Ability->bRequiresDualWeapon && !WeaponEntry.Weapon->IsDualWielded())
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

    const ESpellElement WeaponElement = WeaponEntry.GetElement();
    const bool bAnyElement = ElementHelpers::IsAnySpellSource(WeaponElement);

    for (USpellData *Spell : WeaponEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (!bAnyElement && Spell->Element != WeaponElement)
            return false;
    }

    if (WeaponEntry.AssignedSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
        return false;
    return true;
}

void FWeaponLoadoutEntry::InitializeFromWeapon()
{
    // AssignedAbilities is a pure override list — it starts empty.
    // Preset abilities are merged in at query time by GetAllAbilities().
    AssignedAbilities.Empty();
}
