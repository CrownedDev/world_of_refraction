// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActiveSkillEffect.h"
#include "ESkillEffectType.h"
#include "FEquipmentStatBonus.h"
#include "FSkillEffect.h"
#include "SkillEffectManager.generated.h"

class UCharacterDataComponent;
class URingData;

// ========================================
// DELEGATE DECLARATIONS
// ========================================

/** Broadcast when an effect is applied to an actor */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectApplied, AActor *, Target, const FActiveSkillEffect &, Effect);

/** Broadcast when an effect is removed from an actor */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectRemoved, AActor *, Target, const FActiveSkillEffect &, Effect);

/** Broadcast when an effect triggers/processes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEffectTriggered, AActor *, Target, const FActiveSkillEffect &, Effect);

/** Broadcast when effect stacks change */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEffectStacksChanged, AActor *, Target, const FActiveSkillEffect &, Effect, int32, NewStacks);

/** Broadcast when an effect's duration changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEffectDurationChanged, AActor *, Target, const FActiveSkillEffect &, Effect, int32, RemainingTurns);

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
 * USkillEffectManager
 *
 * GameInstanceSubsystem that manages all skill effects on all actors.
 * Central authority for buff/debuff/DOT tracking during combat.
 *
 * Design Principles:
 * - Durations tick on AFFECTED actor's turn (not global turn count)
 * - Stacking rules configurable per effect
 * - Integrates with ESkillTrigger for conditional effects
 * - Server-authoritative for multiplayer
 *
 * Usage:
 * - CombatOrchestrator calls ProcessStartOfTurnEffects/ProcessEndOfTurnEffects
 * - ActionExecutor calls ApplyEffect when spells/abilities hit
 * - UI binds to events for visual feedback
 */
UCLASS()
class WORLD_OF_REFRACTION_API USkillEffectManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ========================================
	// SUBSYSTEM LIFECYCLE
	// ========================================

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** Listener bound to UActionExecutor::OnDamageDealt. Drives OnHit-style
	 *  skill effects: Lifesteal, ApplyBurn/Chill/StunToTarget. */
	UFUNCTION()
	void OnDamageDealtHandler(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical);

	// ========================================
	// EFFECT APPLICATION
	// ========================================

	/**
	 * Apply a skill effect to a target actor
	 * Handles stacking, duration refresh, and immunity checks
	 *
	 * @param Target Actor to apply effect to
	 * @param Effect The effect to apply (copied, so modifications are safe)
	 * @param Source Actor applying the effect (for attribution)
	 * @param SourceAbility Name of spell/ability applying effect
	 * @param SourceTeam Team index of source (-1 if unknown)
	 * @return Result indicating how the effect was handled
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	EEffectApplicationResult ApplyEffect(AActor *Target, FActiveSkillEffect Effect,
										 AActor *Source = nullptr, const FString &SourceAbility = TEXT(""), int32 SourceTeam = -1);

	/**
	 * Apply multiple effects at once (for abilities with multiple effects)
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	void ApplyEffects(AActor *Target, const TArray<FActiveSkillEffect> &Effects,
					  AActor *Source = nullptr, const FString &SourceAbility = TEXT(""), int32 SourceTeam = -1);

	/**
	 * Apply infusion DOT from ability
	 * Call when an infused ability hits a target
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	void ApplyInfusionDOT(
		AActor *Target,
		const FString &AbilityName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float DOTDamage,
		int32 Duration,
		AActor *Source = nullptr,
		int32 SourceTeam = -1);

	/**
	 * Apply all FSkillEffect-authored effects from an evolution crystal.
	 * Call when evolution is activated on a character.
	 *
	 * @param Target Actor receiving the effects
	 * @param EvolutionName Display prefix for effect names
	 * @param EvolutionID Source identifier; effects are tracked at EvolutionID*100 + index
	 * @param Effects Effects to apply (UItemData::Effects)
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Effects")
	void ApplyEvolutionEffects(
		AActor *Target,
		const FString &EvolutionName,
		int32 EvolutionID,
		const TArray<FSkillEffect> &Effects);

	// ========================================
	// EQUIPMENT EFFECT APPLICATION
	// ========================================

	/**
	 * Apply equipment-level skill effects (weapon/ring Effects array).
	 *
	 * @param Target Actor to apply effects to
	 * @param Effects Equipment effects to apply (from ULoadoutComponent::GetActiveEffects)
	 * @param SourceID Per-source identifier (e.g. weapon/ring instance ID).
	 *        Effects are tracked at SourceID*100 + index — pass a value that
	 *        won't collide with evolution effect IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Effects")
	void ApplyEquipmentEffects(AActor *Target, const TArray<FSkillEffect> &Effects, int32 SourceID);

	/** Remove equipment effects applied with the given SourceID. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Effects")
	void RemoveEquipmentEffects(AActor *Target, int32 SourceID);

	// ========================================
	// WEAPON EFFECT APPLICATION
	// ========================================

	/**
	 * Apply weapon stat bonuses as permanent effects
	 * Call when weapon is equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Weapons")
	void ApplyWeaponBonuses(
		AActor *Target,
		const FString &WeaponName,
		int32 WeaponID,
		int32 BonusRawDamage,
		int32 BonusDefense,
		int32 BonusSpellDamage,
		int32 BonusActionSpeed,
		float BonusCritChance);

	/**
	 * Remove weapon bonuses when weapon is unequipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Weapons")
	void RemoveWeaponBonuses(AActor *Target, int32 WeaponID);

	// ========================================
	// RING EFFECT APPLICATION
	// ========================================

	/**
	 * Apply ring stat bonuses as permanent effects (mirrors ApplyWeaponBonuses).
	 * Reads bonus values directly from the supplied StatBonus — pass the
	 * per-instance FRingInventoryEntry::StatBonus so equipped bonuses reflect
	 * runtime rolls rather than the asset's BaseStatBonus / GeneratedStatBonus.
	 * Call when ring is equipped.
	 *
	 * @param Target Actor to apply bonuses to
	 * @param StatBonus Per-instance bonus values — pass Entry.StatBonus
	 * @param RingName Display name for the bonus effects — pass Entry.Ring->Name
	 * @param RingInstanceID Stable per-instance identifier — pass
	 *        FRingInventoryEntry::InstanceID. Must match the value passed
	 *        to RemoveRingBonuses on unequip.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Rings")
	void ApplyRingBonuses(AActor *Target, const FEquipmentStatBonus &StatBonus, const FString &RingName, int32 RingInstanceID);

	/**
	 * Remove ring bonuses when ring is unequipped. RingInstanceID must
	 * match the value passed to ApplyRingBonuses at equip time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Rings")
	void RemoveRingBonuses(AActor *Target, int32 RingInstanceID);

	/**
	 * Apply physical damage type skill effect (Generic character weapon attacks)
	 * Slash → Bleed, Pierce → Armor Break, Impact → Stun
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Weapons")
	void ApplyPhysicalDamageEffect(
		AActor *Target,
		const FString &WeaponName,
		int32 WeaponID,
		uint8 PhysicalType,
		int32 StatusBuildup,
		float InfusionMultiplier,
		int32 HitCount,
		AActor *Source = nullptr,
		int32 SourceTeam = -1);

	/**
	 * Apply infusion DOT through weapon (with weapon's InfusionStatusMultiplier)
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Weapons")
	void ApplyWeaponInfusionDOT(
		AActor *Target,
		const FString &AbilityName,
		const FString &WeaponName,
		int32 AbilityID,
		ESpellElement InfusedElement,
		float BaseDOTDamage,
		int32 Duration,
		float WeaponInfusionMultiplier,
		AActor *Source = nullptr,
		int32 SourceTeam = -1);

	// ========================================
	// EFFECT REMOVAL
	// ========================================

	/** Remove a specific effect by ID */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	bool RemoveEffectByID(AActor *Target, int32 EffectID);

	/** Remove all effects with a specific name */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveEffectsByName(AActor *Target, const FString &EffectName);

	/** Remove all effects of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveEffectsByType(AActor *Target, ESkillEffectType EffectType);

	/** Remove all buffs from target */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveAllBuffs(AActor *Target);

	/** Remove all debuffs from target */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveAllDebuffs(AActor *Target);

	/** Remove all DOTs from target */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveAllDOTs(AActor *Target);

	/** Remove ALL effects from target (cleanse, death, combat end) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	void RemoveAllEffects(AActor *Target);

	/** Remove all effects from ALL actors (combat end cleanup) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	void ClearAllEffects();

	/** Remove effects applied by a specific source (when source dies, etc.) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveEffectsBySource(AActor *Source);

	// ========================================
	// TURN PROCESSING (Called by CombatOrchestrator)
	// ========================================

	/**
	 * Process all start-of-turn effects for an actor
	 * Called at the beginning of the actor's turn
	 *
	 * @param Actor The actor whose turn is starting
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Turn Processing")
	void ProcessStartOfTurnEffects(AActor *Actor);

	/**
	 * Process all end-of-turn effects for an actor
	 * Called at the end of the actor's turn, also ticks durations
	 *
	 * @param Actor The actor whose turn is ending
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Turn Processing")
	void ProcessEndOfTurnEffects(AActor *Actor);

	/**
	 * Process conditional trigger effects
	 * Called when a trigger event occurs (crit, kill, damage taken, etc.)
	 *
	 * @param Actor The actor to check effects for
	 * @param Trigger The trigger that occurred
	 * @param TriggerValue Optional value for threshold checks (HP%, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Turn Processing")
	void ProcessTriggerEffects(AActor *Actor, ESkillTrigger Trigger, float TriggerValue = 0.0f);

	// ========================================
	// QUERIES
	// ========================================

	/** Get all active effects on an actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	TArray<FActiveSkillEffect> GetActiveEffects(AActor *Actor) const;

	/** Get all effects of a specific type on actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	TArray<FActiveSkillEffect> GetEffectsByType(AActor *Actor, ESkillEffectType EffectType) const;

	/** Check if actor has a specific effect by ID */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	bool HasEffectByID(AActor *Actor, int32 EffectID) const;

	/** Check if actor has any effect of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	bool HasEffectOfType(AActor *Actor, ESkillEffectType EffectType) const;

	/** Get total stat modifier from all effects of a category */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	float GetTotalStatModifier(AActor *Actor, ESkillEffectType ModifierType) const;

	/** Count active effects on actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	int32 GetEffectCount(AActor *Actor) const;

	/** Count buffs on actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	int32 GetBuffCount(AActor *Actor) const;

	/** Count debuffs on actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	int32 GetDebuffCount(AActor *Actor) const;

	// ========================================
	// STATUS CHECKS (Common Gameplay Queries)
	// ========================================

	/** Check if actor is stunned (cannot act) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	bool IsStunned(AActor *Actor) const;

	/** Check if actor is silenced (cannot cast spells) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	bool IsSilenced(AActor *Actor) const;

	/** Check if actor has any DOT effects */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	bool HasActiveDOT(AActor *Actor) const;

	/** Check if actor is immune to a specific effect type */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	bool IsImmuneToEffectType(AActor *Actor, ESkillEffectType EffectType) const;

	// ========================================
	// EVENTS
	// ========================================

	/** Broadcast when effect applied */
	UPROPERTY(BlueprintAssignable, Category = "Skill Effects|Events")
	FOnEffectApplied OnEffectApplied;

	/** Broadcast when effect removed */
	UPROPERTY(BlueprintAssignable, Category = "Skill Effects|Events")
	FOnEffectRemoved OnEffectRemoved;

	/** Broadcast when effect processes (deals damage, applies buff, etc.) */
	UPROPERTY(BlueprintAssignable, Category = "Skill Effects|Events")
	FOnEffectTriggered OnEffectTriggered;

	/** Broadcast when stacks change */
	UPROPERTY(BlueprintAssignable, Category = "Skill Effects|Events")
	FOnEffectStacksChanged OnEffectStacksChanged;

	/** Broadcast when duration changes */
	UPROPERTY(BlueprintAssignable, Category = "Skill Effects|Events")
	FOnEffectDurationChanged OnEffectDurationChanged;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	/** Print all active effects for an actor to log */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug", CallInEditor)
	void DebugPrintEffects(AActor *Actor) const;

	/** Print all effects for all tracked actors */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug", CallInEditor)
	void DebugPrintAllEffects() const;

	/** Get formatted string of all effects on actor (for UI) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug")
	FString GetEffectsSummary(AActor *Actor) const;

	/** Apply immediate (on-hit) skill effect */
	void ApplyImmediateSkillEffect(AActor *Source, AActor *Target, ESkillEffectType StatusType, ESpellElement Element);

	/** Apply triggered (bar-full) skill effect.
	 *
	 *  CONNECTION POINT: this is the single buildup → effect entry point.
	 *  UStatusBuildupManager::TriggerSkillEffectFromBuildup calls here when a target's
	 *  buildup bar caps; this function creates the matching FActiveSkillEffect and
	 *  applies it at full power via ApplyEffect.
	 *
	 *  Public (not private) so the buildup manager — a separate subsystem
	 *  post-split — can invoke it across the boundary. Do not call from inside
	 *  USkillEffectManager itself; effect-side internals should use ApplyEffect
	 *  directly. */
	void ApplyTriggeredSkillEffect(AActor *Source, AActor *Target, ESkillEffectType StatusType, ESpellElement Element);

private:
	// ========================================
	// INTERNAL DATA
	// ========================================

	/** Map of all active effects per actor (not exposed to reflection - TArray in TMap not supported) */
	TMap<TWeakObjectPtr<AActor>, TArray<FActiveSkillEffect>> ActiveEffects;

	/** Next unique effect instance ID (for distinguishing same-type effects) */
	int32 NextInstanceID = 1;

	/** Generate a unique instance ID */
	int32 GenerateInstanceID() { return NextInstanceID++; }

	// ========================================
	// INTERNAL HELPERS
	// ========================================

	/** Process effects with a specific timing */
	void ProcessEffectsWithTiming(AActor *Actor, ESkillEffectTiming Timing);

	/** Tick durations for actor's effects (called at end of turn) */
	void TickDurations(AActor *Actor);

	/** Apply the actual effect logic (damage, heal, stat mod, etc.) */
	void ApplyEffectLogic(AActor *Actor, FActiveSkillEffect &Effect);

	/** Check if a trigger condition is met (handles compound primary + secondary). */
	bool IsTriggerConditionMet(AActor *Actor, const FActiveSkillEffect &Effect, float TriggerValue = 0.0f) const;

	/** Evaluate a single (Trigger, Threshold) pair against Actor's state. */
	bool IsSingleTriggerMet(AActor *Actor, ESkillTrigger Trigger, float Threshold) const;

	/** Find existing effect by ID on actor */
	FActiveSkillEffect *FindEffectByID(AActor *Actor, int32 EffectID);

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Reset turn flags for all effects on actor */
	void ResetTurnFlags(AActor *Actor);

	/** Check if effect type affects speed (requires TurnManager notification) */
	bool IsSpeedEffect(ESkillEffectType EffectType) const;

	/** Notify TurnManager that an actor's speed has changed */
	void NotifySpeedChanged(AActor *Actor);
};