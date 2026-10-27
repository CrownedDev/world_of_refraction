// StatLayers.h
// B-042p (StatMathsRedesign.md §2.1, Pattern P): the four layers of a composed sub-stat, so the
// getter and the GAS source effect build the same number. The stat-derived base is capped alone,
// then gear, stone and buffs multiply it, bounded by the stat's compose clamp (B-042q).
// Debug: FWoRStatLayers::ToString.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatConstants.h"
#include "Combat/Actions/ActionStatModifiers.h"

struct FWoRStatLayers
{
	/** The stat-derived part, capped alone (STAT_MULT_CAP, or the stat's own cap). */
	float StatBase = 1.0f;
	/** 1 + the gear points as a fraction. */
	float GearFactor = 1.0f;
	/** 1 + the attached stone's percent. */
	float StoneFactor = 1.0f;
	/** 1 + the net buff / debuff percent. */
	float TransientFactor = 1.0f;
	/** The compose clamp. */
	float Min = CombatConstants::STAT_MODIFIER_MIN;
	float Max = CombatConstants::STAT_MODIFIER_MAX;

	/** Default layers with the stat's compose clamp: Defense is a reduction up to its ceiling, crit
	 *  damage never drops below a normal hit, Luck is normalized up to its gear ceiling, and the Reflex
	 *  window factor is unbounded (its consumer bounds it). */
	static FWoRStatLayers ForStat(ESubStat Stat)
	{
		FWoRStatLayers Layers;
		switch (Stat)
		{
		case ESubStat::Defense:
			Layers.Max = CombatConstants::DEFENSE_REDUCTION_CEILING;
			break;
		case ESubStat::CritDamage:
			Layers.Min = CombatConstants::CRIT_DMG_BASE;
			Layers.Max = CombatConstants::CRIT_DAMAGE_GEAR_CEILING;
			break;
		case ESubStat::Luck:
			Layers.Max = CombatConstants::LUCK_GEAR_CEILING;
			break;
		case ESubStat::Reflex:
			Layers.Min = TNumericLimits<float>::Lowest();
			Layers.Max = TNumericLimits<float>::Max();
			break;
		default:
			break;
		}
		return Layers;
	}

	/** StatBase x gear x stone x transient, clamped to [Min, Max]. */
	float Compose() const
	{
		return Clamp(StatBase * GearFactor * StoneFactor * TransientFactor);
	}

	/** The compose clamp, shared with the attribute set's clamp on these stats. */
	float Clamp(float Composed) const
	{
		return FMath::Clamp(Composed, Min, Max);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("base %.4f x gear %.4f x stone %.4f x transient %.4f = %.4f [%.2f, %.2f]"), StatBase,
							   GearFactor, StoneFactor, TransientFactor, Compose(), Min, Max);
	}
};
