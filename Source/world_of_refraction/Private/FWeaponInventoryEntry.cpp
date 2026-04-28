// FWeaponInventoryEntry.cpp
// Weapon inventory entry implementation

#include "FWeaponInventoryEntry.h"
#include "WeaponData.h"
#include "ItemData.h"

FWeaponInventoryEntry FWeaponInventoryEntry::CreateFromWeapon(UWeaponData *InWeapon, bool bCopyDefaultCrystal)
{
    FWeaponInventoryEntry Entry;
    Entry.Weapon = InWeapon;

    if (bCopyDefaultCrystal && InWeapon && InWeapon->SlottedCrystal)
    {
        // Create crystal entry from default crystal on data asset
        Entry.AttachedCrystal = FCrystalInventoryEntry::CreateFromCrystal(InWeapon->SlottedCrystal);

        // Copy default spells from weapon asset
        Entry.AssignedSpells = InWeapon->DefaultSpells;
    }

    return Entry;
}
