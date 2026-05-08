// ActionStatModifiers.h
// Per-action stat buff accumulator. Sources stack additively.
// Reality + Evolution contributions land here. Lifecycle = one action.

#pragma once

#include "CoreMinimal.h"
#include "ActionStatModifiers.generated.h"

/** All 9 sub-stats. Used for indexed reads from FActionStatModifiers. */
UENUM(BlueprintType)
enum class ESubStat : uint8
{
	// Mind
	Efficiency UMETA(DisplayName = "Efficiency"),
	EffectDamage UMETA(DisplayName = "Effect Damage"),
	CritChance UMETA(DisplayName = "Crit Chance"),
	SpellSpeed UMETA(DisplayName = "Spell Speed"),
	// Body
	Defense UMETA(DisplayName = "Defense"),
	ActionSpeed UMETA(DisplayName = "Action Speed"),
	RawDamage UMETA(DisplayName = "Raw Damage"),
	// Spirit
	Resistance UMETA(DisplayName = "Resistance"),
	TurnSpeed UMETA(DisplayName = "Turn Speed"),
	Luck UMETA(DisplayName = "Luck")
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
	float EffectDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float CritChance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float SpellSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float Defense = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float ActionSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Action Stat Modifiers")
	float RawDamage = 0.0f;

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
		EffectDamage += Other.EffectDamage;
		CritChance += Other.CritChance;
		SpellSpeed += Other.SpellSpeed;
		Defense += Other.Defense;
		ActionSpeed += Other.ActionSpeed;
		RawDamage += Other.RawDamage;
		Resistance += Other.Resistance;
		TurnSpeed += Other.TurnSpeed;
		Luck += Other.Luck;
	}

	/** Add a flat percentage to all sub-stats. Used by Reality contributions. */
	void AddFlatPercent(float Percent)
	{
		Efficiency += Percent;
		EffectDamage += Percent;
		CritChance += Percent;
		SpellSpeed += Percent;
		Defense += Percent;
		ActionSpeed += Percent;
		RawDamage += Percent;
		Resistance += Percent;
		TurnSpeed += Percent;
		Luck += Percent;
	}

	/** Add a flat percentage to all sub-stats in a specific pillar.
	 *  Used by Evolution Pillar-mode crystals. Mind=4, Body=3, Spirit=3
	 *  represented here. Pool stats (MaxHealth in Body, MaxEnergy in Spirit)
	 *  are not action-time modifiers and are intentionally not represented. */
	void AddPillarPercent(float MindPct, float BodyPct, float SpiritPct)
	{
		// Mind sub-stats
		Efficiency += MindPct;
		EffectDamage += MindPct;
		CritChance += MindPct;
		SpellSpeed += MindPct;
		// Body sub-stats — MaxHealth is a pool stat, not represented here.
		Defense += BodyPct;
		ActionSpeed += BodyPct;
		RawDamage += BodyPct;
		// Spirit sub-stats — MaxEnergy is a pool stat, not represented here.
		Resistance += SpiritPct;
		TurnSpeed += SpiritPct;
		Luck += SpiritPct;
	}

	/** Read the modifier for a specific sub-stat. */
	float GetModifier(ESubStat Stat) const
	{
		switch (Stat)
		{
		case ESubStat::Efficiency:
			return Efficiency;
		case ESubStat::EffectDamage:
			return EffectDamage;
		case ESubStat::CritChance:
			return CritChance;
		case ESubStat::SpellSpeed:
			return SpellSpeed;
		case ESubStat::Defense:
			return Defense;
		case ESubStat::ActionSpeed:
			return ActionSpeed;
		case ESubStat::RawDamage:
			return RawDamage;
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
		return Efficiency != 0.0f || EffectDamage != 0.0f || CritChance != 0.0f || SpellSpeed != 0.0f || Defense != 0.0f || ActionSpeed != 0.0f || RawDamage != 0.0f || Resistance != 0.0f || TurnSpeed != 0.0f || Luck != 0.0f;
	}
};
