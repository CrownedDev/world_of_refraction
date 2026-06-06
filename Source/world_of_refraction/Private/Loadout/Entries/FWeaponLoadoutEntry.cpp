// FWeaponLoadoutEntry.cpp
// Weapon loadout entry implementation

#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/Entries/FAbilityCollection.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Skills/Definitions/ElementHelpers.h"

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

TArray<UAbilityData *> FWeaponLoadoutEntry::GetWhetstoneAbilities() const
{
    // Whetstone abilities exist only while a whetstone is attached — gate the
    // way GetAllSpells gates spells on a spell-capable attachment.
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    const bool bIsWhetstone = Attachment.IsWhetstone();
    if (!bIsWhetstone)
    {
        return TArray<UAbilityData *>();
    }

    // No preset merge — whetstones are asset-less, so there are no preset
    // abilities to fold in. Skip nulls, cap to MAX_WHETSTONE_ABILITIES.
    TArray<UAbilityData *> Result;
    for (UAbilityData *Ability : AssignedWhetstoneAbilities)
    {
        if (Ability)
        {
            Result.Add(Ability);
        }
    }

    if (Result.Num() > LoadoutConstants::MAX_WHETSTONE_ABILITIES)
    {
        Result.SetNum(LoadoutConstants::MAX_WHETSTONE_ABILITIES);
    }

    return Result;
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

bool FWeaponLoadoutEntry::ValidateWhetstoneAbilities(const FAbilityCollection &OwnedAbilities) const
{
    if (!WeaponEntry.Weapon)
    {
        return true; // No weapon = nothing to validate
    }

    // Whetstone abilities are orphaned without an attached whetstone — valid
    // only when none are assigned.
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    const bool bIsWhetstone = Attachment.IsWhetstone();
    if (!bIsWhetstone)
    {
        return AssignedWhetstoneAbilities.Num() == 0;
    }

    EWeaponType WeaponType = WeaponEntry.Weapon->WeaponType;

    for (UAbilityData *Ability : AssignedWhetstoneAbilities)
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
    if (AssignedWhetstoneAbilities.Num() > LoadoutConstants::MAX_WHETSTONE_ABILITIES)
    {
        return false;
    }

    return true;
}

bool FWeaponLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    // A Whetstone grants no spells; its Generic element must not be read as a
    // spell-element mismatch. Treat it like a no-spell attachment — valid only
    // when nothing is assigned (same return as the !CanHaveSpells gate).
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    const bool bIsWhetstone = Attachment.IsWhetstone();

    if (!CanHaveSpells() || bIsWhetstone)
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
    AssignedWhetstoneAbilities.Empty();
}
