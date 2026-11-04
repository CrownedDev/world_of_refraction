// FlawlessParry.h
// B-365 (D-158): a Flawless Parry is every hit of one enemy attack or spell perfectly defended by one defender: a
// Perfect Guard or a Perfect Dodge counts for a hit (D-158 assumption 1). It is a condition only, with no built-in
// reward (assumption 2): UDefenseSystem::OnFlawlessParry reports it once, when the attack's last hit is judged, and
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

		/** Counts a judged hit. True exactly once: at the attack's last hit (Judged reaches HitCount), when every hit
		 *  was perfect. A one-hit attack counts. */
		bool AddHit(bool bPerfectHit, int32 HitCount)
		{
			++Judged;
			Perfect += bPerfectHit ? 1 : 0;
			return Judged == FMath::Max(1, HitCount) && Perfect == Judged;
		}
	};

	inline FString GetTallyString(const FTally &Tally, int32 HitCount)
	{
		return FString::Printf(TEXT("%d of %d hit(s) judged, %d perfect"), Tally.Judged, FMath::Max(1, HitCount), Tally.Perfect);
	}
}
