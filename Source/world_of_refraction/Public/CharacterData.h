// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RefractionElement.h"

#include <world_of_refraction/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterData.generated.h"
class UBaseAttackData;
class UStanceData;
class UCharacterInfusionDisplayData;
class UWeaponData;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ERefractionElement InnateElement = ERefractionElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Character description...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	UTexture2D *Portrait = nullptr;

	// ==================== COMBAT LOADOUT ====================
	// Base attack for unarmed state (Elemental only - Generic uses weapon attacks)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Attack",
			  meta = (EditCondition = "InnateElement != ERefractionElement::Generic", EditConditionHides))
	UBaseAttackData *BaseAttack = nullptr;

	// Use primary weapon/state at combat start?
	// Elemental: true = Armed (Primary), false = Unarmed
	// Generic: true = Primary weapon, false = Secondary weapon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
	bool bUsePrimary = true;

	// Primary weapon (all characters)// Validate weapon loadout
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
	UWeaponData *PrimaryWeapon = nullptr;

	// Secondary weapon (Generic characters only)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons",
			  meta = (EditCondition = "InnateElement == ERefractionElement::Generic", EditConditionHides))
	UWeaponData *SecondaryWeapon = nullptr;

	// ==================== STANCE/INFUSION ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Stance")
	UStanceData *UnarmedStance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Infusion")
	UCharacterInfusionDisplayData *InfusionDisplay = nullptr;

	// ==================== STAT BUDGET ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "20", ClampMax = "40"))
	int32 InitialStatBudget = CombatConstants::STAT_BUDGET_STANDARD;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 PointsPerWorldStatLevel = CombatConstants::POINTS_PER_WORLD_STAT_LEVEL;

	// ==================== WORLD STAT LEVELS ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldMindLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldBodyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldSpiritLevel = 0;

	// ==================== INITIAL SUB-STATS (CHARACTER DNA) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialCostReductionPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialTurnSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialCritChancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialDefensePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialAttackSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialRawDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialEffectDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialResistancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialAbilitySizePoints = 0;

	// ==================== WORLD SUB-STATS (PROGRESSION) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldCostReductionPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldTurnSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldCritChancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldDefensePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldAttackSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldRawDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldEffectDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldResistancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldAbilitySizePoints = 0;

	// ==================== ITEMS ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	int32 MaxItemSlots = 4;

	// ==================== ABILITIES ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	int32 MaxAbilitySlots = 3;

	// ==================== SPELL SCHOOLS ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	int32 MaxSpellsPerSchool = 3;

	// ==================== VISUAL DATA ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	USkeletalMesh *CharacterMesh = nullptr;

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

	// ==================== TOTAL SUB-STATS (GETTERS) ====================

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCostReduction() const { return InitialCostReductionPoints + WorldCostReductionPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalTurnSpeed() const { return InitialTurnSpeedPoints + WorldTurnSpeedPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCritChance() const { return InitialCritChancePoints + WorldCritChancePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalDefense() const { return InitialDefensePoints + WorldDefensePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalAttackSpeed() const { return InitialAttackSpeedPoints + WorldAttackSpeedPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalRawDamage() const { return InitialRawDamagePoints + WorldRawDamagePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalEffectDamage() const { return InitialEffectDamagePoints + WorldEffectDamagePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalResistance() const { return InitialResistancePoints + WorldResistancePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalAbilitySize() const { return InitialAbilitySizePoints + WorldAbilitySizePoints; }

	// ==================== BASE STATS (DERIVED FROM TOTALS) ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseMind() const
	{
		return GetTotalCostReduction() + GetTotalTurnSpeed() + GetTotalCritChance();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseBody() const
	{
		return GetTotalDefense() + GetTotalAttackSpeed() + GetTotalRawDamage();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseSpirit() const
	{
		return GetTotalEffectDamage() + GetTotalResistance() + GetTotalAbilitySize();
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

	// ==================== VALIDATION ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetInitialSubStatSum() const
	{
		return (InitialCostReductionPoints + InitialTurnSpeedPoints + InitialCritChancePoints) + (InitialDefensePoints + InitialAttackSpeedPoints + InitialRawDamagePoints) + (InitialEffectDamagePoints + InitialResistancePoints + InitialAbilitySizePoints);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetWorldSubStatSum() const
	{
		return (WorldCostReductionPoints + WorldTurnSpeedPoints + WorldCritChancePoints) + (WorldDefensePoints + WorldAttackSpeedPoints + WorldRawDamagePoints) + (WorldEffectDamagePoints + WorldResistancePoints + WorldAbilitySizePoints);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetExpectedWorldPoints() const
	{
		return (WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * PointsPerWorldStatLevel;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidInitialDistribution() const
	{
		return GetInitialSubStatSum() == InitialStatBudget;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidWorldDistribution() const
	{
		return GetWorldSubStatSum() == GetExpectedWorldPoints();
	}

	// ==================== MIND CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellCostReduction() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalCostReduction();
		return FMath::Clamp(
			EffectiveMind * TotalPoints * CombatConstants::COST_REDUCTION_PER_POINT,
			0.0f,
			CombatConstants::COST_REDUCTION_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateTurnSpeed() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalTurnSpeed();
		return CombatConstants::TURN_SPEED_BASE + (EffectiveMind * TotalPoints * CombatConstants::TURN_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateCriticalChance() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalCritChance();
		return FMath::Clamp(
			CombatConstants::CRIT_CHANCE_BASE + (EffectiveMind * TotalPoints * CombatConstants::CRIT_CHANCE_PER_POINT),
			CombatConstants::CRIT_CHANCE_BASE,
			CombatConstants::CRIT_CHANCE_MAX);
	}

	// ==================== BODY CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	int32 CalculateFlatDefense() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalDefense();
		return FMath::RoundToInt(EffectiveBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateAttackSpeed() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalAttackSpeed();
		return CombatConstants::ATTACK_SPEED_BASE + (EffectiveBody * TotalPoints * CombatConstants::ATTACK_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateRawDamageMultiplier() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalRawDamage();
		return 1.0f + (EffectiveBody * TotalPoints * CombatConstants::RAW_DAMAGE_PER_POINT);
	}

	// ==================== SPIRIT CALCULATIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateEffectDamageMultiplier() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalEffectDamage();
		return 1.0f + (EffectiveSpirit * TotalPoints * CombatConstants::EFFECT_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateElementalResistance() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalResistance();
		return FMath::Clamp(
			EffectiveSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
			0.0f,
			CombatConstants::RESISTANCE_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateAbilitySizeMultiplier() const
	{
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalAbilitySize();
		return 1.0f + (EffectiveSpirit * TotalPoints * CombatConstants::ABILITY_SIZE_PER_POINT);
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
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override
	{
		EDataValidationResult Result = Super::IsDataValid(Context);

		// Check world stat distribution
		if (!IsValidWorldDistribution())
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("World sub-stats (%d) don't match expected (%d)"),
								GetWorldSubStatSum(), GetExpectedWorldPoints())));
			Result = EDataValidationResult::Invalid;
		}

		// Validate weapon loadout
		if (InnateElement == ERefractionElement::Generic)
		{
			if (PrimaryWeapon == nullptr && SecondaryWeapon == nullptr)
			{
				Context.AddWarning(FText::FromString(TEXT("Generic character has no weapons assigned")));
			}
		}
		else
		{
			if (SecondaryWeapon != nullptr)
			{
				Context.AddError(FText::FromString(TEXT("Elemental characters cannot have a secondary weapon")));
				Result = EDataValidationResult::Invalid;
			}
		}

		return Result;
	}
#endif
};