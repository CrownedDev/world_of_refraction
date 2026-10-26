// GuardRules.h
// Guard (D-084 / D-087, Crown 2026-10-26; B-168): Block and Parry are one defense, Guard. A normal
// Guard is a block; a Perfect Guard is a parry that takes less. Neither reflects: reflecting,
// healing etc. come from effects on "on Perfect Guard". Dodge is unchanged.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"
#include "Combat/Defense/EDefenseType.h"

namespace GuardRules
{
	/** A Parry press is a Guard press: Block is the Guard value, Parry is legacy. */
	inline EDefenseType Normalize(EDefenseType Type)
	{
		return Type == EDefenseType::Parry ? EDefenseType::Block : Type;
	}

	inline bool IsGuard(EDefenseType Type)
	{
		return Normalize(Type) == EDefenseType::Block;
	}

	inline bool IsPerfectGuard(EDefenseType Type, bool bPerfect)
	{
		return IsGuard(Type) && bPerfect;
	}

	/** OBSOLETE (B-202, Q-182 changed by Crown): no caller; every Guard keeps the block montage and
	 *  a Perfect Guard is shown by PlaysPerfectGuardFeedback. Remove after PIE.
	 *  Was: the animation an impact switches the defender to (a Perfect Guard played the parry
	 *  montage, B-182). */
	inline EDefenseType ImpactAnimationOverride(EDefenseType Type, bool bSuccess, bool bPerfect)
	{
		return (bSuccess && IsPerfectGuard(Type, bPerfect)) ? EDefenseType::Parry : EDefenseType::None;
	}

	/** Whether an impact shows the Perfect Guard flash and sound (B-202, Q-182). The animation is
	 *  the same block montage for every Guard. */
	inline bool PlaysPerfectGuardFeedback(EDefenseType Type, bool bSuccess, bool bPerfect)
	{
		return bSuccess && IsPerfectGuard(Type, bPerfect);
	}

	/** What stops an interruptable hit (a choke point): a Perfect Guard or a Dodge, as a parry
	 *  or a dodge did before. */
	inline bool StopsInterruptableHit(EDefenseType Type, bool bSuccess, bool bPerfect)
	{
		return bSuccess && (Type == EDefenseType::Dodge || IsPerfectGuard(Type, bPerfect));
	}

	/** Share of status buildup that still lands through a Guard (a Dodge lands none). */
	inline float GuardBuildupMultiplier(bool bPerfect)
	{
		return bPerfect ? CombatConstants::PARRY_BUILDUP_MULTIPLIER : CombatConstants::BLOCK_BUILDUP_MULTIPLIER;
	}
}
