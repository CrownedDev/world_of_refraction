// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EActionType.h"
#include "AbilityEffectType.h"
#include "SpellElement.h"
#include "ActionExecutor.generated.h"

class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;
class USpellData;
class UAbilityData;
class UItemData;
class UBaseAttackData;
class UUltimateData;
class UItemExecutor;
class UWeaponManager;

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
 *   - StatusEffectManager: Apply status effects, check stun/silence
 *   - CharacterDataComponent: HP/EP changes
 *   - Data Assets: SpellData, AbilityData, etc. for calculations
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

	/**
	 * Validate an action before execution
	 * Checks: energy, cooldowns, requirements, status effects (stun/silence)
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Validation")
	FActionValidationResult ValidateAction(AActor *Actor, const FAction &Action) const;

	/** Check if actor can perform any action (not stunned) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Validation")
	bool CanActorAct(AActor *Actor) const;

	/** Check if actor can cast spells (not silenced) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Validation")
	bool CanActorCastSpells(AActor *Actor) const;

	/** Calculate energy cost for an action */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Validation")
	int32 CalculateActionEnergyCost(AActor *Actor, const FAction &Action) const;

	// ========================================
	// EXECUTION - MAIN ENTRY POINT
	// ========================================

	/**
	 * Execute an action synchronously (for simple cases/testing)
	 * Automatically routes to appropriate Execute* function
	 */
	UFUNCTION(BlueprintCallable, Category = "Action Executor")
	FActionResult ExecuteAction(AActor *Actor, const FAction &Action);

	/**
	 * Execute an action asynchronously (recommended for gameplay)
	 * Handles animation timing, defense windows, and callbacks
	 * @param Actor The actor performing the action
	 * @param Action The action to execute
	 * @param OnComplete Callback when action fully completes (after defense resolution)
	 */
	void ExecuteActionAsync(AActor *Actor, const FAction &Action, FOnActionComplete OnComplete);

	// ========================================
	// INFUSION DATA
	// ========================================

	/** Get spell infusion size multiplier (1.0, 1.5, 2.0) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionSizeMultiplier(int32 InfusionLevel);

	/** Get spell infusion cost multiplier (1.0, 1.3, 1.6) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionCostMultiplier(int32 InfusionLevel);

	/** Get ability power infusion damage multiplier (1.0, 1.3, 1.6) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetAbilityPowerInfusionDamageMultiplier(int32 InfusionLevel);

	/** Get ability power infusion cost multiplier (1.0, 1.3, 1.6) */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetAbilityPowerInfusionCostMultiplier(int32 InfusionLevel);

	// ========================================
	// EXECUTION - SPECIFIC ACTIONS
	// ========================================

	/** Execute a spell */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteSpell(
		AActor *Caster,
		USpellData *Spell,
		const TArray<AActor *> &Targets,
		bool bUseElementalMode = true,
		int32 InfusionLevel = 0);

	/** Execute an ability */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteAbility(
		AActor *User,
		UAbilityData *Ability,
		const TArray<AActor *> &Targets,
		bool bIsElementInfused = false,
		int32 PowerInfusionLevel = 0);

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
		UBaseAttackData *Attack,
		const TArray<AActor *> &Targets,
		bool bIsInfused = false);

	/** Execute an ultimate */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteUltimate(
		AActor *Caster,
		UUltimateData *Ultimate,
		const TArray<AActor *> &Targets);

	/** Execute defend action */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteDefend(AActor *Defender);

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

	/**
	 * Broadcast when a defense window should open
	 * DefenseSystem should bind to this to handle Block/Parry/Dodge
	 */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnDefenseWindowRequested OnDefenseWindowRequested;

	// ========================================
	// ANIMATION/VFX STUBS (Override in subclass or bind via events)
	// ========================================

	/** Called when spell animation should play. Override or bind OnActionStarted for custom handling. */
	virtual void PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize);

	/** Called when spell VFX should spawn. Override or bind OnActionStarted for custom handling. */
	virtual void SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize);

	/** Called when ability animation should play */
	virtual void PlayAbilityAnimation(AActor *User, UAbilityData *Ability);

	/** Called when attack animation should play */
	virtual void PlayAttackAnimation(AActor *Attacker, UBaseAttackData *Attack);

	// ========================================
	// DEBUG
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Action Executor|Debug", CallInEditor)
	void DebugPrintActionResult(const FActionResult &Result) const;

private:
	// ========================================
	// INTERNAL HELPERS
	// ========================================

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Get CharacterData template from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Calculate critical hit */
	bool RollCriticalHit(AActor *Attacker) const;

	/** Apply critical damage multiplier */
	int32 ApplyCriticalMultiplier(int32 Damage, AActor *Attacker) const;

	/** Apply defense reduction */
	int32 ApplyDefense(int32 Damage, AActor *Defender, bool bIsElemental) const;

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

	/** Cached reference to StatusEffectManager */
	UPROPERTY()
	UStatusEffectManager *StatusEffectManagerRef = nullptr;

	/** Cached reference to ItemExecutor */
	UPROPERTY()
	UItemExecutor *ItemExecutorRef = nullptr;

	/** Cached reference to WeaponManager */
	UPROPERTY()
	UWeaponManager *WeaponManagerRef = nullptr;

	/** Get ItemExecutor subsystem */
	UItemExecutor *GetItemExecutor() const;

	/** Get WeaponManager subsystem */
	UWeaponManager *GetWeaponManager() const;

	// ========================================
	// ASYNC EXECUTION STATE
	// ========================================

	/** Pending async action completion callback */
	FOnActionComplete PendingActionCallback;

	/** Current action being executed asynchronously */
	FActionResult PendingActionResult;

	/** Number of pending defense windows to resolve */
	int32 PendingDefenseCount = 0;

	/** Timer handles for staggered target hits */
	TArray<FTimerHandle> PendingHitTimers;

	/** Complete the async action and fire callback */
	void CompleteAsyncAction();

	/** Handle defense resolution for a single target (called by DefenseSystem) */
	void OnDefenseResolved(AActor *Target, int32 FinalDamage, bool bWasDodged, bool bWasBlocked, bool bWasParried);
};
