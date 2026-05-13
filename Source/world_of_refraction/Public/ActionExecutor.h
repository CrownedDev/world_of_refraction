// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EActionType.h"
#include "ESkillEffectType.h"
#include "ESpellElement.h"
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
#include "MovementData.h"
#include "EAbilityExecutionType.h"
#include "FSkillEffect.h"
#include "InfusionCostHelper.h"
#include "ActionStatModifiers.h"
#include "ActionExecutor.generated.h"

class UCharacterDataComponent;
class UCharacterData;
class USkillEffectManager;
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
 *   - SkillEffectManager: Apply status effects, check stun/silence
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
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionCostMultiplier(int32 InfusionLevel);

	// ---- NEW INFUSION TYPE SYSTEM ----

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

	/** Execute an item */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteItem(
		AActor *User,
		UItemData *Item,
		const TArray<AActor *> &Targets);

	/** Execute defend action */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteDefend(AActor *Defender);

	// ========================================
	// COMMIT-TIME COST APPLICATION
	// ========================================

	/**
	 * Apply costs based on the action's infusion source.
	 * Called early in ExecuteAction / ExecuteActionAsync, after validation but
	 * before the action actually fires. Source routing:
	 *   None / Item       → no cost
	 *   Raw / Innate      → HP cost via UInfusionCostHelper
	 *   ActiveRing/Secondary → ring crystal wear via URingManager
	 *   WeaponCrystal     → TODO Phase 4d
	 *   Evolution         → TODO Phase 6 (HP + self-status)
	 *
	 * Mutates actor state directly (HP, crystal durability). Does not return
	 * success/failure — costs always apply. Modal warnings (Phase 5) intercept
	 * before ExecuteAction is called, not here.
	 */
	void ApplyCommitCosts(AActor *Actor, const FAction &Action);

	/** HP cost deduction helper. Reads cost from UInfusionCostHelper, mutates
	 *  CharacterDataComponent::CurrentHP. Floored at 1 HP by the helper. */
	void ApplyHPCostInternal(AActor *Actor, int32 Level);

	/** Compute the full per-action stat modifier accumulator for an action.
	 *  Walks Reality innate, Reality slotted, Reality infused, Evolution slotted,
	 *  and Evolution-infused (stub). Returns a populated FActionStatModifiers. */
	FActionStatModifiers ComputeActionStatModifiers(const FAction &Action, AActor *Actor) const;

	// ========================================
	// POST-CAST PROCESSING
	// ========================================

	/**
	 * Process post-cast logic based on spell source.
	 * Phase 4c: Ring crystal wear MOVED to ApplyCommitCosts (commit-time).
	 * This function now handles only post-success consumption (e.g. Item).
	 */
	void ProcessPostCastBySource(AActor *Caster, USpellData *Spell, ESpellSource Source, int32 InfusionLevel);

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
		ESpellElement Element,
		bool bCanCrit);

	/** Apply damage with defaults (Generic element, can crit) */
	FCombatHitResult ApplyDamage(
		AActor *Attacker,
		AActor *Target,
		int32 BaseDamage)
	{
		return ApplyDamage(Attacker, Target, BaseDamage, ESpellElement::Generic, true);
	}

	/**
	 * Apply healing to target
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Damage")
	FCombatHitResult ApplyHealing(
		AActor *Healer,
		AActor *Target,
		int32 BaseHealing);

	/**
	 * ApplyHit — unified single-hit applicator. Phase A of the ApplyHit
	 * consolidation; see docs/analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md
	 * Section 8 for the migration plan.
	 *
	 * Status: production applicator for all async damage (Phase B) and for spell
	 * + async-attack buildup (Phase C1, C3). Sync Spell/Ability/Attack paths and
	 * ApplySpellStatusBuildup were removed in Phase D. Remaining legacy slated for
	 * Phase E review: ApplyDamage, ProcessMultiHit, ApplyDamageAfterDefense.
	 *
	 * Single-hit by contract — multi-hit looping stays in ProcessMultiHit / the
	 * orchestrators. Routes damage through UDamageCalculator and buildup through
	 * USkillEffectManager::AddStatusBuildup; both are unchanged primitives.
	 */
	FCombatHitResult ApplyHit(const FActionHitInput &Input);

	// ========================================
	// UTILITY
	// ========================================

	/** Get the SkillEffectManager */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	USkillEffectManager *GetSkillEffectManager() const;

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

	// ========================================
	// ANIMATION/VFX STUBS (Override in subclass or bind via events)
	// ========================================

	/** Called when spell animation should play. Override or bind OnActionStarted for custom handling.
	 *  Play rate = CalculateSpellSpeed() × ActionMods.SpellSpeed contribution. */
	virtual void PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize, const FActionStatModifiers &ActionMods = FActionStatModifiers());

	/** Called when spell VFX should spawn. Override or bind OnActionStarted for custom handling. */
	void SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize, const TArray<AActor *> &ExplicitTargets, int32 Damage = 0);

	/** Called when ability animation should play.
	 *  Play rate = 1.0 × CalculateAnimationSpeed() × ActionMods.ActionSpeed contribution.
	 *  (CalculateAnimationSpeed derives from the ActionSpeed sub-stat.) */
	virtual void PlayAbilityAnimation(AActor *User, UAbilityData *Ability, const FActionStatModifiers &ActionMods = FActionStatModifiers());

	/** Called when attack animation should play.
	 *  Play rate = Attack->BaseAnimSpeed × CalculateAnimationSpeed() × ActionMods.ActionSpeed contribution.
	 *  Preserves designer-tuned per-attack pacing; layers stat scaling on top. */
	virtual void PlayAttackAnimation(AActor *Attacker, UWeaponAttackData *Attack, const FActionStatModifiers &ActionMods = FActionStatModifiers());

	// ========================================
	// DEBUG
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Action Executor|Debug", CallInEditor)
	void DebugPrintActionResult(const FActionResult &Result) const;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void DebugAsyncState();

	UFUNCTION(Exec)
	void DebugForceResetAsync();

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

	// ==================== SKILL EFFECT APPLICATION ====================

	/** Apply all effects from an action's Effects[] array.
	 *  Walks the array, gates each effect on its Condition, routes to
	 *  SkillEffectManager. Drain effects heal/restore the User (attacker),
	 *  not the target. Called from FinalizeAsyncAction post-defense for
	 *  abilities, spells, and weapon attacks.
	 *
	 *  SourceName is the display string used for effect log lines and
	 *  the FActiveSkillEffect.EffectName ("<SourceName> Effect"). Passed as
	 *  FString so the caller picks the right name from whichever data
	 *  asset it's iterating (Ability->Name, Spell->Name, etc).
	 */
	void ApplySkillEffects(
		AActor *User,
		const TArray<AActor *> &Targets,
		const TArray<FSkillEffect> &Effects,
		const FString &SourceName,
		FActionResult &Result,
		bool bCausedDeath);

	/** Get targets for an effect based on ETargetType */
	void GetEffectTargets(
		AActor *User,
		const TArray<AActor *> &ActionTargets,
		ETargetType TargetType,
		int32 UserTeam,
		TArray<AActor *> &OutTargets);

	/** Get all enemies for targeting */
	TArray<AActor *> GetAllEnemies(AActor *User, int32 UserTeam);

	/** Get all allies for targeting */
	TArray<AActor *> GetAllAllies(AActor *User, int32 UserTeam);

	/** Get all combatants */
	TArray<AActor *> GetAllCombatants();

	/** Generate unique effect ID for status effects */
	int32 GetUniqueEffectID();

	/** Unique effect ID counter */
	int32 EffectIDCounter = 0;

	// ========================================
	// INTERNAL HELPERS
	// ========================================

	UCombatAnimInstance *GetCombatAnimInstance(AActor *Actor) const;
	void PlayActionMontageOnActor(AActor *Actor, UAnimMontage *Montage, float PlayRate = 1.0f);

	ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;

	/** Resolve the source crystal UItemData* for the given action's infusion source.
	 *  Mirrors GetElementForSourceOption's dispatch shape — returns the crystal
	 *  rather than its element. Returns nullptr for non-crystal sources
	 *  (None/Raw/Innate) or when the resolver lookup fails (no equipped ring etc.). */
	UItemData *ResolveInfusionCrystal(AActor *Actor, const FAction &Action) const;

	// New validation method
	bool CanUseAbility(AActor *Actor, UAbilityData *Ability) const;
	bool CanUseSpell(AActor *Actor, USpellData *Spell) const;

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Get CharacterData template from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Apply damage after defense resolution */
	void ApplyDamageAfterDefense(
		AActor *Attacker,
		AActor *Target,
		const FPendingDefenseContext &Context,
		const FDefenseResult &DefenseResult);

	/** Handle multi-hit abilities */
	int32 ProcessMultiHit(
		AActor *Attacker,
		AActor *Target,
		int32 DamagePerHit,
		int32 HitCount,
		ESpellElement Element,
		bool bCanCrit,
		FActionResult &OutResult);

	/** Spend energy from actor */
	bool SpendEnergy(AActor *Actor, int32 Amount);

	// ========================================
	// INFUSION INTERNAL HELPERS
	// ========================================

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
	/** Get ability charge energy cost multiplier (L1 = 1.0, L2 = 1.5) */
	float GetAbilityChargeCostMultiplier(int32 Level) const;

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

	/** Cached reference to SkillEffectManager */
	UPROPERTY()
	USkillEffectManager *SkillEffectManagerRef = nullptr;

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

	/** True once OnDefenseWindowClosed has been bound to DefenseSystem. Idempotent guard. */
	bool bDefenseEventsBound = false;

	/** Handle for timeout timer */
	FTimerHandle AsyncTimeoutHandle;

	// ========================================
	// Movement BINDING
	// ========================================

	/** Handle for approach complete delegate binding */
	FDelegateHandle MovementCompleteHandle;

	/** Cached actor for approach completion */
	UPROPERTY()
	AActor *PendingExecutionActor = nullptr;

	/** Cached character data for approach completion */
	UPROPERTY()
	UCharacterData *PendingExecutionCharData = nullptr;

	/** Bind to movement component's OnMovementComplete */
	void BindMovementComplete(AActor *Actor);

	/** Unbind from movement component */
	void UnbindMovementComplete(AActor *Actor);

	/** Called when Movement completes - executes the actual action */
	UFUNCTION()
	void OnMovementComplete();

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

	/** True when all defense windows have resolved. Coordinated with bWaitingForAnimationEnd
	 *  via TryFinalizeAsyncAction — finalize only when both are done. */
	bool bAllDefensesResolved = false;

	/** Finalize iff both defenses resolved AND animation finished (or no animation). */
	void TryFinalizeAsyncAction();

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

	/** Execute item with animation - no movement required */
	void ExecuteItemAsync(AActor *Actor, const FAction &Action, UCharacterData *CharData);

	// ==================== ASYNC EXECUTOR SHARED HELPERS ====================

	/** Reject early when an action carries infusion intent but the data asset
	 *  refuses it. Returns true if the action should proceed, false if rejected
	 *  (sets PartialResult error and calls FinalizeAsyncAction). Shared by all
	 *  three async executors. Pass InfusionLevel=0 for attacks (no charge concept). */
	bool ValidateInfusionGate(const FAction &Action, bool bImmuneToInfusion, int32 InfusionLevel);

	/** Combined immunity: true if the action itself is immune OR the wielder's
	 *  active weapon / active ring is immune. Equipment immunity blocks infusion
	 *  only — status buildup is unaffected (bIsRawMode is the only buildup gate). */
	bool IsInfusionImmune(AActor *User, bool bActionImmune) const;

	/** Sync PartialResult.BaseDamageBeforeDefense and compute DamagePerHit after
	 *  raw-mode redirect has folded any buildup into damage. Shared post-redirect
	 *  cleanup across all three async paths.
	 *  Assumes CurrentExecutionContext.IsSet(). */
	void FinalizeDamageInputs(int32 FinalDamage, int32 HitCount, int32& OutDamagePerHit);

	/** Unified log line for async action dispatch. Replaces three near-identical
	 *  trailing UE_LOG calls in the async executors. Spell size is dropped from
	 *  the unified log — was diagnostic noise, not behavioural. */
	void LogActionDispatch(
		EActionType ActionType,
		int32 InfusionLevel,
		int32 FinalDamage,
		int32 NumTargets) const;

	// ==================== RETURN MOVEMENT TRACKING ====================

	/** Track if we're waiting for return movement */
	bool bWaitingForReturn = false;

	/** Cached result to send after return completes */
	FActionResult PendingFinalResult;

	/** Called when return movement completes */
	UFUNCTION()
	void OnReturnComplete();

	/** Final completion after return (or immediate if no return needed) */
	void CompleteAsyncActionFinal(AActor *Executor);

	/**
	 * Open defense windows for all targets (does NOT apply damage yet)
	 * @param Attacker The attacking actor
	 * @param Targets List of targets
	 * @param AttackSize Size of attack (for dodge threshold)
	 * @param BaseDamage Base damage before defense
	 * @param DamagePerHit Damage per hit (for multi-hit)
	 * @param HitCount Number of hits
	 * @param Element Element type
	 * @param bCanCrit Can hits critically strike?
	 * @param ActionType Spell/Ability/Attack — drives ApplyHit stat selection post-defense
	 * @param InfusionLevel 0–2; spells/abilities carry their charge level, attacks pass 0
	 * @param SelectedSource Infusion source for future buildup routing (Phase C)
	 * @param BaseStatusBuildup Per-target buildup amount (defense modifies this) — Phase C1
	 * @param PhysicalDamageType Physical type (Session Y) — drives bar-cap trigger
	 *                           when Element is Generic. None for spells/abilities.
	 * @param WindowDuration Defense window duration
	 */
	void OpenDefenseWindowsForTargets(
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
		EPhysicalDamageType PhysicalDamageType,
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
	UMovementData *GetMovementData(const FAction &Action) const;

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