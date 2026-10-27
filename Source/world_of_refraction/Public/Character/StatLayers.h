// StatLayers.h
// B-042p (StatMathsRedesign.md §2.1, Pattern P): the four layers of a composed sub-stat, so the
// getter and the GAS source effect build the same number. The stat-derived base is capped alone,
// then gear, stone and buffs multiply it, bounded by the compose clamp.
// Debug: FWoRStatLayers::ToString.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"

struct FWoRStatLayers
{
	/** The stat-derived part, capped alone at STAT_MULT_CAP. */
	float StatBase = 1.0f;
	/** 1 + the gear points as a fraction. */
	float GearFactor = 1.0f;
	/** 1 + the attached stone's percent. */
	float StoneFactor = 1.0f;
	/** 1 + the net buff / debuff percent. */
	float TransientFactor = 1.0f;

	/** StatBase x gear x stone x transient, clamped to [STAT_MODIFIER_MIN, STAT_MODIFIER_MAX]. */
	float Compose() const
	{
		return Clamp(StatBase * GearFactor * StoneFactor * TransientFactor);
	}

	/** The compose clamp, shared with the attribute set's clamp on these stats. */
	static float Clamp(float Composed)
	{
		return FMath::Clamp(Composed, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("base %.4f x gear %.4f x stone %.4f x transient %.4f = %.4f"), StatBase, GearFactor,
							   StoneFactor, TransientFactor, Compose());
	}
};
