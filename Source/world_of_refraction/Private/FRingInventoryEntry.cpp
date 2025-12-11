// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "FRingInventoryEntry.h"
#include "RingData.h"
#include "ItemData.h"
#include "EvolutionData.h"
#include "CrystalType.h"

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData *InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;

    if (bCopyDefaultCrystal && InRing && InRing->SlottedCrystal)
    {
        // Copy reference to default crystal from data asset
        Entry.AttachedCrystal = InRing->SlottedCrystal;

        // If crystal is an evolution crystal, set Evolution reference
        if (InRing->SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal &&
            InRing->SlottedCrystal->Evolution)
        {
            Entry.Evolution = InRing->SlottedCrystal->Evolution;
        }
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
