// FWeaponInventoryEntry.cpp
// Weapon inventory entry implementation

#include "FWeaponInventoryEntry.h"
#include "WeaponData.h"
#include "ItemData.h"
#include "EvolutionData.h"
#include "CrystalType.h"

FWeaponInventoryEntry FWeaponInventoryEntry::CreateFromWeapon(UWeaponData *InWeapon, bool bCopyDefaultCrystal)
{
    FWeaponInventoryEntry Entry;
    Entry.Weapon = InWeapon;

    if (bCopyDefaultCrystal && InWeapon && InWeapon->SlottedCrystal)
    {
        // Copy reference to default crystal from data asset
        Entry.AttachedCrystal = InWeapon->SlottedCrystal;

        // If crystal is an evolution crystal, set Evolution reference
        if (InWeapon->SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal &&
            InWeapon->SlottedCrystal->Evolution)
        {
            Entry.Evolution = InWeapon->SlottedCrystal->Evolution;
        }
    }

    return Entry;
}

ESpellElement FWeaponInventoryEntry::GetElement() const
{
    // Evolution takes priority
    if (Evolution)
    {
        return Evolution->Element;
    }

    // Then attached crystal (RUNTIME STATE - not WeaponData.SlottedCrystal)
    if (AttachedCrystal)
    {
        return AttachedCrystal->GetAssociatedElement();
    }

    // Base weapon has no element
    return ESpellElement::Generic;
}
