// FWeaponInventoryEntry.cpp
// Weapon inventory entry implementation

#include "FWeaponInventoryEntry.h"
#include "WeaponData.h"
#include "EvolutionItemData.h"

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

    if (bCopyDefaultCrystal && InWeapon && InWeapon->SlottedCrystal)
    {
        Entry.AttachedItem = FRuntimeAttachedItem::FromAsset(InWeapon->SlottedCrystal);

        // Copy default spells from weapon asset
        Entry.AssignedSpells = InWeapon->DefaultSpells;
    }

    if (InWeapon)
    {
        Entry.StatBonus = InWeapon->GetCombinedStatBonus();
    }

    return Entry;
}
