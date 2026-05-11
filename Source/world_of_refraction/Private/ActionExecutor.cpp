// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActionExecutor.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "ElementHelpers.h"
#include "SkillEffectManager.h"
#include "StatusBuildupManager.h"
#include "StatusEffect.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "WeaponAttackData.h"
#include "ESpellSource.h"
#include "ActionUtils.h"
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

#include "FRingLoadoutEntry.h"
#include "CombatMovementComponent.h"
#include "CombatAnimInstance.h"
#include "CombatGridSubsystem.h"
#include "TurnManager.h"
#include "RealityBoost.h"
#include "GameFramework/Character.h"

class UCharacterDataComponent;
class UCharacterData;
class USkillEffectManager;
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
	SkillEffectManagerRef = nullptr;
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

	// Infusion consistency: Level > 0 requires a real source.
	// None means "no infusion at all" — incompatible with Level > 0.
	// Use Raw for HP-cost infusion without an element.
	const int32 ChargeLevel = Action.GetChargeLevel();
	if (ChargeLevel > 0 && Action.SelectedSource == EInfusionSourceOption::None)
	{
		return FActionValidationResult(
			false,
			TEXT("Infusion level set but no source selected (use Raw for elementless infusion)"));
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
			// Element gate (Caster-only): Reality and BrokenDarkness sources unlock
			// any-spell access per locked design. Generic and Resonator have no
			// element gate.
			if (Action.SpellData && CharData->CharacterClass == ECharacterClass::Caster)
			{
				const ESpellElement SourceElement =
					GetElementForSourceOption(Actor, Action.SelectedSource);
				const bool bAnyElement = ElementHelpers::IsAnySpellSource(SourceElement);
				if (!bAnyElement && CharData->InnateElement != Action.SpellData->Element)
				{
					return FActionValidationResult(false, TEXT("Element restricted"));
				}
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
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager && StatusManager->IsStunned(Actor))
	{
		return false;
	}
	return true;
}

bool UActionExecutor::CanActorCastSpells(AActor *Actor) const
{
	USkillEffectManager *StatusManager = GetSkillEffectManager();
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
			const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
			int32 BaseCost = Action.AbilityData->CalculateEnergyCost(CharData, bIsInfused);
			return BaseCost;
		}
		break;

	case EActionType::Item:
		// Items typically don't cost energy
		return 0;

	case EActionType::Attack:
		if (Action.AttackData && Action.SelectedSource != EInfusionSourceOption::None)
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

	// Apply commit-time costs (HP / crystal wear / etc.) based on infusion source.
	// Costs are paid at commit, not at cast success — wear/HP loss happens even
	// if the action subsequently fails to land.
	ApplyCommitCosts(Actor, Action);

	// Route to appropriate executor.
	// Phase D: Spell/Ability/Attack sync paths retired — async + ApplyHit is the only path
	// for those action types. Sync ExecuteAction now only handles instant-resolution actions
	// (Item, Defend). Spell/Ability/Attack callers must use ExecuteActionAsync; reaching this
	// switch with one of those types is a caller bug.
	switch (Action.ActionType)
	{
	case EActionType::Item:
		Result = ExecuteItem(Actor, Action.ItemData, Action.Targets);
		break;

	case EActionType::Defend:
		Result = ExecuteDefend(Actor);
		break;

	case EActionType::Spell:
	case EActionType::Ability:
	case EActionType::Attack:
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor::ExecuteAction] %s called sync for action type %d — Spell/Ability/Attack must go through ExecuteActionAsync (Phase D)"),
			   *Actor->GetName(), static_cast<int32>(Action.ActionType));
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Spell/Ability/Attack must use ExecuteActionAsync");
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
	// Lazy-bind to DefenseSystem on first use. Subsystem init order is alphabetical, so
	// ActionExecutor::Initialize runs before DefenseSystem exists; binding there silently no-ops.
	// By the time any action runs, DefenseSystem exists. Idempotent — bDefenseEventsBound guards re-binding.
	if (!bDefenseEventsBound)
	{
		BindDefenseSystemEvents();
	}

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

	// Reset coordination flags. FinalizeAsyncAction fires only when both flip true:
	// bAllDefensesResolved (set by CheckAndFinalizeAsyncAction) and !bWaitingForAnimationEnd
	// (cleared by UnbindActionAnimationEnd in OnActionAnimationEnded).
	bWaitingForAnimationEnd = false;
	bAllDefensesResolved = false;

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Starting async action: %s by %s"),
		   *Action.GetActionName(), *Actor->GetName());

	// Get character data for calculations
	UCharacterData *CharData = GetCharacterData(Actor);

	// === BROKEN DARKNESS & INFUSION HOOKS ===
	// Check for Broken Darkness break triggers
	CheckBrokenDarknessBreak(Actor, Action, CharData);

	// Start Movement (if applicable)
	AActor *PrimaryTarget = Action.Targets.Num() > 0 ? Action.Targets[0] : nullptr;
	UMovementData *MovementData = GetMovementData(Action);
	float ExecutionRange = GetExecutionRange(Action);

	// Cache for approach completion callback
	PendingExecutionActor = Actor;
	PendingExecutionCharData = CharData;

	// Handle instant actions (no animation, no movement)
	if (Action.ActionType == EActionType::Defend ||
		Action.ActionType == EActionType::SwitchWeapon ||
		Action.ActionType == EActionType::Flee)
	{
		FActionResult Result = ExecuteAction(Actor, Action);
		CurrentExecutionContext.Reset();
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(Result);
		}
		return;
	}

	// Handle Item - has animation but no movement
	if (Action.ActionType == EActionType::Item)
	{
		ExecuteItemAsync(Actor, Action, CharData);
		return;
	}

	// Apply commit-time costs for Attack / Ability / Spell.
	// Placed AFTER instant-action and Item early-returns so:
	//   - Defend/SwitchWeapon/Flee path goes through synchronous ExecuteAction,
	//     which calls ApplyCommitCosts there (avoiding double-charge here).
	//   - Items have no infusion cost path.
	//   - Attack/Ability/Spell pay exactly once, here, before movement starts.
	ApplyCommitCosts(Actor, Action);

	// Per-action stat modifiers accumulated from all active sources
	// (Reality innate/slotted/infused, Evolution slotted/infused).
	// Stashed on context so all consumers across the async lifecycle
	// (movement → animation → damage → defense) read the same snapshot.
	const FActionStatModifiers ActionMods = ComputeActionStatModifiers(Action, Actor);
	if (CurrentExecutionContext.IsSet())
	{
		CurrentExecutionContext->ActionMods = ActionMods;
	}

	if (ActionMods.IsActive())
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[ActionExecutor] %s ActionMods active — Crit:%.1f%% RawDmg:%.1f%% SpellDmg:%.1f%% StatusMult:%.1f%% ActSpd:%.1f%%"),
			   *Actor->GetName(),
			   ActionMods.CritChance, ActionMods.RawDamage,
			   ActionMods.SpellDamage, ActionMods.StatusMultiplier, ActionMods.ActionSpeed);
	}

	// Attack / Ability / Spell — bind movement complete and start approach
	BindMovementComplete(Actor);

	UCombatMovementComponent *Movement = GetMovementComponent(Actor);
	if (Movement)
	{
		Movement->StartApproach(PrimaryTarget, MovementData, ExecutionRange, CachedArenaCenter, ActionMods);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No CombatMovementComponent on %s - executing immediately"),
			   *Actor->GetName());
		OnMovementComplete();
	}
}

// ========================================
// ASYNC EXECUTOR SHARED HELPERS
// ========================================

bool UActionExecutor::ValidateInfusionGate(const FAction &Action, bool bCanBeInfused, int32 InfusionLevel)
{
	const bool bWantsInfusion =
		(Action.SelectedSource != EInfusionSourceOption::None) ||
		(InfusionLevel > 0);
	if (bWantsInfusion && !bCanBeInfused)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("This action cannot be infused.");
		FinalizeAsyncAction();
		return false;
	}
	return true;
}

void UActionExecutor::FinalizeDamageInputs(int32 FinalDamage, int32 HitCount, int32& OutDamagePerHit)
{
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;
	OutDamagePerHit = FinalDamage / FMath::Max(1, HitCount);
}

void UActionExecutor::LogActionDispatch(
	EActionType ActionType,
	int32 InfusionLevel,
	int32 FinalDamage,
	int32 NumTargets) const
{
	UE_LOG(LogTemp, Log,
		TEXT("[ActionExecutor] %s async L%d - %d damage, opened %d defense windows"),
		*UEnum::GetValueAsString(ActionType),
		InfusionLevel,
		FinalDamage,
		NumTargets);
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

	// Commit 3: reject early when bCanBeInfused is false but the action carries
	// infusion (source selection OR charge level). No energy spent, no damage dealt.
	if (!ValidateInfusionGate(Action, Spell->bCanBeInfused, Action.SpellInfusionLevel))
	{
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

	// Per-action stat modifiers (Reality, Evolution, future buffs) — populated
	// on the execution context by ExecuteActionAsync via ComputeActionStatModifiers.
	const FActionStatModifiers ActionMods = CurrentExecutionContext.IsSet()
												? CurrentExecutionContext->ActionMods
												: FActionStatModifiers();

	// Calculate damage with charge infusion multiplier
	int32 BaseDamage = Spell->CalculateDamage(CasterData, ActionMods);
	float DamageMultiplier = GetSpellChargeDamageMultiplier(Action.SpellInfusionLevel);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Track status multiplier for later application
	float StatusMultiplier = GetSpellChargeStatusMultiplier(Action.SpellInfusionLevel);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell charge L%d - Size: %.1fx, Damage: %d (%.1fx), Status: %.1fx"),
		   Action.SpellInfusionLevel,
		   GetSpellInfusionSizeMultiplier(Action.SpellInfusionLevel),
		   FinalDamage,
		   DamageMultiplier,
		   StatusMultiplier);

	// Store in result for reference. BaseDamageBeforeDefense receives the
	// infused damage because that is what defense will reduce — "before defense"
	// refers to the defense pipeline, not "before infusion".
	CurrentExecutionContext->PartialResult.AttackSize = FinalSpellSize;
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Spell->Element;

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Cache spell data for notify-triggered VFX
	PendingSpellCaster = Caster;
	PendingSpellData = Spell;
	PendingSpellTargets = ValidTargets;
	PendingSpellSize = FinalSpellSize;
	PendingSpellDamage = FinalDamage;

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	bPendingSpellIsBrokenDarkness = BDManager && BDManager->IsTransformed();

	// Bind to notify for VFX timing
	BindSpellNotify(Caster);

	// Play animation - VFX spawns on SpellRelease notify (NOT here)
	PlaySpellAnimation(Caster, Spell, FinalSpellSize, ActionMods);

	// Calculate damage per hit (infused total split across hits)
	int32 DamagePerHit = FinalDamage / FMath::Max(1, Spell->HitCount);

	// Check for forbidden element self-damage (BD casting Dark Light/Void).
	// Backlash scales with the spell's intrinsic power, NOT the infused amount —
	// infusion multiplies output damage, not the metaphysical strain of the cast.
	ProcessForbiddenElementCast(Caster, Spell->Element, static_cast<float>(BaseDamage));

	// Phase C1: Spell buildup now flows through the defense pipeline.
	// Compute base buildup + status type here; ApplyDamageAfterDefense applies
	// the defense-outcome reduction and ApplyHit calls AddStatusBuildup.
	int32 SpellBaseBuildup = 0;
	EStatusType SpellStatusType = EStatusType::None;
	if (Spell->StatusBuildup > 0)
	{
		float Buildup = static_cast<float>(Spell->StatusBuildup);
		if (Action.SpellInfusionLevel == 1)
		{
			Buildup *= CombatConstants::SPELL_L1_BUILDUP_MULT;
		}
		SpellBaseBuildup = FMath::RoundToInt(Buildup);

		if (Spell->bIsRawMode)
		{
			// Dead-after-redirect: ApplyRawModeRedirect overwrites this to None
			// below. Flagged for Phase E sweep alongside DamageCalculator's
			// bIsRawMode gate.
			SpellStatusType = EStatusType::BurstDamage;
		}
		else if (Spell->PrimaryEffect != EStatusType::None)
		{
			SpellStatusType = Spell->PrimaryEffect;
		}
		else
		{
			// Elemental spells without an explicit PrimaryEffect default to DOT.
			SpellStatusType = EStatusType::DOT;
		}
	}

	// Commit 2: if bIsRawMode, fold StatusBuildup into FinalDamage at the
	// orchestrator boundary so downstream defense + ApplyHit see normalised inputs.
	ActionUtils::ApplyRawModeRedirect(Spell->bIsRawMode, FinalDamage, SpellBaseBuildup, SpellStatusType);

	FinalizeDamageInputs(FinalDamage, Spell->HitCount, DamagePerHit);
	PendingSpellDamage = FinalDamage;  // Spell-specific: cached for VFX notify

	// Open defense windows for all targets (damage and buildup both applied after defense resolves)
	OpenDefenseWindowsForTargets(
		Caster,
		ValidTargets,
		FinalSpellSize,
		FinalDamage,
		DamagePerHit,
		Spell->HitCount,
		Spell->Element,
		true,					   // Can crit
		EActionType::Spell,		   // ActionType — drives post-defense stat selection
		Action.SpellInfusionLevel, // InfusionLevel
		Action.SelectedSource,	   // SelectedSource
		SpellBaseBuildup,		   // BaseStatusBuildup (Phase C1)
		SpellStatusType,		   // StatusToBuild (Phase C1)
		0.3f					   // Default window duration - TODO: get from spell data
	);

	LogActionDispatch(EActionType::Spell, Action.SpellInfusionLevel, FinalDamage, ValidTargets.Num());
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

	// Commit 3: reject early when bCanBeInfused is false but the action carries
	// infusion (source selection OR charge level). No energy spent, no damage dealt.
	if (!ValidateInfusionGate(Action, Ability->bCanBeInfused, Action.AbilityInfusionLevel))
	{
		return;
	}

	// Calculate and spend energy
	const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
	int32 BaseEnergyCost = Ability->CalculateEnergyCost(UserData, bIsInfused);

	// Apply charge level energy multiplier (L1 = 1.15x, L2 = 1.30x)
	float CostMultiplier = GetAbilityChargeCostMultiplier(Action.AbilityInfusionLevel);
	int32 FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * CostMultiplier);

	if (!SpendEnergy(User, FinalEnergyCost))
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Failed to spend energy");
		FinalizeAsyncAction();
		return;
	}
	CurrentExecutionContext->PartialResult.EnergySpent = FinalEnergyCost;

	// Calculate base damage. Element-infusion damage penalty was removed per
	// the locked cost matrix — infusion now pays its tax via durability wear /
	// HP backlash / status build / energy multipliers, not flat damage tax.
	int32 BaseDamage = Ability->CalculateDamage(UserData, bIsInfused);

	// Element handling. Infusion routes the user's innate element through;
	// non-infused abilities stay Generic.
	ESpellElement Element = ESpellElement::Generic;
	if (bIsInfused && UserData->HasInnateElement())
	{
		Element = UserData->InnateElement;
	}

	// Apply charge level damage multiplier (L2 = 1.30x, L1 unchanged)
	float DamageMultiplier = GetAbilityChargeDamageMultiplier(Action.AbilityInfusionLevel);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Spell Size (fixed, no character scaling)
	float AttackSize = 1.0f;

	// Store in result. BaseDamageBeforeDefense receives the post-multiplier damage —
	// "before defense" refers to the defense pipeline, not "before infusion".
	CurrentExecutionContext->PartialResult.AttackSize = AttackSize; // remove attack size its pointless for abilities
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Element;

	// Per-action stat modifiers (stashed on context by ExecuteActionAsync).
	const FActionStatModifiers ActionMods = CurrentExecutionContext.IsSet()
												? CurrentExecutionContext->ActionMods
												: FActionStatModifiers();

	// Play animation
	PlayAbilityAnimation(User, Ability, ActionMods);

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Apply charge infusion status buildup (L1 = 1.25x, L2 = 0 — exclusive with damage)
	float StatusMultiplier = GetAbilityChargeStatusMultiplier(Action.AbilityInfusionLevel);
	if (StatusMultiplier > 0.0f && ValidTargets.Num() > 0 && bIsInfused)
	{
		ApplyAbilityInfusionStatus(User, ValidTargets, Action.SelectedSource,
								   Ability->HitCount, StatusMultiplier);
	}

	// Buildup amount only — read unconditionally, mirrors ExecuteSpellAsync.
	// Trigger type intentionally None: when the bar caps,
	// TriggerSkillEffectFromBuildup early-returns and nothing fires.
	// TODO: SetPendingTrigger when status buildup is maxed — add detailed
	// effect to take place. UAbilityData will need a way to declare which
	// EStatusType fires on cap (mirroring USpellData::PrimaryEffect, or via
	// the Effects[] array once that wiring is in place).
	int32 AbilityBaseBuildup = 0;
	if (Ability->StatusBuildup > 0)
	{
		AbilityBaseBuildup = Ability->StatusBuildup;
	}

	EStatusType AbilityStatusType = EStatusType::None;
	ActionUtils::ApplyRawModeRedirect(Ability->bIsRawMode, FinalDamage, AbilityBaseBuildup, AbilityStatusType);

	int32 DamagePerHit = 0;
	FinalizeDamageInputs(FinalDamage, Ability->HitCount, DamagePerHit);

	OpenDefenseWindowsForTargets(
		User,
		ValidTargets,
		AttackSize,
		FinalDamage,
		DamagePerHit,
		Ability->HitCount,
		Element,
		true,						 // Can crit
		EActionType::Ability,		 // ActionType
		Action.AbilityInfusionLevel, // InfusionLevel
		Action.SelectedSource,		 // SelectedSource
		AbilityBaseBuildup,			 // BaseStatusBuildup
		AbilityStatusType,			 // StatusToBuild — None until effect-on-cap is designed
		0.3f);

	LogActionDispatch(EActionType::Ability, Action.AbilityInfusionLevel, FinalDamage, ValidTargets.Num());
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

	// Commit 3: reject early when bCanBeInfused is false but the action carries
	// infusion (source selection). Attacks have no charge level concept. No energy
	// spent, no damage dealt.
	if (!ValidateInfusionGate(Action, Attack->bCanBeInfused, /*InfusionLevel=*/0))
	{
		return;
	}

	// Calculate damage
	float DamageMultiplier = AttackerData->CalculateRawDamage();
	int32 BaseDamage = FMath::RoundToInt(100.0f * DamageMultiplier);

	bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
	if (bIsInfused)
	{
		// Element-infusion damage penalty removed; energy multiplier + source-
		// specific costs (durability/HP/status) cover the infusion tax.

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

	// Per-action stat modifiers (stashed on context by ExecuteActionAsync).
	const FActionStatModifiers ActionMods = CurrentExecutionContext.IsSet()
												? CurrentExecutionContext->ActionMods
												: FActionStatModifiers();

	// Play animation
	PlayAttackAnimation(Attacker, Attack, ActionMods);

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Buildup amount only. Trigger type intentionally None — when the bar caps,
	// TriggerSkillEffectFromBuildup early-returns and nothing fires.
	// TODO: SetPendingTrigger when status buildup is maxed — add detailed
	// effect to take place. UWeaponAttackData::GetBuildupStatusType() already
	// maps PhysicalDamageType -> EStatusType; wire it here when effect-on-cap
	// is designed.
	int32 AttackBaseBuildup = 0;
	if (Attack->StatusBuildup > 0)
	{
		AttackBaseBuildup = Attack->StatusBuildup;
	}

	EStatusType AttackStatusType = EStatusType::None;

	// Commit 2: if bIsRawMode, fold AttackBaseBuildup into BaseDamage at the
	// orchestrator boundary so downstream defense + ApplyHit see normalised inputs.
	ActionUtils::ApplyRawModeRedirect(Attack->bIsRawMode, BaseDamage, AttackBaseBuildup, AttackStatusType);

	int32 DamagePerHit = 0;
	FinalizeDamageInputs(BaseDamage, Attack->HitCount, DamagePerHit);

	OpenDefenseWindowsForTargets(
		Attacker,
		ValidTargets,
		AttackSize,
		BaseDamage,
		DamagePerHit,
		Attack->HitCount,
		Element,
		true,				   // Can crit
		EActionType::Attack,   // ActionType
		0,					   // InfusionLevel — attacks have no L1/L2 concept
		Action.SelectedSource, // SelectedSource
		AttackBaseBuildup,	   // BaseStatusBuildup (Phase C3)
		AttackStatusType,	   // StatusToBuild (Phase C3)
		0.3f);

	LogActionDispatch(EActionType::Attack, 0, BaseDamage, ValidTargets.Num());
}

// ========================================
// EXECUTION - ITEM ASYNC
// ========================================

void UActionExecutor::ExecuteItemAsync(AActor *Actor, const FAction &Action, UCharacterData *CharData)
{
	if (!Actor || !Action.ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ExecuteItemAsync - Invalid actor or item"));
		FinalizeAsyncAction();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Executing item async: %s by %s"),
		   *Action.ItemData->GetFullItemName(), *Actor->GetName());

	// Face target (if not self)
	AActor *Target = Action.Targets.Num() > 0 ? Action.Targets[0] : Actor;
	bool bIsSelfTarget = (Target == Actor);

	if (!bIsSelfTarget)
	{
		FVector Direction = Target->GetActorLocation() - Actor->GetActorLocation();
		Direction.Z = 0;
		if (!Direction.IsNearlyZero())
		{
			Actor->SetActorRotation(Direction.GetSafeNormal().Rotation());
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Item user facing toward %s"), *Target->GetName());
		}
	}

	// Get and play animation
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	UAnimMontage *ItemMontage = Loadout ? Loadout->GetItemUseAnimation(bIsSelfTarget) : nullptr;

	if (ItemMontage)
	{
		BindActionAnimationEnd(Actor);
		PlayActionMontageOnActor(Actor, ItemMontage, 1.0f);
	}

	// Execute item logic (healing, damage, buffs)
	FActionResult Result = ExecuteItem(Actor, Action.ItemData, Action.Targets);

	// Store result for finalization
	if (CurrentExecutionContext.IsSet())
	{
		CurrentExecutionContext->PartialResult = Result;
	}

	// Set timeout as failsafe
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AsyncTimeoutHandle,
			this,
			&UActionExecutor::OnAsyncActionTimeout,
			5.0f,
			false);
	}

	// If no animation, gate on defenses via TryFinalize.
	// Items typically don't open defense windows; if none pending, mark resolved.
	if (!bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No item animation"));
		if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->AreAllDefensesResolved())
		{
			bAllDefensesResolved = true;
		}
		TryFinalizeAsyncAction();
	}
	// Otherwise OnActionAnimationEnded will handle finalization
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
	ESpellElement Element,
	bool bCanCrit,
	EActionType ActionType,
	int32 InfusionLevel,
	EInfusionSourceOption SelectedSource,
	int32 BaseStatusBuildup,
	EStatusType StatusToBuild,
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
				Attacker, Target, DamagePerHit, HitCount, Element, bCanCrit,
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
		DefenseContext.Element = Element;
		DefenseContext.HitCount = HitCount;
		DefenseContext.bCanCrit = bCanCrit;
		DefenseContext.WindowDuration = WindowDuration;
		DefenseContext.ActionType = ActionType;
		DefenseContext.InfusionLevel = InfusionLevel;
		DefenseContext.SelectedSource = SelectedSource;
		DefenseContext.BaseStatusBuildup = BaseStatusBuildup;
		DefenseContext.StatusToBuild = StatusToBuild;

		CurrentExecutionContext->PendingDefenses.Add(Target, DefenseContext);

		// Open defense window in DefenseSystem
		DefenseSys->OpenDefenseWindow(
			Attacker,
			Target,
			AttackSize,
			BaseDamage,
			WindowDuration);

		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Opened defense window for %s (Size: %.1f, Damage: %d)"),
			   *Target->GetName(), AttackSize, BaseDamage);
	}
}

void UActionExecutor::OnDefenseWindowClosed(AActor *Defender, const FDefenseResult &DefenseResult)
{
	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] OnDefenseWindowClosed CALLBACK FIRED for %s"),
		   Defender ? *Defender->GetName() : TEXT("null"));

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

	int32 TotalDamage = 0;
	bool bAnyCrit = false;

	if (DefenseResult.bSuccess && DefenseResult.DefenseType == EDefenseType::Dodge)
	{
		// Dodge cancels damage entirely AND cancels buildup (Phase C1 — applies to
		// spells only today; ability/attack buildup don't reach this path yet).
		// The multi-hit loop is skipped — no ApplyHit calls = no damage + no buildup.
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s dodged attack - 0 damage, 0 buildup"), *Target->GetName());
	}
	else
	{
		// DefenseResult.FinalDamage already has block/parry reduction applied.
		// Buildup reduction by block/parry happens here — multipliers parallel
		// DefenseSystem's hardcoded damage multipliers. Dodge already short-circuited
		// above; "no defense" / failed defense → EffectiveBuildup = base.
		const int32 DamagePerHit = DefenseResult.FinalDamage / FMath::Max(1, Context.HitCount);

		int32 EffectiveBuildup = Context.BaseStatusBuildup;
		if (DefenseResult.bSuccess && DefenseResult.DefenseType == EDefenseType::Block)
		{
			EffectiveBuildup = FMath::RoundToInt(EffectiveBuildup * CombatConstants::BLOCK_BUILDUP_MULTIPLIER);
		}
		else if (DefenseResult.bSuccess && DefenseResult.DefenseType == EDefenseType::Parry)
		{
			EffectiveBuildup = FMath::RoundToInt(EffectiveBuildup * CombatConstants::PARRY_BUILDUP_MULTIPLIER);
		}

		for (int32 i = 0; i < Context.HitCount; ++i)
		{
			FActionHitInput Input;
			Input.Attacker = Attacker;
			Input.Target = Target;
			Input.ActionType = Context.ActionType;
			Input.BaseDamage = DamagePerHit;
			Input.bCanCrit = Context.bCanCrit;
			Input.Element = Context.Element;
			Input.InfusionLevel = Context.InfusionLevel;
			Input.SelectedSource = Context.SelectedSource;
			// Buildup applies once per spell-per-target, not per-hit. First hit
			// carries the buildup; subsequent hits carry zero. Audit's quirks table
			// treats buildup as per-target, not per-hit.
			Input.BaseStatusBuildup = (i == 0) ? EffectiveBuildup : 0;
			Input.StatusToBuild = (i == 0) ? Context.StatusToBuild : EStatusType::None;
			Input.ActionMods = CurrentExecutionContext.IsSet()
								   ? CurrentExecutionContext->ActionMods
								   : FActionStatModifiers();

			const FCombatHitResult HitResult = ApplyHit(Input);

			TotalDamage += HitResult.DamageDealt;
			bAnyCrit = bAnyCrit || HitResult.bWasCritical;

			// Early-out on death — preserves existing ProcessMultiHit behaviour.
			if (HitResult.bTargetDied)
			{
				break;
			}
		}
	}

	// Aggregate into the running PartialResult.
	CurrentExecutionContext->PartialResult.TotalDamageDealt += TotalDamage;
	CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, TotalDamage);
	CurrentExecutionContext->PartialResult.AffectedTargets.AddUnique(Target);
	if (bAnyCrit)
	{
		CurrentExecutionContext->PartialResult.bWasCritical = true;
	}

	// Death broadcast — once per killed target, after the multi-hit loop completes.
	if (!IsTargetAlive(Target))
	{
		CurrentExecutionContext->PartialResult.bCausedDeath = true;
		OnTargetKilled.Broadcast(Attacker, Target);
	}

	// Defense-outcome telemetry. FActionResult doesn't carry per-target Block/Parry/Dodge
	// flags today; logging in Verbose preserves audit-risk-#10 signal until UI is wired.
	if (DefenseResult.bSuccess)
	{
		UE_LOG(LogTemp, Verbose,
			   TEXT("[ApplyDamageAfterDefense] %s defended via %s — TotalDamage=%d"),
			   *Target->GetName(),
			   *UEnum::GetValueAsString(DefenseResult.DefenseType),
			   TotalDamage);
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
		bAllDefensesResolved = true;
		TryFinalizeAsyncAction();
	}
}

void UActionExecutor::TryFinalizeAsyncAction()
{
	if (bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] TryFinalize: waiting for animation"));
		return;
	}

	if (!bAllDefensesResolved)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] TryFinalize: waiting for defenses"));
		return;
	}

	FinalizeAsyncAction();
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
		USkillEffectManager *StatusManager = GetSkillEffectManager();

		if (Action.ActionType == EActionType::Spell && Action.SpellData && StatusManager)
		{
			if (Action.SpellData->PrimaryEffect != EStatusType::None)
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

		// Apply ability effects (buffs, debuffs, drain) from Ability->Effects[].
		// Mirrors the sync path's call site that was removed in Phase D. Runs at
		// the same lifecycle point as spell Primary/Secondary effects — post-defense,
		// post-damage — so Result.TotalDamageDealt / bWasCritical / bCausedDeath
		// are populated for condition checks (OnHit / OnCrit / OnKill).
		if (Action.ActionType == EActionType::Ability &&
			Action.AbilityData &&
			Action.AbilityData->Effects.Num() > 0 &&
			Executor)
		{
			ApplyAbilityEffects(
				Executor,
				FinalResult.AffectedTargets,
				Action.AbilityData,
				FinalResult,
				FinalResult.bCausedDeath);
		}

		// Process post-cast by source (durability wear, etc.)
		if (Action.ActionType == EActionType::Spell && Action.SpellData)
		{
			ProcessPostCastBySource(Executor, Action.SpellData, Action.SpellSource, Action.SpellInfusionLevel);
		}
	}

	// Mark complete
	CurrentExecutionContext->bInProgress = false;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Async action finalized - Success: %s, Damage: %d, Targets: %d"),
		   FinalResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		   FinalResult.TotalDamageDealt,
		   FinalResult.AffectedTargets.Num());

	// Cache result for after return
	PendingFinalResult = FinalResult;
	Executor = CurrentExecutionContext->Executor.Get();

	// Clear context now (action is done, just waiting for return)
	CurrentExecutionContext.Reset();

	// Signal movement component to start return FIRST
	if (Executor)
	{
		SignalActionComplete(Executor);

		// NOW check if return movement started
		UCombatMovementComponent *Movement = GetMovementComponent(Executor);
		if (Movement && Movement->GetMovementState() == ECombatMovementState::Returning)
		{
			// Return is in progress - wait for it
			bWaitingForReturn = true;
			Movement->OnMovementComplete.AddDynamic(this, &UActionExecutor::OnReturnComplete);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Waiting for return movement to complete"));
			return; // Don't fire callback yet!
		}
	}

	// No return needed (or no executor) - complete immediately
	CompleteAsyncActionFinal(Executor);
}

void UActionExecutor::OnReturnComplete()
{
	if (!bWaitingForReturn)
	{
		return;
	}

	bWaitingForReturn = false;

	// Unbind
	if (PendingExecutionActor)
	{
		UCombatMovementComponent *Movement = GetMovementComponent(PendingExecutionActor);
		if (Movement)
		{
			Movement->OnMovementComplete.RemoveDynamic(this, &UActionExecutor::OnReturnComplete);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Return complete - firing completion callback"));

	CompleteAsyncActionFinal(PendingExecutionActor);
}

void UActionExecutor::CompleteAsyncActionFinal(AActor *Executor)
{
	// Fire callback
	if (AsyncActionCallback.IsBound())
	{
		AsyncActionCallback.Execute(PendingFinalResult);
		AsyncActionCallback.Unbind();
	}

	// Broadcast completion
	if (Executor)
	{
		OnAsyncActionCompleted.Broadcast(Executor, PendingFinalResult);
		OnActionCompleted.Broadcast(Executor, PendingFinalResult);
	}

	// Clear pending state
	PendingExecutionActor = nullptr;
	PendingExecutionCharData = nullptr;
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

	// Unbind approach delegate
	if (PendingExecutionActor)
	{
		UnbindMovementComplete(PendingExecutionActor);
		UnbindActionAnimationEnd(PendingExecutionActor);
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
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
	if (bDefenseEventsBound)
	{
		return;
	}

	UDefenseSystem *DefenseSys = GetDefenseSystem();
	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] BindDefenseSystemEvents called - DefenseSys: %s"),
		   DefenseSys ? TEXT("VALID") : TEXT("NULL"));

	if (DefenseSys)
	{
		DefenseSys->OnDefenseWindowClosed.AddDynamic(this, &UActionExecutor::OnDefenseWindowClosed);
		bDefenseEventsBound = true;

		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Bound to DefenseSystem events - IsBound: %s"),
			   DefenseSys->OnDefenseWindowClosed.IsBound() ? TEXT("TRUE") : TEXT("FALSE"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] DefenseSystem not yet available - will retry on first action"));
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
	bDefenseEventsBound = false;
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
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		FStatusEffect DefendBuff = FStatusEffect::CreateBuff(
			TEXT("Defending"),
			9999, // Special ID for defend
			EStatusType::DefenseBuff,
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

void UActionExecutor::ProcessPostCastBySource(AActor *Caster, USpellData *Spell, ESpellSource Source, int32 InfusionLevel)
{
	if (!Caster || !Spell)
		return;

	// Phase 4c: cost-bearing source paths (Ring crystal wear) MOVED to
	// ApplyCommitCosts. This function now handles only post-success consumption
	// (Item) and forward-looking placeholders (Evolution).
	switch (Source)
	{
	case ESpellSource::Innate:
		// No post-cast action; HP cost (if any) was paid at commit.
		break;

	case ESpellSource::RingCrystal:
		// Wear was applied at commit (ApplyCommitCosts). Nothing to do here.
		break;

	case ESpellSource::Evolution:
		// TODO Phase 6: Evolution-specific post-cast effects (if any beyond commit-time backlash)
		break;

	case ESpellSource::Item:
		// TODO: Consume spell item from inventory
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell item used - consumption not yet implemented"));
		break;

	case ESpellSource::WeaponCrystal:
		// Wear was applied at commit (ApplyCommitCosts). Nothing to do here.
		break;
	}
}

// ========================================
// DAMAGE APPLICATION
// ========================================

FCombatHitResult UActionExecutor::ApplyHit(const FActionHitInput &Input)
{
	FCombatHitResult Result;
	Result.Target = Input.Target;

	if (!Input.Target)
	{
		return Result;
	}

	UE_LOG(LogTemp, Verbose,
		   TEXT("[ApplyHit] %s -> %s | ActionType=%s Element=%s Dmg=%d Buildup=%d Status=%s Inf=L%d"),
		   Input.Attacker ? *Input.Attacker->GetName() : TEXT("null"),
		   *Input.Target->GetName(),
		   *UEnum::GetValueAsString(Input.ActionType),
		   *UEnum::GetValueAsString(Input.Element),
		   Input.BaseDamage,
		   Input.BaseStatusBuildup,
		   *UEnum::GetValueAsString(Input.StatusToBuild),
		   Input.InfusionLevel);

	UCharacterDataComponent *TargetComp = Input.Target->FindComponentByClass<UCharacterDataComponent>();

	// Damage path — routes through UDamageCalculator so all Phase 1/2/2b math
	// (EActionType-driven stat selection, ActionMods, element interaction,
	// flat defense, grid modifiers, crit, BD multipliers) runs unchanged.
	if (Input.BaseDamage > 0 && TargetComp)
	{
		if (UDamageCalculator *DamageCalc = GetDamageCalculator())
		{
			FDamageCalculationInput DmgInput;
			DmgInput.BaseDamage = Input.BaseDamage;
			DmgInput.ActionType = Input.ActionType;
			DmgInput.Element = Input.Element;
			DmgInput.bCanCrit = Input.bCanCrit;
			DmgInput.bWasInfused = Input.InfusionLevel > 0;
			DmgInput.InfusionLevel = Input.InfusionLevel;
			DmgInput.ActionMods = Input.ActionMods;

			const FDamageCalculationResult CalcResult = DamageCalc->CalculateDamage(Input.Attacker, Input.Target, DmgInput);

			Result.bWasCritical = CalcResult.bWasCritical;

			const int32 HPBefore = TargetComp->CurrentHP;
			TargetComp->ServerTakeDamage(CalcResult.FinalDamage);
			Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
		}
		else
		{
			// Fallback if DamageCalculator subsystem is unavailable — preserves
			// the existing ApplyDamage degradation path (deal min 1, no math).
			const int32 HPBefore = TargetComp->CurrentHP;
			TargetComp->ServerTakeDamage(FMath::Max(1, Input.BaseDamage));
			Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
		}

		if (!TargetComp->bIsAlive)
		{
			Result.bTargetDied = true;
		}

		// OnDamageDealt fires per-hit. ProcessMultiHit→ApplyDamage did this today;
		// ApplyHit preserves the per-hit broadcast so floating-number widgets,
		// hit-flash VFX, and any other listener see one event per hit landed.
		OnDamageDealt.Broadcast(Input.Attacker, Input.Target, Result.DamageDealt, Result.bWasCritical);
	}

	// Buildup path — UStatusBuildupManager::AddStatusBuildup runs the Phase 2a
	// attacker StatusMultiplier amplifier and the Phase 2b per-element resistance
	// reduction internally. ApplyHit gets both for free.
	if (Input.BaseStatusBuildup > 0)
	{
		UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
		if (BuildupManager)
		{
			BuildupManager->AddStatusBuildup(
				Input.Attacker,
				Input.Target,
				static_cast<float>(Input.BaseStatusBuildup),
				Input.StatusToBuild,
				Input.Element);
		}
	}

	return Result;
}

FCombatHitResult UActionExecutor::ApplyDamage(
	AActor *Attacker,
	AActor *Target,
	int32 BaseDamage,
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
		Input.Element = Element;
		Input.bCanCrit = bCanCrit;
		// ActionMods + ActionType are the canonical per-action signal path
		// (Reality + Evolution + future buffs). Stashed on context at action start
		// so every ApplyDamage call site (defense resolution, beam ticks, projectile
		// impacts, support spells) reads the same snapshot.
		if (CurrentExecutionContext.IsSet())
		{
			Input.ActionMods = CurrentExecutionContext->ActionMods;
			Input.ActionType = CurrentExecutionContext->Action.ActionType;
		}

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

USkillEffectManager *UActionExecutor::GetSkillEffectManager() const
{
	if (!SkillEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->SkillEffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return SkillEffectManagerRef;
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
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		CritChance += StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceBuff);
		CritChance -= StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceDebuff);
	}

	CritChance = FMath::Clamp(CritChance, 0.0f, 100.0f);

	return FMath::FRand() * 100.0f < CritChance;
}

void UActionExecutor::ApplyStatusEffects(
	AActor *Source,
	AActor *Target,
	EStatusType PrimaryEffect,
	float PrimaryValue,
	int32 PrimaryDuration,
	EStatusType SecondaryEffect,
	float SecondaryValue,
	int32 SecondaryDuration,
	ESpellElement Element)
{
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (!StatusManager)
		return;

	// Apply primary effect
	if (PrimaryEffect != EStatusType::None && PrimaryDuration > 0)
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
	if (SecondaryEffect != EStatusType::None && SecondaryDuration > 0)
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
	ESpellElement Element,
	bool bCanCrit,
	FActionResult &OutResult)
{
	int32 TotalDamage = 0;

	for (int32 i = 0; i < HitCount; i++)
	{
		// Each hit can independently crit
		FCombatHitResult HitResult = ApplyDamage(
			Attacker, Target, DamagePerHit, Element, bCanCrit);

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

void UActionExecutor::PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize, const FActionStatModifiers &ActionMods)
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

	// Play rate = CalculateSpellSpeed() × ActionMods.SpellSpeed contribution.
	float PlayRate = 1.0f;
	UCharacterData *CharData = GetCharacterData(Caster);
	if (CharData)
	{
		PlayRate = CharData->CalculateSpellSpeed();
	}
	PlayRate = ActionMods.ApplyTo(PlayRate, ESubStat::SpellSpeed);

	PlayActionMontageOnActor(Caster, Spell->CastAnimation, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing spell animation %s at %.2fx"),
		   *Spell->CastAnimation->GetName(), PlayRate);
}

void UActionExecutor::SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize, const TArray<AActor *> &ExplicitTargets, int32 Damage)
{
	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnSpellVFX - Invalid caster or spell"));
		return;
	}

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	bool bIsBD = BDManager && BDManager->IsTransformed();

	float FinalImpactRadius = Spell->BaseSize * Spell->HitboxRatio * SpellSize;
	float FinalVisualScale = Spell->BaseSize * SpellSize;

	// Use explicit targets if provided, otherwise get from context
	TArray<AActor *> Targets;
	if (ExplicitTargets.Num() > 0)
	{
		Targets = ExplicitTargets;
	}
	else if (CurrentExecutionContext.IsSet())
	{
		for (const auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Value.Target.IsValid())
			{
				Targets.Add(Pair.Value.Target.Get());
			}
		}
	}

	int32 FinalDamage = (Damage > 0) ? Damage : (CurrentExecutionContext.IsSet() ? CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense : 0);

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

	if (Projectile)
	{
		// 1. Assign VFX assets FIRST
		Projectile->SetVFXAssets(
			Spell->MuzzleVFX,
			Spell->SpellVFX,
			Spell->ImpactVFX);

		// 2. Initialize with combat data
		Projectile->InitializeProjectile(
			Spell,
			Caster,
			Target,
			FinalImpactRadius,
			FinalVisualScale,
			FinalDamage);

		// 3. Bind to events
		Projectile->OnSpellImpact.AddDynamic(this, &UActionExecutor::OnProjectileImpact);
		Projectile->OnSpellDodged.AddDynamic(this, &UActionExecutor::OnProjectileDodged);

		if (Spell->DeliveryType == ESpellDeliveryType::Beam)
		{
			Projectile->OnBeamTick.AddDynamic(this, &UActionExecutor::OnBeamTick);
		}

		// 4. Launch (activates VFX and starts movement)
		Projectile->Launch();

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spawned projectile toward %s (Type=%d, Speed=%.1f)"),
			   *Target->GetName(), (int32)Spell->DeliveryType, Spell->ProjectileSpeed);
	}
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
			WindowDuration);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] AOE opened defense window for %s (Block only)"),
			   *Target->GetName());
	}
	else
	{
		// Fallback: Apply damage directly if no defense system
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying AOE damage directly"));
		ApplyDamage(Caster, Target, FinalDamage, Spell->Element, false);
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

	FCombatHitResult Result = ApplyDamage(Caster, Target, FinalDamage, Spell->Element, true);

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
			WindowDuration);
	}
	else
	{
		// Fallback: Apply damage directly
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying projectile damage directly"));
		ApplyDamage(nullptr, Target, Damage, ESpellElement::Generic, true);
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
	ApplyDamage(nullptr, Target, TickDamage, ESpellElement::Generic, false);
}

void UActionExecutor::PlayAbilityAnimation(AActor *User, UAbilityData *Ability, const FActionStatModifiers &ActionMods)
{
	if (!User || !Ability)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlayAbilityAnimation - Invalid user or ability"));
		return;
	}

	if (!Ability->ExecutionMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] PlayAbilityAnimation - No ExecutionMontage on %s"),
			   *Ability->AbilityName);
		return;
	}

	// Play rate = 1.0 × CalculateAnimationSpeed() × ActionMods.ActionSpeed contribution.
	// CalculateAnimationSpeed derives from the ActionSpeed sub-stat — same scaling channel
	// as character movement. At baseline stats, AnimationSpeed=1.0 so existing montages unchanged.
	float PlayRate = 1.0f;
	UCharacterData *CharData = GetCharacterData(User);
	if (CharData)
	{
		PlayRate *= CharData->CalculateAnimationSpeed();
	}
	PlayRate = ActionMods.ApplyTo(PlayRate, ESubStat::ActionSpeed);

	PlayActionMontageOnActor(User, Ability->ExecutionMontage, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing ability animation %s for %s at %.2fx"),
		   *Ability->ExecutionMontage->GetName(), *Ability->AbilityName, PlayRate);
}

void UActionExecutor::PlayAttackAnimation(AActor *Attacker, UWeaponAttackData *Attack, const FActionStatModifiers &ActionMods)
{
	if (!Attacker || !Attack)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlayAttackAnimation - Invalid attacker or attack"));
		return;
	}

	if (!Attack->AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlayAttackAnimation - No montage on %s"),
			   *Attack->AttackName);
		return;
	}

	// Play rate = BaseAnimSpeed × CalculateAnimationSpeed() × ActionMods.ActionSpeed contribution.
	// Preserves designer-tuned per-attack pacing; layers stat scaling on top.
	float PlayRate = Attack->BaseAnimSpeed;
	UCharacterData *CharData = GetCharacterData(Attacker);
	if (CharData)
	{
		PlayRate *= CharData->CalculateAnimationSpeed();
	}
	PlayRate = ActionMods.ApplyTo(PlayRate, ESubStat::ActionSpeed);

	PlayActionMontageOnActor(Attacker, Attack->AttackMontage, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing attack animation %s at %.2fx"),
		   *Attack->AttackMontage->GetName(), PlayRate);
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

TArray<EInfusionSourceOption> UActionExecutor::GetAvailableInfusionSources(AActor *Actor) const
{
	TArray<EInfusionSourceOption> Sources;
	Sources.Add(EInfusionSourceOption::None); // Always available
	Sources.Add(EInfusionSourceOption::Raw);  // Always available — HP-cost elementless infusion

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

	// Primary Ring (Generic/Caster with ring in primary slot)
	URingManager *RM = GetRingManager();
	if (RM && RM->GetPrimaryRing(Actor))
	{
		Sources.Add(EInfusionSourceOption::PrimaryRing);
	}

	// Weapon Crystal — read runtime entry, not data asset.
	// Aligns with FCombatCapabilities::BuildFrom path; the data-asset
	// SlottedCrystal field is the legacy path that diverges from runtime
	// when crystals are slotted/changed at runtime.
	ULoadoutComponent *LC = GetLoadoutComponent(Actor);
	if (LC)
	{
		const FWeaponLoadoutEntry *ActiveWeapon = LC->GetActiveWeaponLoadout();
		if (ActiveWeapon && ActiveWeapon->WeaponEntry.AttachedCrystal.CanProvideSpells())
		{
			Sources.Add(EInfusionSourceOption::WeaponCrystal);
		}
	}

	// Evolution (any class, if evolved)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsEvolved())
	{
		Sources.Add(EInfusionSourceOption::Evolution);
	}

	return Sources;
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

	case EInfusionSourceOption::Raw:
		// Raw is elementless infusion — pays HP, no channeled element
		return ESpellElement::Generic;

	case EInfusionSourceOption::Innate:
		return Data->InnateElement;

	case EInfusionSourceOption::ActiveRing:
	{
		URingManager *RM = GetRingManager();
		return RM ? RM->GetActiveElement(Actor) : ESpellElement::Generic;
	}

	case EInfusionSourceOption::PrimaryRing:
	{
		URingManager *RM = GetRingManager();
		if (RM)
		{
			URingData *Ring = RM->GetPrimaryRing(Actor);
			if (Ring)
			{
				return Ring->GetRingElement();
			}
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
	{
		ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
		if (Loadout && Loadout->IsEvolved())
		{
			FCombatLoadout ActiveLoadout = Loadout->GetActiveLoadout();
			if (ActiveLoadout.PrimaryEvolution)
			{
				return ActiveLoadout.PrimaryEvolution->GetAssociatedElement();
			}
		}
		return ESpellElement::Generic;
	}

	default:
		return ESpellElement::Generic;
	}
}

bool UActionExecutor::DoWeaponStatsApply(EInfusionSourceOption Option) const
{
	return Option == EInfusionSourceOption::None || Option == EInfusionSourceOption::Raw;
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

	// Determine tier and infused state from the action's source data.
	// Spells: use SpellData->Tier. Abilities: use AbilityData->Tier.
	// Other action types (Attack, Defend, Item, etc.) don't trigger break.
	EItemTier Tier = EItemTier::F_Tier;
	int32 InfusionLevel = 0;
	FString TriggerReason;

	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		Tier = Action.SpellData->Tier;
		InfusionLevel = Action.SpellInfusionLevel;
		const bool bInfused = (InfusionLevel >= 1);

		if (UBrokenDarknessManager::DoesSpellExceedRequirements(Action.SpellData, CharData))
		{
			TriggerReason = bInfused
								? FString::Printf(TEXT("Underpowered spell cast (L%d)"), InfusionLevel)
								: TEXT("Underpowered spell cast");
		}
		else if (bInfused)
		{
			TriggerReason = FString::Printf(TEXT("L%d Infusion overcharge (spell)"), InfusionLevel);
		}
		else
		{
			return; // Spell within stats and not infused — no roll
		}
	}
	else if (Action.ActionType == EActionType::Ability && Action.AbilityData)
	{
		Tier = Action.AbilityData->Tier;
		InfusionLevel = Action.AbilityInfusionLevel;
		const bool bInfused = (InfusionLevel >= 1);

		if (UBrokenDarknessManager::DoesAbilityExceedRequirements(Action.AbilityData, CharData))
		{
			TriggerReason = bInfused
								? FString::Printf(TEXT("Underpowered ability use (L%d)"), InfusionLevel)
								: TEXT("Underpowered ability use");
		}
		else if (bInfused)
		{
			TriggerReason = FString::Printf(TEXT("L%d Infusion overcharge (ability)"), InfusionLevel);
		}
		else
		{
			return; // Ability within stats and not infused — no roll
		}
	}
	else
	{
		// Other action types don't trigger break
		return;
	}

	// One roll per cast with the correct tier and infusion level.
	BDManager->RollForBreak(Tier, InfusionLevel, TriggerReason);
}

// ============================================================
// Process Forbidden Element Cast
// ============================================================
void UActionExecutor::ProcessForbiddenElementCast(AActor *Actor, ESpellElement Element, float BaseDamage)
{
	if (!Actor)
		return;

	// Use IsBrokenDarkness() helper to cover both runtime-transformed
	// and character-created BD characters. Forbidden-element self-damage
	// applies whenever the character is behaviourally BD, regardless of
	// how they got there.
	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->IsBrokenDarkness())
	{
		return;
	}

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager)
	{
		return; // Defensive — character-created BDs should have a manager but verify
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

	USkillEffectManager *StatusManager = GetSkillEffectManager();
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

float UActionExecutor::GetAbilityChargeCostMultiplier(int32 Level) const
{
	// Energy cost scales with charge level for abilities. Uses the generic
	// (non-spell) energy multipliers — spells use SPELL_L1/L2_ENERGY_MULT.
	switch (Level)
	{
	case 1:
		return InfusionConstants::L1_ENERGY_MULT; // 1.15x
	case 2:
		return InfusionConstants::L2_ENERGY_MULT; // 1.30x
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

			// TODO: Integrate with SkillEffectManager when API is available
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Would apply %d %s status buildup to %d targets"),
				   FinalBuildup, *UEnum::GetValueAsString(Element), Targets.Num());
		}
	}
}

// ========================================
// DEBUG
// ========================================

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

UMovementData *UActionExecutor::GetMovementData(const FAction &Action) const
{
	switch (Action.ActionType)
	{
	case EActionType::Attack:
		return Action.AttackData ? Action.AttackData->MovementData : nullptr;

	case EActionType::Ability:
		// Only return movement data for Melee abilities
		if (Action.AbilityData && Action.AbilityData->IsMelee())
		{
			return Action.AbilityData->ApproachData;
		}
		return nullptr;

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
		// Only return execution range for Melee abilities
		if (Action.AbilityData && Action.AbilityData->IsMelee())
		{
			return Action.AbilityData->ExecutionRange;
		}
		return 0.0f; // Non-melee abilities don't approach

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

// ========================================
// Movement BINDING
// ========================================

void UActionExecutor::BindMovementComplete(AActor *Actor)
{
	if (UCombatMovementComponent *Movement = GetMovementComponent(Actor))
	{
		// Unbind any existing
		UnbindMovementComplete(Actor);

		// Bind to approach complete
		Movement->OnMovementComplete.AddDynamic(this, &UActionExecutor::OnMovementComplete);

		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Bound to OnMovementComplete for %s"), *Actor->GetName());
	}
}

void UActionExecutor::UnbindMovementComplete(AActor *Actor)
{
	if (UCombatMovementComponent *Movement = GetMovementComponent(Actor))
	{
		Movement->OnMovementComplete.RemoveDynamic(this, &UActionExecutor::OnMovementComplete);
	}
}

void UActionExecutor::OnMovementComplete()
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] OnMovementComplete called but no active context"));
		return;
	}

	AActor *Actor = PendingExecutionActor;
	UCharacterData *CharData = PendingExecutionCharData;
	const FAction &Action = CurrentExecutionContext->Action;

	if (!Actor || !CharData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] OnMovementComplete - missing actor or char data"));
		CancelAsyncAction();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Approach complete - executing %s"), *Action.GetActionName());

	// Unbind approach delegate
	UnbindMovementComplete(Actor);

	// Bind to action animation end BEFORE executing (so we catch the animation)
	BindActionAnimationEnd(Actor);

	// Execute the action (plays animation)
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
	default:
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Unexpected action type in OnMovementComplete"));
		UnbindActionAnimationEnd(Actor);
		FinalizeAsyncAction();
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

	// Check if animation was actually played.
	// If not waiting for animation (no montage), gate on defenses via TryFinalize.
	if (!bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No animation to wait for"));
		if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->AreAllDefensesResolved())
		{
			bAllDefensesResolved = true;
		}
		TryFinalizeAsyncAction();
	}
	// Otherwise, OnActionAnimationEnded will call TryFinalizeAsyncAction
}

UCombatAnimInstance *UActionExecutor::GetCombatAnimInstance(AActor *Actor) const
{
	ACharacter *Character = Cast<ACharacter>(Actor);
	if (Character && Character->GetMesh())
	{
		return Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}
	return nullptr;
}

void UActionExecutor::PlayActionMontageOnActor(AActor *Actor, UAnimMontage *Montage, float PlayRate)
{
	if (!Actor || !Montage)
	{
		return;
	}

	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		CombatAnim->PlayActionMontage(Montage, PlayRate);
		return;
	}

	// Fallback: Direct character montage
	ACharacter *Character = Cast<ACharacter>(Actor);
	if (Character)
	{
		Character->PlayAnimMontage(Montage, PlayRate);
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing montage %s via fallback"), *Montage->GetName());
	}
}

void UActionExecutor::BindActionAnimationEnd(AActor *Actor)
{
	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		// Remove first to prevent duplicate binding error
		CombatAnim->OnActionMontageEnded.RemoveDynamic(this, &UActionExecutor::OnActionAnimationEnded);
		CombatAnim->OnActionMontageEnded.AddDynamic(this, &UActionExecutor::OnActionAnimationEnded);
		bWaitingForAnimationEnd = true;
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Bound to OnActionMontageEnded for %s"), *Actor->GetName());
	}
}

void UActionExecutor::UnbindActionAnimationEnd(AActor *Actor)
{
	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		CombatAnim->OnActionMontageEnded.RemoveDynamic(this, &UActionExecutor::OnActionAnimationEnded);
	}
	bWaitingForAnimationEnd = false;
}

void UActionExecutor::OnActionAnimationEnded(UAnimMontage *Montage, bool bInterrupted)
{
	// ADD THIS BEFORE the bWaitingForAnimationEnd check:
	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] OnActionAnimationEnded called - bWaitingForAnimationEnd: %s, PendingActor: %s"),
		   bWaitingForAnimationEnd ? TEXT("TRUE") : TEXT("FALSE"),
		   PendingExecutionActor ? *PendingExecutionActor->GetName() : TEXT("NULL"));

	if (!bWaitingForAnimationEnd)
	{
		return;
	}

	AActor *Actor = PendingExecutionActor;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Action animation ended%s - finalizing"),
		   bInterrupted ? TEXT(" (interrupted)") : TEXT(""));

	// Restore facing to enemy after action animation
	if (Actor)
	{
		UCombatGridSubsystem *Grid = GetWorld()->GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
		if (Grid)
		{
			Grid->UpdateActorFacing(Actor, FVector::ZeroVector);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Restored facing for %s after animation"),
				   *Actor->GetName());
		}
	}

	// Unbind action animation
	if (Actor)
	{
		UnbindActionAnimationEnd(Actor);
	}

	// Cleanup spell notify binding
	if (PendingSpellCaster)
	{
		UnbindSpellNotify(PendingSpellCaster);
		ClearPendingSpellData();
	}

	// Animation done — gate on defenses too. TryFinalize fires only if defenses already resolved.
	TryFinalizeAsyncAction();
}

void UActionExecutor::BindSpellNotify(AActor *Actor)
{
	UCombatAnimInstance *AnimInst = GetCombatAnimInstance(Actor);
	if (AnimInst)
	{
		AnimInst->OnActionNotify.AddDynamic(this, &UActionExecutor::OnSpellAnimNotify);
	}
}

void UActionExecutor::UnbindSpellNotify(AActor *Actor)
{
	UCombatAnimInstance *AnimInst = GetCombatAnimInstance(Actor);
	if (AnimInst)
	{
		AnimInst->OnActionNotify.RemoveDynamic(this, &UActionExecutor::OnSpellAnimNotify);
	}
}

void UActionExecutor::ClearPendingSpellData()
{
	PendingSpellCaster = nullptr;
	PendingSpellData = nullptr;
	PendingSpellTargets.Empty();
	PendingSpellSize = 1.0f;
	PendingSpellDamage = 0;
	bPendingSpellIsBrokenDarkness = false;
}

void UActionExecutor::OnSpellAnimNotify(FName NotifyName)
{
	if (!PendingSpellCaster || !PendingSpellData)
	{
		return;
	}

	if (NotifyName == FName("SpellCastStart"))
	{
		// Spawn muzzle/charging VFX at caster
		if (PendingSpellData->MuzzleVFX)
		{
			FVector SpawnLocation = PendingSpellCaster->GetActorLocation();
			// TODO: Get hand socket location if available

			FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(
				PendingSpellData->Element, bPendingSpellIsBrokenDarkness);

			UNiagaraComponent *MuzzleComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				PendingSpellData->MuzzleVFX,
				SpawnLocation,
				PendingSpellCaster->GetActorRotation(),
				FVector(PendingSpellSize),
				true, true);

			if (MuzzleComp)
			{
				MuzzleComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			}

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpellCastStart - Muzzle VFX spawned"));
		}
	}
	else if (NotifyName == FName("SpellRelease"))
	{
		// Spawn projectile/main spell VFX
		SpawnSpellVFX(PendingSpellCaster, PendingSpellData, PendingSpellSize,
					  PendingSpellTargets, PendingSpellDamage);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpellRelease - Main spell VFX spawned"));
	}
}

// ==================== ABILITY EFFECT SYSTEM ====================

int32 UActionExecutor::GetUniqueEffectID()
{
	return ++EffectIDCounter;
}

TArray<AActor *> UActionExecutor::GetAllEnemies(AActor *User, int32 UserTeam)
{
	TArray<AActor *> Enemies;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return Enemies;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return Enemies;

	// Get combatants from both teams and filter
	for (int32 TeamIdx = 0; TeamIdx <= 1; TeamIdx++)
	{
		if (TeamIdx != UserTeam)
		{
			TArray<AActor *> TeamMembers = TurnMgr->GetTeamMembers(TeamIdx);
			Enemies.Append(TeamMembers);
		}
	}

	return Enemies;
}

TArray<AActor *> UActionExecutor::GetAllAllies(AActor *User, int32 UserTeam)
{
	TArray<AActor *> Allies;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return Allies;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return Allies;

	Allies = TurnMgr->GetTeamMembers(UserTeam);

	return Allies;
}

TArray<AActor *> UActionExecutor::GetAllCombatants()
{
	TArray<AActor *> All;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return All;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return All;

	// Get both teams
	All.Append(TurnMgr->GetTeamMembers(0));
	All.Append(TurnMgr->GetTeamMembers(1));

	return All;
}

void UActionExecutor::GetEffectTargets(
	AActor *User,
	const TArray<AActor *> &ActionTargets,
	ETargetType TargetType,
	int32 UserTeam,
	TArray<AActor *> &OutTargets)
{
	OutTargets.Empty();

	switch (TargetType)
	{
	case ETargetType::Self:
		OutTargets.Add(User);
		break;

	case ETargetType::SingleEnemy:
		// Use first action target (the enemy we attacked)
		if (ActionTargets.Num() > 0)
		{
			OutTargets.Add(ActionTargets[0]);
		}
		break;

	case ETargetType::AllEnemies:
		OutTargets = GetAllEnemies(User, UserTeam);
		break;

	case ETargetType::SingleAlly:
		// For abilities, SingleAlly typically means self
		// Could expand for ally selection in future
		OutTargets.Add(User);
		break;

	case ETargetType::AllAllies:
		OutTargets = GetAllAllies(User, UserTeam);
		break;

	case ETargetType::Everyone:
		OutTargets = GetAllCombatants();
		break;
	}
}

void UActionExecutor::ApplyAbilityEffects(
	AActor *User,
	const TArray<AActor *> &Targets,
	UAbilityData *Ability,
	FActionResult &Result,
	bool bCausedDeath)
{
	if (!Ability || Ability->Effects.Num() == 0)
	{
		return;
	}

	USkillEffectManager *StatusMgr = GetSkillEffectManager();
	if (!StatusMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ApplyAbilityEffects - No SkillEffectManager"));
		return;
	}

	// Get user's team for targeting
	UGameInstance *GI = GetGameInstance();
	UTurnManager *TurnMgr = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
	int32 UserTeam = TurnMgr ? TurnMgr->GetActorTeam(User) : 0;

	for (const FAbilityEffect &Effect : Ability->Effects)
	{
		if (!Effect.IsValid())
		{
			continue;
		}

		// Check condition
		bool bConditionMet = false;
		switch (Effect.Condition)
		{
		case EPassiveTrigger::Always:
			bConditionMet = true;
			break;

		case EPassiveTrigger::OnHit:
			bConditionMet = Result.TotalDamageDealt > 0;
			break;

		case EPassiveTrigger::OnCrit:
			bConditionMet = Result.bWasCritical;
			break;

		case EPassiveTrigger::OnKill:
			bConditionMet = bCausedDeath;
			break;

		default:
			// Other triggers not applicable to ability effects
			bConditionMet = false;
			break;
		}

		if (!bConditionMet)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Effect %s condition not met"),
				   *UEnum::GetValueAsString(Effect.EffectType));
			continue;
		}

		// Determine effect targets
		TArray<AActor *> EffectTargets;
		GetEffectTargets(User, Targets, Effect.Target, UserTeam, EffectTargets);

		if (EffectTargets.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No targets found for effect %s"),
				   *UEnum::GetValueAsString(Effect.EffectType));
			continue;
		}

		// Handle drain effects specially
		if (Effect.IsDrain() && Effect.Condition == EPassiveTrigger::OnHit)
		{
			int32 DrainAmount = FMath::RoundToInt(Result.TotalDamageDealt * Effect.DrainPercent);

			if (Effect.EffectType == EStatusType::HealthRestore)
			{
				// Heal the user
				UCharacterDataComponent *CharComp = User->FindComponentByClass<UCharacterDataComponent>();
				if (CharComp)
				{
					CharComp->ServerHeal(DrainAmount);

					OnHealingDone.Broadcast(User, User, DrainAmount);
					UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Drain healed %s for %d HP (%.0f%% of %d damage)"),
						   *User->GetName(), DrainAmount, Effect.DrainPercent * 100.0f, Result.TotalDamageDealt);
				}
			}
			else if (Effect.EffectType == EStatusType::EnergyRestore)
			{
				// Restore energy to user
				UCharacterDataComponent *CharComp = User->FindComponentByClass<UCharacterDataComponent>();
				if (CharComp)
				{
					CharComp->ServerGainEnergy(DrainAmount);
					UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Drain restored %s for %d EP (%.0f%% of %d damage)"),
						   *User->GetName(), DrainAmount, Effect.DrainPercent * 100.0f, Result.TotalDamageDealt);
				}
			}
			continue;
		}

		// Apply effect to each target as a status effect
		for (AActor *EffectTarget : EffectTargets)
		{
			FStatusEffect StatusEffect = FStatusEffect::CreateFromSpellEffect(
				Ability->AbilityName + TEXT(" Effect"),
				GetUniqueEffectID(),
				Effect.EffectType,
				Effect.Magnitude,
				FMath::RoundToInt(Effect.Magnitude * 100.0f), // Convert to percentage value
				Effect.Duration,
				ESpellElement::Generic, // Abilities don't have inherent element
				EStatusEffectTiming::StartOfOwnTurn);

			StatusMgr->ApplyEffect(EffectTarget, StatusEffect, User, Ability->AbilityName, UserTeam);
			Result.StatusEffectsApplied++;

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Applied %s to %s"),
				   *Effect.GetDescription(), *EffectTarget->GetName());
		}
	}
}

// ============================================================
// COMMIT-TIME COST APPLICATION (Phase 4c)
// ============================================================

void UActionExecutor::ApplyCommitCosts(AActor *Actor, const FAction &Action)
{
	if (!Actor)
	{
		return;
	}

	// Determine the infusion level for this action type.
	// Spells use SpellInfusionLevel; abilities/attacks use AbilityInfusionLevel.
	int32 Level = 0;
	if (Action.ActionType == EActionType::Spell)
	{
		Level = Action.SpellInfusionLevel;
	}
	else if (Action.ActionType == EActionType::Ability ||
			 Action.ActionType == EActionType::Attack)
	{
		Level = Action.AbilityInfusionLevel;
	}

	// Level 0 = no infusion, no commit cost regardless of source
	if (Level <= 0)
	{
		return;
	}

	// Route by source
	switch (Action.SelectedSource)
	{
	case EInfusionSourceOption::None:
		// No source means no infusion cost path. Should not normally reach here
		// with Level > 0 — log to surface inconsistencies.
		UE_LOG(LogTemp, Verbose,
			   TEXT("[ActionExecutor] %s: SelectedSource=None but InfusionLevel=%d — no cost applied"),
			   *Actor->GetName(), Level);
		break;

	case EInfusionSourceOption::Raw:
	case EInfusionSourceOption::Innate:
		// Raw and Innate both pay HP. Same formula via UInfusionCostHelper.
		ApplyHPCostInternal(Actor, Level);
		break;

	case EInfusionSourceOption::ActiveRing:
	{
		// Ring sources pay durability wear on the active ring's slotted crystal.
		// Action tier resolution per Phase 4d Path A:
		//   - Spell: SpellData->Tier (the spell's own tier)
		//   - Ability/Attack: Weapon->Tier (action tier inherits from weapon)
		URingManager *RingMgr = GetRingManager();
		if (!RingMgr)
		{
			break;
		}

		URingData *Ring = RingMgr->GetActiveRing(Actor);
		if (!Ring)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: ActiveRing infusion but no active ring resolved"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		EItemTier ActionTier = EItemTier::F_Tier; // sensible default

		if (bIsSpell && Action.SpellData)
		{
			ActionTier = Action.SpellData->Tier;
		}
		else
		{
			// Ability or attack: action tier inherits from active weapon.
			if (UWeaponManager *WeaponMgr = GetWeaponManager())
			{
				if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
				{
					ActionTier = Weapon->Tier;
				}
			}
		}

		RingMgr->ProcessPostCastWear(Actor, Ring, ActionTier, Level, bIsSpell);

		// Per-action stat modifiers are computed by ComputeActionStatModifiers
		// at action start (ExecuteActionAsync) and stashed on
		// FActionExecutionContext::ActionMods. No commit-time call needed here.
		break;
	}
	case EInfusionSourceOption::PrimaryRing:
	{
		// Primary Ring source: Generic/Caster with a ring in their primary slot.
		// Mirrors ActiveRing structure — same wear path, same Iolite check,
		// just resolves the primary ring instead of active.
		URingManager *RingMgr = GetRingManager();
		if (!RingMgr)
		{
			break;
		}

		URingData *Ring = RingMgr->GetPrimaryRing(Actor);
		if (!Ring)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: PrimaryRing infusion but no primary ring resolved"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		EItemTier ActionTier = EItemTier::F_Tier;

		if (bIsSpell && Action.SpellData)
		{
			ActionTier = Action.SpellData->Tier;
		}
		else
		{
			if (UWeaponManager *WeaponMgr = GetWeaponManager())
			{
				if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
				{
					ActionTier = Weapon->Tier;
				}
			}
		}

		RingMgr->ProcessPostCastWear(Actor, Ring, ActionTier, Level, bIsSpell);

		// Per-action stat modifiers live on the execution context (action-start), not here.
		break;
	}

	case EInfusionSourceOption::WeaponCrystal:
	{
		// Weapon crystal infusion: durability wear on the slotted crystal.
		// Path A: action tier inherits from the weapon (UWeaponData::Tier),
		// uniformly for spells, abilities, and attacks.
		UWeaponManager *WeaponMgr = GetWeaponManager();
		if (!WeaponMgr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but WeaponManager unavailable"),
				   *Actor->GetName());
			break;
		}

		UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor);
		if (!Weapon)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but no active weapon"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		WeaponMgr->ProcessPostCastWear(Actor, Weapon->Tier, Level, bIsSpell);

		// Per-action stat modifiers live on the execution context (action-start), not here.
		break;
	}

	case EInfusionSourceOption::Evolution:
	{
		// Phase 6 — Evolution backlash:
		//   L1: 5% HP + 15 self-status build (using evolution element)
		//   L2: 10% HP + 25 self-status build
		// HP percentages match Innate/Raw, so ApplyHPCostInternal works unchanged.
		// Self-status build is logged as intent — actual application pending the
		// element-to-status mapping system (separate workstream).

		// 1. Resolve evolution element from active weapon's slotted crystal
		//    for logging; needed by the future status-build wiring.
		UWeaponManager *WeaponMgr = GetWeaponManager();
		if (!WeaponMgr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: Evolution infusion but WeaponManager unavailable"),
				   *Actor->GetName());
			break;
		}

		UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor);
		if (!Weapon || !Weapon->IsEvolved() || !Weapon->SlottedCrystal)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: Evolution infusion but weapon is not evolved or no slotted crystal"),
				   *Actor->GetName());
			break;
		}

		const ESpellElement EvolutionElement = Weapon->SlottedCrystal->GetAssociatedElement();

		// 2. HP cost — same percentages as Innate/Raw (5% L1, 10% L2).
		ApplyHPCostInternal(Actor, Level);

		// 3. Self-status build (logged intent, not yet applied — pending element-to-status mapping)
		const float SelfStatusAmount = (Level == 1)
										   ? InfusionConstants::EVOLUTION_L1_SELF_STATUS_BUILD
										   : InfusionConstants::EVOLUTION_L2_SELF_STATUS_BUILD;
		UE_LOG(LogTemp, Log,
			   TEXT("[ActionExecutor] %s Evolution L%d backlash: HP cost applied. Would apply %.0f %s self-status build (pending mapping system)"),
			   *Actor->GetName(), Level, SelfStatusAmount,
			   *UEnum::GetValueAsString(EvolutionElement));

		break;
	}

	default:
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor] %s: Unknown infusion source %d at L%d"),
			   *Actor->GetName(), static_cast<int32>(Action.SelectedSource), Level);
		break;
	}
}

void UActionExecutor::ApplyHPCostInternal(AActor *Actor, int32 Level)
{
	if (!Actor || Level <= 0)
	{
		return;
	}

	const int32 Cost = UInfusionCostHelper::CalculateHPCost(Actor, Level);
	if (Cost <= 0)
	{
		return;
	}

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor] %s: HP cost calculation returned %d but no CharacterDataComponent — cost lost"),
			   *Actor->GetName(), Cost);
		return;
	}

	const int32 Before = CharComp->CurrentHP;

	// Clamp cost to leave at least 1 HP — infusion cannot directly kill the caster.
	// CalculateHPCost already enforces this, but we re-clamp here as defence in depth.
	const int32 SafeCost = FMath::Min(Cost, FMath::Max(0, CharComp->CurrentHP - 1));

	// Route through ServerTakeDamage so OnHPChanged broadcasts and the
	// CharacterPanelWidget HP bar refreshes. Direct CurrentHP mutation bypasses
	// the delegate and leaves the UI stale.
	CharComp->ServerTakeDamage(SafeCost);

	UE_LOG(LogTemp, Log,
		   TEXT("[ActionExecutor] %s paid %d HP for L%d infusion (HP: %d -> %d)"),
		   *Actor->GetName(), SafeCost, Level, Before, CharComp->CurrentHP);
}
FActionStatModifiers UActionExecutor::ComputeActionStatModifiers(const FAction &Action, AActor *Actor) const
{
	FActionStatModifiers Result;

	if (!Actor)
		return Result;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->CharacterData)
		return Result;

	// 1. Reality innate — Refractor (Caster + InnateElement = Reality)
	if (CharComp->CharacterData->CharacterClass == ECharacterClass::Caster &&
		CharComp->CharacterData->InnateElement == ESpellElement::Reality)
	{
		Result.AddFlatPercent(RealityBoost::INNATE_PERCENT);
	}

	// 2. Evolution crystal slotted as primary
	UItemData *PrimaryEvolutionCrystal = nullptr;
	if (ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		const FCombatLoadout Loadout = LC->GetActiveLoadout();
		if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
		{
			PrimaryEvolutionCrystal = Loadout.PrimaryEvolution;
		}
	}

	if (PrimaryEvolutionCrystal && PrimaryEvolutionCrystal->bIsEvolutionCrystal)
	{
		// Authored stats apply at full magnitude.
		Result.Accumulate(PrimaryEvolutionCrystal->GetActionModifiers(1.0f));

		// If the slotted Evolution crystal is Reality-element, add the slotted bonus.
		if (PrimaryEvolutionCrystal->GetAssociatedElement() == ESpellElement::Reality)
		{
			Result.AddFlatPercent(RealityBoost::SLOTTED_PERCENT);
		}
	}

	// 3. Infusion contributions — only when an infusion is active on this action.
	const int32 InfusionLevel = (Action.ActionType == EActionType::Spell)
									? Action.SpellInfusionLevel
									: Action.AbilityInfusionLevel;

	if (InfusionLevel > 0 && Action.SelectedSource != EInfusionSourceOption::None)
	{
		const ESpellElement SourceElement = GetElementForSourceOption(Actor, Action.SelectedSource);

		// Reality crystal infused — flat percent, scaled by infusion level.
		if (SourceElement == ESpellElement::Reality)
		{
			const float Pct = (InfusionLevel == 1)
								  ? RealityBoost::L1_PERCENT
								  : RealityBoost::L2_PERCENT;
			Result.AddFlatPercent(Pct);
		}

		// TODO: Evolution-infused authored stats. Needs source-crystal resolution
		// (WeaponCrystal → WeaponManager active weapon's slotted crystal,
		//  ActiveRing/PrimaryRing → ring's slotted crystal). Stubbed pending the
		//  source-crystal lookup helper. Reality flat-bump above already covers
		//  the most common Reality-via-infusion case.
		// UItemData *InfusionCrystal = ResolveInfusionCrystal(Actor, Action);
		// if (InfusionCrystal && InfusionCrystal->bIsEvolutionCrystal)
		// {
		//     const float InfusionMultiplier = (InfusionLevel == 1) ? 0.5f : 1.0f;
		//     Result.Accumulate(InfusionCrystal->GetActionModifiers(InfusionMultiplier));
		// }
	}

	return Result;
}

void UActionExecutor::DebugAsyncState()
{
	if (CurrentExecutionContext.IsSet())
	{
		const FActionExecutionContext &Ctx = CurrentExecutionContext.GetValue();
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ASYNC STATE:"));
		UE_LOG(LogTemp, Warning, TEXT("  InProgress: %s"), Ctx.bInProgress ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("  Executor: %s"), Ctx.Executor.IsValid() ? *Ctx.Executor->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Warning, TEXT("  Action: %s"), *Ctx.Action.GetActionName());
		UE_LOG(LogTemp, Warning, TEXT("  Duration: %.2fs"), FPlatformTime::Seconds() - Ctx.StartTime);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No async action in progress"));
	}
}

void UActionExecutor::DebugForceResetAsync()
{
	if (CurrentExecutionContext.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] FORCE RESETTING stuck async action"));
		CurrentExecutionContext.Reset();
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
		bWaitingForAnimationEnd = false;
	}
}