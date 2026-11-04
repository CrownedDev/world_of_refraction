// SkillSlotList.h
// B-351 (D-145, changes CL-20): one skill-slot list per weapon (and, in B-351b, per crystal). The list has one slot per
// tier step (F 1 .. S 6, SlotsForContainerTier) plus the slots an Ability Stone in the stone socket adds. Built-in
// skills pre-fill the slots in order; the player can replace any slot, built-in or not. A replaced built-in is not
// lost: the item still has it, and restoring the slot brings it back. Picks hold the player's choice per slot
// (null = the built-in, or empty past the built-ins).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

namespace SkillSlotList
{
	/** Slots on an item of ItemTier with Stone in its stone socket: the tier's count plus an Ability Stone's slots. */
	inline int32 SlotCount(EItemTier ItemTier, const FRuntimeAttachedItem &Stone)
	{
		return CrystalEffectTable::SlotsForContainerTier(ItemTier) +
			   CrystalEffectTable::GetAttachmentSlotsForTier(CrystalEffectTable::AbilityStoneSlotIdentity(Stone));
	}

	/** The list, slot by slot: the player's pick, else the built-in at that position, else empty (null). */
	template <typename TSkill>
	TArray<TSkill *> Resolve(const TArray<TSkill *> &BuiltIns, const TArray<TSkill *> &Picks, int32 Count)
	{
		TArray<TSkill *> Slots;
		Slots.Reserve(FMath::Max(0, Count));
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			TSkill *Pick = Picks.IsValidIndex(Slot) ? Picks[Slot] : nullptr;
			Slots.Add(Pick ? Pick : (BuiltIns.IsValidIndex(Slot) ? BuiltIns[Slot] : nullptr));
		}
		return Slots;
	}

	/** Puts Skill in Slot (replacing the built-in there, if any). False for a negative slot or a null skill. */
	template <typename TSkill>
	bool Replace(TArray<TSkill *> &Picks, int32 Slot, TSkill *Skill)
	{
		if (Slot < 0 || !Skill)
		{
			return false;
		}
		if (Picks.Num() <= Slot)
		{
			Picks.SetNumZeroed(Slot + 1);
		}
		Picks[Slot] = Skill;
		return true;
	}

	/** Clears the pick in Slot, so the built-in there (if any) is back. False when nothing was picked there. */
	template <typename TSkill>
	bool Restore(TArray<TSkill *> &Picks, int32 Slot)
	{
		if (!Picks.IsValidIndex(Slot) || !Picks[Slot])
		{
			return false;
		}
		Picks[Slot] = nullptr;
		while (Picks.Num() > 0 && !Picks.Last())
		{
			Picks.Pop();
		}
		return true;
	}

	/** True when the player has replaced what Slot holds. */
	template <typename TSkill>
	bool IsReplaced(const TArray<TSkill *> &Picks, int32 Slot)
	{
		return Picks.IsValidIndex(Slot) && Picks[Slot] != nullptr;
	}
}
