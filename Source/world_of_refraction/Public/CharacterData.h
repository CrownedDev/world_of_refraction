// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RefractionElement.h"
#include <world_of_refraction/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

//// Forward declarations
//class USpellData;

#include "CharacterData.generated.h"



/**
 * Character Data Asset - Contains all character stats, abilities, and visual data
 * Supports both base stat distribution and world stat bonuses with sub-stat customization
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString CharacterName = TEXT("Unnamed Character");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ERefractionElement InnateElement = ERefractionElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Character description...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	UTexture2D* Portrait = nullptr;

	// ==================== STAT BUDGET ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "20", ClampMax = "40"))
	int32 DistributablePoints = CombatConstants::STAT_BUDGET_STANDARD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
	int32 DistributedMind = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
	int32 DistributedBody = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
	int32 DistributedSpirit = 10;

	// ==================== WORLD STAT BONUSES ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldMindLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldBodyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldSpiritLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|World Bonuses")
	int32 PointsPerWorldStatLevel = CombatConstants::POINTS_PER_WORLD_STAT_LEVEL;

	// ==================== SUB-STAT DISTRIBUTION (MIND) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 CostReductionPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 TurnSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 CritChancePoints = 0;

	// ==================== SUB-STAT DISTRIBUTION (BODY) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 DefensePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 AttackSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 RawDamagePoints = 0;

	// ==================== SUB-STAT DISTRIBUTION (SPIRIT) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 EffectDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 ResistancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 AbilitySizePoints = 0;

	// ==================== SPELL POOLS ====================

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	//TArray<USpellData*> GenericSpellPool;

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	//TArray<USpellData*> UniqueSpells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells", meta = (ClampMin = "3", ClampMax = "6"))
	int32 MaxGenericSpellSlots = 4;

	// ==================== VISUAL DATA ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	USkeletalMesh* CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSubclassOf<UAnimInstance> AnimationBlueprint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor PrimaryColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor SecondaryColor = FLinearColor::Black;

	// ==================== BALANCE FLAGS ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
	bool bHasBrokenAbilities = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (EditCondition = "bHasBrokenAbilities", MultiLine = true))
	FString BalanceNotes = TEXT("");

	// ==================== EFFECTIVE STATS ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveMind() const
	{
		return DistributedMind * (1.0f + WorldMindLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveBody() const
	{
		return DistributedBody * (1.0f + WorldBodyLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveSpirit() const
	{
		return DistributedSpirit * (1.0f + WorldSpiritLevel * CombatConstants::WORLD_STAT_SCALING_BONUS);
	}

	// ==================== VALIDATION ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetTotalDistributedPoints() const
	{
		return DistributedMind + DistributedBody + DistributedSpirit;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidDistribution() const
	{
		return GetTotalDistributedPoints() == DistributablePoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetAvailableMindPoints() const
	{
		return WorldMindLevel * PointsPerWorldStatLevel;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetUsedMindPoints() const
	{
		return CostReductionPoints + TurnSpeedPoints + CritChancePoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidMindDistribution() const
	{
		return GetUsedMindPoints() == GetAvailableMindPoints();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetAvailableBodyPoints() const
	{
		return WorldBodyLevel * PointsPerWorldStatLevel;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetUsedBodyPoints() const
	{
		return DefensePoints + AttackSpeedPoints + RawDamagePoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidBodyDistribution() const
	{
		return GetUsedBodyPoints() == GetAvailableBodyPoints();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetAvailableSpiritPoints() const
	{
		return WorldSpiritLevel * PointsPerWorldStatLevel;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetUsedSpiritPoints() const
	{
		return EffectDamagePoints + ResistancePoints + AbilitySizePoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidSpiritDistribution() const
	{
		return GetUsedSpiritPoints() == GetAvailableSpiritPoints();
	}

	// ==================== MIND CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellCostReduction() const
	{
		float EffectiveMind = GetEffectiveMind();
		return FMath::Clamp(
			EffectiveMind * CostReductionPoints * CombatConstants::COST_REDUCTION_PER_POINT,
			0.0f,
			CombatConstants::COST_REDUCTION_MAX
		);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateTurnSpeed() const
	{
		float EffectiveMind = GetEffectiveMind();
		return CombatConstants::TURN_SPEED_BASE + (EffectiveMind * TurnSpeedPoints * CombatConstants::TURN_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateCriticalChance() const
	{
		float EffectiveMind = GetEffectiveMind();
		return FMath::Clamp(
			CombatConstants::CRIT_CHANCE_BASE + (EffectiveMind * CritChancePoints * CombatConstants::CRIT_CHANCE_PER_POINT),
			CombatConstants::CRIT_CHANCE_BASE,
			CombatConstants::CRIT_CHANCE_MAX
		);
	}

	// ==================== BODY CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	int32 CalculateFlatDefense() const
	{
		float EffectiveBody = GetEffectiveBody();
		return FMath::RoundToInt(EffectiveBody * DefensePoints * CombatConstants::DEFENSE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateAttackSpeed() const
	{
		float EffectiveBody = GetEffectiveBody();
		return CombatConstants::ATTACK_SPEED_BASE + (EffectiveBody * AttackSpeedPoints * CombatConstants::ATTACK_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateRawDamageMultiplier() const
	{
		float EffectiveBody = GetEffectiveBody();
		return 1.0f + (EffectiveBody * RawDamagePoints * CombatConstants::RAW_DAMAGE_PER_POINT);
	}

	// ==================== SPIRIT CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateEffectDamageMultiplier() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		return 1.0f + (EffectiveSpirit * EffectDamagePoints * CombatConstants::EFFECT_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateElementalResistance() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		return FMath::Clamp(
			EffectiveSpirit * ResistancePoints * CombatConstants::RESISTANCE_PER_POINT,
			0.0f,
			CombatConstants::RESISTANCE_MAX
		);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateAbilitySizeMultiplier() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		return 1.0f + (EffectiveSpirit * AbilitySizePoints * CombatConstants::ABILITY_SIZE_PER_POINT);
	}

	// ==================== HELPER FUNCTIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateTurnRatio(float EnemySpeed) const
	{
		float MySpeed = CalculateTurnSpeed();
		float Difference = MySpeed - EnemySpeed;

		if (Difference >= CombatConstants::TURN_SPEED_THRESHOLD_DOUBLE)
			return CombatConstants::MAX_TURN_RATIO;
		else
			return 1;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateSpellCost(int32 BaseCost) const
	{
		float Reduction = CalculateSpellCostReduction();
		return FMath::RoundToInt(BaseCost * (1.0f - Reduction));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateSpellDamage(int32 BaseDamage) const
	{
		float Multiplier = CalculateEffectDamageMultiplier();
		return FMath::RoundToInt(BaseDamage * Multiplier);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateReducedDamage(int32 IncomingDamage, bool bIsElemental) const
	{
		// Apply defense
		int32 AfterDefense = FMath::Max(0, IncomingDamage - CalculateFlatDefense());

		// Apply resistance if elemental
		if (bIsElemental)
		{
			float Resistance = CalculateElementalResistance();
			return FMath::RoundToInt(AfterDefense * (1.0f - Resistance));
		}

		return AfterDefense;
	}

	// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override
	{
		EDataValidationResult Result = Super::IsDataValid(Context);

		// Check base stat distribution
		if (!IsValidDistribution())
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("Base stat distribution (%d) doesn't match budget (%d)"),
					GetTotalDistributedPoints(), DistributablePoints)
			));
			Result = EDataValidationResult::Invalid;
		}

		// Check Mind sub-stat distribution
		if (WorldMindLevel > 0 && !IsValidMindDistribution())
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("Mind sub-stats (%d) don't match available (%d)"),
					GetUsedMindPoints(), GetAvailableMindPoints())
			));
			Result = EDataValidationResult::Invalid;
		}

		// Check Body sub-stat distribution
		if (WorldBodyLevel > 0 && !IsValidBodyDistribution())
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("Body sub-stats (%d) don't match available (%d)"),
					GetUsedBodyPoints(), GetAvailableBodyPoints())
			));
			Result = EDataValidationResult::Invalid;
		}

		// Check Spirit sub-stat distribution
		if (WorldSpiritLevel > 0 && !IsValidSpiritDistribution())
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("Spirit sub-stats (%d) don't match available (%d)"),
					GetUsedSpiritPoints(), GetAvailableSpiritPoints())
			));
			Result = EDataValidationResult::Invalid;
		}

		return Result;
	}
#endif
};