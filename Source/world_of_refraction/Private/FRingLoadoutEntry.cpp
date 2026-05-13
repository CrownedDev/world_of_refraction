// FRingLoadoutEntry.cpp
// Ring loadout entry implementation

#include "FRingLoadoutEntry.h"
#include "RingData.h"
#include "SpellData.h"
#include "FSpellCollection.h"
#include "ElementHelpers.h"

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
    return LoadoutConstants::MAX_RING_SPELLS - GetLockedSpellCount();
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpells() const
{
    TArray<USpellData *> Result;

    // Locked/preset spells first.
    if (RingEntry.Ring)
    {
        const int32 LockedCount = GetLockedSpellCount();
        for (int32 i = 0; i < LockedCount && i < RingEntry.Ring->PresetSpells.Num(); ++i)
        {
            Result.Add(RingEntry.Ring->PresetSpells[i]);
        }
    }

    // Customisable spells (capped at total ring slot count).
    for (USpellData *Spell : RingEntry.GetSpells())
    {
        if (Result.Num() < LoadoutConstants::MAX_RING_SPELLS)
        {
            Result.Add(Spell);
        }
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
    if (!RingEntry.Ring)
    {
        return;
    }

    // Mirrors FWeaponLoadoutEntry::InitializeFromWeapon:
    //  - bSpellsLocked == true  → all PresetSpells locked, no copy into customisable.
    //  - bSpellsLocked == false → all PresetSpells copied into RingEntry.AssignedSpells
    //                             as starting state (capped at customisable count).
    // Locked spells stay accessible via GetLockedSpells(), not stored in AssignedSpells.
    const int32 LockedCount = GetLockedSpellCount();
    for (int32 i = LockedCount; i < RingEntry.Ring->PresetSpells.Num(); ++i)
    {
        if (RingEntry.AssignedSpells.Num() < GetCustomizableSpellCount())
        {
            RingEntry.AssignedSpells.Add(RingEntry.Ring->PresetSpells[i]);
        }
    }
}
