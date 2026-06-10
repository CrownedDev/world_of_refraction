// ZeroSumBrokenStick.h
// Shared zero-sum broken-stick distribution core, extracted VERBATIM from the
// per-pillar substat roll (FEquipmentStatBonus.cpp) so the substat path and the
// resistance roll (FResistanceBonus) share one proven algorithm.
//
// The algorithm: generate a random negative-offset pool on top of the budget,
// hand out (+1) to random slots until the positive pool is spent, then (-1)
// until the negative pool is spent — each slot's |delta| bounded by PerSlotCap
// (20-retry random pick, then linear-scan fallback). Net signed sum == Share
// (modulo cap pressure), with some slots landing negative.
//
// Pure on an int delta array — no field access, no clamp to the final field
// range (the caller applies + clamps per its own type). RNG draw order is
// identical to the original inline core, so the substat roll stays byte-identical.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

namespace ZeroSumRoll
{
	/** Fill OutDeltas[0..SlotCount) with a zero-sum broken-stick distribution of
	 *  `Share` net points across SlotCount slots; each |delta| <= PerSlotCap.
	 *  Returns the net signed sum applied (== Share unless cap pressure dropped
	 *  points). OutDeltas must hold at least SlotCount entries. */
	inline int32 Distribute(int32 SlotCount, int32 PerSlotCap, int32 Share, FRandomStream &RNG, int32 *OutDeltas)
	{
		for (int32 i = 0; i < SlotCount; ++i)
		{
			OutDeltas[i] = 0;
		}
		if (Share <= 0 || SlotCount <= 0)
		{
			return 0;
		}

		// Random negative offset: 0..Share/2. With a small Share this resolves to
		// 0 most of the time (Share=2 -> max_neg=1), so low budgets stay mostly
		// positive while larger budgets see meaningful negative spread.
		const int32 MaxNeg = Share / 2;
		const int32 NegPool = (MaxNeg > 0) ? RNG.RandRange(0, MaxNeg) : 0;
		int32 RemainingPos = Share + NegPool;
		int32 RemainingNeg = NegPool;

		auto PickSlot = [&](int32 SignWanted) -> int32
		{
			// 20 random retries, then linear-scan fallback. SignWanted is +1 (want
			// a slot whose delta can still grow) or -1 (whose delta can still shrink).
			for (int32 Retry = 0; Retry < 20; ++Retry)
			{
				const int32 Cand = RNG.RandRange(0, SlotCount - 1);
				const bool bFits = (SignWanted > 0)
					? (OutDeltas[Cand] < PerSlotCap)
					: (OutDeltas[Cand] > -PerSlotCap);
				if (bFits)
				{
					return Cand;
				}
			}
			for (int32 i = 0; i < SlotCount; ++i)
			{
				const bool bFits = (SignWanted > 0)
					? (OutDeltas[i] < PerSlotCap)
					: (OutDeltas[i] > -PerSlotCap);
				if (bFits)
				{
					return i;
				}
			}
			return -1;
		};

		while (RemainingPos > 0)
		{
			const int32 Slot = PickSlot(+1);
			if (Slot < 0) { break; }
			OutDeltas[Slot] += 1;
			--RemainingPos;
		}
		while (RemainingNeg > 0)
		{
			const int32 Slot = PickSlot(-1);
			if (Slot < 0) { break; }
			OutDeltas[Slot] -= 1;
			--RemainingNeg;
		}

		int32 Net = 0;
		for (int32 i = 0; i < SlotCount; ++i)
		{
			Net += OutDeltas[i];
		}
		return Net;
	}
}
