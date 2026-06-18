// FWeaponLoadoutEntry.cpp
// Weapon loadout entry implementation

#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/Entries/FAbilityCollection.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

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

    // Gem crystals gate spell slots by tier; evolution keeps the flat ceiling.
    const int32 SlotCap = CrystalEffectTable::ResolveSpellSlotCap(
        WeaponEntry.GetAttachedItem(), LoadoutConstants::MAX_SPELL_SLOTS);
    if (Result.Num() > SlotCap)
    {
        Result.SetNum(SlotCap);
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

TArray<UAbilityData *> FWeaponLoadoutEntry::GetAugmentStoneAbilities() const
{
    // Augment-stone abilities exist only while a augment stone is attached — gate the
    // way GetAllSpells gates spells on a spell-capable attachment.
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    // A direct augment stone, OR a fusion carrying an AbilityStone half, can grant
    // augment-stone abilities. The slot cap (below) resolves to 0 when there is no
    // AbilityStone identity, so a non-AbilityStone attachment still yields nothing.
    if (!Attachment.IsAugmentStone() && !Attachment.IsFusion())
    {
        return TArray<UAbilityData *>();
    }

    // Base layer: the weapon asset's DefaultAbilities — same role PresetAbilities
    // plays in GetAllAbilities (read live from the asset, not a seeded array).
    const TArray<UAbilityData *> Defaults = WeaponEntry.Weapon
                                                ? WeaponEntry.Weapon->DefaultAbilities
                                                : TArray<UAbilityData *>();

    // Locked attachments ignore overrides entirely — defaults only. Mirrors
    // GetAllAbilities's bAbilitiesLocked short-circuit, but keyed on the generic
    // bLockSkills (the lock for whatever skills the attachment provides).
    if (WeaponEntry.Weapon && WeaponEntry.Weapon->bLockSkills)
    {
        return Defaults;
    }

    // Sequential override merge: AssignedAugmentStoneAbilities replace default slots
    // in order; non-null entries beyond the default count are appended.
    TArray<UAbilityData *> Result;
    int32 OverrideIndex = 0;

    for (int32 i = 0; i < Defaults.Num(); ++i)
    {
        if (OverrideIndex < AssignedAugmentStoneAbilities.Num() && AssignedAugmentStoneAbilities[OverrideIndex])
        {
            Result.Add(AssignedAugmentStoneAbilities[OverrideIndex]);
            ++OverrideIndex;
        }
        else
        {
            Result.Add(Defaults[i]);
        }
    }

    while (OverrideIndex < AssignedAugmentStoneAbilities.Num())
    {
        if (AssignedAugmentStoneAbilities[OverrideIndex])
        {
            Result.Add(AssignedAugmentStoneAbilities[OverrideIndex]);
        }
        ++OverrideIndex;
    }

    // Per-tier slot cap from the attachment's ABILITY identity — its own Id for a
    // direct augment stone, or the AbilityStone half for a fusion. DamageStone /
    // no-AbilityStone-half -> 0, so only an AbilityStone grants slots.
    const int32 SlotLimit = CrystalEffectTable::GetAttachmentSlotsForTier(
        CrystalEffectTable::AbilityStoneSlotIdentity(Attachment));
    if (Result.Num() > SlotLimit)
    {
        Result.SetNum(SlotLimit);
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

        // Belt (step 6a): a basic attack (bIsAttack) can't be slotted as an ability.
        if (Ability->IsAttack())
        {
            return false;
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

bool FWeaponLoadoutEntry::ValidateAugmentStoneAbilities(const FAbilityCollection &OwnedAbilities) const
{
    if (!WeaponEntry.Weapon)
    {
        return true; // No weapon = nothing to validate
    }

    // Augment-stone abilities are orphaned without an attached augment stone or a
    // fusion (which may carry an AbilityStone half) — valid only when none are
    // assigned. The slot cap below rejects any beyond the AbilityStone identity's
    // tier (0 when there is no AbilityStone half).
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    if (!Attachment.IsAugmentStone() && !Attachment.IsFusion())
    {
        return AssignedAugmentStoneAbilities.Num() == 0;
    }

    EWeaponType WeaponType = WeaponEntry.Weapon->WeaponType;

    for (UAbilityData *Ability : AssignedAugmentStoneAbilities)
    {
        if (!Ability)
        {
            continue; // Empty slots are OK
        }

        // Belt (step 6a): a basic attack (bIsAttack) can't be slotted as an ability.
        if (Ability->IsAttack())
        {
            return false;
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

    // Check count — per-tier slot cap from the attachment's ABILITY identity (own Id
    // for a direct augment stone, the AbilityStone half for a fusion; 0 otherwise).
    const int32 SlotLimit = CrystalEffectTable::GetAttachmentSlotsForTier(
        CrystalEffectTable::AbilityStoneSlotIdentity(Attachment));
    if (AssignedAugmentStoneAbilities.Num() > SlotLimit)
    {
        return false;
    }

    return true;
}

bool FWeaponLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    // A augment stone grants no spells; its Generic element must not be read as a
    // spell-element mismatch. Treat it like a no-spell attachment — valid only
    // when nothing is assigned (same return as the !CanHaveSpells gate).
    const FRuntimeAttachedItem &Attachment = WeaponEntry.GetAttachedItem();
    const bool bIsAugmentStone = Attachment.IsAugmentStone();

    if (!CanHaveSpells() || bIsAugmentStone)
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

    if (WeaponEntry.AssignedSpells.Num() > CrystalEffectTable::ResolveSpellSlotCap(Attachment, LoadoutConstants::MAX_SPELL_SLOTS))
        return false;
    return true;
}

void FWeaponLoadoutEntry::InitializeFromWeapon()
{
    // AssignedAbilities is a pure override list — it starts empty.
    // Preset abilities are merged in at query time by GetAllAbilities().
    AssignedAbilities.Empty();
    AssignedAugmentStoneAbilities.Empty();
}
