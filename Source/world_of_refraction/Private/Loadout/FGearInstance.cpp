// FGearInstance.cpp

#include "Loadout/FGearInstance.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** Copies the fields both entry types and the gear instance share (they are named alike). */
	template <typename TFrom, typename TTo>
	void CopySharedGearFields(const TFrom &From, TTo &To)
	{
		To.InstanceID = From.InstanceID;
		To.PersistentID = From.PersistentID;
		To.Tier = From.Tier;
		To.Quality = From.Quality;
		To.AttachedItem = From.AttachedItem;
		To.AssignedSpells.Reset(From.AssignedSpells.Num());
		for (USpellData *Spell : From.AssignedSpells)
		{
			To.AssignedSpells.Add(Spell);
		}
		To.StatBonus = From.StatBonus;
		To.ResistanceBonus = From.ResistanceBonus;
		To.StatPool = From.StatPool;
		To.StatMaxPool = From.StatMaxPool;
		To.ResistancePool = From.ResistancePool;
		To.ResistanceMaxPool = From.ResistanceMaxPool;
	}
}

UWeaponData *FGearInstance::GetWeapon() const
{
	return IsWeapon() ? Cast<UWeaponData>(Asset) : nullptr;
}

URingData *FGearInstance::GetAdapter() const
{
	return IsAdapter() ? Cast<URingData>(Asset) : nullptr;
}

ESpellElement FGearInstance::GetElement() const
{
	return HasAttachment() && !AttachedItem.IsBroken() ? AttachedItem.GetElement() : ESpellElement::None;
}

FGearInstance FGearInstance::FromWeaponEntry(const FWeaponInventoryEntry &Entry)
{
	FGearInstance Gear;
	Gear.Asset = Entry.Weapon;
	Gear.Kind = EGearKind::Weapon;
	CopySharedGearFields(Entry, Gear);
	return Gear;
}

FGearInstance FGearInstance::FromRingEntry(const FRingInventoryEntry &Entry)
{
	FGearInstance Gear;
	Gear.Asset = Entry.Ring;
	Gear.Kind = EGearKind::Adapter;
	CopySharedGearFields(Entry, Gear);
	return Gear;
}

FWeaponInventoryEntry FGearInstance::ToWeaponEntry() const
{
	FWeaponInventoryEntry Entry;
	if (UWeaponData *Weapon = GetWeapon())
	{
		Entry.Weapon = Weapon;
		CopySharedGearFields(*this, Entry);
	}
	return Entry;
}

FRingInventoryEntry FGearInstance::ToRingEntry() const
{
	FRingInventoryEntry Entry;
	if (URingData *Ring = GetAdapter())
	{
		Entry.Ring = Ring;
		CopySharedGearFields(*this, Entry);
	}
	return Entry;
}
