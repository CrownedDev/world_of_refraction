// FRingLoadoutEntry.cpp
// Ring loadout entry implementation

#include "FRingLoadoutEntry.h"
#include "RingData.h"
#include "SpellData.h"
#include "FSpellCollection.h"

bool FRingLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    if (!IsValid())
    {
        return true; // Invalid ring = nothing to validate
    }

    ESpellElement RingElement = RingEntry.GetElement();

    for (USpellData *Spell : RingEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (!Spell->bIsUniversalSpell && Spell->Element != RingElement)
            return false;
    }

    if (RingEntry.AssignedSpells.Num() > LoadoutConstants::MAX_SPELL_SLOTS)
        return false;
    return true;
}

void FRingLoadoutEntry::InitializeFromRing()
{
    // Spells now live on RingEntry.AssignedSpells (inventory entry)
    // No initialization needed here - spells are set by CreateFromRing
}