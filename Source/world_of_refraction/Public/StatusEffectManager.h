// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StatusEffect.h"
#include "StatusEffectManager.generated.h"

class UCharacterDataComponent;

// ========================================
// DELEGATE DECLARATIONS
// ========================================

/** Broadcast when an effect is applied to an actor */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectApplied, AActor*, Target, const FStatusEffect&, Effect);

/** Broadcast when an effect is removed from an actor */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectRemoved, AActor*, Target, const FStatusEffect&, Effect);

/** Broadcast when an effect triggers/processes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectTriggered, AActor*, Target, const FStatusEffect&, Effect);

/** Broadcast when effect stacks change */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEffectStacksChanged, AActor*, Target, const FStatusEffect&, Effect, int32, NewStacks);

/** Broadcast when an effect's duration changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEffectDurationChanged, AActor*, Target, const FStatusEffect&, Effect, int32, RemainingTurns);

// ========================================
// ENUMS
// ========================================

/** Result of attempting to apply an effect */
UENUM(BlueprintType)
enum class EEffectApplicationResult : uint8
{
	Applied UMETA(DisplayName = "Applied (New Effect)"),
	StackAdded UMETA(DisplayName = "Stack Added"),
	DurationRefreshed UMETA(DisplayName = "Duration Refreshed"),
	Rejected UMETA(DisplayName = "Rejected (Max Stacks/Immunity)")
};

/**
 * UStatusEffectManager
 *
 * GameInstanceSubsystem that manages all status effects on all actors.
 * Central authority for buff/debuff/DOT tracking during combat.
 *
 * Design Principles:
 * - Durations tick on AFFECTED actor's turn (not global turn count)
 * - Stacking rules configurable per effect
 * - Integrates with EPassiveTrigger for conditional effects
 * - Server-authoritative for multiplayer
 *
 * Usage:
 * - CombatOrchestrator calls ProcessStartOfTurnEffects/ProcessEndOfTurnEffects
 * - ActionExecutor calls ApplyEffect when spells/abilities hit
 * - UI binds to events for visual feedback
 */
UCLASS()
class WORLD_OF_REFRACTION_API UStatusEffectManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ========================================
	// SUBSYSTEM LIFECYCLE
	// ========================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// EFFECT APPLICATION
	// ========================================

	/**
	 * Apply a status effect to a target actor
	 * Handles stacking, duration refresh, and immunity checks
	 *
	 * @param Target Actor to apply effect to
	 * @param Effect The effect to apply (copied, so modifications are safe)
	 * @param Source Actor applying the effect (for attribution)
	 * @param SourceAbility Name of spell/ability applying effect
	 * @param SourceTeam Team index of source (-1 if unknown)
	 * @return Result indicating how the effect was handled
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	EEffectApplicationResult ApplyEffect(AActor* Target, FStatusEffect Effect,
		AActor* Source = nullptr, const FString& SourceAbility = TEXT(""), int32 SourceTeam = -1);

	/**
	 * Apply multiple effects at once (for abilities with multiple effects)
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void ApplyEffects(AActor* Target, const TArray<FStatusEffect>& Effects,
		AActor* Source = nullptr, const FString& SourceAbility = TEXT(""), int32 SourceTeam = -1);

	/**
	 * Apply effects from a spell (Primary + Secondary)
	 * Convenience wrapper for ActionExecutor integration
	 *
	 * @param Target Actor to apply effects to
	 * @param SpellName Name for display/tracking
	 * @param SpellID Unique spell identifier
	 * @param PrimaryType Primary effect type
	 * @param PrimaryMagnitude Primary magnitude (0.0-1.0)
	 * @param PrimaryValue Primary flat value
	 * @param PrimaryDuration Primary duration in turns
	 * @param SecondaryType Secondary effect type (None to skip)
	 * @param SecondaryMagnitude Secondary magnitude
	 * @param SecondaryValue Secondary flat value
	 * @param SecondaryDuration Secondary duration
	 * @param Element Spell element for DOT typing
	 * @param Source Caster actor
	 * @param SourceTeam Caster team index
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void ApplySpellEffects(
		AActor* Target,
		const FString& SpellName,
		int32 SpellID,
		EAbilityEffectType PrimaryType,
		float PrimaryMagnitude,
		int32 PrimaryValue,
		int32 PrimaryDuration,
		EAbilityEffectType SecondaryType,
		float SecondaryMagnitude,
		int32 SecondaryValue,
		int32 SecondaryDuration,
		ERefractionElement Element,
		AActor* Source = nullptr,
		int32 SourceTeam = -1);

	/**
	 * Apply infusion DOT from ability
	 * Call when an infused ability hits a target
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void ApplyInfusionDOT(
		AActor* Target,
		const FString& AbilityName,
		int32 AbilityID,
		ERefractionElement InfusedElement,
		float DOTDamage,
		int32 Duration,
		AActor* Source = nullptr,
		int32 SourceTeam = -1);

	/**
	 * Apply all passive effects from an evolution
	 * Call when evolution is activated on a character
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void ApplyEvolutionPassives(
		AActor* Target,
		const FString& EvolutionName,
		int32 EvolutionID,
		const TArray<EAbilityEffectType>& PassiveTypes,
		const TArray<float>& PassiveValues);

	// ========================================
	// WEAPON EFFECT APPLICATION
	// ========================================

	/**
	 * Apply weapon stat bonuses as permanent effects
	 * Call when weapon is equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Weapons")
	void ApplyWeaponBonuses(
		AActor* Target,
		const FString& WeaponName,
		int32 WeaponID,
		int32 BonusAttack,
		int32 BonusDefense,
		int32 BonusMagicPower,
		int32 BonusSpeed,
		float BonusCritChance,
		float BonusCritDamage);

	/**
	 * Remove weapon bonuses when weapon is unequipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Weapons")
	void RemoveWeaponBonuses(AActor* Target, int32 WeaponID);

	/**
	 * Apply physical damage type status effect (Generic character weapon attacks)
	 * Slash → Bleed, Pierce → Armor Break, Blunt → Stun
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Weapons")
	void ApplyPhysicalDamageEffect(
		AActor* Target,
		const FString& WeaponName,
		int32 WeaponID,
		uint8 PhysicalType,
		int32 StatusBuildup,
		float InfusionMultiplier,
		int32 HitCount,
		AActor* Source = nullptr,
		int32 SourceTeam = -1);

	/**
	 * Apply infusion DOT through weapon (with weapon's InfusionStatusMultiplier)
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Weapons")
	void ApplyWeaponInfusionDOT(
		AActor* Target,
		const FString& AbilityName,
		const FString& WeaponName,
		int32 AbilityID,
		ERefractionElement InfusedElement,
		float BaseDOTDamage,
		int32 Duration,
		float WeaponInfusionMultiplier,
		AActor* Source = nullptr,
		int32 SourceTeam = -1);

	// ========================================
	// EFFECT REMOVAL
	// ========================================

	/** Remove a specific effect by ID */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	bool RemoveEffectByID(AActor* Target, int32 EffectID);

	/** Remove all effects with a specific name */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveEffectsByName(AActor* Target, const FString& EffectName);

	/** Remove all effects of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveEffectsByType(AActor* Target, EAbilityEffectType EffectType);

	/** Remove all buffs from target */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveAllBuffs(AActor* Target);

	/** Remove all debuffs from target */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveAllDebuffs(AActor* Target);

	/** Remove all DOTs from target */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveAllDOTs(AActor* Target);

	/** Remove ALL effects from target (cleanse, death, combat end) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void RemoveAllEffects(AActor* Target);

	/** Remove all effects from ALL actors (combat end cleanup) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	void ClearAllEffects();

	/** Remove effects applied by a specific source (when source dies, etc.) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects")
	int32 RemoveEffectsBySource(AActor* Source);

	// ========================================
	// TURN PROCESSING (Called by CombatOrchestrator)
	// ========================================

	/**
	 * Process all start-of-turn effects for an actor
	 * Called at the beginning of the actor's turn
	 *
	 * @param Actor The actor whose turn is starting
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Turn Processing")
	void ProcessStartOfTurnEffects(AActor* Actor);

	/**
	 * Process all end-of-turn effects for an actor
	 * Called at the end of the actor's turn, also ticks durations
	 *
	 * @param Actor The actor whose turn is ending
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Turn Processing")
	void ProcessEndOfTurnEffects(AActor* Actor);

	/**
	 * Process conditional trigger effects
	 * Called when a trigger event occurs (crit, kill, damage taken, etc.)
	 *
	 * @param Actor The actor to check effects for
	 * @param Trigger The trigger that occurred
	 * @param TriggerValue Optional value for threshold checks (HP%, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Turn Processing")
	void ProcessTriggerEffects(AActor* Actor, EPassiveTrigger Trigger, float TriggerValue = 0.0f);

	// ========================================
	// QUERIES
	// ========================================

	/** Get all active effects on an actor */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	TArray<FStatusEffect> GetActiveEffects(AActor* Actor) const;

	/** Get all effects of a specific type on actor */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	TArray<FStatusEffect> GetEffectsByType(AActor* Actor, EAbilityEffectType EffectType) const;

	/** Check if actor has a specific effect by ID */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	bool HasEffectByID(AActor* Actor, int32 EffectID) const;

	/** Check if actor has any effect of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	bool HasEffectOfType(AActor* Actor, EAbilityEffectType EffectType) const;

	/** Get total stat modifier from all effects of a category */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	float GetTotalStatModifier(AActor* Actor, EAbilityEffectType ModifierType) const;

	/** Count active effects on actor */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	int32 GetEffectCount(AActor* Actor) const;

	/** Count buffs on actor */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	int32 GetBuffCount(AActor* Actor) const;

	/** Count debuffs on actor */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Query")
	int32 GetDebuffCount(AActor* Actor) const;

	// ========================================
	// STATUS CHECKS (Common Gameplay Queries)
	// ========================================

	/** Check if actor is stunned (cannot act) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Status Checks")
	bool IsStunned(AActor* Actor) const;

	/** Check if actor is silenced (cannot cast spells) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Status Checks")
	bool IsSilenced(AActor* Actor) const;

	/** Check if actor is rooted (cannot move/dodge) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Status Checks")
	bool IsRooted(AActor* Actor) const;

	/** Check if actor has any DOT effects */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Status Checks")
	bool HasActiveDOT(AActor* Actor) const;

	/** Check if actor is immune to a specific effect type */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Status Checks")
	bool IsImmuneToEffectType(AActor* Actor, EAbilityEffectType EffectType) const;

	// ========================================
	// EVENTS
	// ========================================

	/** Broadcast when effect applied */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnEffectApplied OnEffectApplied;

	/** Broadcast when effect removed */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnEffectRemoved OnEffectRemoved;

	/** Broadcast when effect processes (deals damage, applies buff, etc.) */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnEffectTriggered OnEffectTriggered;

	/** Broadcast when stacks change */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnEffectStacksChanged OnEffectStacksChanged;

	/** Broadcast when duration changes */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnEffectDurationChanged OnEffectDurationChanged;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	/** Print all active effects for an actor to log */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Debug", CallInEditor)
	void DebugPrintEffects(AActor* Actor) const;

	/** Print all effects for all tracked actors */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Debug", CallInEditor)
	void DebugPrintAllEffects() const;

	/** Get formatted string of all effects on actor (for UI) */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Debug")
	FString GetEffectsSummary(AActor* Actor) const;

private:
	// ========================================
	// INTERNAL DATA
	// ========================================

	/** Map of all active effects per actor (not exposed to reflection - TArray in TMap not supported) */
	TMap<TWeakObjectPtr<AActor>, TArray<FStatusEffect>> ActiveEffects;

	/** Next unique effect instance ID (for distinguishing same-type effects) */
	int32 NextInstanceID = 1;

	/** Generate a unique instance ID */
	int32 GenerateInstanceID() { return NextInstanceID++; }

	// ========================================
	// INTERNAL HELPERS
	// ========================================

	/** Process effects with a specific timing */
	void ProcessEffectsWithTiming(AActor* Actor, EStatusEffectTiming Timing);

	/** Tick durations for actor's effects (called at end of turn) */
	void TickDurations(AActor* Actor);

	/** Apply the actual effect logic (damage, heal, stat mod, etc.) */
	void ApplyEffectLogic(AActor* Actor, FStatusEffect& Effect);

	/** Check if a trigger condition is met */
	bool IsTriggerConditionMet(AActor* Actor, const FStatusEffect& Effect, float TriggerValue = 0.0f) const;

	/** Find existing effect by ID on actor */
	FStatusEffect* FindEffectByID(AActor* Actor, int32 EffectID);

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent* GetCharacterDataComponent(AActor* Actor) const;

	/** Clean up null actor references */
	void CleanupInvalidActors();

	/** Reset turn flags for all effects on actor */
	void ResetTurnFlags(AActor* Actor);

	/** Check if effect type affects speed (requires TurnManager notification) */
	bool IsSpeedEffect(EAbilityEffectType EffectType) const;

	/** Notify TurnManager that an actor's speed has changed */
	void NotifySpeedChanged(AActor* Actor);
};