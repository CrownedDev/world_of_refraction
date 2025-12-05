// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "FRingInventoryEntry.h"
#include "RingData.h"
#include "ItemData.h"
#include "EvolutionData.h"

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData* InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;
    
    if (bCopyDefaultCrystal && InRing && InRing->SlottedCrystal)
    {
        // Copy reference to default crystal from data asset
        // NOTE: This shares the same ItemData instance - if you need true duplication,
        // you'd need to duplicate the asset at runtime
        Entry.AttachedCrystal = InRing->SlottedCrystal;
    }
    
    return Entry;
}

ESpellElement FRingInventoryEntry::GetElement() const
{
    // Evolution takes priority
    if (Evolution)
    {
        return Evolution->Element;
    }
    
    // Then attached crystal (RUNTIME STATE - not RingData.SlottedCrystal)
    if (AttachedCrystal)
    {
        return AttachedCrystal->GetAssociatedElement();
    }
    
    // No crystal = no element (ring can't be used)
    return ESpellElement::Generic;
}
