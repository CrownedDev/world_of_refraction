// GearFind.h
// B-312 (Q-312): the one finder for a weapon / adapter entry by PersistentID in a member's list, folding B-251's
// FindGearIndex and B-252's FindOwnedGear (and the loadout sync's FindOwned). Plain equality: an entry with no ID
// matches a lookup with no ID, as before; callers that must refuse an invalid ID check it first.
// The pool store has its own GUID index (UPoolSubsystem::FindWeaponInPool).

#pragma once

#include "CoreMinimal.h"

namespace WoRGearFind
{
	/** Index of the entry whose PersistentID is ID, or INDEX_NONE. */
	template <class TEntry>
	int32 IndexOf(const TArray<TEntry> &Gear, const FGuid &ID)
	{
		return Gear.IndexOfByPredicate([&ID](const TEntry &Entry) { return Entry.PersistentID == ID; });
	}

	/** The entry whose PersistentID is ID (const or not, as Gear is), or null. */
	template <class TArrayType>
	auto Find(TArrayType &Gear, const FGuid &ID)
	{
		const int32 Index = IndexOf(Gear, ID);
		return Index != INDEX_NONE ? &Gear[Index] : nullptr;
	}
}
