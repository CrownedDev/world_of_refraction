// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EActionType.h"
#include "SpellElement.h"
#include "EInfusionType.h"
#include "EInfusionSource.h"
#include "ActionStructs.generated.h"

class USpellData;
class UAbilityData;
class UItemData;
class UBaseAttackData;
class UUltimateData;

/**
 * FAction
 * Represents a combat action to be executed
 * Filled by UI/AI and passed to ActionExecutor
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAction
{
	GENERATED_BODY()

	/** Type of action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EActionType ActionType = EActionType::None;

	/** Target(s) of the action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<AActor *> Targets;

	/** Spell data (if ActionType == Spell) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	USpellData *SpellData = nullptr;

	/** Ability data (if ActionType == Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UAbilityData *AbilityData = nullptr;

	/** Item data (if ActionType == Item) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UItemData *ItemData = nullptr;

	/** Attack data (if ActionType == Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UBaseAttackData *AttackData = nullptr;

	/** Ultimate data (if ActionType == Ultimate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UUltimateData *UltimateData = nullptr;

	// ==================== INFUSION OPTIONS ====================

	/** Primary infusion type selection (None, Physical, Element) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	EInfusionType InfusionType = EInfusionType::None;

	/** Infusion charge level (0 = none, 1 = first charge, 2 = full charge)
	 *  Set by hold-to-charge input system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 InfusionLevel = 0;

	/** Spell size infusion level (0 = none, 1 = 1.5x size, 2 = 2.0x size + damage)
	 *  Separate from ability/attack infusion - applies to spells only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpellSizeInfusionLevel = 0;

	/** Use elemental mode for toggle spells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	bool bUseElementalMode = true;

	// ==================== DEPRECATED - REMOVE AFTER MIGRATION ====================

	/** @deprecated Use InfusionType + InfusionLevel instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use InfusionType instead"))
	bool bIsElementInfused = false;

	/** @deprecated Use SpellSizeInfusionLevel instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use SpellSizeInfusionLevel instead"))
	int32 SpellInfusionLevel = 0;

	/** @deprecated Use InfusionType + InfusionLevel instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use InfusionType instead"))
	int32 AbilityInfusionLevel = 0;

	// ==================== HELPERS ====================

	bool IsValid() const
	{
		if (ActionType == EActionType::None)
			return false;
		if (ActionType == EActionType::Spell && !SpellData)
			return false;
		if (ActionType == EActionType::Ability && !AbilityData)
			return false;
		if (ActionType == EActionType::Item && !ItemData)
			return false;
		if (ActionType == EActionType::Attack && !AttackData)
			return false;
		if (ActionType == EActionType::Ultimate && !UltimateData)
			return false;
		return true;
	}

	bool RequiresTarget() const
	{
		return ActionType != EActionType::Defend &&
			   ActionType != EActionType::Flee &&
			   ActionType != EActionType::SwitchWeapon;
	}

	/** Is any infusion active? */
	bool IsInfused() const
	{
		return InfusionType != EInfusionType::None && InfusionLevel > 0;
	}

	/** Is spell size infusion active? */
	bool IsSpellSizeInfused() const
	{
		return SpellSizeInfusionLevel > 0;
	}

	FString GetActionName() const
	{
		switch (ActionType)
		{
		case EActionType::Spell:
			return SpellData ? TEXT("Spell") : TEXT("Unknown Spell");
		case EActionType::Ability:
			return AbilityData ? TEXT("Ability") : TEXT("Unknown Ability");
		case EActionType::Item:
			return ItemData ? TEXT("Item") : TEXT("Unknown Item");
		case EActionType::Attack:
			return AttackData ? TEXT("Attack") : TEXT("Basic Attack");
		case EActionType::Ultimate:
			return UltimateData ? TEXT("Ultimate") : TEXT("Unknown Ultimate");
		case EActionType::Defend:
			return TEXT("Defend");
		case EActionType::SwitchWeapon:
			return TEXT("Switch Weapon");
		case EActionType::Flee:
			return TEXT("Flee");
		default:
			return TEXT("Unknown Action");
		}
	}
};

/**
 * FActionValidationResult
 * Result of validating an action before execution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionValidationResult
{
	GENERATED_BODY()

	/** Is the action valid to execute? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bIsValid = false;

	/** Error message if not valid (shown to player) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString ErrorMessage;

	/** Energy cost that would be spent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	int32 EnergyCost = 0;

	// Constructors
	FActionValidationResult() = default;
	FActionValidationResult(bool bValid, const FString &Error = TEXT(""), int32 Cost = 0)
		: bIsValid(bValid), ErrorMessage(Error), EnergyCost(Cost) {}
};

/**
 * FActionResult
 * Result of executing an action
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionResult
{
	GENERATED_BODY()

	/** Was execution successful? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bSuccess = false;

	/** What type of action was executed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	EActionType ActionType = EActionType::None;

	/** Who executed the action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	AActor *Executor = nullptr;

	/** Who was affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TArray<AActor *> AffectedTargets;

	/** Energy spent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 EnergySpent = 0;

	/** Total damage dealt (sum across all targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 TotalDamageDealt = 0;

	/** Total healing done (sum across all targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 TotalHealingDone = 0;

	/** Was any hit a critical? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bWasCritical = false;

	/** Did any target die from this action? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bCausedDeath = false;

	/** Status effects applied to targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 StatusEffectsApplied = 0;

	/** Error message if failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FString ErrorMessage;

	// ==================== INFUSION TRACKING ====================

	/** Was any infusion used? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	bool bWasInfused = false;

	/** Which infusion type was used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	EInfusionType InfusionTypeUsed = EInfusionType::None;

	/** What charge level was used (1 or 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	int32 InfusionLevelUsed = 0;

	/** Where did the element come from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	EInfusionSource InfusionSourceUsed = EInfusionSource::None;

	/** Status buildup applied to targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	int32 StatusBuildupApplied = 0;

	/** HP damage taken by caster (L2 cost) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	int32 SelfDamageTaken = 0;

	/** Self-status buildup applied to caster (Evolution L2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	int32 SelfStatusBuildupApplied = 0;

	/** Break chance increase applied (Ring/Crystal L2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Infusion")
	float BreakChanceIncrease = 0.0f;

	// ==================== DEFENSE SYSTEM DATA ====================

	/**
	 * Final spell/attack size for defense calculations
	 * Used by DefenseSystem to determine if dodge is viable
	 * Size > DodgeThreshold means dodge fails, must Block/Parry
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	float AttackSize = 0.0f;

	/** Base damage before defense applied (for defense calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	int32 BaseDamageBeforeDefense = 0;

	/** Element of the attack (for resistance calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	ESpellElement AttackElement = ESpellElement::Generic;

	/** Is this attack elemental? (affects defense type effectiveness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	bool bIsElementalAttack = false;

	// Per-target damage breakdown (for UI/logging)
	TMap<AActor *, int32> DamagePerTarget;
};

/**
 * FCombatHitResult
 * Result of a single hit on a single target
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatHitResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	AActor *Target = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 DamageDealt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 HealingDone = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasCritical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasBlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasParried = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasDodged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bTargetDied = false;
};