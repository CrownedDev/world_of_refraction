// FWeaponInventoryEntry.cpp
// Weapon inventory entry implementation

#include "FWeaponInventoryEntry.h"
#include "WeaponData.h"
#include "ItemData.h"
#include "EvolutionData.h"

FWeaponInventoryEntry FWeaponInventoryEntry::CreateFromWeapon(UWeaponData* InWeapon, bool bCopyDefaultCrystal)
{
    FWeaponInventoryEntry Entry;
    Entry.Weapon = InWeapon;
    
    if (bCopyDefaultCrystal && InWeapon && InWeapon->SlottedCrystal)
    {
        // Copy reference to default crystal from data asset
        // NOTE: This shares the same ItemData instance - if you need true duplication,
        // you'd need to duplicate the asset at runtime
        Entry.AttachedCrystal = InWeapon->SlottedCrystal;
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
