// FWeaponLoadoutEntry.cpp
// Weapon loadout entry implementation

#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/WeaponSetRules.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/Entries/FAbilityCollection.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Loadout/SkillSlotList.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WeaponLoadoutEntry"

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
    const int32 TierSlots = GetTierSkillSlotCount();
    // B-098 (Q-098, Crown 2026-11-12): a weapon's spell slots became ability slots. The attached gem / evolution adds
    // what were its spell slots while it is attached, broken or not (no ceiling: any other attachment adds none).
    // B-410: these slots take only None, Generic or the crystal's element (CanHoldSkill).
    constexpr int32 NO_SPELL_SOURCE_SLOTS = 0;
    const int32 ConvertedSpellSlots = CrystalEffectTable::ResolveSpellSlotCap(WeaponEntry.GetAttachedItem(), NO_SPELL_SOURCE_SLOTS);
    return TierSlots + ConvertedSpellSlots;
}

int32 FWeaponLoadoutEntry::GetTierSkillSlotCount() const
{
    // D-175: a stone adds none. B-477 (D-211): a pair's slots are the average of its two tiers', rounded down.
    return WeaponSetRules::GetSetTierSlots(*this);
}

bool FWeaponLoadoutEntry::IsGemSkillSlot(int32 Slot) const
{
    return Slot >= GetTierSkillSlotCount() && Slot < GetSkillSlotCount();
}

bool FWeaponLoadoutEntry::CanHoldSkill(int32 Slot, const UAbilityData *Ability, FText *OutReason) const
{
    if (!Ability || !IsGemSkillSlot(Slot))
    {
        return true; // emptying a slot, or a tier slot: no element limit
    }
    const ESpellElement GemElement = WeaponEntry.GetAttachedItem().GetElement();
    if (Ability->Element == ESpellElement::None || Ability->Element == ESpellElement::Generic || Ability->Element == GemElement)
    {
        return true;
    }
    if (OutReason)
    {
        *OutReason = FText::Format(LOCTEXT("GemSlotElement", "{0} is {1}: this slot takes only {2} skills or skills with no element."),
                                   FText::FromString(Ability->Name), WoRGameNames::GetElementName(Ability->Element),
                                   WoRGameNames::GetElementName(GemElement));
    }
    return false;
}

FString FWeaponLoadoutEntry::GetSkillSlotString() const
{
    const TArray<UAbilityData *> BuiltIns = WeaponSetRules::GetSetBuiltIns(*this); // B-477: a pair's built-ins pre-fill both
    const TArray<UAbilityData *> Slots = GetSkillSlots();
    FString Out = FString::Printf(TEXT("%s skill slots: %d (%s)"), *GetNameSafe(WeaponEntry.Weapon), Slots.Num(),
                                  WeaponEntry.bSkillSlotsActive ? TEXT("one list, active") : TEXT("one list not edited yet: the old lists play"));
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        const bool bReplaced = SkillSlotList::IsReplaced(WeaponEntry.SkillPicks, Slot);
        const UAbilityData *BuiltIn = BuiltIns.IsValidIndex(Slot) ? BuiltIns[Slot] : nullptr;
        Out += FString::Printf(TEXT("\n  [%d] %s%s%s"), Slot, Slots[Slot] ? *Slots[Slot]->Name : TEXT("-"),
                               bReplaced ? (BuiltIn ? *FString::Printf(TEXT(" (pick; built-in %s restorable)"), *BuiltIn->Name) : TEXT(" (pick)"))
                                         : (BuiltIn ? TEXT(" (built-in)") : TEXT("")),
                               IsGemSkillSlot(Slot) ? *FString::Printf(TEXT(" [gem slot: %s, None or Generic]"), // B-410
                                                                       *UEnum::GetDisplayValueAsText(WeaponEntry.GetAttachedItem().GetElement()).ToString())
                                                    : TEXT(""));
    }
    return Out;
}

TArray<UAbilityData *> FWeaponLoadoutEntry::GetSkillSlots() const
{
    // B-477 (D-210): one merged list for a pair: both weapons' built-ins pre-fill it, the main hand's first.
    const TArray<UAbilityData *> BuiltIns = WeaponSetRules::GetSetBuiltIns(*this);
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
    if (Slot >= GetSkillSlotCount() || !CanHoldSkill(Slot, Ability) || !SkillSlotList::Replace(WeaponEntry.SkillPicks, Slot, Ability))
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
    // B-098 / B-411 (D-027): weapons give no spells; the spells an old save or asset holds stay in the data, unread.
    return TArray<USpellData *>();
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

#undef LOCTEXT_NAMESPACE
