// FRingLoadoutEntry.cpp
// Ring loadout entry implementation

#include "FRingLoadoutEntry.h"
#include "RingData.h"
#include "SpellData.h"

#include "FSpellCollection.h"

int32 FRingLoadoutEntry::GetLockedSpellCount() const
{
    if (!RingEntry.IsEvolved())
    {
        return 0;
    }

    // Evolution defines how many spells are locked
    // TODO: Add LockedSpellCount to EvolutionData if not present
    // For now, use the equipped spells count from evolution
    return RingEntry.GetSpellCount();
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpells() const
{
    TArray<USpellData *> Result;

    // Start with locked/evolution spells
    if (RingEntry.IsEvolved())
    {
        TArray<USpellData *> EvolutionSpells = RingEntry.GetSpells();
        for (USpellData *Spell : EvolutionSpells)
        {
            if (Result.Num() < LoadoutConstants::MAX_SPELL_SLOTS)
            {
                Result.Add(Spell);
            }
        }
    }

    // Add customizable spells
    for (USpellData *Spell : AssignedSpells)
    {
        if (Result.Num() < LoadoutConstants::MAX_SPELL_SLOTS)
        {
            Result.Add(Spell);
        }
    }

    return Result;
}

TArray<USpellData *> FRingLoadoutEntry::GetLockedSpells() const
{
    TArray<USpellData *> Result;

    if (!RingEntry.IsEvolved())
    {
        return Result;
    }

    return RingEntry.GetSpells();
}

bool FRingLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    if (!IsValid())
    {
        return true; // Invalid ring = nothing to validate
    }

    ESpellElement RingElement = RingEntry.GetElement();

    for (USpellData *Spell : AssignedSpells)
    {
        if (!Spell)
        {
            continue; // Empty slots are OK
        }

        // Check ownership
        if (!OwnedSpells.HasSpell(Spell))
        {
            return false;
        }

        // Check element match (unless universal spell)
        if (!Spell->bIsUniversalSpell && Spell->Element != RingElement)
        {
            return false;
        }
    }

    // Check count
    if (AssignedSpells.Num() > GetCustomizableSpellCount())
    {
        return false;
    }

    return true;
}

void FRingLoadoutEntry::InitializeFromRing()
{
    AssignedSpells.Empty();

    if (!RingEntry.IsValid())
    {
        return;
    }

    // If not evolved, could copy default spells from RingData
    // For now, start with empty customizable slots
}
