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
    }

    return Entry;
}
