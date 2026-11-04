// StatStone.h
// B-353 (D-142): one StatStone kind with a sub-stat field (FCrystalId::StoneStat). The old single-stat stone kinds
// are retired: Normalize maps one to StatStone on load. The stone rows (StoneRowTable) stay keyed by the old kinds,
// so a StatStone reads its row through RowType / AsRowId and its bonus is the old stone's, unchanged.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/StoneRowTable.h"

namespace StatStone
{
	/** True for a retired single-stat stone kind: a stone row with a sub-stat, other than the Damage Stone (stays). */
	inline bool IsRetiredKind(ECrystalType Type)
	{
		const FStoneRow *Row = StoneRowTable::Find(Type);
		return Row && Row->Stat != ESubStat::None && Type != ECrystalType::DamageStone;
	}

	/** A stat stone for Stat at Tier. */
	inline FCrystalId Make(ESubStat Stat, EItemTier Tier)
	{
		return FCrystalId(ECrystalType::StatStone, Tier, Stat);
	}

	/** An old single-stat stone as the StatStone it now is. Anything else keeps its kind, and a non-StatStone loses a
	 *  stray sub-stat (only a StatStone carries one; a stray value would break stack equality). */
	inline FCrystalId Normalize(const FCrystalId &Id)
	{
		if (IsRetiredKind(Id.Type))
		{
			return Make(StoneRowTable::Find(Id.Type)->Stat, Id.Tier);
		}
		if (Id.Type != ECrystalType::StatStone && Id.StoneStat != ESubStat::None)
		{
			return FCrystalId(Id.Type, Id.Tier);
		}
		return Id;
	}

	/** The kind whose stone row a crystal reads: a StatStone's row is the old kind for its sub-stat; else its own kind. */
	inline ECrystalType RowType(const FCrystalId &Id)
	{
		if (Id.Type != ECrystalType::StatStone)
		{
			return Id.Type;
		}
		for (const FStoneRow &Row : StoneRowTable::ROWS)
		{
			if (Row.Stat == Id.StoneStat && Row.Stat != ESubStat::None)
			{
				return Row.Type;
			}
		}
		return ECrystalType::StatStone; // a StatStone with no sub-stat: no row, inert
	}

	/** The crystal as its row kind (same tier): what the per-kind tables, names and descriptions read. */
	inline FCrystalId AsRowId(const FCrystalId &Id)
	{
		return Id.Type == ECrystalType::StatStone ? FCrystalId(RowType(Id), Id.Tier) : Id;
	}
}
