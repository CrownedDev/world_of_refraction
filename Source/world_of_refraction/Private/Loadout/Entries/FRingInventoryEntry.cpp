// FRingInventoryEntry.cpp
// Ring inventory entry implementation

#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Equipment/Rings/RingData.h"

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
    return ESpellElement::None; // No (or broken) crystal — no element
}

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData *InRing, bool bCopyDefaultCrystal)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;
    Entry.InstanceID = GRingInstanceCounter.fetch_add(1);

    if (bCopyDefaultCrystal && InRing && InRing->HasCrystal())
    {
        Entry.AttachedItem = FRuntimeAttachedItem::FromAttachedItem(InRing->AttachedItem);

        // Copy default spells from ring asset
        Entry.AssignedSpells = InRing->DefaultSpells;
    }

    if (InRing)
    {
        // BASE ONLY (U4) — see CreateFromWeapon: asset Generated = preview,
        // never copied; toggle-ON acquisitions roll fresh via ApplyPickupRoll.
        Entry.StatBonus = InRing->BaseStatBonus;
        Entry.ResistanceBonus = InRing->BaseResistance;

        // Cluster 2a: seed per-instance Tier from the asset + placeholder Quality (weighted roll
        // later). Seeded in the factory — not just AddRing — so loadout-inflated entries get the
        // asset tier too. No-op: nothing reads Entry.Tier until cluster 2b.
        Entry.Tier = InRing->Tier;
        Entry.Quality = EItemQuality::C_Quality;
    }

    return Entry;
}
