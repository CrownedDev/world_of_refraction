// OverCapRules.h
// B-364 (D-157, Q-157): past the pillar cap of 7, a pillar point becomes an over-cap point: at most
// MAX_PER_PILLAR per pillar, each raising every sub-stat point under that pillar by BONUS_PER_POINT (one multiplier
// on the pillar's composed value, UCharacterDataComponent::GetEvolutionModified*). Run points: they live with the
// run's levels (UTrialRunSubsystem) and end with the run. Combat Tier doesn't read pillars (D-118), so it is unchanged.

#pragma once

#include "CoreMinimal.h"

namespace OverCapRules
{
	/** Q-157 (1): a pillar reads 7 + 3 at most. */
	constexpr int32 MAX_PER_PILLAR = 3;

	/** Q-157 (2): +5 % per over-cap point (at most +15 %). TODO: tune. */
	constexpr float BONUS_PER_POINT = 0.05f;

	/** The multiplier OverCapPoints give the pillar's sub-stats. */
	inline float ScaleFor(int32 OverCapPoints)
	{
		return 1.0f + BONUS_PER_POINT * FMath::Clamp(OverCapPoints, 0, MAX_PER_PILLAR);
	}
}
