// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActionExecutor.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "WeaponAttackData.h"
#include "ESpellSource.h"
#include "CombatConstants.h"
#include "ItemExecutor.h"
#include "WeaponManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RingManager.h"
#include "WeaponData.h"
#include "ItemData.h"
#include "CrystalType.h"
#include "RingManager.h"
#include "WeaponData.h"
#include "ItemData.h"
#include "DefenseSystem.h"
#include "EDefenseType.h"
#include "BrokenDarknessManager.h"
#include "HybridSpellColors.h"
#include "SpellProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "EInfusionSourceOption.h"
#include "RingData.h"
#include "InfusionVFXComponent.h"
#include "LoadoutComponent.h"
#include "CombatMovementComponent.h"
#include "GameFramework/Character.h"

class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;
class USpellData;
class UAbilityData;
class UItemData;

	void UActionExecutor::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	BindDefenseSystemEvents();

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Initialized"));
}

void UActionExecutor::Deinitialize()
{
	StatusEffectManagerRef = nullptr;
	UnbindDefenseSystemEvents();
	CancelAsyncAction(); // Clean up any pending action

	Super::Deinitialize();
}

// ========================================
// INFUSION MULTIPLIERS (Static)
// ========================================

float UActionExecutor::GetSpellInfusionSizeMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return 1.5f; // Level 1: 50% size increase
	case 2:
		return 2.0f; // Level 2: 100% size increase
	default:
		return 1.0f; // No infusion
	}
}

float UActionExecutor::GetSpellInfusionCostMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return 1.3f; // Level 1: 30% cost increase
	case 2:
		return 1.6f; // Level 2: 60% cost increase
	default:
		return 1.0f; // No infusion
	}
}

// ========================================
// VALIDATION
// ========================================

FActionValidationResult UActionExecutor::ValidateAction(AActor *Actor, const FAction &Action) const
{
	if (!Actor)
	{
		return FActionValidationResult(false, TEXT("Invalid actor"));
	}

	if (!Action.IsValid())
	{
		return FActionValidationResult(false, TEXT("Invalid action data"));
	}

	// Check if actor can act (not stunned)
	if (!CanActorAct(Actor))
	{
		return FActionValidationResult(false, TEXT("Cannot act (Stunned)"));
	}

	// Check if actor can cast spells (not silenced)
	if (Action.ActionType == EActionType::Spell && !CanActorCastSpells(Actor))
	{
		return FActionValidationResult(false, TEXT("Cannot cast spells (Silenced)"));
	}

	// Check targets
	if (Action.RequiresTarget() && Action.Targets.Num() == 0)
	{
		return FActionValidationResult(false, TEXT("No targets selected"));
	}

	// Validate targets are alive
	for (AActor *Target : Action.Targets)
	{
		if (!IsTargetAlive(Target))
		{
			return FActionValidationResult(false, TEXT("Target is dead"));
		}
	}

	// Calculate energy cost
	int32 EnergyCost = CalculateActionEnergyCost(Actor, Action);

	// Check energy
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);
	if (CharComp && CharComp->CurrentEP < EnergyCost)
	{
		return FActionValidationResult(false, TEXT("Not enough energy"), EnergyCost);
	}

	// Check requirements (world stat requirements)
	UCharacterData *CharData = GetCharacterData(Actor);
	if (CharData)
	{
		switch (Action.ActionType)
		{
		case EActionType::Spell:
			if (Action.SpellData && !Action.SpellData->MeetsRequirements(CharData))
			{
				// Allow with penalty, but could warn
				// return FActionValidationResult(false, TEXT("Requirements not met"));
			}
			// Check element restriction
			if (Action.SpellData && !Action.SpellData->CanCharacterCast(CharData))
			{
				return FActionValidationResult(false, TEXT("Element restricted"));
			}
			break;

		case EActionType::Ability:
			if (Action.AbilityData && !Action.AbilityData->MeetsRequirements(CharData))
			{
				// Allow with penalty
			}
			break;

		default:
			break;
		}
	}

	return FActionValidationResult(true, TEXT(""), EnergyCost);
}

bool UActionExecutor::CanActorAct(AActor *Actor) const
{
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager && StatusManager->IsStunned(Actor))
	{
		return false;
	}
	return true;
}

bool UActionExecutor::CanActorCastSpells(AActor *Actor) const
{
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager && StatusManager->IsSilenced(Actor))
	{
		return false;
	}
	return true;
}

int32 UActionExecutor::CalculateActionEnergyCost(AActor *Actor, const FAction &Action) const
{
	UCharacterData *CharData = GetCharacterData(Actor);

	switch (Action.ActionType)
	{
	case EActionType::Spell:
		if (Action.SpellData)
		{
			int32 BaseCost = Action.SpellData->CalculateEnergyCost(CharData);
			// Spell infusion: 1.0x / 1.3x / 1.6x cost
			float CostMultiplier = GetSpellInfusionCostMultiplier(Action.SpellInfusionLevel);
			return FMath::RoundToInt(BaseCost * CostMultiplier);
		}
		break;

	case EActionType::Ability:
		if (Action.AbilityData)
		{
			int32 BaseCost = Action.AbilityData->CalculateEnergyCost(CharData, Action.bIsElementInfused);
			return BaseCost;
		}
		break;

	case EActionType::Item:
		// Items typically don't cost energy
		return 0;

	case EActionType::Attack:
		if (Action.AttackData && Action.bIsElementInfused)
		{
			// Infused attacks cost energy
			return 5; // TODO: Get from constants or attack data
		}
		return 0;

	case EActionType::Defend:
		return 0;

	default:
		break;
	}

	return 0;
}

// ========================================
// EXECUTION - MAIN ENTRY POINT
// ========================================

FActionResult UActionExecutor::ExecuteAction(AActor *Actor, const FAction &Action)
{
	FActionResult Result;
	Result.Executor = Actor;
	Result.ActionType = Action.ActionType;

	// Validate first
	FActionValidationResult Validation = ValidateAction(Actor, Action);
	if (!Validation.bIsValid)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = Validation.ErrorMessage;
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Action validation failed: %s"), *Validation.ErrorMessage);
		return Result;
	}

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s executing %s (Cost: %d EP)"),
		   *Actor->GetName(), *Action.GetActionName(), Validation.EnergyCost);

	// Check for Broken Darkness break triggers BEFORE executing
	UCharacterData *CharData = GetCharacterData(Actor);
	CheckBrokenDarknessBreak(Actor, Action, CharData);

	// Activate infusion VFX if applicable
	int32 MaxInfusionLevel = FMath::Max(Action.SpellInfusionLevel, Action.AbilityInfusionLevel);
	if (MaxInfusionLevel > 0)
	{
		if (UInfusionVFXComponent *InfusionVFX = Actor->FindComponentByClass<UInfusionVFXComponent>())
		{
			InfusionVFX->SetInfusionLevel(MaxInfusionLevel);
		}
	}
	// Route to appropriate executor (existing code)
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		Result = ExecuteSpell(Actor, Action.SpellData, Action.Targets,
							  Action.SpellInfusionLevel);

		// Post-cast processing (ring break checks, item consumption)
		if (Result.bSuccess)
		{
			bool bWasInfused = Action.SpellInfusionLevel > 0 || Action.SpellSizeInfusionLevel > 0;
			ProcessPostCastBySource(Actor, Action.SpellData, Action.SpellSource, bWasInfused);
		}
		break;

	case EActionType::Ability:
		Result = ExecuteAbility(Actor, Action.AbilityData, Action.Targets,
								Action.AbilityInfusionLevel, Action.SelectedSource);
		break;

	case EActionType::Item:
		Result = ExecuteItem(Actor, Action.ItemData, Action.Targets);
		break;

	case EActionType::Attack:
		Result = ExecuteAttack(Actor, Action.AttackData, Action.Targets, Action.bIsElementInfused);
		break;

	case EActionType::Defend:
		Result = ExecuteDefend(Actor);
		break;

	default:
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Unhandled action type");
		break;
	}

	// Broadcast completion
	OnActionCompleted.Broadcast(Actor, Result);

	return Result;
}

// ========================================
// EXECUTION - ASYNC
// ========================================
void UActionExecutor::ExecuteActionAsync(AActor *Actor, const FAction &Action, FOnActionComplete OnComplete)
{
	// Check for existing async action
	if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Cannot start async action - another in progress"));

		FActionResult FailResult;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = TEXT("Another async action in progress");
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return;
	}
	// Validate action
	FActionValidationResult Validation = ValidateAction(Actor, Action);
	if (!Validation.bIsValid)
	{
		FActionResult FailResult;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = Validation.ErrorMessage;

		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Async action validation failed: %s"),
			   *Validation.ErrorMessage);

		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return;
	}

	// Create execution context
	FActionExecutionContext Context;
	Context.Action = Action;
	Context.Executor = Actor;
	Context.bInProgress = true;
	Context.StartTime = FPlatformTime::Seconds();

	// Initialize partial result
	Context.PartialResult.Executor = Actor;
	Context.PartialResult.ActionType = Action.ActionType;
	Context.PartialResult.bSuccess = true;

	// Store context and callback
	CurrentExecutionContext = Context;
	AsyncActionCallback = OnComplete;

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Starting async action: %s by %s"),
		   *Action.GetActionName(), *Actor->GetName());

	// Get character data for calculations
	UCharacterData *CharData = GetCharacterData(Actor);

	// === BROKEN DARKNESS & INFUSION HOOKS ===
	// Check for Broken Darkness break triggers
	CheckBrokenDarknessBreak(Actor, Action, CharData);

	// Start approach movement (if applicable)
	AActor *PrimaryTarget = Action.Targets.Num() > 0 ? Action.Targets[0] : nullptr;
	UApproachData *ApproachData = GetApproachData(Action);
	float ExecutionRange = GetExecutionRange(Action);

	if (UCombatMovementComponent *Movement = GetMovementComponent(Actor))
	{
		Movement->StartApproach(PrimaryTarget, ApproachData, ExecutionRange, CachedArenaCenter);
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Started %s approach for %s"),
			   ApproachData ? *ApproachData->ApproachName : TEXT("Ranged"),
			   *Actor->GetName());
	}

	// Process based on action type
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		ExecuteSpellAsync(Actor, Action, CharData);
		break;

	case EActionType::Ability:
		ExecuteAbilityAsync(Actor, Action, CharData);
		break;

	case EActionType::Attack:
		ExecuteAttackAsync(Actor, Action, CharData);
		break;

	case EActionType::Item:
	case EActionType::Defend:
	case EActionType::SwitchWeapon:
	case EActionType::Flee:
		// These don't have defense windows - execute synchronously
		{
			FActionResult Result = ExecuteAction(Actor, Action);
			CurrentExecutionContext.Reset();
			if (OnComplete.IsBound())
			{
				OnComplete.Execute(Result);
			}
		}
		return;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Unknown action type for async execution"));
		CancelAsyncAction();
		return;
	}

	// Set timeout timer as failsafe
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AsyncTimeoutHandle,
			this,
			&UActionExecutor::OnAsyncActionTimeout,
			CurrentExecutionContext->TimeoutDuration,
			false);
	}

	// Check if any defense windows were opened
	if (!CurrentExecutionContext.IsSet() || CurrentExecutionContext->AreAllDefensesResolved())
	{
		// No defense windows needed - finalize immediately
		FinalizeAsyncAction();
	}
}

void UActionExecutor::ExecuteSpellAsync(AActor *Caster, const FAction &Action, UCharacterData *CasterData)
{
	USpellData *Spell = Action.SpellData;
	if (!Spell || !CasterData)
	{
		CancelAsyncAction();
		return;
	}

	UCharacterDataComponent *CasterComp = GetCharacterDataComponent(Caster);
	if (!CasterComp)
	{
		CancelAsyncAction();
		return;
	}

	// Calculate and spend energy
	int32 BaseEnergyCost = Spell->CalculateEnergyCost(CasterData);
	float CostMultiplier = GetSpellInfusionCostMultiplier(Action.SpellInfusionLevel);
	int32 FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * CostMultiplier);

	if (!SpendEnergy(Caster, FinalEnergyCost))
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Failed to spend energy");
		FinalizeAsyncAction();
		return;
	}
	CurrentExecutionContext->PartialResult.EnergySpent = FinalEnergyCost;

	// Spell size for VFX (BaseSize from SpellData, scaled by infusion)
	float FinalSpellSize = Spell->BaseSize * GetSpellInfusionSizeMultiplier(Action.SpellInfusionLevel);

	// Calculate damage with charge infusion multiplier
	// L1 = base damage (status boost instead), L2 = +30% damage
	int32 BaseDamage = Spell->CalculateDamage(CasterData);
	float DamageMultiplier = GetSpellChargeDamageMultiplier(Action.SpellInfusionLevel);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Track status multiplier for later application
	// L1 = +50% status, L2 = base status
	float StatusMultiplier = GetSpellChargeStatusMultiplier(Action.SpellInfusionLevel);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell charge L%d - Size: %.1fx, Damage: %d (%.1fx), Status: %.1fx"),
		   Action.SpellInfusionLevel,
		   GetSpellInfusionSizeMultiplier(Action.SpellInfusionLevel),
		   FinalDamage,
		   DamageMultiplier,
		   StatusMultiplier);

	// Store in result for reference
	CurrentExecutionContext->PartialResult.AttackSize = FinalSpellSize;
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = BaseDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Spell->Element;
	CurrentExecutionContext->PartialResult.bIsElementalAttack = true;

	// Play animation and VFX
	PlaySpellAnimation(Caster, Spell, FinalSpellSize);
	SpawnSpellVFX(Caster, Spell, FinalSpellSize);

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Calculate damage per hit
	int32 DamagePerHit = BaseDamage / FMath::Max(1, Spell->HitCount);

	// Check for forbidden element self-damage (BD casting Dark Light/Void)
	ProcessForbiddenElementCast(Caster, Spell->Element, static_cast<float>(BaseDamage));

	// Open defense windows for all targets (damage applied after defense resolves)
	OpenDefenseWindowsForTargets(
		Caster,
		ValidTargets,
		FinalSpellSize,
		BaseDamage,
		DamagePerHit,
		Spell->HitCount,
		true, // Spells are elemental
		Spell->Element,
		true, // Can crit
		0.3f  // Default window duration - TODO: get from spell data
	);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell async - opened %d defense windows (Size: %.1f, Damage: %d)"),
		   ValidTargets.Num(), FinalSpellSize, BaseDamage);
}

void UActionExecutor::ExecuteAbilityAsync(AActor *User, const FAction &Action, UCharacterData *UserData)
{
	UAbilityData *Ability = Action.AbilityData;
	if (!Ability || !UserData)
	{
		CancelAsyncAction();
		return;
	}

	UCharacterDataComponent *UserComp = GetCharacterDataComponent(User);
	if (!UserComp)
	{
		CancelAsyncAction();
		return;
	}

	// Calculate and spend energy
	int32 BaseEnergyCost = Ability->CalculateEnergyCost(UserData, Action.bIsElementInfused);
	int32 FinalEnergyCost = BaseEnergyCost;

	if (!SpendEnergy(User, FinalEnergyCost))
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Failed to spend energy");
		FinalizeAsyncAction();
		return;
	}
	CurrentExecutionContext->PartialResult.EnergySpent = FinalEnergyCost;

	// Calculate damage with power infusion
	float DamageMultiplier = UserData->CalculateRawDamageMultiplier();
	int32 BaseDamage = Ability->CalculateDamage(UserData, Action.bIsElementInfused);

	// Element handling
	ESpellElement Element = ESpellElement::Generic;
	bool bIsElemental = Action.bIsElementInfused;
	if (bIsElemental && UserData->HasInnateElement())
	{
		Element = UserData->InnateElement;
		BaseDamage = FMath::RoundToInt(BaseDamage * 0.7f); // 30% penalty for element
	}

	// Ability size (fixed, no character scaling)
	float AttackSize = 1.0f;

	// Store in result
	CurrentExecutionContext->PartialResult.AttackSize = AttackSize; // remove attack size its pointless for abilities
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = BaseDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Element;
	CurrentExecutionContext->PartialResult.bIsElementalAttack = bIsElemental;

	// Play animation
	PlayAbilityAnimation(User, Ability);

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Calculate damage per hit
	int32 DamagePerHit = BaseDamage / FMath::Max(1, Ability->HitCount);

	// Open defense windows
	OpenDefenseWindowsForTargets(
		User,
		ValidTargets,
		AttackSize,
		BaseDamage,
		DamagePerHit,
		Ability->HitCount,
		bIsElemental,
		Element,
		true, // Can crit
		0.3f);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Ability async - opened %d defense windows"),
		   ValidTargets.Num());
}

void UActionExecutor::ExecuteAttackAsync(AActor *Attacker, const FAction &Action, UCharacterData *AttackerData)
{
	UWeaponAttackData *Attack = Action.AttackData;

	// If no attack specified, try to get from weapon
	if (!Attack)
	{
		UWeaponManager *WeaponMgr = GetWeaponManager();
		if (WeaponMgr)
		{
			Attack = WeaponMgr->GetActiveAttack(Attacker);
		}
	}

	if (!Attack || !AttackerData)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No attack available");
		FinalizeAsyncAction();
		return;
	}

	// Calculate damage
	float DamageMultiplier = AttackerData->CalculateRawDamageMultiplier();
	int32 BaseDamage = FMath::RoundToInt(100.0f * DamageMultiplier);

	bool bIsInfused = Action.bIsElementInfused;
	if (bIsInfused)
	{
		BaseDamage = FMath::RoundToInt(BaseDamage * 0.7f); // 30% penalty

		// Spend energy for infused attack
		int32 InfusionCost = 5; // TODO: from constants
		SpendEnergy(Attacker, InfusionCost);
		CurrentExecutionContext->PartialResult.EnergySpent = InfusionCost;
	}

	// Element
	ESpellElement Element = bIsInfused ? AttackerData->InnateElement : ESpellElement::Generic;

	// Attack size
	float AttackSize = 1.5f; // TODO: get from attack data

	// Store in result
	CurrentExecutionContext->PartialResult.AttackSize = AttackSize;
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = BaseDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Element;
	CurrentExecutionContext->PartialResult.bIsElementalAttack = bIsInfused;

	// Play animation
	PlayAttackAnimation(Attacker, Attack);

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Calculate damage per hit
	int32 DamagePerHit = BaseDamage / FMath::Max(1, Attack->HitCount);

	// Open defense windows
	OpenDefenseWindowsForTargets(
		Attacker,
		ValidTargets,
		AttackSize,
		BaseDamage,
		DamagePerHit,
		Attack->HitCount,
		bIsInfused,
		Element,
		true,
		0.3f);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Attack async - opened %d defense windows"),
		   ValidTargets.Num());
}

// ========================================
// EXECUTION - SPELL
// ========================================

FActionResult UActionExecutor::ExecuteSpell(
	AActor *Caster,
	USpellData *Spell,
	const TArray<AActor *> &Targets,
	int32 InfusionLevel)
{
	FActionResult Result;
	Result.Executor = Caster;
	Result.ActionType = EActionType::Spell;

	if (!Caster || !Spell)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid caster or spell");
		return Result;
	}

	UCharacterData *CasterData = GetCharacterData(Caster);
	UCharacterDataComponent *CasterComp = GetCharacterDataComponent(Caster);

	if (!CasterData || !CasterComp)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Caster missing character data");
		return Result;
	}

	// Calculate energy cost with infusion multiplier
	int32 BaseEnergyCost = Spell->CalculateEnergyCost(CasterData);
	float CostMultiplier = GetSpellInfusionCostMultiplier(InfusionLevel);
	int32 FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * CostMultiplier);

	if (!SpendEnergy(Caster, FinalEnergyCost))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to spend energy");
		return Result;
	}
	Result.EnergySpent = FinalEnergyCost;

	// Spell size for VFX (BaseSize from SpellData, scaled by infusion)
	float FinalSpellSize = Spell->BaseSize * GetSpellInfusionSizeMultiplier(InfusionLevel);

	// Store size in result for defense system
	Result.AttackSize = FinalSpellSize;
	Result.AttackElement = Spell->Element;
	Result.bIsElementalAttack = true;

	// Calculate damage (NOT affected by spell infusion - that's Generic's thing)
	int32 BaseDamage = Spell->CalculateDamage(CasterData);
	Result.BaseDamageBeforeDefense = BaseDamage;

	// Play animation and VFX (stubs - override or bind events for real implementation)
	PlaySpellAnimation(Caster, Spell, FinalSpellSize);
	SpawnSpellVFX(Caster, Spell, FinalSpellSize);

	// Process each target
	TArray<AActor *> ValidTargets = FilterValidTargets(Targets);

	for (AActor *Target : ValidTargets)
	{
		// Broadcast defense window request
		// DefenseSystem should bind to this and handle Block/Parry/Dodge
		OnDefenseWindowRequested.Broadcast(Caster, Target, FinalSpellSize, BaseDamage);

		// For now, apply damage directly (defense system will intercept via events when implemented)
		// Multi-hit processing
		int32 TotalDamage = ProcessMultiHit(
			Caster, Target,
			BaseDamage / FMath::Max(1, Spell->HitCount),
			Spell->HitCount,
			true, // Spells are elemental
			Spell->Element,
			true, // Can crit
			Result);

		Result.TotalDamageDealt += TotalDamage;
		Result.DamagePerTarget.Add(Target, TotalDamage);
		Result.AffectedTargets.Add(Target);

		// Check for kills
		if (!IsTargetAlive(Target))
		{
			Result.bCausedDeath = true;
			OnTargetKilled.Broadcast(Caster, Target);
		}
	}

	// Apply status effects from spell
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager && Spell->PrimaryEffect != EAbilityEffectType::None)
	{
		for (AActor *Target : ValidTargets)
		{
			ApplyStatusEffects(
				Caster, Target,
				Spell->PrimaryEffect,
				Spell->PrimaryEffectMagnitude * 100.0f,
				Spell->PrimaryEffectDuration,
				Spell->SecondaryEffect,
				Spell->SecondaryEffectMagnitude * 100.0f,
				Spell->SecondaryEffectDuration,
				Spell->Element);
			Result.StatusEffectsApplied++;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s cast %s (Size: %.1f, Infusion: %d) - %d damage to %d targets"),
		   *Caster->GetName(), *Spell->GetName(), FinalSpellSize, InfusionLevel,
		   Result.TotalDamageDealt, ValidTargets.Num());

	return Result;
}

// ============================================================
// 6. DEFENSE WINDOW INTEGRATION
// ============================================================

void UActionExecutor::OpenDefenseWindowsForTargets(
	AActor *Attacker,
	const TArray<AActor *> &Targets,
	float AttackSize,
	int32 BaseDamage,
	int32 DamagePerHit,
	int32 HitCount,
	bool bIsElemental,
	ESpellElement Element,
	bool bCanCrit,
	float WindowDuration)
{
	if (!CurrentExecutionContext.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No execution context for defense windows"));
		return;
	}

	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (!DefenseSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] DefenseSystem not available - applying damage directly"));

		// Fallback: apply damage without defense
		for (AActor *Target : Targets)
		{
			int32 TotalDamage = ProcessMultiHit(
				Attacker, Target, DamagePerHit, HitCount, bIsElemental, Element, bCanCrit,
				CurrentExecutionContext->PartialResult);

			CurrentExecutionContext->PartialResult.TotalDamageDealt += TotalDamage;
			CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, TotalDamage);
			CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);
		}
		return;
	}

	// Create pending defense context for each target
	for (AActor *Target : Targets)
	{
		FPendingDefenseContext DefenseContext;
		DefenseContext.Attacker = Attacker;
		DefenseContext.Target = Target;
		DefenseContext.BaseDamage = BaseDamage;
		DefenseContext.DamagePerHit = DamagePerHit;
		DefenseContext.AttackSize = AttackSize;
		DefenseContext.bIsElemental = bIsElemental;
		DefenseContext.Element = Element;
		DefenseContext.HitCount = HitCount;
		DefenseContext.bCanCrit = bCanCrit;
		DefenseContext.WindowDuration = WindowDuration;

		CurrentExecutionContext->PendingDefenses.Add(Target, DefenseContext);

		// Open defense window in DefenseSystem
		DefenseSys->OpenDefenseWindow(
			Attacker,
			Target,
			AttackSize,
			BaseDamage,
			WindowDuration,
			bIsElemental);

		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Opened defense window for %s (Size: %.1f, Damage: %d)"),
			   *Target->GetName(), AttackSize, BaseDamage);
	}
}

void UActionExecutor::OnDefenseWindowClosed(AActor *Defender, const FDefenseResult &DefenseResult)
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Defense window closed but no async action in progress"));
		return;
	}

	// Find the pending defense context for this defender
	FPendingDefenseContext *ContextPtr = CurrentExecutionContext->PendingDefenses.Find(Defender);
	if (!ContextPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Defense resolved for unknown target: %s"),
			   *Defender->GetName());
		return;
	}

	FPendingDefenseContext Context = *ContextPtr;

	// Apply damage based on defense result
	ApplyDamageAfterDefense(
		Context.Attacker.Get(),
		Defender,
		Context,
		DefenseResult);
	// Broken Darkness absorption from defense
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Defender);
	if (BDManager && BDManager->IsTransformed())
	{
		// Get attack info from pending context
		if (CurrentExecutionContext.IsSet())
		{
			FPendingDefenseContext *BDContext = CurrentExecutionContext->PendingDefenses.Find(Defender);
			if (BDContext)
			{
				// Get spell/ability energy cost for absorption calculation
				float EnergyCost = 0.0f;
				if (CurrentExecutionContext->Action.SpellData)
				{
					EnergyCost = CurrentExecutionContext->Action.SpellData->EnergyCost;
				}
				else if (CurrentExecutionContext->Action.AbilityData)
				{
					EnergyCost = CurrentExecutionContext->Action.AbilityData->BaseEnergyCost;
				}

				BDManager->OnDefenseResolved(
					DefenseResult.DefenseType,
					DefenseResult,
					BDContext->Element,
					EnergyCost);
			}
		}
	}
	// Remove from pending list
	CurrentExecutionContext->PendingDefenses.Remove(Defender);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Defense resolved for %s - Type: %d, FinalDamage: %d, Pending: %d"),
		   *Defender->GetName(),
		   static_cast<int32>(DefenseResult.DefenseType),
		   DefenseResult.FinalDamage,
		   CurrentExecutionContext->GetPendingCount());

	// Check if all defenses resolved
	CheckAndFinalizeAsyncAction();
}

void UActionExecutor::ApplyDamageAfterDefense(
	AActor *Attacker,
	AActor *Target,
	const FPendingDefenseContext &Context,
	const FDefenseResult &DefenseResult)
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	int32 FinalDamage = 0;

	if (DefenseResult.bSuccess && DefenseResult.DefenseType == EDefenseType::Dodge)
	{
		// Dodge successful - no damage
		FinalDamage = 0;
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s dodged attack - 0 damage"), *Target->GetName());
	}
	else
	{
		// Apply damage based on defense result
		// DefenseResult.FinalDamage already has reduction applied
		int32 DamagePerHit = DefenseResult.FinalDamage / FMath::Max(1, Context.HitCount);

		FinalDamage = ProcessMultiHit(
			Attacker,
			Target,
			DamagePerHit,
			Context.HitCount,
			Context.bIsElemental,
			Context.Element,
			Context.bCanCrit,
			CurrentExecutionContext->PartialResult);
	}

	// Update result
	CurrentExecutionContext->PartialResult.TotalDamageDealt += FinalDamage;
	CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, FinalDamage);
	CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);

	// Check for kills
	if (!IsTargetAlive(Target))
	{
		CurrentExecutionContext->PartialResult.bCausedDeath = true;
		OnTargetKilled.Broadcast(Attacker, Target);
	}

	// Track defense type used
	if (DefenseResult.bSuccess)
	{
		FCombatHitResult HitResult;
		HitResult.Target = Target;
		HitResult.DamageDealt = FinalDamage;
		HitResult.bWasBlocked = (DefenseResult.DefenseType == EDefenseType::Block);
		HitResult.bWasParried = (DefenseResult.DefenseType == EDefenseType::Parry);
		HitResult.bWasDodged = (DefenseResult.DefenseType == EDefenseType::Dodge);
		// Could store these in PartialResult if needed
	}
}

// ============================================================
// 7. ASYNC FINALIZATION
// ============================================================

void UActionExecutor::CheckAndFinalizeAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	if (CurrentExecutionContext->AreAllDefensesResolved())
	{
		FinalizeAsyncAction();
	}
}

void UActionExecutor::FinalizeAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	// Clear timeout timer
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AsyncTimeoutHandle);
	}

	// Get final result
	FActionResult FinalResult = CurrentExecutionContext->PartialResult;
	FAction Action = CurrentExecutionContext->Action;
	AActor *Executor = CurrentExecutionContext->Executor.Get();

	// Apply post-action effects (status effects, etc.)
	if (FinalResult.bSuccess && Executor)
	{
		// Apply status effects from spell/ability
		UStatusEffectManager *StatusManager = GetStatusEffectManager();

		if (Action.ActionType == EActionType::Spell && Action.SpellData && StatusManager)
		{
			if (Action.SpellData->PrimaryEffect != EAbilityEffectType::None)
			{
				for (AActor *Target : FinalResult.AffectedTargets)
				{
					ApplyStatusEffects(
						Executor, Target,
						Action.SpellData->PrimaryEffect,
						Action.SpellData->PrimaryEffectMagnitude * 100.0f,
						Action.SpellData->PrimaryEffectDuration,
						Action.SpellData->SecondaryEffect,
						Action.SpellData->SecondaryEffectMagnitude * 100.0f,
						Action.SpellData->SecondaryEffectDuration,
						Action.SpellData->Element);
					FinalResult.StatusEffectsApplied++;
				}
			}
		}

		// Process post-cast by source (ring break checks, etc.)
		if (Action.ActionType == EActionType::Spell && Action.SpellData)
		{
			bool bWasInfused = Action.SpellInfusionLevel > 0 || Action.SpellSizeInfusionLevel > 0;
			ProcessPostCastBySource(Executor, Action.SpellData, Action.SpellSource, bWasInfused);
		}
	}

	// Mark complete
	CurrentExecutionContext->bInProgress = false;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Async action finalized - Success: %s, Damage: %d, Targets: %d"),
		   FinalResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		   FinalResult.TotalDamageDealt,
		   FinalResult.AffectedTargets.Num());

	// Fire callback
	if (AsyncActionCallback.IsBound())
	{
		AsyncActionCallback.Execute(FinalResult);
		AsyncActionCallback.Unbind();
	}

	// Signal movement component to start return
	if (Executor)
	{
		SignalActionComplete(Executor);
	}

	// Broadcast completion
	if (Executor)
	{
		OnAsyncActionCompleted.Broadcast(Executor, FinalResult);
		OnActionCompleted.Broadcast(Executor, FinalResult);
	}

	// Clear context
	CurrentExecutionContext.Reset();
}

void UActionExecutor::OnAsyncActionTimeout()
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Async action timed out with %d pending defenses"),
		   CurrentExecutionContext->GetPendingCount());

	// Apply full damage to any remaining targets
	for (auto &Pair : CurrentExecutionContext->PendingDefenses)
	{
		FPendingDefenseContext &Context = Pair.Value;

		// Create failed defense result (full damage)
		FDefenseResult FailedDefense;
		FailedDefense.bSuccess = false;
		FailedDefense.FinalDamage = Context.BaseDamage;

		ApplyDamageAfterDefense(
			Context.Attacker.Get(),
			Context.Target.Get(),
			Context,
			FailedDefense);
	}

	CurrentExecutionContext->PendingDefenses.Empty();
	FinalizeAsyncAction();
}

void UActionExecutor::CancelAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Cancelling async action"));

	// Clear timer
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AsyncTimeoutHandle);
	}

	// Close any open defense windows
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		for (auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Key.IsValid())
			{
				DefenseSys->CloseDefenseWindow(Pair.Key.Get());
			}
		}
	}

	// Mark failed
	CurrentExecutionContext->PartialResult.bSuccess = false;
	CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Action cancelled");

	FinalizeAsyncAction();
}

bool UActionExecutor::IsAsyncActionInProgress() const
{
	return CurrentExecutionContext.IsSet() && CurrentExecutionContext->bInProgress;
}

const FActionExecutionContext *UActionExecutor::GetCurrentExecutionContext() const
{
	return CurrentExecutionContext.IsSet() ? &CurrentExecutionContext.GetValue() : nullptr;
}

// ============================================================
// 8. DEFENSE SYSTEM BINDING
// ============================================================

UDefenseSystem *UActionExecutor::GetDefenseSystem() const
{
	if (DefenseSystemRef)
	{
		return DefenseSystemRef;
	}

	UGameInstance *GI = GetGameInstance();
	if (GI)
	{
		UDefenseSystem *DefenseSys = GI->GetSubsystem<UDefenseSystem>();
		const_cast<UActionExecutor *>(this)->DefenseSystemRef = DefenseSys;
		return DefenseSys;
	}

	return nullptr;
}

void UActionExecutor::BindDefenseSystemEvents()
{
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		// Dynamic delegates use AddDynamic macro
		DefenseSys->OnDefenseWindowClosed.AddDynamic(this, &UActionExecutor::OnDefenseWindowClosed);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Bound to DefenseSystem events"));
	}
}

void UActionExecutor::UnbindDefenseSystemEvents()
{
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		DefenseSys->OnDefenseWindowClosed.RemoveDynamic(this, &UActionExecutor::OnDefenseWindowClosed);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Unbound from DefenseSystem events"));
	}
}

// ========================================
// EXECUTION - ABILITY
// ========================================

FActionResult UActionExecutor::ExecuteAbility(
	AActor *User,
	UAbilityData *Ability,
	const TArray<AActor *> &Targets,
	int32 AbilityInfusionLevel,
	EInfusionSourceOption SelectedSource)
{
	FActionResult Result;
	Result.Executor = User;
	Result.ActionType = EActionType::Ability;

	if (!User || !Ability)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid user or ability");
		return Result;
	}

	UCharacterData *UserData = GetCharacterData(User);
	UCharacterDataComponent *UserComp = GetCharacterDataComponent(User);

	if (!UserData || !UserComp)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("User missing character data");
		return Result;
	}

	// Determine if using elemental source
	// If charging (L1/L2): use SelectedSource
	// If not charging (L0): fall back to toggle (passed via SelectedSource::Innate if toggled on)
	bool bIsElementInfused = (SelectedSource != EInfusionSourceOption::None);

	// Calculate energy cost
	int32 BaseEnergyCost = Ability->CalculateEnergyCost(UserData, bIsElementInfused);
	int32 FinalEnergyCost = BaseEnergyCost;

	if (!SpendEnergy(User, FinalEnergyCost))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to spend energy");
		return Result;
	}
	Result.EnergySpent = FinalEnergyCost;

	// Calculate base damage
	int32 BaseDamage = Ability->CalculateDamage(UserData, bIsElementInfused);

	// Apply charge infusion damage multiplier
	// L1 = base damage, L2 = +30% damage
	float DamageMultiplier = GetAbilityChargeDamageMultiplier(AbilityInfusionLevel);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Determine element
	ESpellElement Element = ESpellElement::Generic;
	if (bIsElementInfused && Ability->bCanBeInfused)
	{
		Element = GetElementForSourceOption(User, SelectedSource);
	}

	// Store defense info
	Result.AttackElement = Element;
	Result.bIsElementalAttack = bIsElementInfused;
	Result.BaseDamageBeforeDefense = FinalDamage;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Ability charge L%d - Source: %d, Damage: %d (%.1fx)"),
		   AbilityInfusionLevel, static_cast<int32>(SelectedSource), FinalDamage, DamageMultiplier);

	// Play animation
	PlayAbilityAnimation(User, Ability);

	// Process each target
	TArray<AActor *> ValidTargets = FilterValidTargets(Targets);

	// Apply charge infusion status buildup (L1 only)
	float StatusMultiplier = GetAbilityChargeStatusMultiplier(AbilityInfusionLevel);
	if (StatusMultiplier > 0.0f && ValidTargets.Num() > 0)
	{
		ApplyAbilityInfusionStatus(User, ValidTargets, SelectedSource, Ability->HitCount, StatusMultiplier);
	}

	for (AActor *Target : ValidTargets)
	{
		// Multi-hit processing
		int32 TotalDamage = ProcessMultiHit(
			User, Target,
			FinalDamage / FMath::Max(1, Ability->HitCount),
			Ability->HitCount,
			bIsElementInfused,
			Element,
			true,
			Result);

		Result.TotalDamageDealt += TotalDamage;
		Result.DamagePerTarget.Add(Target, TotalDamage);
		Result.AffectedTargets.Add(Target);

		if (!IsTargetAlive(Target))
		{
			Result.bCausedDeath = true;
			OnTargetKilled.Broadcast(User, Target);
		}
	}

	// Apply status effects from ability (existing system)
	if (Ability->EffectType != EAbilityEffectType::None)
	{
		for (AActor *Target : ValidTargets)
		{
			ApplyStatusEffects(
				User, Target,
				Ability->EffectType,
				Ability->EffectValue,
				Ability->EffectDuration,
				EAbilityEffectType::None, 0.0f, 0,
				Element);
			Result.StatusEffectsApplied++;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s used %s (L%d) - %d damage to %d targets"),
		   *User->GetName(), *Ability->GetName(),
		   AbilityInfusionLevel,
		   Result.TotalDamageDealt, ValidTargets.Num());
	return Result;
}

// ========================================
// EXECUTION - ITEM
// ========================================

FActionResult UActionExecutor::ExecuteItem(
	AActor *User,
	UItemData *Item,
	const TArray<AActor *> &Targets)
{
	FActionResult Result;
	Result.Executor = User;
	Result.ActionType = EActionType::Item;

	if (!User || !Item)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid user or item");
		return Result;
	}

	// === Find and use item slot in LoadoutComponent ===
	ULoadoutComponent *Loadout = GetLoadoutComponent(User);
	int32 ItemSlotIndex = -1;

	if (Loadout && Loadout->IsReadyForBattle())
	{
		// Find which slot contains this item
		TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
		for (int32 i = 0; i < UsableItems.Num(); ++i)
		{
			if (UsableItems[i].Crystal == Item)
			{
				ItemSlotIndex = i;
				break;
			}
		}

		if (ItemSlotIndex < 0)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("Item not in loadout or no uses remaining");
			return Result;
		}
	}

	// Delegate to ItemExecutor for full item handling
	UItemExecutor *ItemExec = GetItemExecutor();
	if (!ItemExec)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("ItemExecutor not available");
		return Result;
	}

	// Items don't cost energy
	Result.EnergySpent = 0;

	// Determine target (self if not specified)
	AActor *Target = Targets.Num() > 0 ? Targets[0] : User;

	// Execute through ItemExecutor
	if (Targets.Num() > 1)
	{
		FItemUseResult ItemResult = ItemExec->UseItemMultiTarget(User, Item, Targets);

		Result.bSuccess = ItemResult.bSuccess;
		Result.ErrorMessage = ItemResult.ErrorMessage;
		Result.TotalDamageDealt = ItemResult.DamageDealt;
		Result.TotalHealingDone = ItemResult.HealingDone;
		Result.StatusEffectsApplied = ItemResult.BuffsApplied + ItemResult.DebuffsRemoved;

		for (AActor *T : Targets)
		{
			Result.AffectedTargets.Add(T);
		}
	}
	else
	{
		FItemUseResult ItemResult = ItemExec->UseItem(User, Item, Target);

		Result.bSuccess = ItemResult.bSuccess;
		Result.ErrorMessage = ItemResult.ErrorMessage;
		Result.TotalDamageDealt = ItemResult.DamageDealt;
		Result.TotalHealingDone = ItemResult.HealingDone;
		Result.StatusEffectsApplied = ItemResult.BuffsApplied + ItemResult.DebuffsRemoved;
		Result.AffectedTargets.Add(Target);
	}

	// === Consume item use from loadout ===
	if (Result.bSuccess && Loadout && ItemSlotIndex >= 0)
	{
		Loadout->UseItem(ItemSlotIndex);
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Consumed 1 use from item slot %d"), ItemSlotIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s used item %s - delegated to ItemExecutor"),
		   *User->GetName(), *Item->GetFullItemName());

	return Result;
}

// ========================================
// EXECUTION - ATTACK
// ========================================

FActionResult UActionExecutor::ExecuteAttack(
	AActor *Attacker,
	UWeaponAttackData *Attack,
	const TArray<AActor *> &Targets,
	bool bIsInfused)
{
	FActionResult Result;
	Result.Executor = Attacker;
	Result.ActionType = EActionType::Attack;

	// If no explicit attack provided, delegate to WeaponManager for equipped weapon
	if (!Attack)
	{
		UWeaponManager *WeaponMgr = GetWeaponManager();
		if (WeaponMgr)
		{
			FWeaponAttackResult WeaponResult = WeaponMgr->ExecuteAttackWithInfusion(Attacker, Targets, bIsInfused);

			Result.bSuccess = WeaponResult.bSuccess;
			Result.ErrorMessage = WeaponResult.ErrorMessage;
			Result.TotalDamageDealt = WeaponResult.TotalDamageDealt;
			Result.EnergySpent = WeaponResult.EnergySpent;
			Result.bWasCritical = WeaponResult.bWasCritical;
			Result.bCausedDeath = WeaponResult.bCausedDeath;
			Result.bIsElementalAttack = WeaponResult.bWasInfused;
			Result.AttackElement = WeaponResult.InfusedElement;

			for (const auto &Pair : WeaponResult.DamagePerTarget)
			{
				Result.AffectedTargets.Add(Pair.Key);
				Result.DamagePerTarget.Add(Pair.Key, Pair.Value);
			}

			return Result;
		}
		else
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("No attack specified and WeaponManager unavailable");
			return Result;
		}
	}

	// Direct attack execution (explicit attack data provided)
	if (!Attacker)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid attacker");
		return Result;
	}

	UCharacterData *AttackerData = GetCharacterData(Attacker);
	if (!AttackerData)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Attacker missing character data");
		return Result;
	}

	// Infused attacks cost energy
	if (bIsInfused)
	{
		int32 EnergyCost = 5; // TODO: Get from constants
		if (!SpendEnergy(Attacker, EnergyCost))
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("Not enough energy for infused attack");
			return Result;
		}
		Result.EnergySpent = EnergyCost;
	}

	// Calculate damage - attacks use character's RawDamageMultiplier
	// Base damage is 100, scaled by the attack's damage distribution and character stats
	float DamageMultiplier = AttackerData->CalculateRawDamageMultiplier();
	int32 BaseDamage = FMath::RoundToInt(100.0f * DamageMultiplier);
	if (bIsInfused)
	{
		// Infusion damage penalty (30%)
		BaseDamage = FMath::RoundToInt(BaseDamage * 0.7f);
	}

	// Determine element
	ESpellElement Element = ESpellElement::Generic;
	if (bIsInfused)
	{
		Element = AttackerData->InnateElement;
	}

	// Store defense info
	Result.AttackElement = Element;
	Result.bIsElementalAttack = bIsInfused;
	Result.BaseDamageBeforeDefense = BaseDamage;

	// Play animation
	PlayAttackAnimation(Attacker, Attack);

	// Process each target
	TArray<AActor *> ValidTargets = FilterValidTargets(Targets);
	for (AActor *Target : ValidTargets)
	{
		int32 TotalDamage = ProcessMultiHit(
			Attacker, Target,
			BaseDamage / FMath::Max(1, Attack->HitCount),
			Attack->HitCount,
			bIsInfused,
			Element,
			true,
			Result);

		Result.TotalDamageDealt += TotalDamage;
		Result.DamagePerTarget.Add(Target, TotalDamage);
		Result.AffectedTargets.Add(Target);

		if (!IsTargetAlive(Target))
		{
			Result.bCausedDeath = true;
			OnTargetKilled.Broadcast(Attacker, Target);
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s attacked%s - %d damage"),
		   *Attacker->GetName(),
		   bIsInfused ? TEXT(" (Infused)") : TEXT(""),
		   Result.TotalDamageDealt);

	return Result;
}

// ========================================
// EXECUTION - DEFEND
// ========================================

FActionResult UActionExecutor::ExecuteDefend(AActor *Defender)
{
	FActionResult Result;
	Result.Executor = Defender;
	Result.ActionType = EActionType::Defend;
	Result.EnergySpent = 0;

	if (!Defender)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid defender");
		return Result;
	}

	// Apply defense buff
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		FStatusEffect DefendBuff = FStatusEffect::CreateBuff(
			TEXT("Defending"),
			9999, // Special ID for defend
			EAbilityEffectType::DefenseBuff,
			50.0f, // 50% defense boost
			1);	   // Lasts until next turn

		StatusManager->ApplyEffect(Defender, DefendBuff, Defender, TEXT("Defend"), -1);
		Result.StatusEffectsApplied = 1;
	}

	Result.AffectedTargets.Add(Defender);
	Result.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s is defending"), *Defender->GetName());

	return Result;
}
// ========================================
// POST-CAST PROCESSING
// ========================================

void UActionExecutor::ProcessPostCastBySource(AActor *Caster, USpellData *Spell, ESpellSource Source, bool bWasInfused)
{
	if (!Caster || !Spell)
		return;

	switch (Source)
	{
	case ESpellSource::Innate:
		// Caster's natural spells - no risk
		break;

	case ESpellSource::Ring:
		// Break check for Resonators
		if (URingManager *RingMgr = GetRingManager())
		{
			bool bBroke = RingMgr->ProcessPostCastBreakCheck(Caster, Spell, bWasInfused);
			if (bBroke)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Ring broke after casting %s"), *Spell->SpellName);
			}
		}
		break;

	case ESpellSource::Evolution:
		// TODO: Evolution crystal logic
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Evolution spell cast - no post-cast logic yet"));
		break;

	case ESpellSource::Item:
		// TODO: Consume spell item from inventory
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell item used - consumption not yet implemented"));
		break;
	}
}

// ========================================
// DAMAGE APPLICATION
// ========================================
FCombatHitResult UActionExecutor::ApplyDamage(
	AActor *Attacker,
	AActor *Target,
	int32 BaseDamage,
	bool bIsElemental,
	ESpellElement Element,
	bool bCanCrit)
{
	FCombatHitResult Result;
	Result.Target = Target;

	if (!Target)
	{
		return Result;
	}

	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		return Result;
	}

	// Use centralized DamageCalculator
	UDamageCalculator *DamageCalc = GetDamageCalculator();
	if (DamageCalc)
	{
		FDamageCalculationInput Input;
		Input.BaseDamage = BaseDamage;
		Input.bIsElemental = bIsElemental;
		Input.Element = Element;
		Input.bCanCrit = bCanCrit;

		FDamageCalculationResult CalcResult = DamageCalc->CalculateDamage(Attacker, Target, Input);

		Result.bWasCritical = CalcResult.bWasCritical;

		// Apply the calculated damage
		int32 HPBefore = TargetComp->CurrentHP;
		TargetComp->ServerTakeDamage(CalcResult.FinalDamage);
		Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
	}
	else
	{
		// Fallback if DamageCalculator unavailable
		int32 HPBefore = TargetComp->CurrentHP;
		TargetComp->ServerTakeDamage(FMath::Max(1, BaseDamage));
		Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
	}

	// Check for death
	if (!TargetComp->bIsAlive)
	{
		Result.bTargetDied = true;
	}

	// Broadcast damage event
	OnDamageDealt.Broadcast(Attacker, Target, Result.DamageDealt, Result.bWasCritical);

	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] %s dealt %d damage to %s%s"),
		   Attacker ? *Attacker->GetName() : TEXT("Unknown"),
		   Result.DamageDealt,
		   *Target->GetName(),
		   Result.bWasCritical ? TEXT(" (CRIT)") : TEXT(""));

	return Result;
}

FCombatHitResult UActionExecutor::ApplyHealing(
	AActor *Healer,
	AActor *Target,
	int32 BaseHealing)
{
	FCombatHitResult Result;
	Result.Target = Target;

	if (!Target)
	{
		return Result;
	}

	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		return Result;
	}

	// Use centralized DamageCalculator for healing
	int32 FinalHealing = BaseHealing;
	UDamageCalculator *DamageCalc = GetDamageCalculator();
	if (DamageCalc)
	{
		FinalHealing = DamageCalc->CalculateHealing(Healer, Target, BaseHealing);
	}

	// Apply healing
	int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerHeal(FinalHealing);
	Result.HealingDone = TargetComp->CurrentHP - HPBefore;

	// Broadcast healing event
	OnHealingDone.Broadcast(Healer, Target, Result.HealingDone);

	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] %s healed %s for %d"),
		   Healer ? *Healer->GetName() : TEXT("Unknown"),
		   *Target->GetName(),
		   Result.HealingDone);

	return Result;
}

// ========================================
// UTILITY
// ========================================

UStatusEffectManager *UActionExecutor::GetStatusEffectManager() const
{
	if (!StatusEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->StatusEffectManagerRef =
				GI->GetSubsystem<UStatusEffectManager>();
		}
	}
	return StatusEffectManagerRef;
}

UDamageCalculator *UActionExecutor::GetDamageCalculator() const
{
	if (UGameInstance *GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDamageCalculator>();
	}
	return nullptr;
}

bool UActionExecutor::IsTargetAlive(AActor *Target) const
{
	if (!Target)
		return false;

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Target);
	return Comp && Comp->bIsAlive;
}

TArray<AActor *> UActionExecutor::FilterValidTargets(const TArray<AActor *> &Targets) const
{
	TArray<AActor *> ValidTargets;
	for (AActor *Target : Targets)
	{
		if (IsTargetAlive(Target))
		{
			ValidTargets.Add(Target);
		}
	}
	return ValidTargets;
}

// ========================================
// INTERNAL HELPERS
// ========================================

UCharacterDataComponent *UActionExecutor::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

UCharacterData *UActionExecutor::GetCharacterData(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	return Comp ? Comp->CharacterData : nullptr;
}

bool UActionExecutor::RollCriticalHit(AActor *Attacker) const
{
	UCharacterData *Data = GetCharacterData(Attacker);
	if (!Data)
		return false;

	float CritChance = Data->CalculateCritChance() * 100.0f; // Returns 0-1, need 0-100

	// Add crit chance buffs from status effects
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		CritChance += StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceBuff);
		CritChance -= StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceDebuff);
	}

	CritChance = FMath::Clamp(CritChance, 0.0f, 100.0f);

	return FMath::FRand() * 100.0f < CritChance;
}

void UActionExecutor::ApplyStatusEffects(
	AActor *Source,
	AActor *Target,
	EAbilityEffectType PrimaryEffect,
	float PrimaryValue,
	int32 PrimaryDuration,
	EAbilityEffectType SecondaryEffect,
	float SecondaryValue,
	int32 SecondaryDuration,
	ESpellElement Element)
{
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
		return;

	// Apply primary effect
	if (PrimaryEffect != EAbilityEffectType::None && PrimaryDuration > 0)
	{
		FStatusEffect Primary = FStatusEffect::CreateBuff(
			TEXT("Primary Effect"),
			FMath::Rand(),
			PrimaryEffect,
			PrimaryValue,
			PrimaryDuration);
		Primary.Element = Element;

		StatusManager->ApplyEffect(Target, Primary, Source, TEXT("Action"), -1);
	}

	// Apply secondary effect
	if (SecondaryEffect != EAbilityEffectType::None && SecondaryDuration > 0)
	{
		FStatusEffect Secondary = FStatusEffect::CreateBuff(
			TEXT("Secondary Effect"),
			FMath::Rand(),
			SecondaryEffect,
			SecondaryValue,
			SecondaryDuration);
		Secondary.Element = Element;

		StatusManager->ApplyEffect(Target, Secondary, Source, TEXT("Action"), -1);
	}
}

int32 UActionExecutor::ProcessMultiHit(
	AActor *Attacker,
	AActor *Target,
	int32 DamagePerHit,
	int32 HitCount,
	bool bIsElemental,
	ESpellElement Element,
	bool bCanCrit,
	FActionResult &OutResult)
{
	int32 TotalDamage = 0;

	for (int32 i = 0; i < HitCount; i++)
	{
		// Each hit can independently crit
		FCombatHitResult HitResult = ApplyDamage(
			Attacker, Target, DamagePerHit, bIsElemental, Element, bCanCrit);

		TotalDamage += HitResult.DamageDealt;

		if (HitResult.bWasCritical)
		{
			OutResult.bWasCritical = true;
		}

		// Stop if target died
		if (HitResult.bTargetDied)
		{
			break;
		}
	}

	return TotalDamage;
}

bool UActionExecutor::SpendEnergy(AActor *Actor, int32 Amount)
{
	if (Amount <= 0)
		return true;

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	if (!Comp)
		return false;

	if (Comp->CurrentEP < Amount)
		return false;

	Comp->ServerSpendEnergy(Amount);
	return true;
}

// ========================================
// ANIMATION/VFX STUBS
// ========================================

void UActionExecutor::PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize)
{
	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlaySpellAnimation - Invalid caster or spell"));
		return;
	}

	if (!Spell->CastAnimation)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] PlaySpellAnimation - No CastAnimation on %s"),
			   *Spell->SpellName);
		return;
	}

	ACharacter *Character = Cast<ACharacter>(Caster);
	if (Character)
	{
		float Duration = Character->PlayAnimMontage(Spell->CastAnimation, 1.0f);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing spell cast montage %s (%.2fs) for %s"),
			   *Spell->CastAnimation->GetName(),
			   Duration,
			   *Spell->SpellName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlaySpellAnimation - Caster %s is not a Character"),
			   *Caster->GetName());
	}
}

void UActionExecutor::SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize)
{
	// This is now a wrapper that calls SpawnSpellDelivery
	// Called from ExecuteSpell after damage calculation

	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnSpellVFX - Invalid caster or spell"));
		return;
	}

	// Get BD status for coloring
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	bool bIsBD = BDManager && BDManager->IsTransformed();

	// Calculate final values
	float FinalImpactRadius = Spell->BaseSize * Spell->HitboxRatio * SpellSize;
	float FinalVisualScale = Spell->BaseSize * SpellSize;

	// Get targets from current execution context
	TArray<AActor *> Targets;
	if (CurrentExecutionContext.IsSet())
	{
		for (const auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Value.Target.IsValid())
			{
				Targets.Add(Pair.Value.Target.Get());
			}
		}
	}

	// Get damage from context
	int32 FinalDamage = CurrentExecutionContext.IsSet() ? CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense : 0;

	SpawnSpellDelivery(Caster, Targets, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBD);
}

void UActionExecutor::SpawnSpellDelivery(
	AActor *Caster,
	const TArray<AActor *> &Targets,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness)
{
	if (!Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnSpellDelivery - No spell data"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpawnSpellDelivery - Type=%d, Targets=%d, Radius=%.2f"),
		   (int32)Spell->DeliveryType, Targets.Num(), FinalImpactRadius);
	// Check if offensive or supportive spell
	bool bIsOffensive = (Spell->TargetType == ETargetType::SingleEnemy ||
						 Spell->TargetType == ETargetType::AllEnemies);

	if (!bIsOffensive)
	{
		// Self/Ally spells - spawn VFX, apply healing/buff directly (no defense)
		SpawnSupportSpellEffect(Caster, Targets, Spell, FinalVisualScale, bIsBrokenDarkness);
		return;
	}

	switch (Spell->DeliveryType)
	{
	case ESpellDeliveryType::Projectile:
	case ESpellDeliveryType::Homing:
	case ESpellDeliveryType::Beam:
		// Spawn projectile actor for each target
		for (AActor *Target : Targets)
		{
			SpawnProjectileActor(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBrokenDarkness);
		}
		break;

	case ESpellDeliveryType::AOE:
		// Spawn VFX at each target, open defense window immediately
		for (AActor *Target : Targets)
		{
			SpawnAOEEffect(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBrokenDarkness);
		}
		break;

	case ESpellDeliveryType::Instant:
		// No travel time, immediate resolution
		for (AActor *Target : Targets)
		{
			ResolveInstantSpell(Caster, Target, Spell, FinalImpactRadius, FinalDamage, bIsBrokenDarkness);
		}
		break;
	}
}

void UActionExecutor::SpawnSupportSpellEffect(
	AActor *Caster,
	const TArray<AActor *> &Targets,
	USpellData *Spell,
	float FinalVisualScale,
	bool bIsBrokenDarkness)
{
	for (AActor *Target : Targets)
	{
		// Spawn VFX at target
		if (Spell->SpellVFX)
		{
			FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

			UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				Spell->SpellVFX,
				Target->GetActorLocation(),
				FRotator::ZeroRotator,
				FVector(FinalVisualScale),
				true, true);

			if (NiagaraComp)
			{
				NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			}
		}

		// Apply healing/buff directly - no defense window
		// Healing/buffs handled by existing ExecuteSpell logic
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Support spell VFX spawned for %d targets"), Targets.Num());
}

void UActionExecutor::SpawnProjectileActor(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness)
{
	if (!Caster || !Target || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnProjectileActor - Invalid parameters"));
		return;
	}

	// Check for projectile class
	TSubclassOf<ASpellProjectile> ProjectileClass = DefaultProjectileClass;
	if (!ProjectileClass)
	{
		// Fallback to base class if no BP assigned
		ProjectileClass = ASpellProjectile::StaticClass();
	}

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Caster;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASpellProjectile *Projectile = GetWorld()->SpawnActor<ASpellProjectile>(
		ProjectileClass,
		Caster->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!Projectile)
	{
		UE_LOG(LogTemp, Error, TEXT("[ActionExecutor] Failed to spawn projectile!"));
		return;
	}

	// Initialize with combat data
	Projectile->InitializeProjectile(
		Spell,
		Caster,
		Target,
		FinalImpactRadius,
		FinalVisualScale,
		FinalDamage);

	// Assign VFX from SpellData
	Projectile->SetVFXAssets(
		Spell->MuzzleVFX,
		Spell->SpellVFX,
		Spell->ImpactVFX);

	// Bind to events
	Projectile->OnSpellImpact.AddDynamic(this, &UActionExecutor::OnProjectileImpact);
	Projectile->OnSpellDodged.AddDynamic(this, &UActionExecutor::OnProjectileDodged);

	if (Spell->DeliveryType == ESpellDeliveryType::Beam)
	{
		Projectile->OnBeamTick.AddDynamic(this, &UActionExecutor::OnBeamTick);
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spawned projectile toward %s (Type=%d, Speed=%.1f)"),
		   *Target->GetName(), (int32)Spell->DeliveryType, Spell->ProjectileSpeed);
}

void UActionExecutor::SpawnAOEEffect(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness)
{
	if (!Target || !Spell)
	{
		return;
	}

	FVector SpawnLocation = Target->GetActorLocation();

	// Get colors for VFX
	FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

	// Spawn VFX at target location
	if (Spell->SpellVFX)
	{
		UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Spell->SpellVFX,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector(FinalVisualScale),
			true, // bAutoDestroy
			true, // bAutoActivate
			ENCPoolMethod::None,
			true // bPreCullCheck
		);

		// Apply element colors
		if (NiagaraComp)
		{
			NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			NiagaraComp->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
			NiagaraComp->SetColorParameter(FName("TrailColor"), Colors.SecondaryColor);
		}
	}

	// AOE always hits - open defense window immediately
	// AOE can only be blocked (no dodge, no parry)
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		float WindowDuration = 0.5f; // AOE has longer window

		DefenseSys->OpenDefenseWindow(
			Caster,
			Target,
			FinalImpactRadius,
			FinalDamage,
			WindowDuration,
			true // bIsElemental
		);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] AOE opened defense window for %s (Block only)"),
			   *Target->GetName());
	}
	else
	{
		// Fallback: Apply damage directly if no defense system
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying AOE damage directly"));
		ApplyDamage(Caster, Target, FinalDamage, true, Spell->Element, false);
	}
}

void UActionExecutor::ResolveInstantSpell(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	int32 FinalDamage,
	bool bIsBrokenDarkness)
{
	if (!Target || !Spell)
	{
		return;
	}

	// Get colors for VFX
	FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

	// Spawn VFX at target immediately
	if (Spell->SpellVFX)
	{
		UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Spell->SpellVFX,
			Target->GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.f),
			true,
			true);

		if (NiagaraComp)
		{
			NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			NiagaraComp->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
		}
	}

	// Instant spells are unavoidable - apply damage directly
	// No defense window
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Instant spell hit %s - unavoidable, applying damage"),
		   *Target->GetName());

	FCombatHitResult Result = ApplyDamage(Caster, Target, FinalDamage, true, Spell->Element, true);

	// Update execution context
	if (CurrentExecutionContext.IsSet())
	{
		CurrentExecutionContext->PartialResult.TotalDamageDealt += Result.DamageDealt;
		CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, Result.DamageDealt);
		CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);

		if (Result.bWasCritical)
		{
			CurrentExecutionContext->PartialResult.bWasCritical = true;
		}

		if (Result.bTargetDied)
		{
			CurrentExecutionContext->PartialResult.bCausedDeath = true;
		}
	}
}

// ========================================
// PROJECTILE EVENT HANDLERS
// ========================================

void UActionExecutor::OnProjectileImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage)
{
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Projectile impact on %s - Damage=%d, Radius=%.2f"),
		   Target ? *Target->GetName() : TEXT("None"), Damage, ImpactRadius);

	if (!Target)
	{
		return;
	}

	// Open defense window for Block/Parry
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		float WindowDuration = 0.3f; // Standard window for projectile impact

		DefenseSys->OpenDefenseWindow(
			nullptr, // Caster not tracked here (could store in projectile if needed)
			Target,
			ImpactRadius,
			Damage,
			WindowDuration,
			true // bIsElemental (assume true for spells)
		);
	}
	else
	{
		// Fallback: Apply damage directly
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying projectile damage directly"));
		ApplyDamage(nullptr, Target, Damage, true, ESpellElement::Generic, true);
	}
}

void UActionExecutor::OnProjectileDodged(AActor *Target, FVector ImpactLocation)
{
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Projectile dodged by %s at %s"),
		   Target ? *Target->GetName() : TEXT("None"), *ImpactLocation.ToString());

	// No damage applied - target successfully moved out of impact zone
	// Could broadcast event for UI feedback here

	// Update execution context if tracking
	if (CurrentExecutionContext.IsSet() && Target)
	{
		CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);
		CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, 0); // 0 damage = dodged
	}
}

void UActionExecutor::OnBeamTick(AActor *Target, float DeltaTime, bool bTargetInBeam)
{
	// Beam applies damage over time while target is in beam
	if (!bTargetInBeam || !Target)
	{
		return;
	}

	// TODO: Calculate per-tick damage based on beam total damage and duration
	// For now, apply small damage each tick
	int32 TickDamage = 5; // Placeholder

	// Apply damage without defense window (beam is continuous)
	ApplyDamage(nullptr, Target, TickDamage, true, ESpellElement::Generic, false);
}

void UActionExecutor::PlayAbilityAnimation(AActor *User, UAbilityData *Ability)
{
	// Stub - override in subclass for ability animations
	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] PlayAbilityAnimation stub - %s using %s"),
		   User ? *User->GetName() : TEXT("None"),
		   Ability ? *Ability->GetName() : TEXT("None"));
}

void UActionExecutor::PlayAttackAnimation(AActor *Attacker, UWeaponAttackData *Attack)
{
	// Stub - override in subclass for attack animations
	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] PlayAttackAnimation stub - %s attacking with %s"),
		   Attacker ? *Attacker->GetName() : TEXT("None"),
		   Attack ? *Attack->GetName() : TEXT("None"));
}

// ========================================
// DEBUG
// ========================================

void UActionExecutor::DebugPrintActionResult(const FActionResult &Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== ACTION RESULT ==="));
	UE_LOG(LogTemp, Display, TEXT("Success: %s"), Result.bSuccess ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Executor: %s"), Result.Executor ? *Result.Executor->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("Energy Spent: %d"), Result.EnergySpent);
	UE_LOG(LogTemp, Display, TEXT("Total Damage: %d"), Result.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("Total Healing: %d"), Result.TotalHealingDone);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Caused Death: %s"), Result.bCausedDeath ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Status Effects: %d"), Result.StatusEffectsApplied);
	UE_LOG(LogTemp, Display, TEXT("Targets: %d"), Result.AffectedTargets.Num());

	if (!Result.ErrorMessage.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Error: %s"), *Result.ErrorMessage);
	}

	UE_LOG(LogTemp, Display, TEXT("====================="));
}

// ========================================
// SUBSYSTEM GETTERS
// ========================================

UItemExecutor *UActionExecutor::GetItemExecutor() const
{
	if (!ItemExecutorRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->ItemExecutorRef =
				GI->GetSubsystem<UItemExecutor>();
		}
	}
	return ItemExecutorRef;
}

UWeaponManager *UActionExecutor::GetWeaponManager() const
{
	if (!WeaponManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->WeaponManagerRef =
				GI->GetSubsystem<UWeaponManager>();
		}
	}
	return WeaponManagerRef;
}

bool UActionExecutor::CanUseInfusionType(AActor *Actor, EInfusionType Type) const
{
	if (Type == EInfusionType::None)
	{
		return true; // Always can choose no infusion
	}

	if (Type == EInfusionType::Physical)
	{
		return true; // Physical always available
	}

	if (Type == EInfusionType::Element)
	{
		return CanUseElementInfusion(Actor);
	}

	return false;
}

TArray<EInfusionType> UActionExecutor::GetAvailableInfusionTypes(AActor *Actor) const
{
	TArray<EInfusionType> Available;
	Available.Add(EInfusionType::None);
	Available.Add(EInfusionType::Physical); // Always available

	if (CanUseElementInfusion(Actor))
	{
		Available.Add(EInfusionType::Element);
	}

	return Available;
}

bool UActionExecutor::CanUseElementInfusion(AActor *Actor) const
{
	return GetInfusionSource(Actor) != EInfusionSource::None;
}

EInfusionSource UActionExecutor::GetInfusionSource(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return EInfusionSource::None;
	}

	// 1. Check weapon crystal first (highest priority)
	UWeaponManager *WM = GetWeaponManager();
	if (!WM)
		return EInfusionSource::None;
	UWeaponData *Weapon = WM->GetActiveWeapon(Actor);
	if (Weapon && Weapon->SlottedCrystal)
	{
		// Iolite = physical enhancement, no element - skip to next source
		if (Weapon->SlottedCrystal->CrystalType != ECrystalType::Iolite)
		{
			return EInfusionSource::Crystal;
		}
	}

	// 2. Caster innate element
	if (Data->IsCaster() && Data->InnateElement != ESpellElement::Generic)
	{
		return EInfusionSource::Innate;
	}

	// 3. Resonator ring element
	if (Data->IsResonator())
	{
		URingManager *RM = GetRingManager();
		if (RM && RM->GetActiveRing(Actor))
		{
			return EInfusionSource::Ring;
		}
	}

	// 4. Generic with secondary ring
	if (Data->IsGeneric() && Data->HasRingInSecondary())
	{
		return EInfusionSource::Ring;
	}

	// 4. No element source
	return EInfusionSource::None;
}
TArray<EInfusionSourceOption> UActionExecutor::GetAvailableInfusionSources(AActor *Actor) const
{
	TArray<EInfusionSourceOption> Sources;
	Sources.Add(EInfusionSourceOption::None); // Always available

	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return Sources;
	}

	// Innate (Caster only)
	if (Data->IsCaster())
	{
		Sources.Add(EInfusionSourceOption::Innate);
	}

	// Active Ring (Resonator only)
	if (Data->IsResonator())
	{
		URingManager *RM = GetRingManager();
		if (RM && RM->GetActiveRing(Actor) != nullptr)
		{
			Sources.Add(EInfusionSourceOption::ActiveRing);
		}
	}

	// Secondary Ring (Generic only)
	if (Data->IsGeneric() && Data->HasRingInSecondary())
	{
		Sources.Add(EInfusionSourceOption::SecondaryRing);
	}

	// Weapon Crystal (any class, if non-Ilodite)
	if (!HasIoliteEquipped(Actor) && GetInfusionSource(Actor) == EInfusionSource::Crystal)
	{
		Sources.Add(EInfusionSourceOption::WeaponCrystal);
	}

	// Evolution (any class, if evolved)
	if (Data->IsEvolved())
	{
		Sources.Add(EInfusionSourceOption::Evolution);
	}

	return Sources;
}

EInfusionSource UActionExecutor::MapSourceOptionToSource(EInfusionSourceOption Option) const
{
	switch (Option)
	{
	case EInfusionSourceOption::None:
		return EInfusionSource::None;
	case EInfusionSourceOption::Innate:
		return EInfusionSource::Innate;
	case EInfusionSourceOption::ActiveRing:
	case EInfusionSourceOption::SecondaryRing:
		return EInfusionSource::Ring;
	case EInfusionSourceOption::WeaponCrystal:
		return EInfusionSource::Crystal;
	case EInfusionSourceOption::Evolution:
		return EInfusionSource::Evolution;
	default:
		return EInfusionSource::None;
	}
}

EInfusionType UActionExecutor::MapSourceOptionToType(EInfusionSourceOption Option) const
{
	if (Option == EInfusionSourceOption::None)
	{
		return EInfusionType::Physical;
	}
	return EInfusionType::Element;
}

ESpellElement UActionExecutor::GetElementForSourceOption(AActor *Actor, EInfusionSourceOption Option) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return ESpellElement::Generic;
	}

	switch (Option)
	{
	case EInfusionSourceOption::None:
		return ESpellElement::Generic;

	case EInfusionSourceOption::Innate:
		return Data->InnateElement;

	case EInfusionSourceOption::ActiveRing:
	{
		URingManager *RM = GetRingManager();
		return RM ? RM->GetActiveElement(Actor) : ESpellElement::Generic;
	}

	case EInfusionSourceOption::SecondaryRing:
	{
		if (Data->IsGeneric() && Data->SecondaryRing)
		{
			return Data->SecondaryRing->Element;
		}
		return ESpellElement::Generic;
	}

	case EInfusionSourceOption::WeaponCrystal:
	{
		UWeaponManager *WM = GetWeaponManager();
		if (WM)
		{
			UWeaponData *Weapon = WM->GetActiveWeapon(Actor);
			if (Weapon)
			{
				return Weapon->GetWeaponElement();
			}
		}
		return ESpellElement::Generic;
	}

	case EInfusionSourceOption::Evolution:
		return Data->GetSecondaryElement();

	default:
		return ESpellElement::Generic;
	}
}

bool UActionExecutor::DoWeaponStatsApply(EInfusionSourceOption Option) const
{
	return Option == EInfusionSourceOption::None;
}

EInfusionSource UActionExecutor::GetSpellSource(AActor *Actor, USpellData *Spell) const
{
	if (!Actor || !Spell)
	{
		return EInfusionSource::None;
	}

	// Check if spell comes from evolution weapon
	UWeaponManager *WM = GetWeaponManager();
	if (WM)
	{
		UWeaponData *Weapon = WM->GetActiveWeapon(Actor);
		if (Weapon && Weapon->IsEvolved())
		{
			if (Spell->Element == Weapon->SlottedCrystal->GetAssociatedElement())
			{
				return EInfusionSource::Evolution;
			}
		}
	}

	// Check if spell comes from ring (Resonator)
	UCharacterData *Data = GetCharacterData(Actor);
	if (Data && Data->IsResonator())
	{
		URingManager *RM = GetRingManager();
		if (RM && RM->GetAvailableSpells(Actor).Contains(Spell))
		{
			return EInfusionSource::Ring;
		}
	}

	// Must be innate (Caster)
	if (Data && Data->IsCaster())
	{
		return EInfusionSource::Innate;
	}

	return EInfusionSource::None;
}

ESpellElement UActionExecutor::GetInfusionElement(AActor *Actor) const
{
	EInfusionSource Source = GetInfusionSource(Actor);
	UCharacterData *Data = GetCharacterData(Actor);

	if (!Data)
	{
		return ESpellElement::Generic;
	}

	switch (Source)
	{
	case EInfusionSource::None:
		return ESpellElement::Generic;

	case EInfusionSource::Innate:
		return Data->InnateElement;

	case EInfusionSource::Ring:
	{
		URingManager *RM = GetRingManager();
		if (RM)
		{
			return RM->GetActiveElement(Actor);
		}
		return ESpellElement::Generic;
	}

	case EInfusionSource::Crystal:
	case EInfusionSource::Evolution:
	{
		UWeaponManager *WM = GetWeaponManager();
		if (WM)
		{
			UWeaponData *Weapon = WM->GetActiveWeapon(Actor);
			if (Weapon && Weapon->SlottedCrystal)
			{
				return Weapon->SlottedCrystal->GetAssociatedElement();
			}
		}
		return ESpellElement::Generic;
	}

	default:
		return ESpellElement::Generic;
	}
}

bool UActionExecutor::HasIoliteEquipped(AActor *Actor) const
{
	UWeaponManager *WM = GetWeaponManager();
	if (!WM)
	{
		return false;
	}

	UWeaponData *Weapon = WM->GetActiveWeapon(Actor);
	if (!Weapon || !Weapon->SlottedCrystal)
	{
		return false;
	}

	return Weapon->SlottedCrystal->CrystalType == ECrystalType::Iolite;
}

// ========================================
// INFUSION MULTIPLIERS
// ========================================

float UActionExecutor::GetInfusionEnergyCostMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return InfusionConstants::L1_ENERGY_MULT;
	case 2:
		return InfusionConstants::L2_ENERGY_MULT;
	default:
		return 1.0f;
	}
}

float UActionExecutor::GetSpellSizeMultiplier(int32 SpellSizeInfusionLevel)
{
	switch (SpellSizeInfusionLevel)
	{
	case 1:
		return InfusionConstants::SPELL_L1_SIZE_MULT;
	case 2:
		return InfusionConstants::SPELL_L2_SIZE_MULT;
	default:
		return 1.0f;
	}
}

float UActionExecutor::GetSpellSizeEnergyCostMultiplier(int32 SpellSizeInfusionLevel)
{
	switch (SpellSizeInfusionLevel)
	{
	case 1:
		return InfusionConstants::SPELL_L1_ENERGY_MULT;
	case 2:
		return InfusionConstants::SPELL_L2_ENERGY_MULT;
	default:
		return 1.0f;
	}
}

// ========================================
// INFUSION EFFECT APPLICATION
// ========================================

void UActionExecutor::ApplyInfusionEffects(
	FActionResult &Result,
	AActor *Actor,
	EInfusionType Type,
	int32 Level)
{
	if (Type == EInfusionType::None || Level == 0)
	{
		return;
	}

	Result.bWasInfused = true;
	Result.InfusionTypeUsed = Type;
	Result.InfusionLevelUsed = Level;

	UCharacterData *Data = GetCharacterData(Actor);

	switch (Type)
	{
	case EInfusionType::Physical:
		ApplyPhysicalInfusion(Result, Actor, Level);
		break;

	case EInfusionType::Element:
		ApplyElementInfusion(Result, Actor, Level);
		break;
	}

	// Apply L2 costs
	if (Level >= 2)
	{
		if (Type == EInfusionType::Physical && HasIoliteEquipped(Actor))
		{
			// Iolite L2: Stat buff + break chance
			ApplyIoliteStatBuff(Actor);
			Result.BreakChanceIncrease = InfusionConstants::CRYSTAL_L2_BREAK_INCREASE;
		}
		else if (Type == EInfusionType::Element)
		{
			EInfusionSource Source = GetInfusionSource(Actor);
			Result.InfusionSourceUsed = Source;
			ApplyL2InfusionCost(Result, Actor, Source);
		}
	}
}

void UActionExecutor::ApplyPhysicalInfusion(FActionResult &Result, AActor *Actor, int32 Level)
{
	// L1: Add weapon status buildup (handled by WeaponManager)
	// L2: Add damage boost
	if (Level >= 2)
	{
		Result.TotalDamageDealt = FMath::RoundToInt(
			Result.TotalDamageDealt * InfusionConstants::PHYSICAL_L2_DAMAGE_MULT);
	}

	Result.bIsElementalAttack = false;
}

void UActionExecutor::ApplyElementInfusion(FActionResult &Result, AActor *Actor, int32 Level)
{
	ESpellElement Element = GetInfusionElement(Actor);
	Result.AttackElement = Element;
	Result.bIsElementalAttack = true;

	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return;
	}

	// Calculate status buildup
	float StatusMultiplier = 1.0f;
	if (Level == 1)
	{
		StatusMultiplier = InfusionConstants::ELEMENT_L1_STATUS_MULT;
	}
	else if (Level >= 2)
	{
		StatusMultiplier = InfusionConstants::ELEMENT_L2_STATUS_MULT;

		// L2 also adds damage
		Result.TotalDamageDealt = FMath::RoundToInt(
			Result.TotalDamageDealt * InfusionConstants::ELEMENT_L2_DAMAGE_MULT);
	}

	// Apply status buildup (base amount from ability/spell * multiplier * character's EffectDamage)
	// Note: Base status amount should be set before calling this
	float EffectDamageMultiplier = Data->CalculateEffectDamageMultiplier();
	Result.StatusBuildupApplied = FMath::RoundToInt(
		Result.StatusBuildupApplied * StatusMultiplier * EffectDamageMultiplier);
}

// ========================================
// L2 COST APPLICATION
// ========================================

void UActionExecutor::ApplyL2InfusionCost(
	FActionResult &Result,
	AActor *Actor,
	EInfusionSource Source)
{
	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return;
	}

	switch (Source)
	{
	case EInfusionSource::Innate:
	{
		// Caster innate: HP cost
		int32 HPCost = FMath::RoundToInt(
			Data->CalculateMaxHP() * InfusionConstants::INNATE_L2_HP_COST_PERCENT);
		ApplySelfDamage(Actor, HPCost);
		Result.SelfDamageTaken = HPCost;

		OnInfusionCostApplied.Broadcast(Actor, Source, HPCost, 0.0f);
		break;
	}

	case EInfusionSource::Ring:
	{
		// Resonator ring: Break chance increase
		Result.BreakChanceIncrease = InfusionConstants::RING_L2_BREAK_INCREASE;

		OnInfusionCostApplied.Broadcast(Actor, Source, 0, Result.BreakChanceIncrease);
		break;
	}

	case EInfusionSource::Crystal:
	{
		// Weapon crystal: Break chance increase
		Result.BreakChanceIncrease = InfusionConstants::CRYSTAL_L2_BREAK_INCREASE;

		OnInfusionCostApplied.Broadcast(Actor, Source, 0, Result.BreakChanceIncrease);
		break;
	}

	case EInfusionSource::Evolution:
	{
		// Evolution: HP cost + self-status
		int32 HPCost = FMath::RoundToInt(
			Data->CalculateMaxHP() * InfusionConstants::EVOLUTION_L2_HP_COST_PERCENT);
		ApplySelfDamage(Actor, HPCost);
		Result.SelfDamageTaken = HPCost;

		// Self-status buildup
		ESpellElement Element = GetInfusionElement(Actor);
		int32 SelfStatusAmount = FMath::RoundToInt(
			Result.StatusBuildupApplied * InfusionConstants::EVOLUTION_L2_SELF_STATUS_MULT);
		ApplySelfStatusBuildup(Actor, Element, SelfStatusAmount);
		Result.SelfStatusBuildupApplied = SelfStatusAmount;

		OnInfusionCostApplied.Broadcast(Actor, Source, HPCost, 0.0f);
		break;
	}

	default:
		break;
	}
}

void UActionExecutor::ApplySpellSizeL2Cost(
	FActionResult &Result,
	AActor *Actor,
	USpellData *Spell)
{
	EInfusionSource Source = GetSpellSource(Actor, Spell);
	UCharacterData *Data = GetCharacterData(Actor);

	if (!Data)
	{
		return;
	}

	switch (Source)
	{
	case EInfusionSource::Innate:
	{
		// Caster innate spell: HP cost
		int32 HPCost = FMath::RoundToInt(
			Data->CalculateMaxHP() * InfusionConstants::INNATE_L2_HP_COST_PERCENT);
		ApplySelfDamage(Actor, HPCost);
		Result.SelfDamageTaken = HPCost;
		break;
	}

	case EInfusionSource::Ring:
	{
		// Resonator ring spell: Higher break chance
		Result.BreakChanceIncrease = InfusionConstants::RING_L2_SPELL_BREAK_INCREASE;
		break;
	}

	case EInfusionSource::Evolution:
	{
		// Evolution weapon spell: HP + self-status
		int32 HPCost = FMath::RoundToInt(
			Data->CalculateMaxHP() * InfusionConstants::EVOLUTION_L2_HP_COST_PERCENT);
		ApplySelfDamage(Actor, HPCost);
		Result.SelfDamageTaken = HPCost;

		ESpellElement Element = Spell ? Spell->Element : ESpellElement::Generic;
		int32 BaseStatus = Spell ? Spell->CalculateStatusBuildup(Data) : 0;
		int32 SelfStatusAmount = FMath::RoundToInt(BaseStatus * InfusionConstants::EVOLUTION_L2_SELF_STATUS_MULT);
		ApplySelfStatusBuildup(Actor, Element, SelfStatusAmount);
		Result.SelfStatusBuildupApplied = SelfStatusAmount;
		break;
	}

	default:
		break;
	}
}

// ============================================================
//  - Get BrokenDarknessManager
// ============================================================

UBrokenDarknessManager *UActionExecutor::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

// ============================================================
// Check and Roll for Break
// ============================================================
void UActionExecutor::CheckBrokenDarknessBreak(AActor *Actor, const FAction &Action, UCharacterData *CharData)
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager)
	{
		return; // Not a potential BD character
	}

	// Already transformed - no more break checks needed
	if (BDManager->IsTransformed())
	{
		return;
	}

	// Check 1: Spell below stat requirements
	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		if (UBrokenDarknessManager::DoesSpellExceedRequirements(Action.SpellData, CharData))
		{
			BDManager->RollForBreak(TEXT("Underpowered spell cast"));
		}
	}

	// Check 2: Ability below stat requirements
	if (Action.ActionType == EActionType::Ability && Action.AbilityData)
	{
		if (UBrokenDarknessManager::DoesAbilityExceedRequirements(Action.AbilityData, CharData))
		{
			BDManager->RollForBreak(TEXT("Underpowered ability use"));
		}
	}

	// Check 3: L2 Infusion (any action type)
	if (Action.SpellInfusionLevel >= 2 || Action.AbilityInfusionLevel >= 2)
	{
		BDManager->RollForBreak(TEXT("L2 Infusion overcharge"));
	}
}

// ============================================================
// Process Forbidden Element Cast
// ============================================================
void UActionExecutor::ProcessForbiddenElementCast(AActor *Actor, ESpellElement Element, float BaseDamage)
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager || !BDManager->IsTransformed())
	{
		return;
	}

	// ProcessForbiddenCast checks if element is forbidden internally
	BDManager->ProcessForbiddenCast(Element, BaseDamage);
}

// ========================================
// RING MANAGER GETTER
// ========================================

URingManager *UActionExecutor::GetRingManager() const
{
	if (!RingManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->RingManagerRef =
				GI->GetSubsystem<URingManager>();
		}
	}
	return RingManagerRef;
}

// ========================================
// HELPER IMPLEMENTATIONS
// ========================================

void UActionExecutor::ApplyIoliteStatBuff(AActor *Actor)
{
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		return;
	}

	// Apply +5% to all stats for 1 turn
	// TODO: Create a proper status effect for this or use existing buff system
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Iolite L2 stat buff applied to %s (+%d%% all stats)"),
		   Actor ? *Actor->GetName() : TEXT("None"),
		   FMath::RoundToInt(InfusionConstants::IOLITE_L2_STAT_BUFF * 100.0f));
}

void UActionExecutor::ApplySelfDamage(AActor *Actor, int32 Amount)
{
	if (!Actor || Amount <= 0)
	{
		return;
	}

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	if (Comp)
	{
		Comp->ServerTakeDamage(Amount);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s took %d self-damage from infusion cost"),
			   *Actor->GetName(), Amount);
	}
}

void UActionExecutor::ApplySelfStatusBuildup(AActor *Actor, ESpellElement Element, int32 Amount)
{
	if (!Actor || Amount <= 0)
	{
		return;
	}

	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		// Apply element status to self
		// TODO: Implement status buildup on self
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s received %d self-status buildup (%s) from Evolution L2"),
			   *Actor->GetName(), Amount, *UEnum::GetValueAsString(Element));
	}
}

// ========================================
// CHARGE INFUSION HELPERS
// ========================================

float UActionExecutor::GetSpellChargeStatusMultiplier(int32 SpellInfusionLevel) const
{
	switch (SpellInfusionLevel)
	{
	case 1:
		return InfusionConstants::CHARGE_L1_STATUS_MULT; // 1.5f - status boost
	case 2:
		return 1.0f; // L2 gets BASE status, not boosted
	default:
		return 1.0f;
	}
}

float UActionExecutor::GetSpellChargeDamageMultiplier(int32 SpellInfusionLevel) const
{
	switch (SpellInfusionLevel)
	{
	case 1:
		return 1.0f; // L1 gets status boost, not damage
	case 2:
		return InfusionConstants::CHARGE_L2_DAMAGE_MULT; // 1.3f - damage boost
	default:
		return 1.0f;
	}
}

float UActionExecutor::GetAbilityChargeStatusMultiplier(int32 AbilityInfusionLevel) const
{
	switch (AbilityInfusionLevel)
	{
	case 1:
		return InfusionConstants::CHARGE_L1_STATUS_MULT; // 1.5f - status boost
	case 2:
		return 0.0f; // L2 gets NO status
	default:
		return 0.0f; // L0 = no status from charge
	}
}

float UActionExecutor::GetAbilityChargeDamageMultiplier(int32 AbilityInfusionLevel) const
{
	switch (AbilityInfusionLevel)
	{
	case 1:
		return 1.0f; // L1 gets status boost, not damage
	case 2:
		return InfusionConstants::CHARGE_L2_DAMAGE_MULT; // 1.3f - damage boost
	default:
		return 1.0f;
	}
}

void UActionExecutor::ApplyAbilityInfusionStatus(
	AActor *User,
	const TArray<AActor *> &Targets,
	EInfusionSourceOption Source,
	int32 HitCount,
	float StatusMultiplier)
{
	if (StatusMultiplier <= 0.0f || Targets.Num() == 0)
	{
		return;
	}

	if (Source == EInfusionSourceOption::None)
	{
		// Physical source - TODO: Integrate with WeaponManager when API is available
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Would apply physical status to %d targets (x%.1f mult)"),
			   Targets.Num(), StatusMultiplier);
	}
	else
	{
		// Elemental source - apply element status buildup
		ESpellElement Element = GetElementForSourceOption(User, Source);

		if (Element != ESpellElement::Generic)
		{
			int32 BaseBuildup = 10 * HitCount; // TODO: Get from CombatConstants
			int32 FinalBuildup = FMath::RoundToInt(BaseBuildup * StatusMultiplier);

			// TODO: Integrate with StatusEffectManager when API is available
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Would apply %d %s status buildup to %d targets"),
				   FinalBuildup, *UEnum::GetValueAsString(Element), Targets.Num());
		}
	}
}

// ========================================
// DEBUG
// ========================================

void UActionExecutor::DebugPrintInfusionInfo(AActor *Actor) const
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Display, TEXT("=== INFUSION INFO: No Actor ==="));
		return;
	}

	UCharacterData *Data = GetCharacterData(Actor);
	EInfusionSource Source = GetInfusionSource(Actor);
	ESpellElement Element = GetInfusionElement(Actor);
	TArray<EInfusionType> Available = GetAvailableInfusionTypes(Actor);

	UE_LOG(LogTemp, Display, TEXT("=== INFUSION INFO: %s ==="), *Actor->GetName());
	UE_LOG(LogTemp, Display, TEXT("Class: %s"),
		   Data ? *UEnum::GetValueAsString(Data->CharacterClass) : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("Element Source: %s"),
		   *InfusionSourceHelpers::GetSourceName(Source));
	UE_LOG(LogTemp, Display, TEXT("Element: %s"),
		   *UEnum::GetValueAsString(Element));
	UE_LOG(LogTemp, Display, TEXT("Has Iolite: %s"),
		   HasIoliteEquipped(Actor) ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Display, TEXT("Available Types: %d"), Available.Num());

	for (EInfusionType Type : Available)
	{
		UE_LOG(LogTemp, Display, TEXT("  - %s"),
			   *InfusionTypeHelpers::GetInfusionName(Type));
	}

	UE_LOG(LogTemp, Display, TEXT("L2 Cost: %s"),
		   *InfusionSourceHelpers::GetL2CostDescription(Source));
	UE_LOG(LogTemp, Display, TEXT("================================"));
}

ULoadoutComponent *UActionExecutor::GetLoadoutComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<ULoadoutComponent>();
}

bool UActionExecutor::CanUseAbility(AActor *Actor, UAbilityData *Ability) const
{
	if (!Actor || !Ability)
	{
		return false;
	}

	// Check LoadoutComponent first (new system)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsReadyForBattle())
	{
		TArray<UAbilityData *> Available = Loadout->GetAvailableAbilities();
		return Available.Contains(Ability);
	}
	return false;
}

bool UActionExecutor::CanUseSpell(AActor *Actor, USpellData *Spell) const
{
	if (!Actor || !Spell)
	{
		return false;
	}

	// Check LoadoutComponent first (new system)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsReadyForBattle())
	{
		TArray<USpellData *> Available = Loadout->GetAvailableSpells();
		return Available.Contains(Spell);
	}

	return false;
}

// ==================== MOVEMENT INTEGRATION ====================

UApproachData *UActionExecutor::GetApproachData(const FAction &Action) const
{
	switch (Action.ActionType)
	{
	case EActionType::Attack:
		return Action.AttackData ? Action.AttackData->ApproachData : nullptr;

	case EActionType::Ability:
		return Action.AbilityData ? Action.AbilityData->ApproachData : nullptr;

	case EActionType::Spell:
		return nullptr; // Spells are always ranged

	default:
		return nullptr;
	}
}

float UActionExecutor::GetExecutionRange(const FAction &Action) const
{
	switch (Action.ActionType)
	{
	case EActionType::Attack:
		return Action.AttackData ? Action.AttackData->ExecutionRange : 100.0f;

	case EActionType::Ability:
		return Action.AbilityData ? Action.AbilityData->ExecutionRange : 150.0f;

	case EActionType::Spell:
		return 0.0f; // Spells are always ranged, no approach

	default:
		return 0.0f;
	}
}

UCombatMovementComponent *UActionExecutor::GetMovementComponent(AActor *Actor) const
{
	return Actor ? Actor->FindComponentByClass<UCombatMovementComponent>() : nullptr;
}

void UActionExecutor::SignalActionComplete(AActor *Actor)
{
	if (UCombatMovementComponent *Movement = GetMovementComponent(Actor))
	{
		Movement->OnActionExecutionComplete();
	}
}