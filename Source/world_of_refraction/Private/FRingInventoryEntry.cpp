// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "FRingInventoryEntry.h"
#include "RingData.h"
#include "ItemData.h"

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData* InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;

    if (bCopyDefaultCrystal && InRing && InRing->SlottedCrystal)
    {
        // Create crystal entry from default crystal on data asset
        Entry.AttachedCrystal = FCrystalInventoryEntry::CreateFromCrystal(InRing->SlottedCrystal);
    }

    return Entry;
}
