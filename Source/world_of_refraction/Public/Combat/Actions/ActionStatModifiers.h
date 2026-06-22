// ActionStatModifiers.h
// Per-action stat buff accumulator. Sources stack additively.
// Reality + Evolution contributions land here. Lifecycle = one action.

#pragma once

#include "CoreMinimal.h"
#include "ActionStatModifiers.generated.h"

/** Action-time sub-stats (11) plus a None sentinel ("targets no sub-stat"). Pool stats
 *  (MaxHealth, MaxEnergy) are intentionally not represented. TurnSpeed IS a represented
 *  ESubStat (the attached-stone reads key off it elsewhere) but is DELIBERATELY never
 *  written into FActionStatModifiers by any infusion path (mapping, Reality flat,
 *  pillar-mode) — turn speed comes only from the slot-level paths (innate evolution
 *  stats / pillar-modified Spirit), never from infusion. */
UENUM(BlueprintType)
enum class ESubStat : uint8
{
	// Mind
	Efficiency UMETA(DisplayName = "Efficiency"),
	SpellDamage UMETA(DisplayName = "Spell Damage"),
	StatusMultiplier UMETA(DisplayName = "Status Multiplier"),
	CritDamage UMETA(DisplayName = "Crit Damage"),
	SpellSpeed UMETA(DisplayName = "Spell Speed"),
	// Body
	Defense UMETA(DisplayName = "Defense"),
	ActionSpeed UMETA(DisplayName = "Action Speed"),
	RawDamage UMETA(DisplayName = "Raw Damage"),
	// Spirit
	Resistance UMETA(DisplayName = "Resistance"),
	TurnSpeed UMETA(DisplayName = "Turn Speed"),
	Luck UMETA(DisplayName = "Luck"),
	// Appended (serialization-safe) — "targets no sub-stat" sentinel, e.g.
	// StoneTargetStat for a non-stat-stone. GetModifier's default returns 0 for it.
	None UMETA(DisplayName = "None"),
	// Appended AFTER None (serialization-safe — append-only, never reordered; None is a switch
	// sentinel / default-value, NOT a count or loop-bound, so appending past it is safe).
	// Body-pillar Reflex: the gearable/buffable defense-window stat (Cluster B). The live window
	// read uses the ReflexBuff/ReflexDebuff skill-effect pair (B-4), NOT this struct's field.
	Reflex UMETA(DisplayName = "Reflex")
};

/** Per-action stat modifier accumulator. Values are percentages (5.0f = +5%).
 *  Sources stack additively. ApplyTo composes (1 + percent/100) onto a base stat. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionStatModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Efficiency = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float SpellDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float StatusMultiplier = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float CritDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float SpellSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Defense = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float ActionSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float RawDamage = 0.0f;

	// Body substat. INERT for the defense window: the window reads the ReflexBuff/ReflexDebuff
	// skill-effect pair (Cluster B-4), not this attacker-scoped field. Present only for ESubStat
	// symmetry (every sub-stat needs a GetForSubStat/GetModifier case). Wire a consumer that reads
	// ActionMods.Reflex before treating this as live.
	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Reflex = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Resistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float TurnSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Luck = 0.0f;

	/** Add another modifier set into this one (additive accumulation). */
	void Accumulate(const FActionStatModifiers &Other)
	{
		Efficiency += Other.Efficiency;
		SpellDamage += Other.SpellDamage;
		StatusMultiplier += Other.StatusMultiplier;
		CritDamage += Other.CritDamage;
		SpellSpeed += Other.SpellSpeed;
		Defense += Other.Defense;
		ActionSpeed += Other.ActionSpeed;
		RawDamage += Other.RawDamage;
		Reflex += Other.Reflex;
		Resistance += Other.Resistance;
		TurnSpeed += Other.TurnSpeed;
		Luck += Other.Luck;
	}

	/** Add a flat percentage to all sub-stats. Used by Reality contributions. */
	void AddFlatPercent(float Percent)
	{
		Efficiency += Percent;
		SpellDamage += Percent;
		StatusMultiplier += Percent;
		CritDamage += Percent;
		SpellSpeed += Percent;
		Defense += Percent;
		ActionSpeed += Percent;
		RawDamage += Percent;
		Reflex += Percent;
		Resistance += Percent;
		// TurnSpeed excluded — infused/Reality contributions must not alter turn order.
		Luck += Percent;
	}

	/** Add a flat percentage to all sub-stats in a specific pillar.
	 *  Used by Evolution Pillar-mode crystals. Mind=4, Body=4, Spirit=3
	 *  represented here. Pool stats (MaxHealth in Body, MaxEnergy in Spirit)
	 *  are not action-time modifiers and are intentionally not represented. */
	void AddPillarPercent(float MindPct, float BodyPct, float SpiritPct)
	{
		// Mind sub-stats
		Efficiency += MindPct;
		SpellDamage += MindPct;
		CritDamage += MindPct;
		SpellSpeed += MindPct;
		// Body sub-stats — MaxHealth is a pool stat, not represented here.
		Defense += BodyPct;
		ActionSpeed += BodyPct;
		RawDamage += BodyPct;
		Reflex += BodyPct;
		// Spirit sub-stats — MaxEnergy is a pool stat, not represented here.
		Resistance += SpiritPct;
		StatusMultiplier += SpiritPct;
		// TurnSpeed excluded — evolution pillar-mode infusion must not alter turn order.
		Luck += SpiritPct;
	}

	/** Read the modifier for a specific sub-stat. */
	float GetModifier(ESubStat Stat) const
	{
		switch (Stat)
		{
		case ESubStat::Efficiency:
			return Efficiency;
		case ESubStat::SpellDamage:
			return SpellDamage;
		case ESubStat::StatusMultiplier:
			return StatusMultiplier;
		case ESubStat::CritDamage:
			return CritDamage;
		case ESubStat::SpellSpeed:
			return SpellSpeed;
		case ESubStat::Defense:
			return Defense;
		case ESubStat::ActionSpeed:
			return ActionSpeed;
		case ESubStat::RawDamage:
			return RawDamage;
		case ESubStat::Reflex:
			return Reflex;
		case ESubStat::Resistance:
			return Resistance;
		case ESubStat::TurnSpeed:
			return TurnSpeed;
		case ESubStat::Luck:
			return Luck;
		default:
			return 0.0f;
		}
	}

	/** Compose modifier onto a base stat: result = Stat * (1 + Modifier/100). */
	float ApplyTo(float StatValue, ESubStat Stat) const
	{
		const float Mod = GetModifier(Stat);
		return (Mod == 0.0f) ? StatValue : StatValue * (1.0f + Mod / 100.0f);
	}

	/** Integer overload — rounds after composition. */
	int32 ApplyTo(int32 StatValue, ESubStat Stat) const
	{
		const float Mod = GetModifier(Stat);
		if (Mod == 0.0f)
			return StatValue;
		return FMath::RoundToInt(StatValue * (1.0f + Mod / 100.0f));
	}

	/** Has any non-zero contribution. */
	bool IsActive() const
	{
		return Efficiency != 0.0f || SpellDamage != 0.0f || StatusMultiplier != 0.0f || CritDamage != 0.0f || SpellSpeed != 0.0f || Defense != 0.0f || ActionSpeed != 0.0f || RawDamage != 0.0f || Reflex != 0.0f || Resistance != 0.0f || TurnSpeed != 0.0f || Luck != 0.0f;
	}
};
