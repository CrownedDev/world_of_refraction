// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EActionType.h"
#include "ESpellElement.h"
#include "EInfusionType.h"
#include "EInfusionSource.h"
#include "ESpellSource.h"
#include "EInfusionSourceOption.h"
#include "EChargeInfusionType.h"
#include "ActionStructs.generated.h"

class USpellData;
class UAbilityData;
class UItemData;
class UWeaponAttackData;

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

	/** Source of spell (Innate, Ring, Evolution, Item) - determines post-cast logic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Spell")
	ESpellSource SpellSource = ESpellSource::Innate;

	/** Attack data (if ActionType == Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UWeaponAttackData *AttackData = nullptr;

	// ==================== INFUSION OPTIONS ====================

	/** Selected infusion source - determines element and weapon stat trade-off
	 *  Set by player via Combat Menu BEFORE charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;

	/** Spell charge infusion level (0 = none, 1 = status boost, 2 = damage boost)
	 *  L1: 1.5x size, +50% status buildup, base damage
	 *  L2: 2.0x size, base status, +30% damage
	 *  Set by hold-to-charge for SPELLS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpellInfusionLevel = 0;

	/** Ability charge infusion level (0 = none, 1 = status boost, 2 = damage boost)
	 *  L1: +status buildup from SelectedSource (physical or element)
	 *  L2: +30% damage, no status
	 *  Set by hold-to-charge for ABILITIES */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 AbilityInfusionLevel = 0;

	// ==================== DEPRECATED - REMOVE AFTER MIGRATION ====================

	/** @deprecated Use SpellInfusionLevel or AbilityInfusionLevel instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use SpellInfusionLevel or AbilityInfusionLevel"))
	int32 InfusionLevel = 0;

	/** @deprecated Replaced by AbilityInfusionLevel + SelectedSource */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use AbilityInfusionLevel + SelectedSource"))
	bool bIsElementInfused = false;

	/** @deprecated Use SpellInfusionLevel instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use SpellInfusionLevel"))
	int32 SpellSizeInfusionLevel = 0;

	/** @deprecated Replaced by SelectedSource */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (DeprecatedProperty, DeprecationMessage = "Use SelectedSource"))
	EInfusionType InfusionType = EInfusionType::None;

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

	/** Is spell charge infusion active? */
	bool IsSpellInfused() const
	{
		return SpellInfusionLevel > 0;
	}

	/** Is ability charge infusion active? */
	bool IsAbilityInfused() const
	{
		return AbilityInfusionLevel > 0;
	}

	/** Get the active charge level for current action type */
	int32 GetChargeLevel() const
	{
		if (ActionType == EActionType::Spell)
		{
			return SpellInfusionLevel;
		}
		else if (ActionType == EActionType::Ability)
		{
			return AbilityInfusionLevel;
		}
		return 0;
	}

	/** Is using physical source (weapon stats apply)? */
	bool IsPhysicalSource() const
	{
		return SelectedSource == EInfusionSourceOption::None;
	}

	/** Is using elemental source (weapon stats don't apply)? */
	bool IsElementalSource() const
	{
		return SelectedSource != EInfusionSourceOption::None;
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

	/** Which source option was selected */
	UPROPERTY(BlueprintReadOnly, Category = "Result|Infusion")
	EInfusionSourceOption SourceOptionUsed = EInfusionSourceOption::None;

	/** Whether weapon stats were applied */
	UPROPERTY(BlueprintReadOnly, Category = "Result|Infusion")
	bool bWeaponStatsApplied = false;

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
// ==================== ASYNC EXECUTION CONTEXT ====================

/**
 * FPendingDefenseContext
 * Tracks a single target's pending defense resolution during async action execution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPendingDefenseContext
{
	GENERATED_BODY()

	/** Target awaiting defense resolution */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Target;

	/** Attacker who initiated the attack */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Attacker;

	/** Base damage before defense */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 BaseDamage = 0;

	/** Attack size for dodge calculations */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float AttackSize = 1.0f;

	/** Is this an elemental attack? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bIsElemental = false;

	/** Element of the attack */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	ESpellElement Element = ESpellElement::Generic;

	/** Number of hits to apply */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 HitCount = 1;

	/** Can this attack crit? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bCanCrit = true;

	/** Damage per hit (for multi-hit attacks) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 DamagePerHit = 0;

	/** Window duration from attack data */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float WindowDuration = 0.3f;

	/** Unique ID for this defense context */
	UPROPERTY(BlueprintReadOnly, Category = "Defense", Meta = (IgnoreForMemberInitializationTest))
	FGuid ContextId;

	FPendingDefenseContext()
	{
		ContextId = FGuid::NewGuid();
	}
};

/**
 * FActionExecutionContext
 * Tracks an entire async action execution across multiple targets
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionExecutionContext
{
	GENERATED_BODY()

	/** Unique ID for this action execution */
	UPROPERTY(BlueprintReadOnly, Category = "Execution", Meta = (IgnoreForMemberInitializationTest))
	FGuid ExecutionId;

	/** The action being executed */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	FAction Action;

	/** Actor executing the action */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	TWeakObjectPtr<AActor> Executor;

	/** Running result (updated as defenses resolve) */
	FActionResult PartialResult;

	/** Pending defense contexts per target */
	TMap<TWeakObjectPtr<AActor>, FPendingDefenseContext> PendingDefenses;

	/** Is execution still in progress? */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	bool bInProgress = false;

	/** Maximum time to wait for all defenses (prevents soft-lock) */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	float TimeoutDuration = 10.0f;

	/** Time execution started */
	double StartTime = 0.0;

	FActionExecutionContext()
	{
		ExecutionId = FGuid::NewGuid();
	}

	/** Check if all defenses are resolved */
	bool AreAllDefensesResolved() const
	{
		return PendingDefenses.Num() == 0;
	}

	/** Get number of pending defenses */
	int32 GetPendingCount() const
	{
		return PendingDefenses.Num();
	}
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