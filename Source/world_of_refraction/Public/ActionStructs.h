// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EActionType.h"
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
	TArray<AActor*> Targets;

	/** Spell data (if ActionType == Spell) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	USpellData* SpellData = nullptr;

	/** Ability data (if ActionType == Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UAbilityData* AbilityData = nullptr;

	/** Item data (if ActionType == Item) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UItemData* ItemData = nullptr;

	/** Attack data (if ActionType == Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UBaseAttackData* AttackData = nullptr;

	/** Ultimate data (if ActionType == Ultimate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UUltimateData* UltimateData = nullptr;

	// ==================== INFUSION OPTIONS ====================

	/** Is ability infused with element? (Casters only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	bool bIsElementInfused = false;

	/** Spell infusion level (0 = none, 1 = level 1, 2 = level 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	int32 SpellInfusionLevel = 0;

	/** Ability power infusion level (Generic only, 0-2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	int32 AbilityInfusionLevel = 0;

	/** Use elemental mode for toggle spells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	bool bUseElementalMode = true;

	// ==================== HELPERS ====================

	bool IsValid() const
	{
		if (ActionType == EActionType::None) return false;
		if (ActionType == EActionType::Spell && !SpellData) return false;
		if (ActionType == EActionType::Ability && !AbilityData) return false;
		if (ActionType == EActionType::Item && !ItemData) return false;
		if (ActionType == EActionType::Attack && !AttackData) return false;
		if (ActionType == EActionType::Ultimate && !UltimateData) return false;
		return true;
	}

	bool RequiresTarget() const
	{
		return ActionType != EActionType::Defend && 
			   ActionType != EActionType::Flee &&
			   ActionType != EActionType::SwitchWeapon;
	}

	FString GetActionName() const
	{
		switch (ActionType)
		{
		case EActionType::Spell:
			return SpellData ? SpellData->GetName() : TEXT("Unknown Spell");
		case EActionType::Ability:
			return AbilityData ? AbilityData->GetName() : TEXT("Unknown Ability");
		case EActionType::Item:
			return ItemData ? ItemData->GetName() : TEXT("Unknown Item");
		case EActionType::Attack:
			return AttackData ? AttackData->GetName() : TEXT("Basic Attack");
		case EActionType::Ultimate:
			return UltimateData ? UltimateData->GetName() : TEXT("Unknown Ultimate");
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
	FActionValidationResult(bool bValid, const FString& Error = TEXT(""), int32 Cost = 0)
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
	AActor* Executor = nullptr;

	/** Who was affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TArray<AActor*> AffectedTargets;

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

	/** Status effects applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 StatusEffectsApplied = 0;

	/** Error message if failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FString ErrorMessage;

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
	ERefractionElement AttackElement = ERefractionElement::None;

	/** Is this attack elemental? (affects defense type effectiveness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	bool bIsElementalAttack = false;

	// Per-target damage breakdown (for UI/logging)
	TMap<AActor*, int32> DamagePerTarget;
};

/**
 * FHitResult
 * Result of a single hit on a single target
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatHitResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	AActor* Target = nullptr;

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
