// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EStatusEffectTiming.h"
#include "EPassiveTrigger.h"
#include "EStatusType.h"
#include "ESpellElement.h"
#include "StatusEffect.generated.h"

/**
 * FStatusEffect
 * Runtime status effect instance applied to actors during combat
 *
 * Supports:
 * - Multiple timing modes (immediate, start/end of turn, conditional, persistent)
 * - Stacking with configurable limits
 * - Source tracking for effect ownership
 * - Conditional triggers via EPassiveTrigger
 * - Duration tracking per affected actor's turn (not global)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FStatusEffect
{
	GENERATED_BODY()

	// ========================================
	// IDENTITY
	// ========================================

	/** Display name for UI and logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString EffectName = TEXT("Unnamed Effect");

	/** Unique identifier for stacking/removal purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	int32 EffectID = 0;

	/** Optional description for UI tooltips */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString Description = TEXT("");

	// ========================================
	// TIMING
	// ========================================

	/** When this effect processes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EStatusEffectTiming ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;

	/** If ProcessTiming = OnTrigger, what condition activates it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
			  meta = (EditCondition = "ProcessTiming == EStatusEffectTiming::OnTrigger"))
	EPassiveTrigger TriggerCondition = EPassiveTrigger::None;

	/** Threshold for HP/Energy percentage triggers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
			  meta = (ClampMin = "0", ClampMax = "100",
					  EditCondition = "ProcessTiming == EStatusEffectTiming::OnTrigger"))
	float TriggerThreshold = 30.0f;

	/** Whether this trigger condition is currently active (for conditional effects) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bTriggerActive = false;

	// ========================================
	// DURATION
	// ========================================

	/** Turns remaining (ticks on affected actor's turn, not global) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	int32 RemainingTurns = 3;

	/** Initial duration (for UI display of "X/Y turns remaining") */
	UPROPERTY(BlueprintReadOnly, Category = "Duration")
	int32 InitialDuration = 3;

	/** Effect never expires (equipment bonuses, auras, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration")
	bool bPermanent = false;

	// ========================================
	// EFFECT DATA
	// ========================================

	/** Type of effect (buff, debuff, DOT, etc.) - uses unified enum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	EStatusType EffectType = EStatusType::None;

	/** Effect magnitude (damage amount, buff percentage, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float EffectValue = 0.0f;

	/** Element for elemental DOTs and resistance calculations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	ESpellElement Element = ESpellElement::Generic;

	// ========================================
	// STACKING
	// ========================================

	/** Can multiple instances of this effect stack on same target? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
	bool bCanStack = false;

	/** Current stack count */
	UPROPERTY(BlueprintReadOnly, Category = "Stacking")
	int32 CurrentStacks = 1;

	/** Maximum stacks allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking",
			  meta = (EditCondition = "bCanStack", ClampMin = "1", ClampMax = "99"))
	int32 MaxStacks = 3;

	/** If true, reapplying refreshes duration instead of adding stacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
	bool bRefreshDurationOnReapply = true;

	// ========================================
	// SOURCE TRACKING
	// ========================================

	/** Actor who applied this effect (for damage attribution, cleanse targeting) */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	TWeakObjectPtr<AActor> SourceActor;

	/** Name of spell/ability that applied this effect */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	FString SourceAbilityName = TEXT("");

	/** Team index of source (for friendly fire checks, cleanse logic) */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	int32 SourceTeamIndex = -1;

	// ========================================
	// RUNTIME FLAGS
	// ========================================

	/** Has this effect been processed this turn? (prevents double-processing) */
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	bool bProcessedThisTurn = false;

	/** Is this effect marked for removal at end of processing? */
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	bool bPendingRemoval = false;

	// ========================================
	// CONSTRUCTORS
	// ========================================

	FStatusEffect()
	{
		InitialDuration = RemainingTurns;
	}

	// ========================================
	// FACTORY METHODS - FROM SPELL/ABILITY DATA
	// ========================================

	/**
	 * Create effects from SpellData (handles Primary + Secondary)
	 * Call this when a spell hits a target
	 *
	 * @param SpellName Display name for the effect
	 * @param EffectIDBase Base ID (primary gets +0, secondary gets +1)
	 * @param EffectType The EStatusType from SpellData
	 * @param Magnitude Percentage modifier (0.0-1.0 range from SpellData)
	 * @param Value Flat value from SpellData
	 * @param Duration Turns from SpellData
	 * @param InElement Element for DOT effects
	 * @param Timing When to process (usually StartOfOwnTurn for buffs, EndOfOwnTurn for DOTs)
	 */
	static FStatusEffect CreateFromSpellEffect(
		const FString &SpellName,
		int32 EffectIDBase,
		EStatusType EffectType,
		float Magnitude,
		int32 Value,
		int32 Duration,
		ESpellElement InElement,
		EStatusEffectTiming Timing = EStatusEffectTiming::StartOfOwnTurn)
	{
		FStatusEffect Effect;
		Effect.EffectName = SpellName;
		Effect.EffectID = EffectIDBase;
		Effect.EffectType = EffectType;
		Effect.EffectValue = (Value != 0) ? static_cast<float>(Value) : (Magnitude * 100.0f);
		Effect.RemainingTurns = (Duration > 0) ? Duration : 1;
		Effect.InitialDuration = Effect.RemainingTurns;
		Effect.Element = InElement;
		Effect.ProcessTiming = Timing;

		// Auto-detect timing based on effect type
		if (Effect.IsDOT())
		{
			Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
		}
		else if (Effect.EffectType == EStatusType::HealthRestore ||
				 Effect.EffectType == EStatusType::EnergyRestore)
		{
			Effect.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;
		}
		else if (Effect.EffectType == EStatusType::EnergyDrain)
		{
			Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
		}
		return Effect;
	}

	/**
	 * Create effects from Evolution passive (TArray<FPassiveEffect>)
	 * For permanent stat modifiers from evolutions
	 *
	 * @param EvolutionName Name of the evolution
	 * @param EvolutionID Unique evolution ID
	 * @param PassiveType The effect type from FPassiveEffect
	 * @param Value The effect value
	 * @param PassiveIndex Index in the PassiveEffects array (for unique IDs)
	 */
	static FStatusEffect CreateFromEvolutionPassive(
		const FString &EvolutionName,
		int32 EvolutionID,
		EStatusType PassiveType,
		float Value,
		int32 PassiveIndex)
	{
		FStatusEffect Effect;
		Effect.EffectName = EvolutionName + TEXT(" Passive");
		Effect.EffectID = EvolutionID * 100 + PassiveIndex;
		Effect.EffectType = PassiveType;
		Effect.EffectValue = Value;
		Effect.bPermanent = true;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		return Effect;
	}

	/**
	 * Create DOT effect from ability infusion
	 * When ability is infused, it gains elemental DOT
	 */
	static FStatusEffect CreateFromInfusion(
		const FString &AbilityName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float DOTDamage,
		int32 Duration)
	{
		FStatusEffect Effect = CreateDOT(
			AbilityName + TEXT(" Infusion"),
			AbilityID * 10 + 5, // +5 offset for infusion effects
			DOTDamage,
			Duration,
			InfusedElement);
		return Effect;
	}

	// ========================================
	// FACTORY METHODS - WEAPON SYSTEM
	// ========================================

	/**
	 * Create permanent stat bonuses from equipped weapon
	 * Call when weapon is equipped, remove when unequipped
	 *
	 * @param WeaponName Name of weapon for display
	 * @param WeaponID Unique weapon identifier
	 * @param BonusRawDamage Raw damage bonus from weapon
	 * @param BonusDefense Defense bonus from weapon
	 * @param BonusSpellDamage Spell damage bonus
	 * @param BonusActionSpeed Action speed bonus
	 * @param BonusCritChance Crit chance bonus (percentage)
	 */
	static TArray<FStatusEffect> CreateFromWeaponBonuses(
		const FString &WeaponName,
		int32 WeaponID,
		int32 BonusRawDamage,
		int32 BonusDefense,
		int32 BonusSpellDamage,
		int32 BonusActionSpeed,
		float BonusCritChance)
	{
		TArray<FStatusEffect> Effects;
		int32 BaseID = WeaponID * 100; // Offset for weapon bonuses

		if (BonusRawDamage != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				WeaponName + TEXT(" (Raw Damage)"),
				BaseID + 1,
				BonusRawDamage > 0 ? EStatusType::RawDamageBuff : EStatusType::RawDamageDebuff,
				static_cast<float>(FMath::Abs(BonusRawDamage)));
			Effects.Add(Bonus);
		}

		if (BonusDefense != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				WeaponName + TEXT(" (Defense)"),
				BaseID + 2,
				BonusDefense > 0 ? EStatusType::DefenseBuff : EStatusType::DefenseDebuff,
				static_cast<float>(FMath::Abs(BonusDefense)));
			Effects.Add(Bonus);
		}

		if (BonusSpellDamage != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				WeaponName + TEXT(" (Spell Damage)"),
				BaseID + 3,
				BonusSpellDamage > 0 ? EStatusType::EffectDamageBuff : EStatusType::EffectDamageDebuff,
				static_cast<float>(FMath::Abs(BonusSpellDamage)));
			Effects.Add(Bonus);
		}

		if (BonusActionSpeed != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				WeaponName + TEXT(" (Action Speed)"),
				BaseID + 4,
				BonusActionSpeed > 0 ? EStatusType::SpeedBuff : EStatusType::SpeedDebuff,
				static_cast<float>(FMath::Abs(BonusActionSpeed)));
			Effects.Add(Bonus);
		}

		if (BonusCritChance != 0.0f)
		{
			FStatusEffect Bonus = CreatePersistent(
				WeaponName + TEXT(" (Crit Chance)"),
				BaseID + 5,
				BonusCritChance > 0 ? EStatusType::CritChanceBuff : EStatusType::CritChanceDebuff,
				FMath::Abs(BonusCritChance));
			Effects.Add(Bonus);
		}

		return Effects;
	}

	// ========================================
	// FACTORY METHODS - RING SYSTEM
	// ========================================

	/**
	 * Create permanent stat bonuses from equipped ring
	 * Mirrors CreateFromWeaponBonuses; namespaced ID range (+50 offset within
	 * the RingID*100 block) to avoid collision with weapon-bonus IDs.
	 *
	 * @param RingName Name of ring for display
	 * @param RingID Unique ring identifier
	 * @param BonusRawDamage Raw damage bonus from ring
	 * @param BonusDefense Defense bonus from ring
	 * @param BonusSpellDamage Spell damage bonus
	 * @param BonusActionSpeed Action speed bonus
	 * @param BonusCritChance Crit chance bonus (percentage)
	 */
	static TArray<FStatusEffect> CreateFromRingBonuses(
		const FString &RingName,
		int32 RingID,
		int32 BonusRawDamage,
		int32 BonusDefense,
		int32 BonusSpellDamage,
		int32 BonusActionSpeed,
		float BonusCritChance)
	{
		TArray<FStatusEffect> Effects;
		int32 BaseID = RingID * 100 + 50; // +50 offset isolates ring-bonus IDs from weapon-bonus IDs (+1..+6) in the same RingID*100 block

		if (BonusRawDamage != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				RingName + TEXT(" (Raw Damage)"),
				BaseID + 1,
				BonusRawDamage > 0 ? EStatusType::RawDamageBuff : EStatusType::RawDamageDebuff,
				static_cast<float>(FMath::Abs(BonusRawDamage)));
			Effects.Add(Bonus);
		}

		if (BonusDefense != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				RingName + TEXT(" (Defense)"),
				BaseID + 2,
				BonusDefense > 0 ? EStatusType::DefenseBuff : EStatusType::DefenseDebuff,
				static_cast<float>(FMath::Abs(BonusDefense)));
			Effects.Add(Bonus);
		}

		if (BonusSpellDamage != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				RingName + TEXT(" (Spell Damage)"),
				BaseID + 3,
				BonusSpellDamage > 0 ? EStatusType::EffectDamageBuff : EStatusType::EffectDamageDebuff,
				static_cast<float>(FMath::Abs(BonusSpellDamage)));
			Effects.Add(Bonus);
		}

		if (BonusActionSpeed != 0)
		{
			FStatusEffect Bonus = CreatePersistent(
				RingName + TEXT(" (Action Speed)"),
				BaseID + 4,
				BonusActionSpeed > 0 ? EStatusType::SpeedBuff : EStatusType::SpeedDebuff,
				static_cast<float>(FMath::Abs(BonusActionSpeed)));
			Effects.Add(Bonus);
		}

		if (BonusCritChance != 0.0f)
		{
			FStatusEffect Bonus = CreatePersistent(
				RingName + TEXT(" (Crit Chance)"),
				BaseID + 5,
				BonusCritChance > 0 ? EStatusType::CritChanceBuff : EStatusType::CritChanceDebuff,
				FMath::Abs(BonusCritChance));
			Effects.Add(Bonus);
		}

		return Effects;
	}

	/**
	 * Create status effect from physical damage type (Generic character weapon attacks)
	 * Slash → Bleed DOT, Pierce → Armor Break, Impact → Stun
	 *
	 * @param WeaponName Name of weapon
	 * @param WeaponID Unique weapon identifier
	 * @param PhysicalType The physical damage type (0=Slash, 1=Pierce, 2=Impact)
	 * @param StatusBuildup Base buildup value from WeaponAttackData
	 * @param InfusionMultiplier Weapon's InfusionStatusMultiplier (1.0 = normal)
	 * @param HitCount Number of hits (multiplies buildup)
	 */
	static FStatusEffect CreateFromPhysicalDamageType(
		const FString &WeaponName,
		int32 WeaponID,
		uint8 PhysicalType,
		int32 StatusBuildup,
		float InfusionMultiplier,
		int32 HitCount)
	{
		FStatusEffect Effect;
		Effect.EffectID = WeaponID * 10 + 7; // +7 offset for physical status

		// Calculate actual buildup: base × multiplier × hits
		float TotalBuildup = static_cast<float>(StatusBuildup) * InfusionMultiplier * static_cast<float>(HitCount);

		// Physical damage types: 0=Slash, 1=Pierce, 2=Impact
		switch (PhysicalType)
		{
		case 0: // Slash → Bleed DOT
			Effect.EffectName = WeaponName + TEXT(" Bleed");
			Effect.EffectType = EStatusType::DOT;
			Effect.Element = ESpellElement::Generic;  // Physical damage
			Effect.EffectValue = TotalBuildup * 0.5f; // DOT damage = half of buildup
			Effect.RemainingTurns = 3;
			Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
			Effect.bCanStack = true;
			break;

		case 1: // Pierce → Armor Break (defense debuff)
			Effect.EffectName = WeaponName + TEXT(" Armor Break");
			Effect.EffectType = EStatusType::DefenseDebuff;
			Effect.EffectValue = TotalBuildup * 0.3f; // Defense reduction
			Effect.RemainingTurns = 2;
			Effect.ProcessTiming = EStatusEffectTiming::Persistent;
			Effect.bCanStack = true;
			Effect.MaxStacks = 3;
			break;

		case 2: // Impact → Stun (skip turn - uses special handling)
			Effect.EffectName = WeaponName + TEXT(" Stun");
			Effect.EffectType = EStatusType::None; // TODO: Add Stun effect type
			Effect.EffectValue = 1.0f;			   // Stun duration multiplier
			Effect.RemainingTurns = 1;
			Effect.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;
			Effect.bCanStack = false; // Stun doesn't stack, refreshes
			Effect.bRefreshDurationOnReapply = true;
			break;

		default:
			Effect.EffectName = TEXT("Unknown Physical Effect");
			Effect.EffectType = EStatusType::None;
			break;
		}

		Effect.InitialDuration = Effect.RemainingTurns;
		return Effect;
	}

	/**
	 * Create weapon infusion DOT with weapon's multiplier applied
	 * For Generic characters using infused abilities through weapons
	 */
	static FStatusEffect CreateFromWeaponInfusion(
		const FString &AbilityName,
		const FString &WeaponName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float BaseDOTDamage,
		int32 Duration,
		float WeaponInfusionMultiplier)
	{
		float AdjustedDamage = BaseDOTDamage * WeaponInfusionMultiplier;
		FStatusEffect Effect = CreateDOT(
			AbilityName + TEXT(" (") + WeaponName + TEXT(" Infusion)"),
			AbilityID * 10 + 8, // +8 offset for weapon-infused abilities
			AdjustedDamage,
			Duration,
			InfusedElement);
		return Effect;
	}

	/** Quick constructor for common buff/debuff creation */
	static FStatusEffect CreateBuff(const FString &Name, int32 ID, EStatusType Type, float Value, int32 Duration)
	{
		FStatusEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = Type;
		Effect.EffectValue = Value;
		Effect.RemainingTurns = Duration;
		Effect.InitialDuration = Duration;
		Effect.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;
		return Effect;
	}

	/** Quick constructor for DOT effects */
	static FStatusEffect CreateDOT(const FString &Name, int32 ID, float DamagePerTurn, int32 Duration, ESpellElement InElement)
	{
		FStatusEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = EStatusType::DOT; // Generic DOT type
		Effect.EffectValue = DamagePerTurn;
		Effect.RemainingTurns = Duration;
		Effect.InitialDuration = Duration;
		Effect.Element = InElement;
		Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;

		// No element-specific switch needed - display name handled by StatusDisplayNames

		return Effect;
	}

	/** Quick constructor for persistent stat modifiers */
	static FStatusEffect CreatePersistent(const FString &Name, int32 ID, EStatusType Type, float Value)
	{
		FStatusEffect Effect;
		Effect.EffectName = Name;
		Effect.EffectID = ID;
		Effect.EffectType = Type;
		Effect.EffectValue = Value;
		Effect.bPermanent = true;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		return Effect;
	}

	// ========================================
	// HELPER FUNCTIONS
	// ========================================

	/** Check if this is a buff (positive effect) */
	bool IsBuff() const
	{
		switch (EffectType)
		{
		case EStatusType::MindBuff:
		case EStatusType::BodyBuff:
		case EStatusType::SpiritBuff:
		case EStatusType::DamageBuff:
		case EStatusType::DefenseBuff:
		case EStatusType::SpeedBuff:
		case EStatusType::CritChanceBuff:
		case EStatusType::AttackSpeedBuff:
		case EStatusType::RawDamageBuff:
		case EStatusType::EffectDamageBuff:
		case EStatusType::SpellCostBuff:
		case EStatusType::ResistanceBuff:
		case EStatusType::SpellSizeBuff:
		case EStatusType::MaxEnergyBuff:
		case EStatusType::HealthRestore:
		case EStatusType::EnergyRestore:
			return true;
		default:
			return false;
		}
	}

	/** Check if this is a debuff (negative effect) */
	bool IsDebuff() const
	{
		switch (EffectType)
		{
		case EStatusType::MindDebuff:
		case EStatusType::BodyDebuff:
		case EStatusType::SpiritDebuff:
		case EStatusType::DamageDebuff:
		case EStatusType::DefenseDebuff:
		case EStatusType::SpeedDebuff:
		case EStatusType::CritDebuff:
		case EStatusType::EnergyDebuff:
		case EStatusType::AttackSpeedDebuff:
		case EStatusType::RawDamageDebuff:
		case EStatusType::EffectDamageDebuff:
		case EStatusType::SpellCostDebuff:
		case EStatusType::ResistanceDebuff:
		case EStatusType::SpellSizeDebuff:
		case EStatusType::MaxEnergyDebuff:
		case EStatusType::DOT:
		case EStatusType::SkipTurn:
		case EStatusType::RandomDebuff:
		case EStatusType::Stun:
		case EStatusType::HealBlock:
		case EStatusType::Silenced:
		case EStatusType::RandomSkill:
			return true;
		default:
			return false;
		}
	}

	/** Check if this is DOT damage */
	bool IsDOT() const
	{
		return EffectType == EStatusType::DOT;
	}

	/** Check if this effect deals damage */
	bool DealsDamage() const
	{
		return IsDOT() ||
			   EffectType == EStatusType::RetaliationDamage ||
			   EffectType == EStatusType::SelfDamage ||
			   EffectType == EStatusType::BurstDamage;
	}

	/** Get effective value considering stacks */
	float GetStackedValue() const
	{
		return EffectValue * static_cast<float>(CurrentStacks);
	}

	/** Check if can add another stack */
	bool CanAddStack() const
	{
		return bCanStack && CurrentStacks < MaxStacks;
	}

	/** Get duration display string (e.g., "2/3 turns") */
	FString GetDurationString() const
	{
		if (bPermanent)
		{
			return TEXT("Permanent");
		}
		return FString::Printf(TEXT("%d/%d turns"), RemainingTurns, InitialDuration);
	}

	/** Get stack display string (e.g., "x3") */
	FString GetStackString() const
	{
		if (!bCanStack || CurrentStacks <= 1)
		{
			return TEXT("");
		}
		return FString::Printf(TEXT("x%d"), CurrentStacks);
	}

	/** Equality check by EffectID (for finding existing effects) */
	bool operator==(const FStatusEffect &Other) const
	{
		return EffectID == Other.EffectID;
	}

	/** Reset turn-based processing flags */
	void ResetTurnFlags()
	{
		bProcessedThisTurn = false;
	}
};

/** Hash function for TMap usage */
FORCEINLINE uint32 GetTypeHash(const FStatusEffect &Effect)
{
	return GetTypeHash(Effect.EffectID);
}