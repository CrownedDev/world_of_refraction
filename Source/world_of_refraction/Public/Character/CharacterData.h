// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Character/ECharacterClass.h"
#include "Character/StatConstants.h"
#include <Combat/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterData.generated.h"

class USpellData;
class UEvolutionItemData;
class UInventoryData;
class UCosmeticsData;

/**
 * Describes what a character loses/gains from evolution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionCostResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	bool bCanEvolve = true;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	FString CostDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	FString GainDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	TArray<FString> Warnings;
};

/**
 * Character Data Asset - Contains all character stats, abilities, and visual data
 * Stats split between Initial (DNA) and World (progression) for clear tracking
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString Name = TEXT("Unnamed Character");

	/** Character class - determines combat style and equipment options */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ECharacterClass CharacterClass = ECharacterClass::Caster;

	/** Innate element - only for Casters (locked at creation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Caster", EditConditionHides))
	ESpellElement InnateElement = ESpellElement::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Character description...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	UTexture2D *Portrait = nullptr;

	// ==================== CONTROL ====================

	/** If true, this character is controlled by AI in combat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control")
	bool bIsAIControlled = false;

	// ==================== INVENTORY ====================

	/** The character's inventory asset. References the items they own and the
	 *  saved loadout configurations they can switch between. Sole source of
	 *  truth for runtime loadout population. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	UInventoryData *Inventory = nullptr;

	// ==================== WORLD STAT LEVELS ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldMindLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldBodyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldSpiritLevel = 0;

	// ==================== INITIAL SUB-STATS (CHARACTER DNA) ====================
	// ==================== MIND SUB-STATS (4) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 Efficiency = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 SpellDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 CritChance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 SpellSpeed = 0;

	// ==================== BODY SUB-STATS (4) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 Defense = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 ActionSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 RawDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 MaxHealth = 0;

	// ==================== SPIRIT SUB-STATS (5) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 MaxEnergy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Resistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 TurnSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Luck = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 StatusMultiplier = 0;

	// ==================== COSMETICS ====================

	/** Visual/animation data (defense montages, stances, item-use montages,
	 *  infusion VFX, weather variant). Sole source of truth — read sites query
	 *  this asset directly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UCosmeticsData *Cosmetics = nullptr;

	// ==================== CLASS HELPERS ====================

	/** Check if this character should use AI control */
	UFUNCTION(BlueprintPure, Category = "Character|Control")
	bool ShouldUseAI() const { return bIsAIControlled; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsGeneric() const { return CharacterClass == ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsCaster() const { return CharacterClass == ECharacterClass::Caster; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsResonator() const { return CharacterClass == ECharacterClass::Resonator; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanUseSpells() const { return CharacterClass != ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanUseAbilities() const { return CharacterClass == ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool HasInnateElement() const { return CharacterClass == ECharacterClass::Caster; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool UsesRings() const { return CharacterClass == ECharacterClass::Resonator; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanDualWield() const { return CharacterClass == ECharacterClass::Generic; }

	/** Get element - Caster returns InnateElement, others return Generic */
	UFUNCTION(BlueprintPure, Category = "Character|Class")
	ESpellElement GetElement() const
	{
		return IsCaster() ? InnateElement : ESpellElement::Generic;
	}

	// ==================== STAT BUDGET VALIDATION ====================
	// ==================== STAT POOL ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetTotalPool() const
	{
		return StatConstants::INITIAL_STAT_BUDGET +
			   ((WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * StatConstants::POINTS_PER_WORLD_STAT_LEVEL);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetTotalSpent() const
	{
		return Efficiency + SpellDamage + CritChance + SpellSpeed +
			   Defense + ActionSpeed + RawDamage + MaxHealth +
			   MaxEnergy + Resistance + TurnSpeed + Luck + StatusMultiplier;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetPointsRemaining() const { return GetTotalPool() - GetTotalSpent(); }

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidDistribution() const { return GetTotalSpent() <= GetTotalPool(); }

	// ==================== TOTAL SUB-STATS ====================
	// Initial + World points combined

	// ----- MIND (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalEfficiency() const { return Efficiency; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellDamage() const { return SpellDamage; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCritChance() const { return CritChance; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellSpeed() const { return SpellSpeed; }

	// ----- BODY (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalDefense() const { return Defense; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalActionSpeed() const { return ActionSpeed; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalRawDamage() const { return RawDamage; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalMaxHealth() const { return MaxHealth; }

	// ----- SPIRIT (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalMaxEnergy() const { return MaxEnergy; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalResistance() const { return Resistance; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalTurnSpeed() const { return TurnSpeed; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalLuck() const { return Luck; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalStatusMultiplier() const { return StatusMultiplier; }

	// ==================== BASE STATS (DERIVED FROM TOTALS) ====================
	// Sum of sub-stat points per category (before world level scaling)

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseMind() const
	{
		// Mind (4): Efficiency, SpellDamage, CritChance, SpellSpeed
		return GetTotalEfficiency() + GetTotalSpellDamage() + GetTotalCritChance() + GetTotalSpellSpeed();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseBody() const
	{
		// Body (4): Defense, ActionSpeed, RawDamage, MaxHealth
		return GetTotalDefense() + GetTotalActionSpeed() + GetTotalRawDamage() + GetTotalMaxHealth();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseSpirit() const
	{
		// Spirit (5): MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier
		return GetTotalMaxEnergy() + GetTotalResistance() + GetTotalTurnSpeed() + GetTotalLuck() + GetTotalStatusMultiplier();
	}

	// ==================== EFFECTIVE STATS (WITH SCALING) ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveMind() const
	{
		int32 BaseMind = GetBaseMind();
		return BaseMind * (1.0f + WorldMindLevel * CombatConstants::WORLD_MIND_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveBody() const
	{
		int32 BaseBody = GetBaseBody();
		return BaseBody * (1.0f + WorldBodyLevel * CombatConstants::WORLD_BODY_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveSpirit() const
	{
		int32 BaseSpirit = GetBaseSpirit();
		return BaseSpirit * (1.0f + WorldSpiritLevel * CombatConstants::WORLD_SPIRIT_SCALING_BONUS);
	}

	// ==================== MIND CALCULATIONS ====================
	// Mind (4): Efficiency, SpellDamage, CritChance, SpellSpeed

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateEfficiencyMultiplier() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEfficiency();
		return FMath::Clamp(
			1.0f - (EffectiveMind * TotalPoints * CombatConstants::EFFICIENCY_PER_POINT),
			1.0f - CombatConstants::EFFICIENCY_MAX,
			1.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateEfficiencyRingBreakReduction() const
	{
		// Only Resonators get ring break reduction from Efficiency
		if (CharacterClass != ECharacterClass::Resonator)
			return 0.0f;

		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEfficiency();
		return FMath::Clamp(
			EffectiveMind * TotalPoints * CombatConstants::EFFICIENCY_RING_BREAK_PER_POINT,
			0.0f,
			CombatConstants::EFFICIENCY_RING_BREAK_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateStatusMultiplier() const
	{
		// Drives status buildup scaling (consumed by StatusBuildupManager and
		// the per-skill CalculateStatusBuildup helpers). Spirit-driven — moved
		// off Mind alongside the StatusMultiplier substat pillar move.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalStatusMultiplier();
		return 1.0f + (EffectiveSpirit * TotalPoints * CombatConstants::STATUS_MULTIPLIER_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellDamage() const
	{
		// Spell damage multiplier — applied once via
		// DamageCalculator::GetAttackerDamageMultiplier for EActionType::Spell.
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalSpellDamage();
		return 1.0f + (EffectiveMind * TotalPoints * CombatConstants::SPELL_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateCritChance() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalCritChance();
		return FMath::Clamp(
			CombatConstants::CRIT_CHANCE_BASE + (EffectiveMind * TotalPoints * CombatConstants::CRIT_CHANCE_PER_POINT),
			CombatConstants::CRIT_CHANCE_BASE,
			CombatConstants::CRIT_CHANCE_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellSpeed() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalSpellSpeed();
		return CombatConstants::SPELL_SPEED_BASE + (EffectiveMind * TotalPoints * CombatConstants::SPELL_SPEED_PER_POINT);
	}

	// ==================== BODY CALCULATIONS ====================
	// Body (3): Defense, ActionSpeed, RawDamage

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	int32 CalculateFlatDefense() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalDefense();
		return FMath::RoundToInt(EffectiveBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateActionSpeed() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalActionSpeed();
		return CombatConstants::MOVEMENT_SPEED_BASE * (1.0f + EffectiveBody * TotalPoints * CombatConstants::MOVEMENT_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateAnimationSpeed() const
	{
		// Uses same stat as ActionSpeed
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalActionSpeed();
		return CombatConstants::ANIMATION_SPEED_BASE + (EffectiveBody * TotalPoints * CombatConstants::ANIMATION_SPEED_PER_POINT);
	}

	// ==================== SPIRIT CALCULATIONS ====================
	// Spirit (5): MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateTurnSpeed() const
	{
		// NOTE: Moved from Mind to Spirit
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalTurnSpeed();
		return CombatConstants::TURN_SPEED_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::TURN_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateLuck() const
	{
		// Multi-system fortune stat. Returns raw 0.0-LUCK_RAW_MAX multiplier.
		// Consumers apply their own per-system caps (LUCK_CRIT_BONUS_MAX,
		// LUCK_DODGE_MAX, LUCK_BREAK_SKIP_MAX, LUCK_DROP_CHANCE_MAX,
		// LUCK_DROP_QUALITY_MAX) at their respective sites.
		// Same shape as Resistance.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalLuck();
		return FMath::Clamp(
			EffectiveSpirit * TotalPoints * CombatConstants::LUCK_PER_POINT,
			0.0f,
			CombatConstants::LUCK_RAW_MAX);
	}

	// ==================== HELPER FUNCTIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxHealth() const
	{
		// NOTE: Moved from Spirit to Body. HP is a physical pool stat.
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalMaxHealth();
		return FMath::RoundToInt(CombatConstants::MAX_HEALTH_BASE + (EffectiveBody * TotalPoints * CombatConstants::MAX_HEALTH_PER_POINT));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxEnergy() const
	{
		// Now uses explicit MaxEnergy stat (Spirit-based)
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalMaxEnergy();
		return FMath::RoundToInt(CombatConstants::MAX_ENERGY_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::MAX_ENERGY_PER_POINT));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	float CalculateRawDamage() const
	{
		// Raw damage multiplier for physical attacks (Body-based)
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalRawDamage();
		return 1.0f + (EffectiveBody * TotalPoints * CombatConstants::RAW_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	int32 CalculateStatusMultiplierFlat() const
	{
		// Flat StatusMultiplier point value (debug inspection). Spirit-driven
		// after the pillar move from Mind.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalStatusMultiplier();
		return FMath::RoundToInt(EffectiveSpirit * TotalPoints);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateResistance() const
	{
		// Reduces status effect damage and buildup
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalResistance();
		return FMath::Clamp(
			EffectiveSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
			0.0f,
			CombatConstants::RESISTANCE_MAX);
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};