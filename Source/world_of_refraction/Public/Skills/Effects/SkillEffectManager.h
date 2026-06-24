// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
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

	/** Listener bound to UDefenseSystem::OnDefenseResolved. Drives defense-outcome conditional
	 *  effects (OnParry/Block/Dodge + perfect tiers, OnTakeDamage). C3b: evaluates the armed
	 *  conditionals and LOGS matches only — ApplyEffect wiring lands in C3c.
	 *  AttackEnergyCost = the attack's BASE (pre-efficiency) energy cost; RECEIVED here for
	 *  attack-scaled reactive absorb but not yet consumed (Cluster B wires the scaling). */
	UFUNCTION()
	void OnDefenseResolvedHandler(AActor *Defender, AActor *Attacker,
								  EDefenseType DefenseType, bool bPerfect, int32 ImpactIndex,
								  float AttackEnergyCost);

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

	// ========================================
	// EQUIPMENT EFFECT APPLICATION
	// ========================================

	/**
	 * Apply STARTING gear effects (the non-conditional subset, once at combat start).
	 *
	 * @param Target Actor to apply effects to
	 * @param Effects Gathered starting effects (from ULoadoutComponent::GetActiveEffectsGathered).
	 *        Each carries its source definition's id + bundle index, so the EffectID is
	 *        packed per-DEFINITION (PackEffectID(DefID, BundleIndex, payload)) — the SAME
	 *        def referenced by any gear/skill yields the SAME id and MERGES on apply.
	 */
	void ApplyEquipmentEffects(AActor *Target, const TArray<FGatheredEffect> &Effects);

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

	/** Reset per-match state at the start of a new combat (clears the fires-once set +
	 *  armed conditionals). */
	UFUNCTION()
	void ResetForNewCombat();

	/** Store the gear conditional effects for an actor (replaces any prior entry).
	 *  Called at combat start; the OnDefenseResolved handler reads these (C3b). */
	void ArmConditionalEffects(AActor *Actor, const TArray<FGatheredEffect> &Conditionals);

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
	// WOR_ DEBUG SUITE
	// ========================================

	/** Starting-effects inspector for the ACTIVE combatant. PRE: per gear source
	 *  (weapon / ring / innate evolution), the starting effects that WILL apply
	 *  (GetStartingEffects off each source asset), plus a GetActiveEffects total
	 *  cross-check so coverage drift in this tool is self-reporting. POST: effects
	 *  currently present in the actor's equipment SourceID window
	 *  (Actor->GetUniqueID()*100 + i). Console: "wor.StartingEffects" in PIE.
	 *  Trigger is a wor.* console command in the .cpp — a UGameInstanceSubsystem
	 *  is not on the engine's Exec dispatch chain. */
	void WOR_StartingEffects();

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

	/** Last Stand support (death-DENIAL, not resurrection). Finds the bearer's LastStand effect
	 *  (the HIGHEST-EffectValue one if several exist), consumes ONE of its charges via the C1
	 *  ConsumeCharge path (so it's removed only when charges hit 0), and returns that effect's
	 *  EffectValue — the restore HP as a PERCENT of MaxHP (e.g. 60 = survive at 60% MaxHP). Returns
	 *  a negative sentinel (-1) when the bearer holds NO LastStand effect (caller proceeds to
	 *  death). A returned 0 means "found but unset" — the caller applies its own fallback percent.
	 *  Operates on the live array entry so the charge consumption / removal lands on the stored
	 *  effect. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|LastStand")
	float ConsumeLastStandCharge(AActor *Owner);

	/** Get total stat modifier from all effects of a category */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	float GetTotalStatModifier(AActor *Actor, ESkillEffectType ModifierType) const;

	/** Element-keyed variant: sums only effects of ModifierType whose Element EXACTLY matches
	 *  BuildupElement. Pass None to read the generic (element-agnostic / unset) contribution only;
	 *  pass a real element to read that element's keyed contribution only. The two are DISJOINT
	 *  (None vs non-None), so a generic-None read plus an element read never double-count the same
	 *  effect. Used by the element-keyed StatusMultiplierBuff path: the generic (None) layer is
	 *  delivered by GetEffectiveStatusMultiplier, the element-keyed layer is added at the
	 *  status-buildup site. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	float GetTotalStatModifierForElement(AActor *Actor, ESkillEffectType ModifierType, ESpellElement BuildupElement) const;

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

	/** Print all active effects for an actor to log. Console trigger: wor.PrintEffects
	 *  (in the .cpp; targets the active combatant). No CallInEditor — a
	 *  UGameInstanceSubsystem has no Details panel to host a button. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug")
	void DebugPrintEffects(AActor *Actor) const;

	/** Print all effects for all tracked actors. Console trigger: wor.PrintAllEffects
	 *  (in the .cpp). No CallInEditor — a UGameInstanceSubsystem has no Details panel
	 *  to host a button. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug")
	void DebugPrintAllEffects() const;

	/** Get formatted string of all effects on actor (for UI) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Debug")
	FString GetEffectsSummary(AActor *Actor) const;

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

	/** EffectIDs that have fired this combat (for bFiresOncePerMatch). Keyed on the stable
	 *  EffectID; cleared per combat by ResetForNewCombat(). */
	TSet<int32> FiredOnceThisMatch;

	/** Gear conditional effects armed per actor at combat start (C3a). The
	 *  OnDefenseResolved handler (C3b) reads these to fire on matching impact outcomes;
	 *  nothing reads it yet. Cleared per combat by ResetForNewCombat(). TWeakObjectPtr key
	 *  mirrors ActiveEffects (non-reflected; no cross-combat actor retention). */
	TMap<TWeakObjectPtr<AActor>, TArray<FGatheredEffect>> ArmedConditionals;

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

	/** Consume one charge after a fire. The SINGLE charge-expiry path every fire site calls.
	 *  Charges == 0 (no charge system) → no-op (existing effects untouched). Charges > 0 →
	 *  decrement; at <= 0 remove the effect from the actor's list + broadcast OnEffectRemoved.
	 *  This is INDEPENDENT of TickDurations (which only does turn-expiry and skips bPermanent),
	 *  so it is the ONLY reaper for a permanent charged effect (e.g. a phoenix revive buff).
	 *  Effect must be a reference into the actor's ActiveEffects array.
	 *  Returns true if the effect was REMOVED (charges hit 0) — callers iterating the array by
	 *  index must compensate (the removed slot shifts subsequent elements down). */
	bool ConsumeCharge(AActor *Actor, FActiveSkillEffect &Effect);

	/** Apply the actual effect logic (damage, heal, stat mod, etc.) */
	void ApplyEffectLogic(AActor *Actor, FActiveSkillEffect &Effect);

	/** Check if a trigger condition is met (handles compound primary + secondary).
	 *  Target is the owner's target for target-side condition subjects (null → target-side
	 *  subjects are skipped, the pre-C2a behaviour). */
	bool IsTriggerConditionMet(AActor *Actor, const FActiveSkillEffect &Effect, float TriggerValue = 0.0f, AActor *Target = nullptr) const;

	/** Evaluate a single (Trigger, Threshold) pair against Actor's state. */
	bool IsSingleTriggerMet(AActor *Actor, ESkillTrigger Trigger, float Threshold) const;

	/** Resolve a condition Subject to the actor(s) it evaluates against: Self → {Owner},
	 *  Target → {Target}, SelfTeam/TargetTeam → that side's team members (empty when there is
	 *  no TurnManager or the side's anchor actor is null). */
	TArray<AActor *> ResolveSubjectActors(ECondSubject Subject, AActor *Owner, AActor *Target) const;

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