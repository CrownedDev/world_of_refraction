// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "FRingInventoryEntry.h"
#include "RingData.h"
#include "EvolutionItemData.h"

#include <atomic>

// Process-local monotonic counter for stable per-instance IDs. Self-contained
// to this TU — not routed through SkillEffectManager so inventory code can
// generate IDs without depending on the subsystem being initialized.
namespace
{
    static std::atomic<int32> GRingInstanceCounter{1};
}

ESpellElement FRingInventoryEntry::GetElement() const
{
    if (HasCrystal() && !AttachedItem.IsBroken())
    {
        return AttachedItem.GetElement();
    }
    return ESpellElement::Generic;
}

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData *InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;
    Entry.InstanceID = GRingInstanceCounter.fetch_add(1);

    if (bCopyDefaultCrystal && InRing && InRing->SlottedCrystal)
    {
        Entry.AttachedItem = FRuntimeAttachedItem::FromAsset(InRing->SlottedCrystal);

        // Copy default spells from ring asset
        Entry.AssignedSpells = InRing->DefaultSpells;
    }

    if (InRing)
    {
        Entry.StatBonus = InRing->GetCombinedStatBonus();
    }

    return Entry;
}
