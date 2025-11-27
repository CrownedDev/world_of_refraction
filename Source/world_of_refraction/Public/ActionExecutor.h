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
class URingManager;

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
	FActionValidationResult ValidateAction(AActor *Actor, const FAction &Action);

	/** Calculate action's energy cost (including infusion multipliers) */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Validation")
	int32 CalculateActionEnergyCost(AActor *Actor, const FAction &Action);

	// ========================================
	// EXECUTION - MAIN ENTRY POINTS
	// ========================================

	/** Execute a combat action synchronously */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteAction(AActor *Actor, const FAction &Action);

	/**
	 * Execute a combat action asynchronously (for defense window integration)
	 * @param Actor The actor performing the action
	 * @param Action The action to execute
	 * @param OnComplete Callback when action fully completes (after defense resolution)
	 */
	void ExecuteActionAsync(AActor *Actor, const FAction &Action, FOnActionComplete OnComplete);

	// ========================================
	// INFUSION SYSTEM
	// ========================================

	/** Get spell infusion size multiplier (1.0, 1.5, 2.0) - LEGACY, use GetSpellSizeMultiplier */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionSizeMultiplier(int32 InfusionLevel);

	/** Get spell infusion cost multiplier (1.0, 1.3, 1.6) - LEGACY, use GetSpellSizeEnergyCostMultiplier */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionCostMultiplier(int32 InfusionLevel);

	/** Get ability power infusion damage multiplier (1.0, 1.3, 1.6) - LEGACY */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetAbilityPowerInfusionDamageMultiplier(int32 InfusionLevel);

	/** Get ability power infusion cost multiplier (1.0, 1.3, 1.6) - LEGACY */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetAbilityPowerInfusionCostMultiplier(int32 InfusionLevel);

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

	/** Check if character has Ilodite crystal equipped */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	bool HasIloditeEquipped(AActor *Actor) const;

	/** Get energy cost multiplier for infusion level */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetInfusionEnergyCostMultiplier(int32 InfusionLevel);

	/** Get spell size multiplier for spell size infusion level */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetSpellSizeMultiplier(int32 SpellSizeInfusionLevel);

	/** Get energy cost multiplier for spell size infusion */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	static float GetSpellSizeEnergyCostMultiplier(int32 SpellSizeInfusionLevel);

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

	/** Get the RingManager */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Utility")
	URingManager *GetRingManager() const;

	/** Get the WeaponManager */
	UWeaponManager *GetWeaponManager() const;

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

	/** Broadcast when infusion L2 cost is applied */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnInfusionCostApplied OnInfusionCostApplied;

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

	UFUNCTION(BlueprintCallable, Category = "Action Executor|Debug", CallInEditor)
	void DebugPrintInfusionInfo(AActor *Actor) const;

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

	// ========================================
	// INFUSION INTERNAL HELPERS
	// ========================================

	/** Apply infusion effects to action result (damage/status boost) */
	void ApplyInfusionEffects(
		FActionResult &Result,
		AActor *Actor,
		EInfusionType Type,
		int32 Level);

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

	/** Apply Ilodite L2 stat buff */
	void ApplyIloditeStatBuff(AActor *Actor);

	/** Apply self-damage for HP costs */
	void ApplySelfDamage(AActor *Actor, int32 Amount);

	/** Apply self-status buildup (Evolution L2) */
	void ApplySelfStatusBuildup(AActor *Actor, ESpellElement Element, int32 Amount);

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