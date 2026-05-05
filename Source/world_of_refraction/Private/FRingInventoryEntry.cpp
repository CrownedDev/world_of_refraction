// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "FRingInventoryEntry.h"
#include "RingData.h"
#include "ItemData.h"

ESpellElement FRingInventoryEntry::GetElement() const
{
    if (HasCrystal() && !AttachedCrystal.Crystal->IsBroken())
    {
        return AttachedCrystal.GetElement();
    }
    return ESpellElement::Generic;
}

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData *InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;

    if (bCopyDefaultCrystal && InRing && InRing->SlottedCrystal)
    {
        Entry.AttachedCrystal = FCrystalInventoryEntry::CreateFromCrystal(InRing->SlottedCrystal);

        // Copy default spells from ring asset
        Entry.AssignedSpells = InRing->DefaultSpells;
    }

    return Entry;
}
