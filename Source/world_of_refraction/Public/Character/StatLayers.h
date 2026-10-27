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
		case ESubStat::Efficiency:
			// The cost REDUCTION (the multiplier is 1 - this), up to its gear ceiling.
			Layers.Max = CombatConstants::EFFICIENCY_GEAR_CEILING;
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

	/** A pillar's layers (B-042t): the pillar modifier is capped alone, so the compose clamp is that cap
	 *  times the world base, and the base itself is never clamped. */
	static FWoRStatLayers ForPillar(float WorldBase)
	{
		FWoRStatLayers Layers;
		Layers.StatBase = WorldBase;
		Layers.Min = FMath::Min(WorldBase * CombatConstants::STAT_MODIFIER_MIN, WorldBase * CombatConstants::STAT_MODIFIER_MAX);
		Layers.Max = FMath::Max(WorldBase * CombatConstants::STAT_MODIFIER_MIN, WorldBase * CombatConstants::STAT_MODIFIER_MAX);
		return Layers;
	}

	/** Turn speed's layers (B-042v): the cached base (the stat, its gear and ceiling, already rounded) x the
	 *  attached stone x the net buff, never below 1; the turn manager rounds the product. */
	static FWoRStatLayers ForTurnSpeed(float CachedBase)
	{
		FWoRStatLayers Layers;
		Layers.StatBase = CachedBase;
		Layers.Min = CombatConstants::TURN_SPEED_FLOOR;
		Layers.Max = TNumericLimits<float>::Max();
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
		const FString Bounds = Max == TNumericLimits<float>::Max() ? FString(TEXT("unbounded"))
																	: FString::Printf(TEXT("[%.2f, %.2f]"), Min, Max);
		return FString::Printf(TEXT("base %.4f x gear %.4f x stone %.4f x transient %.4f = %.4f %s"), StatBase, GearFactor,
							   StoneFactor, TransientFactor, Compose(), *Bounds);
	}
};

/** B-042u (StatMathsRedesign.md §2.1, the pools' exception): a max pool is (the stat portion, capped alone,
 *  + the gear's flat amount) x one summed multiply of the stone and buff percents, rounded once. */
struct FWoRPoolLayers
{
	/** The pool base + pillar points, capped alone at the pool cap. */
	float StatPortion = 0.0f;
	/** The gear's flat amount, outside the cap. */
	float GearAmount = 0.0f;
	/** 1 + (the attached pool stone's percent + the net pool buff percent) / 100. */
	float PercentFactor = 1.0f;

	int32 Compose() const
	{
		return FMath::RoundToInt((StatPortion + GearAmount) * PercentFactor);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(stat %.4f + gear %.4f) x percent %.4f = %d"), StatPortion, GearAmount, PercentFactor, Compose());
	}
};
