// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "GameplayTagContainer.h"
#include "SkillEffectManager.generated.h"

class UCharacterDataComponent;
class URingData;

/** One fires-once use (D-011): per holder and per source of the effect, so two wearers
 *  of the same once-per-match gear each get theirs. */
struct FWoRFiredOnceKey
{
	TObjectKey<AActor> Holder;
	int32 EffectID = 0;
	int32 SourceKey = 0;

	bool operator==(const FWoRFiredOnceKey &Other) const
	{
		return Holder == Other.Holder && EffectID == Other.EffectID && SourceKey == Other.SourceKey;
	}

	friend uint32 GetTypeHash(const FWoRFiredOnceKey &Key)
	{
		return HashCombine(HashCombine(GetTypeHash(Key.Holder), GetTypeHash(Key.EffectID)), GetTypeHash(Key.SourceKey));
	}
};

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
	 *        packed per-DEFINITION (PackEffectID(DefID, BundleIndex, payload)), and its item
	 *        as the source (FGatheredEffect::SourceKey): two items with one def add up (D-053).
	 */
	void ApplyEquipmentEffects(AActor *Target, const TArray<FGatheredEffect> &Effects);

	/**
	 * B-081 (D-008): a mid-fight weapon toggle or ring switch. Removes the starting effects only
	 * the old gear gave and applies those only the new gear gives; gear on both sides keeps its
	 * effects as they are (no reset, no stacking). Skill and enemy effects are untouched.
	 * Q-081: only passives (every payload bPermanent) come on; start-of-fight one-shots don't.
	 * @return the number of gathered effects removed plus applied.
	 */
	int32 SwapEquipmentEffects(AActor *Target, const TArray<FGatheredEffect> &OldEffects,
							   const TArray<FGatheredEffect> &NewEffects);

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

	/** Remove the one effect with this identity (EffectID + SourceKey, D-012): a gear copy and a
	 *  cast copy of the same definition share an ID, so a cleanse names which one. */
	bool RemoveEffectByIdentity(AActor *Target, int32 EffectID, int32 SourceKey);

	/** Remove all effects with a specific name */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveEffectsByName(AActor *Target, const FString &EffectName);

	/** Remove all effects of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveEffectsByType(AActor *Target, ESkillEffectType EffectType);

	/** Cleanse effects of a type: like RemoveEffectsByType, but gear effects stay (D-015) */
	int32 CleanseEffectsByType(AActor *Target, ESkillEffectType EffectType);

	/** Remove all buffs from target, except gear effects (cleanse rule, D-015) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects")
	int32 RemoveAllBuffs(AActor *Target);

	/** Remove all debuffs from target, except gear effects (cleanse rule, D-015) */
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

	/** True once battle prep armed this actor's gear (until combat end): a swap then re-gathers it. */
	bool IsGearArmed(const AActor *Actor) const { return ArmedConditionals.Contains(Actor); }

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

	/** HP one tick of a DOT takes off Actor: its stacked value x Actor's resistance to the DOT's
	 *  element (B-064b, Q-064; same 1:1, +-50% step as hits). Non-elemental DOTs are unchanged.
	 *  The tick and the AI's lethality checks both read this. */
	static int32 GetDotTickDamage(AActor *Actor, const FActiveSkillEffect &Effect);

	/** Get all effects of a specific type on actor */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	TArray<FActiveSkillEffect> GetEffectsByType(AActor *Actor, ESkillEffectType EffectType) const;

	/** Check if actor has a specific effect by ID */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	bool HasEffectByID(AActor *Actor, int32 EffectID) const;

	/** Check if actor has any effect of a specific type */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	bool HasEffectOfType(AActor *Actor, ESkillEffectType EffectType) const;

	/** GAS stage 1 (B-040): the Status.* / Immunity.* tags the actor's active effects give it
	 *  (WoRTags::EffectTypeToTag). Read-only view of the same effects HasEffectOfType reads. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	FGameplayTagContainer GetEffectTags(AActor *Actor) const;

	/** Does this condition hold right now (B-045 live lighting)? A state check (HP / Energy
	 *  threshold) against its subject's actors, any one of a team; an event trigger always
	 *  reads as held, as in IsTriggerConditionMet. A target-side check with no Target reads as
	 *  held: the runtime skips it, so it never blocks. */
	bool IsConditionHeld(const FSkillCondition &Condition, AActor *Owner, AActor *Target) const;

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

	/** A sub-stat's net transient modifier in percent: Modify + Buff - Debuff over the
	 *  channels StatModifierTable lists for it (B-020). 0 when nothing is active. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Query")
	float GetNetStatModifier(AActor *Actor, ESubStat Stat) const;

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

	/** Check if actor is silenced (part of its EP is sealed, D-038) */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	bool IsSilenced(AActor *Actor) const;

	/** EP that Silenced seals: SILENCE_SEALED_EP_FRACTION of max EP while silenced, else 0 (D-038). */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	int32 GetSealedEP(AActor *Actor) const;

	/** EP the actor can spend now: current EP minus the sealed part, floored at 0 (D-038).
	 *  Every EP spend (spells, abilities, infusion) validates and pays against this. */
	UFUNCTION(BlueprintCallable, Category = "Skill Effects|Status Checks")
	int32 GetSpendableEP(AActor *Actor) const;

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

	/** Fires-once uses spent this combat (bFiresOncePerMatch), per holder + EffectID + source
	 *  (D-011); cleared per combat by ResetForNewCombat(). Existing per-fight state on this
	 *  subsystem; moves to the battle's actors with co-op (D-065). */
	TSet<FWoRFiredOnceKey> FiredOnceThisMatch;

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

	/** Holder-turn spans (FTurnSpan): count the turn that just ended. */
	void CountHolderTurnSpans(AActor *Actor);

	/** Holder-turn spans: remove the ones that are over, as the holder's turn starts. */
	void ExpireFinishedHolderTurnSpans(AActor *Actor);

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

	/** Find existing effect by ID on actor (the first, whichever source gave it) */
	FActiveSkillEffect *FindEffectByID(AActor *Actor, int32 EffectID);

	/** Find the effect with this ID from this source (D-012, D-053): the copy a re-apply refreshes. */
	FActiveSkillEffect *FindEffectByIdentity(AActor *Actor, int32 EffectID, int32 SourceKey);

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Reset turn flags for all effects on actor */
	void ResetTurnFlags(AActor *Actor);

	/** Check if effect type affects speed (requires TurnManager notification) */
	bool IsSpeedEffect(ESkillEffectType EffectType) const;

	/** Notify TurnManager that an actor's speed has changed */
	void NotifySpeedChanged(AActor *Actor);
};