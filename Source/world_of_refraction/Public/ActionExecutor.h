// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EActionType.h"
#include "AbilityEffectType.h"
#include "SpellElement.h"
#include "EInfusionType.h"
#include "EInfusionSource.h"
#include "ECharacterClass.h"
#include "InfusionConstants.h"
#include "EDefenseType.h"
#include "DefenseSystem.h"
#include "DamageCalculator.h"
#include "SpellProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "EInfusionSourceOption.h"
#include "LoadoutComponent.h"
#include "CombatMovementComponent.h"
#include "ApproachData.h"
#include "ActionExecutor.generated.h"

class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;
class USpellData;
class UAbilityData;
class UItemData;
class UWeaponAttackData;
class UItemExecutor;
class UWeaponManager;
class URingManager;
class UDefenseSystem;
class UBrokenDarknessManager;
struct FHybridSpellColorData;
struct FDefenseResult;
struct FActionExecutionContext;
struct FPendingDefenseContext;
class UCombatAnimInstance;

// ========================================
// DELEGATES
// ========================================

/** Broadcast when action execution starts (for animations/VFX) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnActionStarted, AActor *, Executor, const FAction &, Action, int32, EnergyCost);

/** Broadcast when action execution completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionCompleted, AActor *, Executor, const FActionResult &, Result);

/** Broadcast when damage is dealt (for floating numbers, hit effects) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDamageDealt, AActor *, Attacker, AActor *, Target, int32, Damage, bool, bCritical);

/** Broadcast when healing is done */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealingDone, AActor *, Healer, AActor *, Target, int32, Amount);

/** Broadcast when a target dies from action */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetKilled, AActor *, Killer, AActor *, Victim);

/** Callback for async action completion */
DECLARE_DELEGATE_OneParam(FOnActionComplete, const FActionResult &);

/** Broadcast when defense window should open (for DefenseSystem integration) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDefenseWindowRequested, AActor *, Attacker, AActor *, Defender, float, AttackSize, int32, BaseDamage);

/** Broadcast when infusion L2 cost is applied (HP damage, break chance, self-status) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnInfusionCostApplied, AActor *, Actor, EInfusionSource, Source, int32, HPCost, float, BreakChanceIncrease);

/**
 * UActionExecutor
 *
 * GameInstanceSubsystem that handles all combat action execution.
 * Validates actions, calculates damage/effects, applies results.
 *
 * Usage:
 *   UActionExecutor* Executor = GetGameInstance()->GetSubsystem<UActionExecutor>();
 *   FActionValidationResult Validation = Executor->ValidateAction(Actor, Action);
 *   if (Validation.bIsValid)
 *       FActionResult Result = Executor->ExecuteAction(Actor, Action);
 *
 * Integrations:
 *   - StatusEffectManager: Apply status effects, check stun/silence
 *   - CharacterDataComponent: HP/EP changes
 *   - Data Assets: SpellData, AbilityData, etc.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UActionExecutor : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// VALIDATION
	// ========================================

	/** Validate if an action can be executed */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Validation")
	FActionValidationResult ValidateAction(AActor *Actor, const FAction &Action) const;

	/** Calculate action's energy cost (including infusion multipliers) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Validation")
	int32 CalculateActionEnergyCost(AActor *Actor, const FAction &Action) const;

	/** Check if actor can perform any action (not stunned/dead) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Validation")
	bool CanActorAct(AActor *Actor) const;

	/** Check if actor can cast spells (not silenced) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Validation")
	bool CanActorCastSpells(AActor *Actor) const;

	// ========================================
	// EXECUTION - MAIN ENTRY POINTS
	// ========================================

	void ExecuteActionAsync(AActor *Actor, const FAction &Action, FOnActionComplete OnComplete);
	/** Execute a combat action synchronously */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteAction(AActor *Actor, const FAction &Action);

	// ========================================
	// INFUSION SYSTEM
	// ========================================

	/** Get spell infusion size multiplier (1.0, 1.5, 2.0) - LEGACY, use GetSpellSizeMultiplier */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionSizeMultiplier(int32 InfusionLevel);

	/** Get spell infusion cost multiplier (1.0, 1.3, 1.6) - LEGACY, use GetSpellSizeEnergyCostMultiplier */
	UFUNCTION(BlueprintCallable, Category = "Action Execfutor|Infusion")
	static float GetSpellInfusionCostMultiplier(int32 InfusionLevel);

	// ---- NEW INFUSION TYPE SYSTEM ----

	/** Check if character can use this infusion type */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	bool CanUseInfusionType(AActor *Actor, EInfusionType Type) const;

	/** Get available infusion types for character */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	TArray<EInfusionType> GetAvailableInfusionTypes(AActor *Actor) const;

	/** Get the element source for a character (for determining L2 costs) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	EInfusionSource GetInfusionSource(AActor *Actor) const;

	/** Get the element source for a specific spell (handles evolution weapon spells) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	EInfusionSource GetSpellSource(AActor *Actor, USpellData *Spell) const;

	/** Get element for the character's current infusion source */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	ESpellElement GetInfusionElement(AActor *Actor) const;

	/** Check if character can use element infusion (has an element source) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	bool CanUseElementInfusion(AActor *Actor) const;

	/** Check if character has Iolite crystal equipped (physical enhancement) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	bool HasIoliteEquipped(AActor *Actor) const;

	/** Get energy cost multiplier for infusion level */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetInfusionEnergyCostMultiplier(int32 InfusionLevel);

	/** Get spell size multiplier for spell size infusion level */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetSpellSizeMultiplier(int32 SpellSizeInfusionLevel);

	/** Get energy cost multiplier for spell size infusion */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetSpellSizeEnergyCostMultiplier(int32 SpellSizeInfusionLevel);

	/** Get available infusion sources for character based on class and loadout */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	TArray<EInfusionSourceOption> GetAvailableInfusionSources(AActor *Actor) const;

	/** Map source option to infusion source (for L2 cost determination) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	EInfusionSource MapSourceOptionToSource(EInfusionSourceOption Option) const;

	/** Map source option to infusion type (Physical or Element) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	EInfusionType MapSourceOptionToType(EInfusionSourceOption Option) const;

	/** Get element for selected source option */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	ESpellElement GetElementForSourceOption(AActor *Actor, EInfusionSourceOption Option) const;

	/** Check if weapon stats apply for this source */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	bool DoWeaponStatsApply(EInfusionSourceOption Option) const;

	// ==================== MOVEMENT ====================
	/** Set arena center for movement calculations */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Movement")
	void SetArenaCenter(const FVector &ArenaCenter) { CachedArenaCenter = ArenaCenter; }

	// ========================================
	// EXECUTION - SPECIFIC ACTIONS
	// ========================================

	/** Default projectile class to spawn (set to BP_Projectile1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell Delivery")
	TSubclassOf<ASpellProjectile> DefaultProjectileClass;

	/** Execute a spell */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteSpell(
		AActor *Caster,
		USpellData *Spell,
		const TArray<AActor *> &Targets,
		int32 InfusionLevel = 0);

	/** Execute an ability */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteAbility(
		AActor *User,
		UAbilityData *Ability,
		const TArray<AActor *> &Targets,
		int32 AbilityInfusionLevel = 0,
		EInfusionSourceOption SelectedSource = EInfusionSourceOption::None);

	/** Execute an item */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteItem(
		AActor *User,
		UItemData *Item,
		const TArray<AActor *> &Targets);

	/** Execute a basic attack */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteAttack(
		AActor *Attacker,
		UWeaponAttackData *Attack,
		const TArray<AActor *> &Targets,
		bool bIsInfused = false);

	/** Execute defend action */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteDefend(AActor *Defender);

	// ========================================
	// POST-CAST PROCESSING
	// ========================================

	/**
	 * Process post-cast logic based on spell source
	 * - Innate: No action
	 * - Ring: Break check
	 * - Evolution: TBD
	 * - Item: Consume item
	 */
	void ProcessPostCastBySource(AActor *Caster, USpellData *Spell, ESpellSource Source, bool bWasInfused);

	// ========================================
	// DAMAGE APPLICATION
	// ========================================

	/**
	 * Apply damage to target
	 * Handles defense calculations, critical hits, death
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Damage")
	FCombatHitResult ApplyDamage(
		AActor *Attacker,
		AActor *Target,
		int32 BaseDamage,
		bool bIsElemental,
		ESpellElement Element,
		bool bCanCrit);

	/** Apply damage with defaults (non-elemental, can crit) */
	FCombatHitResult ApplyDamage(
		AActor *Attacker,
		AActor *Target,
		int32 BaseDamage,
		bool bIsElemental)
	{
		return ApplyDamage(Attacker, Target, BaseDamage, bIsElemental, ESpellElement::Generic, true);
	}

	/**
	 * Apply healing to target
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Damage")
	FCombatHitResult ApplyHealing(
		AActor *Healer,
		AActor *Target,
		int32 BaseHealing);

	// ========================================
	// UTILITY
	// ========================================

	/** Get the StatusEffectManager */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	UStatusEffectManager *GetStatusEffectManager() const;

	/** Get the RingManager */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	URingManager *GetRingManager() const;

	/** Get the WeaponManager */
	UWeaponManager *GetWeaponManager() const;

	/** Get the DamageCalculator subsystem */
	UDamageCalculator *GetDamageCalculator() const;

	/** Check if target is alive */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	bool IsTargetAlive(AActor *Target) const;

	/** Get all valid targets for an action */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	TArray<AActor *> FilterValidTargets(const TArray<AActor *> &Targets) const;

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionStarted OnActionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionCompleted OnActionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnDamageDealt OnDamageDealt;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnHealingDone OnHealingDone;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnTargetKilled OnTargetKilled;

	/** Broadcast when async action fully completes (after all defense windows) */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionCompleted OnAsyncActionCompleted;

	/**
	 * Broadcast when a defense window should open
	 * DefenseSystem should bind to this to handle Block/Parry/Dodge
	 */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnDefenseWindowRequested OnDefenseWindowRequested;

	/** Broadcast when infusion L2 cost is applied */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnInfusionCostApplied OnInfusionCostApplied;

	// ========================================
	// ANIMATION/VFX STUBS (Override in subclass or bind via events)
	// ========================================

	/** Called when spell animation should play. Override or bind OnActionStarted for custom handling. */
	virtual void PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize);

	/** Called when spell VFX should spawn. Override or bind OnActionStarted for custom handling. */
	void SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize, const TArray<AActor *> &ExplicitTargets, int32 Damage = 0);

	/** Called when ability animation should play */
	virtual void PlayAbilityAnimation(AActor *User, UAbilityData *Ability);

	/** Called when attack animation should play */
	virtual void PlayAttackAnimation(AActor *Attacker, UWeaponAttackData *Attack);

	// ========================================
	// DEBUG
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Action Executor|Debug", CallInEditor)
	void DebugPrintActionResult(const FActionResult &Result) const;

	UFUNCTION(BlueprintCallable, Category = "Action Executor|Debug", CallInEditor)
	void DebugPrintInfusionInfo(AActor *Actor) const;

private:
	// ==================== SPELL VFX NOTIFY STATE ====================

	/** Cached spell data for notify-triggered VFX */
	UPROPERTY()
	AActor *PendingSpellCaster = nullptr;

	UPROPERTY()
	USpellData *PendingSpellData = nullptr;

	UPROPERTY()
	TArray<AActor *> PendingSpellTargets;

	float PendingSpellSize = 1.0f;
	int32 PendingSpellDamage = 0;
	bool bPendingSpellIsBrokenDarkness = false;

	/** Handle animation notify for spell VFX timing */
	UFUNCTION()
	void OnSpellAnimNotify(FName NotifyName);

	/** Bind to CombatAnimInstance notify delegate */
	void BindSpellNotify(AActor *Actor);

	/** Unbind from notify delegate */
	void UnbindSpellNotify(AActor *Actor);

	/** Clear cached spell data */
	void ClearPendingSpellData();

		// ========================================
	// INTERNAL HELPERS
	// ========================================

	UCombatAnimInstance *GetCombatAnimInstance(AActor *Actor) const;
	void PlayActionMontageOnActor(AActor *Actor, UAnimMontage *Montage, float PlayRate = 1.0f);

	ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;

	// New validation method
	bool CanUseAbility(AActor *Actor, UAbilityData *Ability) const;
	bool CanUseSpell(AActor *Actor, USpellData *Spell) const;

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Get CharacterData template from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Calculate critical hit */
	bool RollCriticalHit(AActor *Attacker) const;

	/** Apply damage after defense resolution */
	void ApplyDamageAfterDefense(
		AActor *Attacker,
		AActor *Target,
		const FPendingDefenseContext &Context,
		const FDefenseResult &DefenseResult);

	/** Apply status effects from spell/ability */
	void ApplyStatusEffects(
		AActor *Source,
		AActor *Target,
		EAbilityEffectType PrimaryEffect,
		float PrimaryValue,
		int32 PrimaryDuration,
		EAbilityEffectType SecondaryEffect = EAbilityEffectType::None,
		float SecondaryValue = 0.0f,
		int32 SecondaryDuration = 0,
		ESpellElement Element = ESpellElement::Generic);

	/** Handle multi-hit abilities */
	int32 ProcessMultiHit(
		AActor *Attacker,
		AActor *Target,
		int32 DamagePerHit,
		int32 HitCount,
		bool bIsElemental,
		ESpellElement Element,
		bool bCanCrit,
		FActionResult &OutResult);

	/** Spend energy from actor */
	bool SpendEnergy(AActor *Actor, int32 Amount);

	// ========================================
	// INFUSION INTERNAL HELPERS
	// ========================================

	/** Apply infusion effects to action result (damage/status boost) */
	void ApplyInfusionEffects(
		FActionResult &Result,
		AActor *Actor,
		EInfusionType Type,
		int32 Level);

	/** Apply physical infusion effects (weapon status, damage boost) */
	void ApplyPhysicalInfusion(FActionResult &Result, AActor *Actor, int32 Level);

	/** Apply element infusion effects (element status, damage boost) */
	void ApplyElementInfusion(FActionResult &Result, AActor *Actor, int32 Level);

	/** Apply L2 infusion cost based on source (HP, break chance, or self-status) */
	void ApplyL2InfusionCost(
		FActionResult &Result,
		AActor *Actor,
		EInfusionSource Source);

	/** Apply spell size L2 cost based on source */
	void ApplySpellSizeL2Cost(
		FActionResult &Result,
		AActor *Actor,
		USpellData *Spell);

	/** Apply Iolite L2 stat buff */
	void ApplyIoliteStatBuff(AActor *Actor);

	/** Apply self-damage for HP costs */
	void ApplySelfDamage(AActor *Actor, int32 Amount);

	/** Apply self-status buildup (Evolution L2) */
	void ApplySelfStatusBuildup(AActor *Actor, ESpellElement Element, int32 Amount);

	// ========================================
	// CHARGE INFUSION HELPERS (NEW)
	// ========================================

	/** Get spell charge status multiplier (L1 = 1.5, L2 = 1.0) */
	float GetSpellChargeStatusMultiplier(int32 SpellInfusionLevel) const;

	/** Get spell charge damage multiplier (L1 = 1.0, L2 = 1.3) */
	float GetSpellChargeDamageMultiplier(int32 SpellInfusionLevel) const;

	/** Get ability charge status multiplier (L1 = 1.5, L2 = 0.0) */
	float GetAbilityChargeStatusMultiplier(int32 AbilityInfusionLevel) const;

	/** Get ability charge damage multiplier (L1 = 1.0, L2 = 1.3) */
	float GetAbilityChargeDamageMultiplier(int32 AbilityInfusionLevel) const;

	/** Apply ability infusion status buildup to targets */
	void ApplyAbilityInfusionStatus(
		AActor *User,
		const TArray<AActor *> &Targets,
		EInfusionSourceOption Source,
		int32 HitCount,
		float StatusMultiplier);

	// ========================================
	// CACHED REFERENCES
	// ========================================

	/** Cached reference to StatusEffectManager */
	UPROPERTY()
	UStatusEffectManager *StatusEffectManagerRef = nullptr;

	/** Cached reference to ItemExecutor */
	UPROPERTY()
	UItemExecutor *ItemExecutorRef = nullptr;

	/** Cached reference to WeaponManager */
	UPROPERTY()
	UWeaponManager *WeaponManagerRef = nullptr;

	/** Cached reference to RingManager */
	UPROPERTY()
	URingManager *RingManagerRef = nullptr;

	/** Get ItemExecutor subsystem */
	UItemExecutor *GetItemExecutor() const;

	// ========================================
	// ASYNC EXECUTION SYSTEM
	// ========================================

	/**
	 * Execute a combat action asynchronously with full defense window integration.
	 * Damage is NOT applied until all defense windows resolve.
	 *
	 * Flow:
	 * 1. Validate action, spend energy
	 * 2. Open defense window for each target
	 * 3. Wait for DefenseSystem to resolve all windows
	 * 4. Apply damage based on FDefenseResult (reduced by Block/Parry, 0 if Dodge)
	 * 5. Fire OnComplete callback with final result
	 *
	 * @param Actor The actor performing the action
	 * @param Action The action to execute
	 * @param OnComplete Callback fired when ALL defense windows resolve
	 */

	/**
	 * Check if any async action is currently in progress
	 */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Async")
	bool IsAsyncActionInProgress() const;

	/**
	 * Get the current async execution context (if any)
	 */
	const FActionExecutionContext *GetCurrentExecutionContext() const;

	/**
	 * Cancel current async action (applies full damage, fires callback)
	 * Use for combat end, actor death, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Async")
	void CancelAsyncAction();

private:
	// ========================================
	// ASYNC EXECUTION - INTERNAL STATE
	// ========================================

	/** Current async execution context */
	TOptional<FActionExecutionContext> CurrentExecutionContext;

	/** Callback for current async action */
	FOnActionComplete AsyncActionCallback;

	/** Cached DefenseSystem reference */
	UPROPERTY()
	UDefenseSystem *DefenseSystemRef = nullptr;

	/** Handle for timeout timer */
	FTimerHandle AsyncTimeoutHandle;

	// ========================================
	// APPROACH MOVEMENT BINDING
	// ========================================

	/** Handle for approach complete delegate binding */
	FDelegateHandle ApproachCompleteHandle;

	/** Cached actor for approach completion */
	UPROPERTY()
	AActor *PendingExecutionActor = nullptr;

	/** Cached character data for approach completion */
	UPROPERTY()
	UCharacterData *PendingExecutionCharData = nullptr;

	/** Bind to movement component's OnApproachComplete */
	void BindApproachComplete(AActor *Actor);

	/** Unbind from movement component */
	void UnbindApproachComplete(AActor *Actor);

	/** Called when approach movement completes - executes the actual action */
	UFUNCTION()
	void OnApproachComplete();

private:
	// ==================== ACTION ANIMATION BINDING ====================

	/** Bind to CombatAnimInstance for action completion */
	void BindActionAnimationEnd(AActor *Actor);

	/** Unbind from CombatAnimInstance */
	void UnbindActionAnimationEnd(AActor *Actor);

	/** Called when action animation completes - triggers return movement */
	UFUNCTION()
	void OnActionAnimationEnded(UAnimMontage *Montage, bool bInterrupted);

	/** Track whether we're waiting for animation */
	bool bWaitingForAnimationEnd = false;

	// ========================================
	// ASYNC EXECUTION - INTERNAL METHODS
	// ========================================

	/** Get or cache DefenseSystem */
	UDefenseSystem *GetDefenseSystem() const;

	/** Bind to DefenseSystem events */
	void BindDefenseSystemEvents();

	/** Unbind from DefenseSystem events */
	void UnbindDefenseSystemEvents();

	/**
	 * Called by DefenseSystem when a defense window closes
	 * Applies damage based on defense result, removes from pending list
	 * NOTE: Must be UFUNCTION for dynamic delegate binding
	 */
	UFUNCTION()
	void OnDefenseWindowClosed(AActor *Defender, const FDefenseResult &DefenseResult);

	/**
	 * Check if all defenses resolved, finalize action if so
	 */
	void CheckAndFinalizeAsyncAction();

	/**
	 * Finalize async action - apply remaining effects, fire callback
	 */
	void FinalizeAsyncAction();

	/**
	 * Called when async action times out (failsafe)
	 */
	void OnAsyncActionTimeout();

	/**
	 * Execute spell action asynchronously (opens defense windows)
	 */
	void ExecuteSpellAsync(AActor *Caster, const FAction &Action, UCharacterData *CasterData);

	/**
	 * Execute ability action asynchronously (opens defense windows)
	 */
	void ExecuteAbilityAsync(AActor *User, const FAction &Action, UCharacterData *UserData);

	/**
	 * Execute attack action asynchronously (opens defense windows)
	 */
	void ExecuteAttackAsync(AActor *Attacker, const FAction &Action, UCharacterData *AttackerData);

	/**
	 * Open defense windows for all targets (does NOT apply damage yet)
	 * @param Attacker The attacking actor
	 * @param Targets List of targets
	 * @param AttackSize Size of attack (for dodge threshold)
	 * @param BaseDamage Base damage before defense
	 * @param DamagePerHit Damage per hit (for multi-hit)
	 * @param HitCount Number of hits
	 * @param bIsElemental Is this elemental damage?
	 * @param Element Element type
	 * @param bCanCrit Can hits critically strike?
	 * @param WindowDuration Defense window duration
	 */
	void OpenDefenseWindowsForTargets(
		AActor *Attacker,
		const TArray<AActor *> &Targets,
		float AttackSize,
		int32 BaseDamage,
		int32 DamagePerHit,
		int32 HitCount,
		bool bIsElemental,
		ESpellElement Element,
		bool bCanCrit,
		float WindowDuration = 0.3f);

	// == == == == == == == == == == == == == == == == == == == ==
	// BROKEN DARKNESS INTEGRATION
	// ========================================

	/**
	 * Get BrokenDarknessManager component from actor
	 */
	UBrokenDarknessManager *GetBrokenDarknessManager(AActor *Actor) const;

	/**
	 * Check and roll for Broken Darkness transformation
	 * Called at start of action execution
	 * Triggers on: underpowered cast, L2 infusion
	 */
	void CheckBrokenDarknessBreak(AActor *Actor, const FAction &Action, UCharacterData *CharData);

	/**
	 * Process forbidden element cast for BD characters
	 * Applies self-damage if casting Dark Light or Dark Void
	 */
	void ProcessForbiddenElementCast(AActor *Actor, ESpellElement Element, float BaseDamage);

	// ==================== MOVEMENT INTEGRATION ====================
	/** Get approach data from action data */
	UApproachData *GetApproachData(const FAction &Action) const;

	/** Get execution range from action data */
	float GetExecutionRange(const FAction &Action) const;

	/** Get movement component from actor */
	UCombatMovementComponent *GetMovementComponent(AActor *Actor) const;

	/** Signal movement component that action execution is done */
	void SignalActionComplete(AActor *Actor);

	/** Arena center for movement calculations */
	FVector CachedArenaCenter = FVector::ZeroVector;

protected:
	/**
	 * Spawn spell delivery based on DeliveryType
	 * - Projectile/Homing/Beam: Spawns ASpellProjectile
	 * - AOE/Instant: Spawns VFX directly, opens defense window
	 */
	virtual void SpawnSpellDelivery(
		AActor *Caster,
		const TArray<AActor *> &Targets,
		USpellData *Spell,
		float FinalImpactRadius,
		float FinalVisualScale,
		int32 FinalDamage,
		bool bIsBrokenDarkness);

	/** Spawn projectile actor (Projectile/Homing/Beam) */
	void SpawnProjectileActor(
		AActor *Caster,
		AActor *Target,
		USpellData *Spell,
		float FinalImpactRadius,
		float FinalVisualScale,
		int32 FinalDamage,
		bool bIsBrokenDarkness);

	/** Spawn AOE effect (no projectile) */
	void SpawnAOEEffect(
		AActor *Caster,
		AActor *Target,
		USpellData *Spell,
		float FinalImpactRadius,
		float FinalVisualScale,
		int32 FinalDamage,
		bool bIsBrokenDarkness);

	/** Resolve instant spell (immediate hit) */
	void ResolveInstantSpell(
		AActor *Caster,
		AActor *Target,
		USpellData *Spell,
		float FinalImpactRadius,
		int32 FinalDamage,
		bool bIsBrokenDarkness);

	/** Spawn support spell VFX (Self/Ally - no defense window) */
	void SpawnSupportSpellEffect(
		AActor *Caster,
		const TArray<AActor *> &Targets,
		USpellData *Spell,
		float FinalVisualScale,
		bool bIsBrokenDarkness);

	/** Called when projectile impacts target */
	UFUNCTION()
	void OnProjectileImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage);

	/** Called when target dodged projectile */
	UFUNCTION()
	void OnProjectileDodged(AActor *Target, FVector ImpactLocation);

	/** Called each tick while beam is active */
	UFUNCTION()
	void OnBeamTick(AActor *Target, float DeltaTime, bool bTargetInBeam);
};