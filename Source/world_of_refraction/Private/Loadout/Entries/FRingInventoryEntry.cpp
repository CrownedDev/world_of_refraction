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

FRingInventoryEntry FRingInventoryEntry::CreateFromAuthoredRing(URingData *InRing)
{
    FRingInventoryEntry Entry = CreateFromRing(InRing);
    if (InRing && InRing->HasCrystal())
    {
        Entry.AttachedItem = FRuntimeAttachedItem::FromAttachedItem(InRing->AttachedItem);

        // TODO: (B-518, D-224) stage 6 stops this copy: the list is OBSOLETE and no longer plays (the picks are the list).
        Entry.AssignedSpells = InRing->DefaultSpells;
    }
    return Entry;
}

FRingInventoryEntry FRingInventoryEntry::CreateFromRing(URingData *InRing)
{
    FRingInventoryEntry Entry;
    Entry.Ring = InRing;
    Entry.InstanceID = GRingInstanceCounter.fetch_add(1);
    // B-521 (D-224, asset overhaul stage 3): a new piece starts empty; see CreateFromAuthoredRing.

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
        // C_Quality placeholder: real Quality is rolled at the fresh-pickup mint point (AddRing,
        // bRandomGenerateOnPickup → EconomyYield::RollQuality). The factory must NOT roll —
        // loadout re-hydration reuses it and would re-roll on every equip.
        // TODO(shop-roll): purchased items currently get this C_Quality placeholder. The real
        // design: the SHOP stocks pre-rolled items (tier+quality rolled at shelf-population), and
        // purchase CARRIES that shelf-rolled quality through — no roll at point-of-sale, not a
        // fixed C. Gated on the loot/shop generator (does not exist yet). Until then, purchase = C.
        Entry.Quality = EItemQuality::C_Quality;
    }

    return Entry;
}
