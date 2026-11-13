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
#include "Loadout/SkillSlotList.h"
#include "Loadout/WeaponSpellRules.h"

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

int32 FWeaponLoadoutEntry::GetCustomizableAbilityCount() const
{
    // Cap keys on the weapon's own tier; no weapon -> the flat ceiling.
    const int32 Cap = WeaponEntry.Weapon
                          ? CrystalEffectTable::SlotsForContainerTier(WeaponEntry.Tier)
                          : LoadoutConstants::MAX_WEAPON_ABILITIES;
    return Cap - GetLockedAbilityCount();
}

int32 FWeaponLoadoutEntry::GetSkillSlotCount() const
{
    const int32 TierSlots = CrystalEffectTable::SlotsForContainerTier(WeaponEntry.Tier); // D-175: a stone adds none
    // B-098 (Q-098, Crown 2026-11-12): a weapon's spell slots became ability slots. The attached gem / evolution adds
    // what were its spell slots while it is attached, broken or not (no ceiling: any other attachment adds none).
    // TODO: B-410 — these slots take only None, Generic or the crystal's element once abilities carry an element.
    constexpr int32 NO_SPELL_SOURCE_SLOTS = 0;
    const int32 ConvertedSpellSlots = WeaponSpellRules::WeaponsGiveSpells()
                                          ? 0
                                          : CrystalEffectTable::ResolveSpellSlotCap(WeaponEntry.GetAttachedItem(), NO_SPELL_SOURCE_SLOTS);
    return TierSlots + ConvertedSpellSlots;
}

FString FWeaponLoadoutEntry::GetSkillSlotString() const
{
    const TArray<UAbilityData *> BuiltIns = WeaponEntry.Weapon ? WeaponEntry.Weapon->PresetAbilities : TArray<UAbilityData *>();
    const TArray<UAbilityData *> Slots = GetSkillSlots();
    FString Out = FString::Printf(TEXT("%s skill slots: %d (%s)"), *GetNameSafe(WeaponEntry.Weapon), Slots.Num(),
                                  WeaponEntry.bSkillSlotsActive ? TEXT("one list, active") : TEXT("one list not edited yet: the old lists play"));
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        const bool bReplaced = SkillSlotList::IsReplaced(WeaponEntry.SkillPicks, Slot);
        const UAbilityData *BuiltIn = BuiltIns.IsValidIndex(Slot) ? BuiltIns[Slot] : nullptr;
        Out += FString::Printf(TEXT("\n  [%d] %s%s"), Slot, Slots[Slot] ? *Slots[Slot]->Name : TEXT("-"),
                               bReplaced ? (BuiltIn ? *FString::Printf(TEXT(" (pick; built-in %s restorable)"), *BuiltIn->Name) : TEXT(" (pick)"))
                                         : (BuiltIn ? TEXT(" (built-in)") : TEXT("")));
    }
    return Out;
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetSkillSlots() const
{
    const TArray<UAbilityData *> BuiltIns = WeaponEntry.Weapon ? WeaponEntry.Weapon->PresetAbilities : TArray<UAbilityData *>();
    return SkillSlotList::Resolve(BuiltIns, WeaponEntry.SkillPicks, GetSkillSlotCount());
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetSkillSlotAbilities() const
{
    if (!WeaponEntry.bSkillSlotsActive)
    {
        return GetAllAbilities(); // pre-D-145 lists until the player edits this weapon's one list (OBSOLETE path)
    }
    TArray<UAbilityData *> Abilities = GetSkillSlots();
    Abilities.RemoveAll([](const UAbilityData *Ability) { return Ability == nullptr; });
    return Abilities;
}

bool FWeaponLoadoutEntry::ReplaceSkill(int32 Slot, UAbilityData *Ability)
{
    if (Slot >= GetSkillSlotCount() || !SkillSlotList::Replace(WeaponEntry.SkillPicks, Slot, Ability))
    {
        return false;
    }
    WeaponEntry.bSkillSlotsActive = true;
    return true;
}

bool FWeaponLoadoutEntry::RestoreSkill(int32 Slot)
{
    if (!SkillSlotList::Restore(WeaponEntry.SkillPicks, Slot))
    {
        return false;
    }
    WeaponEntry.bSkillSlotsActive = true;
    return true;
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
    if (!WeaponSpellRules::WeaponsGiveSpells())
    {
        return TArray<USpellData *>(); // B-098 (D-027): weapons give no spells; the data stays, unread
    }
    TArray<USpellData *> Result = GetAllSpellsUncapped();
    if (Result.Num() > GetSpellSlotCap())
    {
        Result.SetNum(GetSpellSlotCap());
    }
    return Result;
}

int32 FWeaponLoadoutEntry::GetSpellSlotCap() const
{
    // Gem crystals and evolutions (Q-147) gate spell slots by their own tier; otherwise
    // the cap falls through to the weapon's own tier (SlotsForContainerTier), MAX_SPELL_SLOTS only
    // as the no-weapon ceiling.
    return CrystalEffectTable::ResolveSpellSlotCap(
        WeaponEntry.GetAttachedItem(),
        WeaponEntry.Weapon ? CrystalEffectTable::SlotsForContainerTier(WeaponEntry.Tier)
                           : LoadoutConstants::MAX_SPELL_SLOTS);
}

int32 FWeaponLoadoutEntry::GetSpellsOverCap() const
{
    if (!WeaponSpellRules::WeaponsGiveSpells())
    {
        return 0; // B-098: nothing is listed, so nothing is over the cap
    }
    return FMath::Max(0, GetAllSpellsUncapped().Num() - GetSpellSlotCap());
}

TArray<USpellData *> FWeaponLoadoutEntry::GetSpellSlots() const
{
    if (!WeaponSpellRules::WeaponsGiveSpells() || !WeaponEntry.AttachedItem.CanProvideSpells())
    {
        return TArray<USpellData *>();
    }
    return SkillSlotList::Resolve(WeaponEntry.AssignedSpells, WeaponEntry.SpellPicks, GetSpellSlotCap());
}

bool FWeaponLoadoutEntry::ReplaceSpell(int32 Slot, USpellData *Spell)
{
    if (!WeaponSpellRules::WeaponsGiveSpells() || !WeaponEntry.AttachedItem.CanProvideSpells() || Slot >= GetSpellSlotCap() ||
        !SkillSlotList::Replace(WeaponEntry.SpellPicks, Slot, Spell))
    {
        return false;
    }
    WeaponEntry.bSpellSlotsActive = true;
    return true;
}

bool FWeaponLoadoutEntry::RestoreSpell(int32 Slot)
{
    if (!WeaponSpellRules::WeaponsGiveSpells() || !SkillSlotList::Restore(WeaponEntry.SpellPicks, Slot))
    {
        return false;
    }
    WeaponEntry.bSpellSlotsActive = true;
    return true;
}

FString FWeaponLoadoutEntry::GetSpellSlotString() const
{
    const TArray<USpellData *> Slots = GetSpellSlots();
    FString Out = FString::Printf(TEXT("%s spell slots: %d (%s)"), *GetNameSafe(WeaponEntry.Weapon), Slots.Num(),
                                  WeaponEntry.bSpellSlotsActive ? TEXT("one list, active") : TEXT("one list not edited yet: the old merge plays"));
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        const USpellData *BuiltIn = WeaponEntry.AssignedSpells.IsValidIndex(Slot) ? WeaponEntry.AssignedSpells[Slot] : nullptr;
        const bool bReplaced = SkillSlotList::IsReplaced(WeaponEntry.SpellPicks, Slot);
        Out += FString::Printf(TEXT("\n  [%d] %s%s"), Slot, *GetNameSafe(Slots[Slot]),
                               bReplaced ? (BuiltIn ? *FString::Printf(TEXT(" (pick; built-in %s restorable)"), *GetNameSafe(BuiltIn)) : TEXT(" (pick)"))
                                         : (BuiltIn ? TEXT(" (built-in)") : TEXT("")));
    }
    return Out;
}

TArray<USpellData *> FWeaponLoadoutEntry::GetAllSpellsUncapped() const
{
    // B-351d (D-145): once the player has edited the crystal's one list, it is the list (the loadout's AssignedSpells
    // override is OBSOLETE then).
    if (WeaponEntry.bSpellSlotsActive)
    {
        TArray<USpellData *> Spells = GetSpellSlots();
        Spells.RemoveAll([](const USpellData *Spell) { return Spell == nullptr; });
        return Spells;
    }

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

        // Wield-mode gate: abilities may require a specific off-hand configuration.
        if (!Ability->AllowsWieldMode(WeaponEntry.Weapon->WieldMode))
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

    for (USpellData *Spell : WeaponEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, WeaponElement))
            return false;
    }

    if (WeaponEntry.AssignedSpells.Num() > CrystalEffectTable::ResolveSpellSlotCap(
            Attachment,
            WeaponEntry.Weapon ? CrystalEffectTable::SlotsForContainerTier(WeaponEntry.Tier)
                               : LoadoutConstants::MAX_SPELL_SLOTS))
        return false;
    return true;
}

void FWeaponLoadoutEntry::InitializeFromWeapon()
{
    // AssignedAbilities is a pure override list — it starts empty.
    // Preset abilities are merged in at query time by GetAllAbilities().
    AssignedAbilities.Empty();
}
