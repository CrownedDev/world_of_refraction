// FlawlessParry.h
// B-365 (D-158): a Flawless Parry is every hit of one enemy attack or spell perfectly defended by one defender: a
// Perfect Guard or a Perfect Dodge counts for a hit (D-158 assumption 1). It is a condition only, with no built-in
// reward (assumption 2): UDefenseSystem::OnFlawlessParry reports it once, when the attack's last hit is judged (or,
// B-367 / D-161, when a perfect defense ends the attack at an interrupt point with every hit so far perfect), and
// the shared conditions read it (EWoRConditionTrigger / EWoRConditionSubject FlawlessParry).
// The tally lives on the attack's per-defender context (FPendingDefenseContext). Debug: GetTallyString; the
// "[Defense] Flawless Parry" log line.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Defense/EDefenseType.h"

namespace FlawlessParry
{
	/** A hit counts when a Guard or a Dodge succeeded on Perfect timing. */
	inline bool IsPerfectHit(EDefenseType DefenseType, bool bSuccess, bool bPerfect)
	{
		return bPerfect && bSuccess && DefenseType != EDefenseType::None;
	}

	/** One defender's hits of one attack. */
	struct FTally
	{
		int32 Judged = 0;
		int32 Perfect = 0;
		/** B-367: a Flawless Parry was reported for this attack (one per defender per attack). */
		bool bReported = false;

		/** Counts a judged hit. True exactly once: at the attack's last hit (Judged reaches HitCount), when every hit
		 *  was perfect. A one-hit attack counts. */
		bool AddHit(bool bPerfectHit, int32 HitCount)
		{
			++Judged;
			Perfect += bPerfectHit ? 1 : 0;
			const bool bFlawless = !bReported && Judged == FMath::Max(1, HitCount) && Perfect == Judged;
			bReported |= bFlawless;
			return bFlawless;
		}

		/** B-367 (D-161): the attack ends early through an interrupt point. True once when every hit judged so far
		 *  was perfect (none judged, or already reported: false). Only the interrupt calls it; an attack stopped any
		 *  other way reports nothing. */
		bool EndAtInterrupt()
		{
			const bool bFlawless = IsFlawlessSoFar();
			bReported |= bFlawless;
			return bFlawless;
		}

		/** Every hit judged so far was perfect and nothing was reported yet (does not report). */
		bool IsFlawlessSoFar() const
		{
			return !bReported && Judged > 0 && Perfect == Judged;
		}
	};

	inline FString GetTallyString(const FTally &Tally, int32 HitCount)
	{
		return FString::Printf(TEXT("%d of %d hit(s) judged, %d perfect"), Tally.Judged, FMath::Max(1, HitCount), Tally.Perfect);
	}
}
