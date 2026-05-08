// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ESpellElement.h"
#include "ECharacterClass.h"
#include "StatConstants.h"
#include <CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterData.generated.h"

class USpellData;
class UItemData;
class ULoadoutData;
class UStanceData;
class UInfusionDisplayData;
class UAnimMontage;

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
	FString CharacterName = TEXT("Unnamed Character");

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

	// ==================== DEFAULT LOADOUT ====================

	/** Default loadout for AI/template use (nullptr = build from inventory) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout",
			  meta = (DisplayName = "Default Loadout"))
	ULoadoutData *DefaultLoadout = nullptr;

	// ==================== EVOLUTION COST FUNCTIONS ====================

	/** Check if character can apply this evolution crystal */
	UFUNCTION(BlueprintPure, Category = "Evolution|Cost")
	bool CanApplyEvolution(UItemData *EvolutionCrystal) const;

	/** Calculate evolution cost for this crystal */
	UFUNCTION(BlueprintPure, Category = "Evolution|Cost")
	FEvolutionCostResult CalculateEvolutionCost(UItemData *EvolutionCrystal) const;

	/** Get user-friendly cost description */
	UFUNCTION(BlueprintPure, Category = "Evolution|Cost")
	FString GetEvolutionCostDescription(UItemData *EvolutionCrystal) const;

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
	int32 EffectDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 CritChance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 SpellSpeed = 0;

	// ==================== BODY SUB-STATS (4) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 Defense = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 MovementSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 RawDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 MaxHealth = 0;

	// ==================== SPIRIT SUB-STATS (4) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 MaxEnergy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Resistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 TurnSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Luck = 0;

	// ==================== DEFENSE ANIMATIONS ====================

	/** Dodge left animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *DodgeLeftMontage = nullptr;

	/** Dodge right animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *DodgeRightMontage = nullptr;

	/** Block animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *BlockMontage = nullptr;

	/** Parry animation (can be overridden by weapon) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *ParryMontage = nullptr;

	/** Use active weapon's parry animation instead of character's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	bool bUseWeaponParryAnimation = false;

	// ==================== COSMETICS ====================

	/** Default unarmed stance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UStanceData *UnarmedStance = nullptr;

	/** Animation for self-targeted item use (healing, energy, cleanse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Item Use")
	UAnimMontage *ItemUseSelfMontage = nullptr;

	/** Animation for target-directed item use (damage, ally buffs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Item Use")
	UAnimMontage *ItemUseTargetMontage = nullptr;

	/** Animation for Resonator ring switching */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Resonator", EditConditionHides))
	UAnimMontage *RingSwitchMontage = nullptr;

	/** Visual effect for element infusion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UInfusionDisplayData *InfusionDisplay = nullptr;

	// Weather variant equipped for when this character is team leader
	// Leave null to use element default
	// Generic and Resonator classes ignore this
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Weather")
	UPrimaryDataAsset *EquippedWeatherVariant = nullptr;

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
		return Efficiency + EffectDamage + CritChance + SpellSpeed +
			   Defense + MovementSpeed + RawDamage + MaxHealth +
			   MaxEnergy + Resistance + TurnSpeed + Luck;
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
	int32 GetTotalEffectDamage() const { return EffectDamage; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCritChance() const { return CritChance; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellSpeed() const { return SpellSpeed; }

	// ----- BODY (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalDefense() const { return Defense; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalMovementSpeed() const { return MovementSpeed; }

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

	// ==================== BASE STATS (DERIVED FROM TOTALS) ====================
	// Sum of sub-stat points per category (before world level scaling)

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseMind() const
	{
		return GetTotalEfficiency() + GetTotalEffectDamage() + GetTotalCritChance() + GetTotalSpellSpeed();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseBody() const
	{
		// Body (4): Defense, MovementSpeed, RawDamage, MaxHealth
		return GetTotalDefense() + GetTotalMovementSpeed() + GetTotalRawDamage() + GetTotalMaxHealth();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseSpirit() const
	{
		// Spirit (4): MaxEnergy, Resistance, TurnSpeed, Luck
		return GetTotalMaxEnergy() + GetTotalResistance() + GetTotalTurnSpeed() + GetTotalLuck();
	}

	// ==================== EFFECTIVE STATS (WITH SCALING) ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveMind() const
	{
		int32 BaseMind = GetBaseMind();
		return BaseMind * (1.0f + WorldMindLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveBody() const
	{
		int32 BaseBody = GetBaseBody();
		return BaseBody * (1.0f + WorldBodyLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveSpirit() const
	{
		int32 BaseSpirit = GetBaseSpirit();
		return BaseSpirit * (1.0f + WorldSpiritLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	// ==================== MIND CALCULATIONS ====================
	// Mind (4): Efficiency, EffectDamage, CritChance, SpellSpeed

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

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateEffectDamageMultiplier() const
	{
		// NOTE: Moved from Spirit to Mind
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEffectDamage();
		return 1.0f + (EffectiveMind * TotalPoints * CombatConstants::EFFECT_DAMAGE_PER_POINT);
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
	// Body (3): Defense, MovementSpeed, RawDamage

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	int32 CalculateFlatDefense() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalDefense();
		return FMath::RoundToInt(EffectiveBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateMovementSpeed() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalMovementSpeed();
		return CombatConstants::MOVEMENT_SPEED_BASE * (1.0f + EffectiveBody * TotalPoints * CombatConstants::MOVEMENT_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateAnimationSpeed() const
	{
		// Uses same stat as MovementSpeed
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalMovementSpeed();
		return CombatConstants::ANIMATION_SPEED_BASE + (EffectiveBody * TotalPoints * CombatConstants::ANIMATION_SPEED_PER_POINT);
	}

	// ==================== SPIRIT CALCULATIONS ====================
	// Spirit (4): MaxEnergy, MaxHealth, Resistance, TurnSpeed

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

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateEffectDamage() const
	{
		// Flat effect damage value
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEffectDamage();
		return FMath::RoundToInt(EffectiveMind * TotalPoints);
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
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;

#endif
};