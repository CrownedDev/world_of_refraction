// FRingLoadoutEntry.cpp
// Ring loadout entry implementation

#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

int32 FRingLoadoutEntry::GetLockedSpellCount() const
{
    if (!RingEntry.Ring)
    {
        return 0;
    }
    // Use ring's locked status — if spells are locked, all preset spells are locked.
    // Mirrors FWeaponLoadoutEntry::GetLockedAbilityCount.
    if (RingEntry.Ring->bSpellsLocked)
    {
        return RingEntry.Ring->PresetSpells.Num();
    }
    return 0;
}

int32 FRingLoadoutEntry::GetCustomizableSpellCount() const
{
    return FMath::Max(0,
                      CrystalEffectTable::ResolveSpellSlotCap(
                          RingEntry.GetAttachedItem(), LoadoutConstants::MAX_RING_SPELLS)
                          - GetLockedSpellCount());
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpells() const
{
    // Preset spells from the ring asset.
    const TArray<USpellData *> Presets = RingEntry.Ring
                                            ? RingEntry.Ring->PresetSpells
                                            : TArray<USpellData *>();

    // Locked rings (conjured) ignore overrides entirely — presets only.
    if (RingEntry.Ring && RingEntry.Ring->bSpellsLocked)
    {
        return Presets;
    }

    // Sequential override merge: customisable spells replace preset slots in
    // order; non-null entries beyond the preset count are appended.
    const TArray<USpellData *> Overrides = RingEntry.GetSpells();

    TArray<USpellData *> Result;
    int32 OverrideIndex = 0;

    for (int32 i = 0; i < Presets.Num(); ++i)
    {
        if (OverrideIndex < Overrides.Num() && Overrides[OverrideIndex])
        {
            Result.Add(Overrides[OverrideIndex]);
            ++OverrideIndex;
        }
        else
        {
            Result.Add(Presets[i]);
        }
    }

    while (OverrideIndex < Overrides.Num())
    {
        if (Overrides[OverrideIndex])
        {
            Result.Add(Overrides[OverrideIndex]);
        }
        ++OverrideIndex;
    }

    // Gem crystals gate spell slots by tier; evolution keeps the flat ceiling.
    const int32 SlotCap = CrystalEffectTable::ResolveSpellSlotCap(
        RingEntry.GetAttachedItem(), LoadoutConstants::MAX_RING_SPELLS);
    if (Result.Num() > SlotCap)
    {
        Result.SetNum(SlotCap);
    }

    return Result;
}

TArray<USpellData *> FRingLoadoutEntry::GetLockedSpells() const
{
    TArray<USpellData *> Result;

    if (!RingEntry.Ring)
    {
        return Result;
    }

    const int32 LockedCount = GetLockedSpellCount();
    for (int32 i = 0; i < LockedCount && i < RingEntry.Ring->PresetSpells.Num(); ++i)
    {
        Result.Add(RingEntry.Ring->PresetSpells[i]);
    }

    return Result;
}

TArray<USpellData *> FRingLoadoutEntry::GetCustomizableSpells() const
{
    return RingEntry.GetSpells();
}

bool FRingLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    if (!IsValid())
    {
        return true; // Invalid ring = nothing to validate
    }

    const ESpellElement RingElement = RingEntry.GetElement();
    const bool bAnyElement = ElementHelpers::IsAnySpellSource(RingElement);

    for (USpellData *Spell : RingEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (!bAnyElement && Spell->Element != RingElement)
            return false;
    }

    // Customisable count must fit in the unlocked partition.
    if (RingEntry.AssignedSpells.Num() > GetCustomizableSpellCount())
        return false;
    return true;
}

void FRingLoadoutEntry::InitializeFromRing()
{
    // RingEntry.AssignedSpells is a pure override list — it starts empty.
    // Preset spells are merged in at query time by GetAllSpells().
    RingEntry.AssignedSpells.Empty();
}
