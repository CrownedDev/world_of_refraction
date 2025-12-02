// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SpellElement.h"
#include "ECharacterClass.h"
#include "StatConstants.h"
#include "EWeaponSlotType.h"
#include <CombatConstants.h>
#include "InfusionDisplayData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterData.generated.h"

class UWeaponAttackData;
class UStanceData;

class UWeaponData;
class URingData;
class USpellData;
class UEvolutionData;

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
	ESpellElement InnateElement = ESpellElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Character description...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	UTexture2D *Portrait = nullptr;

	// ==================== EVOLUTION ====================

	/** Is this character evolved? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	bool bIsEvolved = false;

	/** Active evolution data (if evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
			  meta = (EditCondition = "bIsEvolved", EditConditionHides))
	UEvolutionData *ActiveEvolution = nullptr;
#if WITH_EDITORONLY_DATA
	/** Display only - shows element from ActiveEvolution */
	UPROPERTY(VisibleAnywhere, Category = "Evolution",
			  meta = (EditCondition = "bIsEvolved && ActiveEvolution != nullptr", EditConditionHides))
	FString EvolutionElementDisplay = TEXT("(Set by ActiveEvolution)");
#endif

	// ==================== EVOLUTION HELPERS ====================

	UFUNCTION(BlueprintPure, Category = "Evolution")
	bool IsEvolved() const { return bIsEvolved && ActiveEvolution != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Evolution")
	ESpellElement GetSecondaryElement() const;

	UFUNCTION(BlueprintPure, Category = "Evolution")
	bool HasSecondaryElement() const;

	UFUNCTION(BlueprintPure, Category = "Evolution")
	bool IsDualElementCaster() const;

	/** Get spells for combat - returns Evolution's EquippedSpells if evolved, else InnateSpells */
	UFUNCTION(BlueprintPure, Category = "Evolution|Spells")
	TArray<USpellData *> GetCombatSpells() const;

	/** Check if character has weapon access (evolved Casters lose weapons) */
	UFUNCTION(BlueprintPure, Category = "Evolution|Loadout")
	bool HasWeaponAccess() const;

	// ==================== COMBAT LOADOUT ====================

	// Use primary weapon/state at combat start?
	// Caster/Resonator: true = Armed (Primary), false = Unarmed
	// Generic: true = Primary weapon, false = Secondary weapon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
	bool bUsePrimary = true;

	// ==================== PRIMARY SLOT ====================

	/** Primary slot type (Caster only - choose Weapon or Ring, hidden when evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Primary",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Caster && !bIsEvolved", EditConditionHides))
	EPrimarySlotType PrimarySlotType = EPrimarySlotType::Weapon;

	/** Primary weapon (Generic: always, Caster: if Weapon and not evolved, Resonator: always) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Primary",
			  meta = (EditCondition = "(CharacterClass != ECharacterClass::Caster || (PrimarySlotType == EPrimarySlotType::Weapon && !bIsEvolved))", EditConditionHides))
	UWeaponData *PrimaryWeapon = nullptr;

	/** Primary ring (Caster only, when PrimarySlotType == Ring, hidden when evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Primary",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring && !bIsEvolved", EditConditionHides))
	URingData *PrimaryRing = nullptr;

	// ==================== SECONDARY SLOT (Generic Only) ====================

	/** Secondary slot type (Generic only, hidden when evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Secondary",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Generic && !bIsEvolved", EditConditionHides))
	ESecondarySlotType SecondarySlotType = ESecondarySlotType::None;

	/** Secondary weapon (Generic only, when SecondarySlotType == Weapon, hidden when evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Secondary",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Generic && !bIsEvolved && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
	UWeaponData *SecondaryWeapon = nullptr;

	/** Secondary ring (Generic only, when SecondarySlotType == Ring, hidden when evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Secondary",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Generic && !bIsEvolved && SecondarySlotType == ESecondarySlotType::Ring", EditConditionHides))
	URingData *SecondaryRing = nullptr;

	// ==================== CASTER SPELLS ====================

	/** Innate spells (Caster only, hidden when evolved - uses Evolution spells instead) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Spells",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Caster", EditConditionHides))
	TArray<USpellData *> InnateSpells;

	// ==================== RESONATOR RINGS ====================

	/** Equipped rings (Resonator only - up to 5 slots, max 2 evolved) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Rings",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Resonator", EditConditionHides))
	TArray<URingData *> EquippedRings;

	// ==================== STANCE ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Stance")
	UStanceData *UnarmedStance = nullptr;

	// ==================== INFUSION ====================
	/** Visual effect when infusion is active */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UInfusionDisplayData *InfusionDisplay = nullptr;

	// Charge level animations (play when reaching L1/L2)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion|Animation")
	UAnimMontage *InfusionL1Animation; // needs to be removed we will make it so that the infusion display increases size for each level

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion|Animation")
	UAnimMontage *InfusionL2Animation; // needs to be removed we will make it so that the infusion display increases size for each level

	// ==================== STAT BUDGET ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "20", ClampMax = "40"))
	int32 InitialStatBudget = CombatConstants::STAT_BUDGET_STANDARD;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 PointsPerWorldStatLevel = CombatConstants::POINTS_PER_WORLD_STAT_LEVEL;

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
	// 11 Stats: Mind(4), Body(3), Spirit(4)
	// Total must equal InitialStatBudget (default 30)

	// ----- MIND (4 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialEfficiencyPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialEffectDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialCritChancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|Initial", meta = (ClampMin = "0"))
	int32 InitialSpellSpeedPoints = 0;

	// ----- BODY (3 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialDefensePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialMovementSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|Initial", meta = (ClampMin = "0"))
	int32 InitialRawDamagePoints = 0;

	// ----- SPIRIT (4 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialMaxEnergyPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialMaxHealthPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialResistancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|Initial", meta = (ClampMin = "0"))
	int32 InitialTurnSpeedPoints = 0;

	// ==================== WORLD SUB-STATS (PROGRESSION BONUSES) ====================
	// Points generated from World Stat Levels (3 points per level)
	// Must match: (WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * PointsPerWorldStatLevel

	// ----- MIND (4 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldEfficiencyPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldEffectDamagePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldCritChancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind|World", meta = (ClampMin = "0"))
	int32 WorldSpellSpeedPoints = 0;

	// ----- BODY (3 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldDefensePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldMovementSpeedPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body|World", meta = (ClampMin = "0"))
	int32 WorldRawDamagePoints = 0;

	// ----- SPIRIT (4 stats) -----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldMaxEnergyPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldMaxHealthPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldResistancePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit|World", meta = (ClampMin = "0"))
	int32 WorldTurnSpeedPoints = 0;

	// ==================== CLASS HELPERS ====================

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
	// 11 Stats: Mind(4) + Body(3) + Spirit(4)

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetInitialSubStatSum() const
	{
		// Mind (4): Efficiency, EffectDamage, CritChance, SpellSpeed
		// Body (3): Defense, MovementSpeed, RawDamage
		// Spirit (4): MaxEnergy, MaxHealth, Resistance, TurnSpeed
		return InitialEfficiencyPoints + InitialEffectDamagePoints + InitialCritChancePoints + InitialSpellSpeedPoints +
			   InitialDefensePoints + InitialMovementSpeedPoints + InitialRawDamagePoints +
			   InitialMaxEnergyPoints + InitialMaxHealthPoints + InitialResistancePoints + InitialTurnSpeedPoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidInitialDistribution() const
	{
		return GetInitialSubStatSum() == InitialStatBudget;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetExpectedWorldPoints() const
	{
		return (WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * PointsPerWorldStatLevel;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	int32 GetWorldSubStatSum() const
	{
		return WorldEfficiencyPoints + WorldEffectDamagePoints + WorldCritChancePoints + WorldSpellSpeedPoints +
			   WorldDefensePoints + WorldMovementSpeedPoints + WorldRawDamagePoints +
			   WorldMaxEnergyPoints + WorldMaxHealthPoints + WorldResistancePoints + WorldTurnSpeedPoints;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidWorldDistribution() const
	{
		return GetWorldSubStatSum() == GetExpectedWorldPoints();
	}

	// ==================== TOTAL SUB-STATS ====================
	// Initial + World points combined

	// ----- MIND (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalEfficiency() const { return InitialEfficiencyPoints + WorldEfficiencyPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalEffectDamage() const { return InitialEffectDamagePoints + WorldEffectDamagePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCritChance() const { return InitialCritChancePoints + WorldCritChancePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellSpeed() const { return InitialSpellSpeedPoints + WorldSpellSpeedPoints; }

	// ----- BODY (3 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalDefense() const { return InitialDefensePoints + WorldDefensePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalMovementSpeed() const { return InitialMovementSpeedPoints + WorldMovementSpeedPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalRawDamage() const { return InitialRawDamagePoints + WorldRawDamagePoints; }

	// ----- SPIRIT (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalMaxEnergy() const { return InitialMaxEnergyPoints + WorldMaxEnergyPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalMaxHealth() const { return InitialMaxHealthPoints + WorldMaxHealthPoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalResistance() const { return InitialResistancePoints + WorldResistancePoints; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalTurnSpeed() const { return InitialTurnSpeedPoints + WorldTurnSpeedPoints; }

	// ==================== BASE STATS (DERIVED FROM TOTALS) ====================
	// Sum of sub-stat points per category (before world level scaling)

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseMind() const
	{
		// Mind (4): Efficiency, EffectDamage, CritChance, SpellSpeed
		return GetTotalEfficiency() + GetTotalEffectDamage() + GetTotalCritChance() + GetTotalSpellSpeed();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseBody() const
	{
		// Body (3): Defense, MovementSpeed, RawDamage
		return GetTotalDefense() + GetTotalMovementSpeed() + GetTotalRawDamage();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseSpirit() const
	{
		// Spirit (4): MaxEnergy, MaxHealth, Resistance, TurnSpeed
		return GetTotalMaxEnergy() + GetTotalMaxHealth() + GetTotalResistance() + GetTotalTurnSpeed();
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

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateRawDamageMultiplier() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalRawDamage();
		return 1.0f + (EffectiveBody * TotalPoints * CombatConstants::RAW_DAMAGE_PER_POINT);
	}

	// ==================== SPIRIT CALCULATIONS ====================
	// Spirit (4): MaxEnergy, MaxHealth, Resistance, TurnSpeed

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateStatusResistance() const
	{
		// Reduces status effect damage and buildup
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalResistance();
		return FMath::Clamp(
			EffectiveSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
			0.0f,
			CombatConstants::RESISTANCE_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateTurnSpeed() const
	{
		// NOTE: Moved from Mind to Spirit
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalTurnSpeed();
		return CombatConstants::TURN_SPEED_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::TURN_SPEED_PER_POINT);
	}

	// ==================== HELPER FUNCTIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxHP() const
	{
		// Now uses explicit MaxHealth stat (Spirit-based)
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalMaxHealth();
		return FMath::RoundToInt(CombatConstants::MAX_HEALTH_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::MAX_HEALTH_PER_POINT));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxEP() const
	{
		// Now uses explicit MaxEnergy stat (Spirit-based)
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalMaxEnergy();
		return FMath::RoundToInt(CombatConstants::MAX_ENERGY_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::MAX_ENERGY_PER_POINT));
	}

	// ==================== EQUIPMENT HELPERS ====================

	/** Get currently active weapon based on bUsePrimary and class */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UWeaponData *GetActiveWeapon() const;

	// ==================== STANCE HELPERS ====================

	/** Get current idle montage based on current weapon state */
	UFUNCTION(BlueprintPure, Category = "Stance")
	UAnimMontage *GetCurrentIdleMontage() const;

	/** Get current stance based on current weapon state */
	UFUNCTION(BlueprintPure, Category = "Stance")
	UStanceData *GetCurrentStance() const;

	/** Get current attack based on weapon state (nullptr if unarmed) */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UWeaponAttackData *GetCurrentAttack() const;

	/** Get current attack montage based on weapon state */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UAnimMontage *GetCurrentAttackMontage() const;

	/** Is character currently in armed state? */
	UFUNCTION(BlueprintPure, Category = "Stance")
	bool IsArmed() const;

	/** Check if secondary slot is a ring (Generic only) */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasRingInSecondary() const
	{
		return IsGeneric() &&
			   SecondarySlotType == ESecondarySlotType::Ring &&
			   SecondaryRing != nullptr &&
			   !IsEvolved();
	}

private:
	/** Character's active weapon based on bUsePrimary - external systems use WeaponManager */
	UWeaponData *GetActiveCharacterWeapon() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;

#endif
};