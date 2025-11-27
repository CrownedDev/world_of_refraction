// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActionExecutor.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "BaseAttackData.h"

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

class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;
class USpellData;
class UAbilityData;
class UItemData;
class UBaseAttackData;

class UItemExecutor;
class UWeaponManager;
class URingManager;

void UActionExecutor::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Initialized"));
}

void UActionExecutor::Deinitialize()
{
	StatusEffectManagerRef = nullptr;
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

float UActionExecutor::GetAbilityPowerInfusionDamageMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return 1.3f; // Level 1: 30% damage increase
	case 2:
		return 1.6f; // Level 2: 60% damage increase
	default:
		return 1.0f; // No infusion
	}
}

float UActionExecutor::GetAbilityPowerInfusionCostMultiplier(int32 InfusionLevel)
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
			int32 BaseCost = Action.SpellData->CalculateEnergyCost(CharData, Action.bUseElementalMode);
			// Spell infusion: 1.0x / 1.3x / 1.6x cost
			float CostMultiplier = GetSpellInfusionCostMultiplier(Action.SpellInfusionLevel);
			return FMath::RoundToInt(BaseCost * CostMultiplier);
		}
		break;

	case EActionType::Ability:
		if (Action.AbilityData)
		{
			int32 BaseCost = Action.AbilityData->CalculateEnergyCost(CharData, Action.bIsElementInfused);
			// Ability power infusion (Generic only): 1.0x / 1.3x / 1.6x cost
			float CostMultiplier = GetAbilityPowerInfusionCostMultiplier(Action.AbilityInfusionLevel);
			return FMath::RoundToInt(BaseCost * CostMultiplier);
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

	// Route to appropriate executor
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		Result = ExecuteSpell(Actor, Action.SpellData, Action.Targets,
							  Action.bUseElementalMode, Action.SpellInfusionLevel);
		break;

	case EActionType::Ability:
		Result = ExecuteAbility(Actor, Action.AbilityData, Action.Targets,
								Action.bIsElementInfused, Action.AbilityInfusionLevel);
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
	// Store callback
	PendingActionCallback = OnComplete;
	PendingDefenseCount = 0;

	// Clear any pending timers
	for (FTimerHandle &Handle : PendingHitTimers)
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	PendingHitTimers.Empty();

	// Validate first
	FActionValidationResult Validation = ValidateAction(Actor, Action);
	if (!Validation.bIsValid)
	{
		FActionResult FailResult;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = Validation.ErrorMessage;
		FailResult.Executor = Actor;
		FailResult.ActionType = Action.ActionType;

		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Async action validation failed: %s"), *Validation.ErrorMessage);

		// Immediate callback for validation failure
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return;
	}

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s executing %s async (Cost: %d EP)"),
		   *Actor->GetName(), *Action.GetActionName(), Validation.EnergyCost);

	// Execute synchronously for now - defense windows will be handled via events
	// DefenseSystem should bind to OnDefenseWindowRequested to intercept damage
	PendingActionResult = ExecuteAction(Actor, Action);

	// If no defense windows are pending, complete immediately
	if (PendingDefenseCount == 0)
	{
		CompleteAsyncAction();
	}
	// Otherwise, defense system will call OnDefenseResolved for each target
}

void UActionExecutor::CompleteAsyncAction()
{
	if (PendingActionCallback.IsBound())
	{
		PendingActionCallback.Execute(PendingActionResult);
		PendingActionCallback.Unbind();
	}

	// Reset state
	PendingDefenseCount = 0;
	PendingActionResult = FActionResult();
}

void UActionExecutor::OnDefenseResolved(AActor *Target, int32 FinalDamage, bool bWasDodged, bool bWasBlocked, bool bWasParried)
{
	// Update pending result with actual damage after defense
	if (PendingActionResult.DamagePerTarget.Contains(Target))
	{
		int32 OriginalDamage = PendingActionResult.DamagePerTarget[Target];
		int32 DamageDifference = OriginalDamage - FinalDamage;

		PendingActionResult.TotalDamageDealt -= DamageDifference;
		PendingActionResult.DamagePerTarget[Target] = FinalDamage;
	}

	PendingDefenseCount--;

	if (PendingDefenseCount <= 0)
	{
		CompleteAsyncAction();
	}
}

// ========================================
// EXECUTION - SPELL
// ========================================

FActionResult UActionExecutor::ExecuteSpell(
	AActor *Caster,
	USpellData *Spell,
	const TArray<AActor *> &Targets,
	bool bUseElementalMode,
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
	int32 BaseEnergyCost = Spell->CalculateEnergyCost(CasterData, bUseElementalMode);
	float CostMultiplier = GetSpellInfusionCostMultiplier(InfusionLevel);
	int32 FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * CostMultiplier);

	if (!SpendEnergy(Caster, FinalEnergyCost))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to spend energy");
		return Result;
	}
	Result.EnergySpent = FinalEnergyCost;

	// Calculate spell SIZE with infusion (NOT damage)
	// Size affects dodge viability in defense system
	float BaseAbilitySize = CasterData->CalculateAbilitySizeMultiplier();
	float SizeMultiplier = GetSpellInfusionSizeMultiplier(InfusionLevel);
	float FinalSpellSize = BaseAbilitySize * SizeMultiplier;

	// Store size in result for defense system
	Result.AttackSize = FinalSpellSize;
	Result.AttackElement = Spell->Element;
	Result.bIsElementalAttack = true;

	// Calculate damage (NOT affected by spell infusion - that's Generic's thing)
	int32 BaseDamage = Spell->CalculateDamage(CasterData, bUseElementalMode);
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
		PendingDefenseCount++;

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

// ========================================
// EXECUTION - ABILITY
// ========================================

FActionResult UActionExecutor::ExecuteAbility(
	AActor *User,
	UAbilityData *Ability,
	const TArray<AActor *> &Targets,
	bool bIsElementInfused,
	int32 PowerInfusionLevel)
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

	// Calculate energy cost
	// Element infusion (Casters): handled by CalculateEnergyCost with bIsElementInfused
	// Power infusion (Generic): additional multiplier
	int32 BaseEnergyCost = Ability->CalculateEnergyCost(UserData, bIsElementInfused);
	float PowerCostMultiplier = GetAbilityPowerInfusionCostMultiplier(PowerInfusionLevel);
	int32 FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * PowerCostMultiplier);

	if (!SpendEnergy(User, FinalEnergyCost))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to spend energy");
		return Result;
	}
	Result.EnergySpent = FinalEnergyCost;

	// Calculate damage
	// Element infusion (Casters): 30% damage penalty (handled in CalculateDamage)
	int32 BaseDamage = Ability->CalculateDamage(UserData, bIsElementInfused);

	// Power infusion (Generic only): damage multiplier 1.3x / 1.6x
	float PowerDamageMultiplier = GetAbilityPowerInfusionDamageMultiplier(PowerInfusionLevel);
	BaseDamage = FMath::RoundToInt(BaseDamage * PowerDamageMultiplier);

	// Determine element
	ESpellElement Element = ESpellElement::Generic;
	if (bIsElementInfused && Ability->bCanBeInfused)
	{
		Element = UserData->InnateElement;
	}

	// Store defense info
	Result.AttackElement = Element;
	Result.bIsElementalAttack = bIsElementInfused;
	Result.BaseDamageBeforeDefense = BaseDamage;

	// Play animation
	PlayAbilityAnimation(User, Ability);

	// Process each target
	TArray<AActor *> ValidTargets = FilterValidTargets(Targets);
	for (AActor *Target : ValidTargets)
	{
		// Multi-hit processing
		int32 TotalDamage = ProcessMultiHit(
			User, Target,
			BaseDamage / FMath::Max(1, Ability->HitCount),
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

	// Apply status effects from ability
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

	// Apply status buildup if infused
	if (bIsElementInfused && Ability->bCanBeInfused)
	{
		int32 StatusBuildup = Ability->CalculateStatusBuildup(UserData);
		// TODO: Apply status buildup to trigger elemental status
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s used %s%s%s - %d damage to %d targets"),
		   *User->GetName(), *Ability->GetName(),
		   bIsElementInfused ? TEXT(" (Element)") : TEXT(""),
		   PowerInfusionLevel > 0 ? *FString::Printf(TEXT(" (Power %d)"), PowerInfusionLevel) : TEXT(""),
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

	// TODO: Consume item from inventory

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s used item %s - delegated to ItemExecutor"),
		   *User->GetName(), *Item->GetFullItemName());

	return Result;
}

// ========================================
// EXECUTION - ATTACK
// ========================================

FActionResult UActionExecutor::ExecuteAttack(
	AActor *Attacker,
	UBaseAttackData *Attack,
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

	int32 FinalDamage = BaseDamage;

	// Apply defense
	FinalDamage = ApplyDefense(FinalDamage, Target, bIsElemental);

	// Check for critical hit
	if (bCanCrit && RollCriticalHit(Attacker))
	{
		FinalDamage = ApplyCriticalMultiplier(FinalDamage, Attacker);
		Result.bWasCritical = true;
	}

	// Apply damage modifiers from status effects
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		// Attacker damage buff/debuff
		float AttackerDamageMod = StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::DamageBuff);
		AttackerDamageMod -= StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::DamageDebuff);
		FinalDamage = FMath::RoundToInt(FinalDamage * (1.0f + AttackerDamageMod / 100.0f));
	}

	// Ensure minimum damage of 1
	FinalDamage = FMath::Max(1, FinalDamage);

	// Apply damage
	int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerTakeDamage(FinalDamage);
	Result.DamageDealt = HPBefore - TargetComp->CurrentHP;

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

	int32 FinalHealing = BaseHealing;

	// Apply healing modifiers from status effects
	// (Could add healing buff/debuff if needed)

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

int32 UActionExecutor::ApplyCriticalMultiplier(int32 Damage, AActor *Attacker) const
{
	// Crit damage is fixed at 1.5x (CharacterData doesn't have this method)
	constexpr float CritMultiplier = 1.5f;

	return FMath::RoundToInt(Damage * CritMultiplier);
}

int32 UActionExecutor::ApplyDefense(int32 Damage, AActor *Defender, bool bIsElemental) const
{
	UCharacterData *Data = GetCharacterData(Defender);
	if (!Data)
		return Damage;

	int32 FinalDamage = Damage;

	// Step 1: Apply flat defense (subtraction) - affects all damage types
	int32 FlatDefense = Data->CalculateFlatDefense();

	// Add defense buffs/debuffs from status effects (percentage modifier to defense)
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (StatusManager)
	{
		float DefenseBuffPercent = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::DefenseBuff);
		float DefenseDebuffPercent = StatusManager->GetTotalStatModifier(Defender, EAbilityEffectType::DefenseDebuff);
		float DefenseModifier = 1.0f + (DefenseBuffPercent - DefenseDebuffPercent) / 100.0f;
		FlatDefense = FMath::RoundToInt(FlatDefense * FMath::Max(0.0f, DefenseModifier));
	}

	FinalDamage = FMath::Max(0, FinalDamage - FlatDefense);

	// Step 2: Apply resistance (percentage reduction) - elemental damage only
	if (bIsElemental && FinalDamage > 0)
	{
		float Resistance = Data->CalculateElementalResistance(); // Returns 0.0-1.0
		// Cap resistance at 80%
		Resistance = FMath::Clamp(Resistance, 0.0f, 0.8f);
		FinalDamage = FMath::RoundToInt(FinalDamage * (1.0f - Resistance));
	}

	return FinalDamage;
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
	// Stub - override in subclass or bind to OnActionStarted for custom animation handling
	// In full implementation:
	// 1. Get AnimInstance from Caster's mesh
	// 2. Play Spell->CastAnimation montage
	// 3. Scale VFX based on SpellSize
	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] PlaySpellAnimation stub - %s casting %s (Size: %.1f)"),
		   Caster ? *Caster->GetName() : TEXT("None"),
		   Spell ? *Spell->GetName() : TEXT("None"),
		   SpellSize);
}

void UActionExecutor::SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize)
{
	// Stub - override in subclass for Niagara/particle spawning
	// In full implementation:
	// 1. Get VFX asset from Spell
	// 2. Spawn at target location(s)
	// 3. Scale system by SpellSize
	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] SpawnSpellVFX stub - %s VFX (Size: %.1f)"),
		   Spell ? *Spell->GetName() : TEXT("None"),
		   SpellSize);
}

void UActionExecutor::PlayAbilityAnimation(AActor *User, UAbilityData *Ability)
{
	// Stub - override in subclass for ability animations
	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] PlayAbilityAnimation stub - %s using %s"),
		   User ? *User->GetName() : TEXT("None"),
		   Ability ? *Ability->GetName() : TEXT("None"));
}

void UActionExecutor::PlayAttackAnimation(AActor *Attacker, UBaseAttackData *Attack)
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

	// 4. No element source
	return EInfusionSource::None;
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
