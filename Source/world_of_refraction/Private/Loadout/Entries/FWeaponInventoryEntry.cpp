// FWeaponInventoryEntry.cpp
// Weapon inventory entry implementation

#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Equipment/Weapons/WeaponData.h"

#include <atomic>

// Process-local monotonic counter for stable per-instance IDs. Self-contained
// to this TU — not routed through SkillEffectManager so inventory code can
// generate IDs without depending on the subsystem being initialized.
namespace
{
    static std::atomic<int32> GWeaponInstanceCounter{1};
}

FWeaponInventoryEntry FWeaponInventoryEntry::CreateFromWeapon(UWeaponData *InWeapon, bool bCopyDefaultCrystal)
{
    FWeaponInventoryEntry Entry;
    Entry.Weapon = InWeapon;
    Entry.InstanceID = GWeaponInstanceCounter.fetch_add(1);

    if (bCopyDefaultCrystal && InWeapon && InWeapon->HasCrystal())
    {
        Entry.AttachedItem = FRuntimeAttachedItem::FromAttachedItem(InWeapon->AttachedItem);

        // Copy default spells from weapon asset
        Entry.AssignedSpells = InWeapon->DefaultSpells;
    }

    if (InWeapon)
    {
        // BASE ONLY (U4) — the asset's Generated layers are designer PREVIEW and
        // must not leak into real instances. Toggle-ON acquisitions overwrite
        // these with Base + a fresh per-instance roll (ApplyPickupRoll, U2).
        Entry.StatBonus = InWeapon->BaseStatBonus;
        Entry.ResistanceBonus = InWeapon->BaseResistance;

        // Cluster 2a: seed per-instance Tier from the asset (leveling mutates it later) + a
        // placeholder Quality (the weighted drop-roll lands in a later cluster). Seeded HERE in the
        // factory — not just AddWeapon — so loadout-inflated entries (the primary equip path) also
        // carry the asset tier. Still a behavioural no-op: nothing reads Entry.Tier until cluster 2b.
        Entry.Tier = InWeapon->Tier;
        // C_Quality placeholder: the real Quality is rolled at the fresh-pickup mint point
        // (AddWeapon, bRandomGenerateOnPickup → EconomyYield::RollQuality). The factory must NOT
        // roll — loadout re-hydration reuses it and would re-roll on every equip.
        // TODO(shop-roll): purchased items currently get this C_Quality placeholder. The real
        // design: the SHOP stocks pre-rolled items (tier+quality rolled at shelf-population), and
        // purchase CARRIES that shelf-rolled quality through — no roll at point-of-sale, not a
        // fixed C. Gated on the loot/shop generator (does not exist yet). Until then, purchase = C.
        Entry.Quality = EItemQuality::C_Quality;
    }

    return Entry;
}
