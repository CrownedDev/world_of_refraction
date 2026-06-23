// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Actions/EActionType.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Character/ECharacterClass.h"
#include "Infusion/InfusionConstants.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Infusion/EInfusionMode.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/Actions/EAbilityExecutionType.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Infusion/InfusionCostHelper.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Inventory/ItemTier.h"
#include "Combat/CombatNotify.h"
#include "Skills/Definitions/SkillVFXEntry.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "ActionExecutor.generated.h"

class UCharacterDataComponent;
class UCharacterData;
class USkillEffectManager;
class USkillDataBase;
class USpellData;
class UAbilityData;
class UEvolutionItemData;
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
class USkillDataBase;
class UMotionWarpingComponent;

// ========================================
// MONTAGE CHAIN
// ========================================

/** Leg of the explicit execution montage chain (Option B):
 *  [Ritual] → Skill → [Return] → Done. None = no chain active. Internal
 *  runner state, not Blueprint-exposed. */
enum class EMontagePhase : uint8
{
	None,
	Ritual,
	Skill,
	Return,
	Done
};

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

/** Broadcast when a skill with ActivationDelay > 0 ARMS instead of executing
 *  (D8). The orchestrator registers the deferred activation; firing is 8c. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnActionDeferredArmed, AActor *, Caster, const FAction &, Action, int32, DelayTurns);

/** Broadcast at each melee impact frame (Impact-family Combat Notify). ImpactIndex is
 *  the per-impact ordinal (0..HitCount-1). Stage 0: fires into the void (logged);
 *  Stage 2's per-impact resolver binds this. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnImpactFrame, AActor *, Executor, int32, ImpactIndex);

/** Callback for async action completion */
DECLARE_DELEGATE_OneParam(FOnActionComplete, const FActionResult &);

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

	/** Spell infusion size multiplier — 1.0 / 1.5 / 2.0 for infusion L0 / L1 / L2. */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionSizeMultiplier(int32 InfusionLevel);

	/** Spell infusion energy-cost multiplier — 1.0 / 1.3 / 1.6 for infusion L0 / L1 / L2. */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Infusion")
	static float GetSpellInfusionCostMultiplier(int32 InfusionLevel);

	/** Get available infusion sources for character based on class and loadout */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	TArray<EInfusionSourceOption> GetAvailableInfusionSources(AActor *Actor) const;

	/** The allowed infusion source(s) for a SPELL, by its origin (1:1 via ResolveSpellSource),
	 *  with the BD/Reality exception that an Evolution spell may ALSO use Innate. Single source of
	 *  truth for the spell origin->source binding (consumed by the AI and, later, validation).
	 *  SPELL-only — abilities are not origin-bound. Item-origin / null / unresolvable -> empty
	 *  (not infusable). Plain C++ (not BlueprintPure): a rule helper for AI + validation. */
	TArray<EInfusionSourceOption> GetAllowedInfusionSourcesForSpell(AActor *Actor, const USpellData *Spell) const;

	/** Get element for selected source option */
	UFUNCTION(BlueprintPure, Category = "Action Executor|Infusion")
	ESpellElement GetElementForSourceOption(AActor *Actor, EInfusionSourceOption Option) const;

	/**
	 *  Resolve a spell's effective CAST element (Cluster 3 — the Generic-inherit step).
	 *  - Non-Generic spells (incl. None) pass through unchanged — only Generic is polymorphic.
	 *  - A Generic spell adopts the element of the SOURCE/POOL it is slotted into:
	 *      * Broken Darkness: the spell's pool element — Darkness if in InnateSpells,
	 *        else the matching BDSpellPools entry's Element (a dedicated walk, because
	 *        ResolveSpellSource collapses every BD pool to Innate and loses the pool).
	 *      * Otherwise: the spell's ORIGIN (innate / ring / weapon / evolution) via
	 *        ResolveSpellSource -> GetElementForSourceOption. A Reality source yields
	 *        Reality here (valid — Reality is NOT rejected by the resolver).
	 *  - Unresolvable (no source found, or the origin carries no element) -> None + warning.
	 *  The source is the spell's ORIGIN (where it is slotted), derived via
	 *  ResolveSpellSource — NOT a player infusion pick (Action.SelectedSource), so no
	 *  explicit infusion selection is needed to resolve a slotted Generic spell.
	 *  Pure read-only rule helper. NOT yet wired into the cast path — Cluster 3c
	 *  substitutes this at the cast boundary (and feeds the display-name prefix).
	 */
	ESpellElement ResolveSpellCastElement(AActor *Caster, const USpellData *Spell) const;

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
	TSubclassOf<ASkillProjectile> DefaultProjectileClass;

	/** Execute an item */
	UFUNCTION(BlueprintCallable, Category = "Action Executor|Execute")
	FActionResult ExecuteItem(
		AActor *User,
		FCrystalId Id,
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

	/** Deduct the deferred infusion HP cost stashed at commit
	 *  (FActionExecutionContext::PendingInfusionHPCost). Called from
	 *  FinalizeAsyncAction, AFTER the infused effect resolves. No clamp — a
	 *  lethal cost routes through ServerTakeDamage -> CheckDeath -> OnDied. */
	void ApplyPendingInfusionHPCost(AActor *Actor);

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

	/** Apply damage with defaults (no element, can crit) */
	FCombatHitResult ApplyDamage(
		AActor *Attacker,
		AActor *Target,
		int32 BaseDamage)
	{
		return ApplyDamage(Attacker, Target, BaseDamage, ESpellElement::None, true);
	}

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

	/** Tier-gap damage multiplier for an action (D9): the action's own tier vs
	 *  the channel it is used through. 1.0 when no channel resolves (innate/item
	 *  spells, unarmed). Non-logging — safe for per-candidate AI scoring; the
	 *  execution path logs via its private wrapper. Single source of truth for
	 *  both real damage (B2 assembly points) and AI damage previews. */
	float GetTierGapDamageMultiplier(AActor *Actor, const FAction &Action) const;

	/** Tier-gap COST multiplier for an action (Cluster 3a): reciprocal of the
	 *  damage gap — its own ladder (TierGapConstants COST_*). 1.0 when no channel
	 *  resolves. Applied at the cost sites (CalculateActionEnergyCost) and the
	 *  HP-infusion basis (ApplyCommitCosts). Does NOT reuse the damage multiplier. */
	float GetTierGapCostMultiplier(AActor *Actor, const FAction &Action) const;

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

	/** D8: a delayed skill armed (costs paid, queued on the orchestrator). */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionDeferredArmed OnActionDeferredArmed;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionCompleted OnActionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnDamageDealt OnDamageDealt;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnHealingDone OnHealingDone;

	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnTargetKilled OnTargetKilled;

	/** Fires at each melee impact frame (Impact-family Combat Notify), once per impact. */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnImpactFrame OnImpactFrame;

	/** Broadcast when async action fully completes (after all defense windows) */
	UPROPERTY(BlueprintAssignable, Category = "Action Executor|Events")
	FOnActionCompleted OnAsyncActionCompleted;

	// ========================================
	// ANIMATION/VFX STUBS (Override in subclass or bind via events)
	// ========================================

	/** Called when spell animation should play. Override or bind OnActionStarted for custom handling.
	 *  Play rate = CalculateSpellSpeed() × ActionMods.SpellSpeed contribution. */
	virtual void PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize, const FActionStatModifiers &ActionMods = FActionStatModifiers());

	/** Called when spell VFX should spawn. Override or bind OnActionStarted for custom handling. */
	void SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize, const TArray<AActor *> &ExplicitTargets, int32 Damage = 0);

	/** Merged skill-animation play (Cluster 2): takes the unified base pointer (attacks + abilities).
	 *  Play rate = BaseAnimSpeed × CalculateAnimationSpeed() × ActionMods.ActionSpeed contribution.
	 *  (BaseAnimSpeed + SkillMontage are uniform on USkillDataBase since D7.) */
	virtual void PlaySkillAnimation(AActor *User, USkillDataBase *Ability, const FActionStatModifiers &ActionMods = FActionStatModifiers());

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
	// ==================== TIER-GAP RESOLUTION (D9) ====================

	/** The action's own tier: spell → SpellData->Tier; ability/attack → active
	 *  weapon's Tier (F_Tier when unarmed). Shared extraction of the dispatch the
	 *  ApplyCommitCosts wear blocks use — they keep their inline copies until the
	 *  swap is approved separately. */
	EItemTier ResolveActionTier(AActor *Actor, const FAction &Action) const;

	/** The tier of the channel the action is used THROUGH: attack/ability →
	 *  active weapon; spell → the catalyst crystal per Action.SpellSource
	 *  (ring/weapon slotted crystal, fusion gem half, or evolution item). Unset =
	 *  no channel (innate/item spells, unarmed) — callers skip scaling (1.0).
	 *  Broken crystals still report their tier: breaks land at turn end, so the
	 *  channel is intact for the cast being assembled. */
	TOptional<EItemTier> ResolveChannelTier(AActor *Actor, const FAction &Action) const;

	/** Execution-path wrapper around GetTierGapDamageMultiplier: same value
	 *  (delegates for the math — real damage and AI previews share one source),
	 *  plus the [TierGap] log line. Applied at the three assembly points as the
	 *  FINAL multiplicative factor (B2). No channel → 1.0, damage unchanged. */
	float ResolveTierGapMultiplier(AActor *Actor, const FAction &Action, const FString &ActionName) const;

	// ==================== DEFERRED ACTIVATION (D8) ====================

	/** The skill's ActivationDelay for this action (spell/ability/attack data).
	 *  0 for non-castable action types. An attack with null AttackData resolves
	 *  the effective attack read-only (active weapon's OverrideAttack/default —
	 *  the same resolver ExecuteAttackAsync uses) so basic attacks can defer. */
	int32 GetActionActivationDelay(AActor *Actor, const FAction &Action) const;

	/** Stage 8b arm interception: pay costs now, broadcast OnActionDeferredArmed
	 *  (orchestrator queues it), complete the turn WITHOUT executing the skill.
	 *  Returns true if the action armed (caller returns immediately). */
	bool TryArmDeferredActivation(AActor *Actor, const FAction &Action, FOnActionComplete &OnComplete);

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

	/** Cluster 3c: the cast's resolved element (Generic -> source/pool), cached for the
	 *  deferred notify-triggered VFX (muzzle + delivery colours). Mirrors
	 *  bPendingSpellIsBrokenDarkness — set in ExecuteSpellAsync, reset in ClearPendingSpellData. */
	ESpellElement PendingResolvedElement = ESpellElement::None;

	/** Handle animation notify for spell VFX timing */
	UFUNCTION()
	void OnSpellAnimNotify(FName NotifyName);

	/** Bind to CombatAnimInstance notify delegate */
	void BindSpellNotify(AActor *Actor);

	/** Unbind from notify delegate */
	void UnbindSpellNotify(AActor *Actor);

	/** Clear cached spell data */
	void ClearPendingSpellData();

	// ==================== FUSED-MONTAGE RUNNER (Stage 12) ====================

	/** Runner notify spine: receives UCombatNotify (Family, Index) from the
	 *  actor's UCombatAnimInstance and routes by family. VFX → VFXArray[Index]
	 *  spawn; Hit (SC4) / Cast (SC3) / SFX (deferred) are log-only stubs this
	 *  sub-cluster. Coexists with the name-based OnSpellAnimNotify path until
	 *  montages carry UCombatNotify instances (content-gated retirement). */
	UFUNCTION()
	void OnCombatNotifyReceived(ECombatNotifyFamily Family, int32 Index);

	void BindCombatNotify(AActor *Actor);
	void UnbindCombatNotify(AActor *Actor);

	/** The executing action's skill data (spell/ability/attack); null for
	 *  items or outside an action. */
	USkillDataBase *GetCurrentSkillData() const;

	/** First VFXArray entry of the given role with a non-null asset, or null
	 *  (callers fall back to the loose VFX fields — D5 reader switch, SC5). */
	static const FSkillVFXEntry *GetVFXEntryByRole(const USkillDataBase *Skill, EVFXRole Role);

	/** Find-or-create the actor's Motion Warping component (W1, the spike's
	 *  pattern): a persistent component authored on the character BP is
	 *  found; characters without one get a runtime "CombatWarp" fallback. */
	UMotionWarpingComponent *GetOrCreateWarpComponent(AActor *Actor) const;

	/** Family=VFX: spawn VFXArray[Index] honoring Attach/tint/Scale — the
	 *  spike's VFX spawn productionized (socket attach follows the hand). */
	void SpawnVFXArrayEntry(const FSkillVFXEntry &Entry, int32 Index);

	/** Resolve a non-attached spawn location for a VFX entry. False = skip
	 *  (warned) — e.g. Target attach with no valid target. */
	bool ResolveVFXAttachLocation(EVFXAttach Attach, FName SocketName, int32 Index, FVector &OutLoc) const;

	/** Current leg of the explicit montage chain (Option B). OnActionAnimationEnded
	 *  reads this to advance Ritual → Skill → Return → Done; finalize
	 *  runs only on the Done transition (FinishMontageChain). Replaces the old
	 *  bPlayingRitualLeadIn / bPlayingReturnMontage flag pair. */
	EMontagePhase MontagePhase = EMontagePhase::None;

	/** The montage the active chain leg is actually playing — set by each
	 *  Play{Ritual,Skill,Return}Step just before it plays. OnActionAnimationEnded
	 *  compares the ended montage against this and ignores ends for ANY other
	 *  montage (a blended-out residual/leftover action interrupt), so a spurious
	 *  bInterrupted end can't run FinishMontageChain mid-chain (return-skip fix). */
	UPROPERTY()
	UAnimMontage *CurrentChainMontage = nullptr;

	/** Actor whose montage chain is live; the chain advances off THIS, never
	 *  PendingExecutionActor (which finalize nulls mid-Skill on a deferred fire).
	 *  Set in BeginMontageChain, cleared in FinishMontageChain / CancelAsyncAction
	 *  (return-skip fix A — the chain survives the null and still reaches Return). */
	TWeakObjectPtr<AActor> ChainActor;

	/** Skill the live chain is playing; replaces GetCurrentSkillData() reads in the
	 *  chain path so the chain advances off its own handle, not pending context. */
	UPROPERTY()
	USkillDataBase *ChainSkill = nullptr;

	/** True while a deferred ARM turn's ritual-cast montage plays (2b). The arm
	 *  plays ONLY RitualCastMontage — no approach/skill/return, no skill effects,
	 *  no damage — and completes via FinishArmTurn (OnActionAnimationEnded routes
	 *  here before the chain dispatch). A terminal one-leg flow, not part of the
	 *  linear chain, so it's a flag rather than an EMontagePhase value. */
	bool bArmingRitual = false;

	/** Play-rate carried across the chain — ritual + skill play at this rate; the
	 *  return leg plays at 1.0. Set by BeginMontageChain. */
	float PendingMontagePlayRate = 1.0f;

	/** Explicit montage chain (Option B): [RitualCastMontage] → SkillMontage →
	 *  [ReturnMontage], each step named + logged, advanced by the montage-end
	 *  dispatcher. Presence-driven — null ritual/return legs skip to the next
	 *  step. BeginMontageChain is the entry (replaces PlaySkillMontageChain). */
	void BeginMontageChain(AActor *Actor, USkillDataBase *Skill, float PlayRate);
	void PlayRitualStep(AActor *Actor, USkillDataBase *Skill);
	void PlaySkillStep(AActor *Actor, USkillDataBase *Skill);
	void PlayReturnStep(AActor *Actor, USkillDataBase *Skill);
	void FinishMontageChain(AActor *Actor);

	/** Complete a deferred ARM turn (2b): the ritual-cast montage ended (or was
	 *  interrupted). Unbinds the anim/notify spine, settles the held execution
	 *  context, and fires the stashed armed-success callback ONCE (ends the turn).
	 *  NO skill effects / damage / defense — costs were paid and the ritual queued
	 *  at arm; the montage was only the visible channel. */
	void FinishArmTurn(AActor *Actor);

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
	 *
	 *  Authored-DoT routing (feature/authored-skill-dots): a SPELL's DOT-type
	 *  effect inherits ResolvedCastElement; an ABILITY/ATTACK's DOT-type effect
	 *  routes through the physical-type mapping (Slash->Bleed, Pierce->ArmorBreak,
	 *  Impact->Stun) using PhysicalType — the wielded weapon's declared type,
	 *  resolved by the caller; None falls back to the legacy Generic shape.
	 */
	void ApplySkillEffects(
		AActor *User,
		const TArray<AActor *> &Targets,
		const TArray<FGatheredEffect> &Effects,
		const FString &SourceName,
		FActionResult &Result,
		bool bCausedDeath,
		ESpellElement ResolvedCastElement = ESpellElement::Generic,
		EActionType ActionKind = EActionType::Ability,
		EPhysicalDamageType PhysicalType = EPhysicalDamageType::None);

public:
	/** Get targets for an effect based on ETargetType + ETargetCount.
	 *  Public (Decision B): USkillEffectManager's defense-trigger fire (C3c) reuses this
	 *  resolver per-payload instead of duplicating the ETargetType/ETargetCount logic. The
	 *  GetAll* helpers below stay private — the resolver calls them in-class. */
	void GetEffectTargets(
		AActor *User,
		const TArray<AActor *> &ActionTargets,
		ETargetType TargetType,
		ETargetCount TargetCount,
		int32 UserTeam,
		TArray<AActor *> &OutTargets);

private:
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

	/** Resolve the source attachment (FRuntimeAttachedItem snapshot) for the
	 *  given action's infusion source. Mirrors GetElementForSourceOption's
	 *  dispatch shape — returns the attachment rather than its element.
	 *  Returns a default (empty) attachment for non-crystal sources
	 *  (None/Raw/Innate) or when the resolver lookup fails (no equipped
	 *  ring etc.). */
	FRuntimeAttachedItem ResolveInfusionAttachment(AActor *Actor, const FAction &Action) const;

	/** Combined efficiency multiplier for spell/ability energy costs — character
	 *  CalculateEfficiencyMultiplier × an equipment BonusEfficiency multiplier
	 *  (same clamp shape). Returns 1.0f (no reduction) when CharData / Loadout
	 *  resolution fails. Does NOT apply to attacks (Efficiency explicitly excludes
	 *  attacks per CombatConstants comment). */
	float GetEffectiveEnergyCostEfficiencyMultiplier(AActor *Actor) const;

	// New validation method
	bool CanUseAbility(AActor *Actor, UAbilityData *Ability) const;
	bool CanUseSpell(AActor *Actor, USpellData *Spell) const;

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;

	/** Get CharacterData template from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Apply damage after defense resolution. Stage 2: applies only the UNDRAINED TAIL
	 *  [ImpactsLanded, HitCount) — melee drained the rest per-impact-frame; non-melee
	 *  (ImpactsLanded==0) applies all, the unchanged lumped path. */
	void ApplyDamageAfterDefense(
		AActor *Attacker,
		AActor *Target,
		const FPendingDefenseContext &Context,
		const FDefenseResult &DefenseResult);

	/** OBSOLETE (Stage 3): superseded by ResolveImpactDefense, which resolves a fresh
	 *  per-impact defense (split-then-reduce + match-and-consume) instead of locking one
	 *  action-level result at the first impact. Retained un-deleted until the per-impact
	 *  path is PIE-verified; no live callers remain.
	 *
	 *  Stage 2: lazily compute + stash the single action-level defense result for a
	 *  defender (idempotent), via the public DefenseSystem API. */
	void EnsureResultResolved(AActor *Defender);

	/** Stage 3: resolve ONE impact's defense for a defender. Split-then-reduce — slice
	 *  BaseDamage by ResolvedDamageSplit[ImpactIndex] FIRST, then CalculateDefenseResult on
	 *  that slice against the impact's match-and-consumed input (MatchAndConsumeInput at
	 *  ImpactTime). Stashes the per-impact result (ResolvedFinalDamage now holds the reduced
	 *  SLICE, not the full total) + bResolvedPerfect onto the context, reflects a parried
	 *  slice (ApplyParryReflect), and broadcasts OnDefenseResolved / OnDefensePerfect. */
	void ResolveImpactDefense(AActor *Defender, int32 ImpactIndex, double ImpactTime, const FDefenseDifficultyTriple &ImpactDifficulty);

	/** Stage 2/3: apply ONE impact's damage and accumulate into PartialResult. Shared by the
	 *  per-impact-frame path and the close-time undrained tail.
	 *  @param bDamageIsPerImpact true (melee per-impact frame): ResolvedFinalDamage is already
	 *         this impact's reduced slice (ResolveImpactDefense) — apply it directly. false
	 *         (non-melee close-time tail, default): ResolvedFinalDamage is the full reduced
	 *         total — slice it by ResolvedDamageSplit[ImpactIndex] here (legacy lumped path). */
	void ApplyOneImpact(AActor *Attacker, AActor *Target, const FPendingDefenseContext &Context, int32 ImpactIndex, bool bDamageIsPerImpact = false);

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

public:
	// ========================================
	// CHARGE INFUSION HELPERS (NEW)
	// ========================================
	// Pure const queries — public so the AI prediction/guard (and the 6-5-d mirror
	// reconcile) can reuse the real combat math instead of duplicating it.

	/** Charge DAMAGE multiplier: per-mode base (focus / off-focus / balanced by EInfusionMode) ×
	 *  upside-only stat surcharge (1 + max(0, stat-1)). Damage stat = bIsSpell ? SpellDamage :
	 *  RawDamage. L0 (or null Comp) → 1.0. Shared by ability + spell (bIsSpell picks the stat). */
	float GetChargeDamageMultiplier(int32 Level, EInfusionMode Mode, bool bIsSpell, const UCharacterDataComponent *Comp) const;

	/** Charge STATUS multiplier: per-mode base (status is the focus in Status mode) × the same
	 *  upside-only surcharge over GetEffectiveStatusMultiplier. L0 (or null Comp) → 1.0. */
	float GetChargeStatusMultiplier(int32 Level, EInfusionMode Mode, const UCharacterDataComponent *Comp) const;

	/** Resolve which equipment/character's InfusionMode applies for an action's infusion source:
	 *  Raw/WeaponCrystal → weapon, Innate → character data, ActiveRing/PrimaryRing → ring,
	 *  Evolution → primary-slot evolution. Null-safe → Balanced. */
	EInfusionMode ResolveInfusionMode(EInfusionSourceOption Source, AActor *Actor) const;
	/** Get ability charge energy cost multiplier (L1 = 1.0, L2 = 1.5) */
	float GetAbilityChargeCostMultiplier(int32 Level) const;

	/** Infused energy-cost multiplier package: the charge multiplier (x1.5/x2.0 at
	 *  L1/L2, 1.0 at L0) times an upside-only stat surcharge (1 + max(0, stat-1)).
	 *  Ability scales with RawDamage, spell with SpellDamage. Shared by the spell
	 *  cost, ability spend, and ability preview so all three agree. */
	float ComputeInfusionCostMultiplier(int32 Level, bool bIsSpell, const UCharacterDataComponent *Comp) const;


private:
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

	/** B2 interrupt — true iff EVERY currently-pending defender has resolved the current interruptable hit
	 *  AND parried/dodged it (the all-targets abort gate). Empty set → false. Single-target (N=1) degenerates
	 *  to the one defender's outcome. Read between the melee resolve and close loops (contexts still present). */
	bool AllInterruptSucceeded() const;

	/** B2 spell tally — record a target's choke outcome (parried/dodged) for the current interruptable cast
	 *  entry; returns true once EVERY target has resolved AND all parried/dodged (→ caller aborts). Survives
	 *  the per-defender close that removes contexts (vs AllInterruptSucceeded's live-context iteration). */
	bool RecordChokeAndShouldAbort(AActor *Target, bool bDefended);

	/** B2 interrupt — abort the rest of the action after an interruptable hit was parried/dodged by all
	 *  targets: stop pending spawns/timers, clear+close the pending defense windows (no tail apply), mark the
	 *  result interrupted, then play the ReturnMontage (warp back to origin) if authored, else hard-stop the
	 *  montage and finalize. Skill is the chain skill (UWeaponAttackData / spell — both USkillDataBase). */
	void InterruptAsyncAction(AActor *Attacker, USkillDataBase *Skill);

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
	// ASYNC EXECUTION STATE
	// ========================================

	/** Cached actor for the in-flight async action. */
	UPROPERTY()
	AActor *PendingExecutionActor = nullptr;

	/** Cached character data for the in-flight async action. */
	UPROPERTY()
	UCharacterData *PendingExecutionCharData = nullptr;

	/** Execution begins (W3): start-facing (nearest living enemy), approach-warp
	 *  target, notify/anim-end binding, dispatch to the three Execute*Async
	 *  paths, failsafe timer. ALL action types enter here directly from
	 *  ExecuteActionAsync — there is no separate approach leg. */
	void BeginSkillExecution(AActor *Actor);

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

	/** One-shot latch: the async completion callback fires EXACTLY ONCE per action.
	 *  Reset in ExecuteActionAsync alongside the coordination flags; raised in
	 *  CompleteAsyncActionFinal before Execute so a double finalize (e.g. timeout
	 *  failsafe + legitimate finalize) cannot double-fire or no-op the advance. */
	bool bAsyncCompletionFired = false;

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
	 * Merged attack+ability async dispatch (Cluster 2). Reads the unified skill pointer
	 * (ResolveActionSkill); an attack is an ability with AbilityInfusionLevel=0 (charge multipliers
	 * L0-neutral). Routed from both the Ability and Attack switch labels until Cluster 3 collapses
	 * the enum. Opens defense windows.
	 */
	void ExecuteSkillAsync(AActor *User, const FAction &Action, UCharacterData *UserData);

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
	 *  cleanup across all three async paths. Also resolves the skill's DamageSplit
	 *  into ResolvedDamageSplit on the context (D1) — stash only; OutDamagePerHit
	 *  stays the legacy even split until the runner consumes the table (Stage 12).
	 *  Assumes CurrentExecutionContext.IsSet(). */
	void FinalizeDamageInputs(const USkillDataBase *Skill, int32 FinalDamage, int32 HitCount, int32 &OutDamagePerHit);

	/** Unified log line for async action dispatch. Replaces three near-identical
	 *  trailing UE_LOG calls in the async executors. Spell size is dropped from
	 *  the unified log — was diagnostic noise, not behavioural. */
	void LogActionDispatch(
		EActionType ActionType,
		int32 InfusionLevel,
		int32 FinalDamage,
		int32 NumTargets) const;

	// ==================== ASYNC COMPLETION ====================

	/** Cached result sent when the action completes. */
	FActionResult PendingFinalResult;

	/** Final completion — fires the async callback + completion broadcasts. */
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
	/** Get execution range from action data (warp striking offset) */
	float GetExecutionRange(const FAction &Action) const;

	/** Arena center for movement calculations */
	FVector CachedArenaCenter = FVector::ZeroVector;

	// ==================== ORIGIN SNAPSHOT (SC-D) ====================
	/** Warp-origin snapshot — the pre-action pose the ReturnMontage warp targets.
	 *  Written at execution start (ExecuteActionAsync), read at the warp-return
	 *  (PlayReturnStep). Position is owned by root-motion montages + warp (W3);
	 *  the runner records where the action started so the return leg can warp back. */
	FVector GridPosition = FVector::ZeroVector;
	FRotator GridRotation = FRotator::ZeroRotator;
	bool bHasGridPosition = false;

protected:
	/**
	 * Spawn spell delivery based on DeliveryType
	 * - Projectile: Spawns ASkillProjectile
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

	/** Spawn projectile actor (Projectile). Entry non-null = the
	 *  Cast-entry dispatch path (D6): entry class/speed/Trail win;
	 *  null = legacy loose-field path. ONE spawn site for both. */
	void SpawnProjectileActor(
		AActor *Caster,
		AActor *Target,
		USkillDataBase *Skill,
		float FinalImpactRadius,
		float FinalVisualScale,
		int32 FinalDamage,
		bool bIsBrokenDarkness,
		ESpellElement InResolvedElement,
		const FSkillCastEntry *Entry = nullptr,
		int32 CastEntryIndex = INDEX_NONE);

	/** Spawn AOE effect (no projectile). Entry non-null = ground visual from
	 *  the entry's Trail; null = loose SpellVFX. */
	void SpawnAOEEffect(
		AActor *Caster,
		AActor *Target,
		USkillDataBase *Skill,
		ESpellElement InElement,
		float FinalImpactRadius,
		float FinalVisualScale,
		int32 FinalDamage,
		bool bIsBrokenDarkness,
		const FSkillCastEntry *Entry = nullptr,
		int32 CastEntryIndex = INDEX_NONE);

	/** Resolve instant spell (immediate hit, now defendable per-impact at the Cast
	 *  notify — Hard difficulty). Entry non-null = visual from the entry's Trail;
	 *  null = loose SpellVFX. CastEntryIndex keys the per-cast-entry defense
	 *  difficulty (INDEX_NONE → Easy ×1.0), mirroring SpawnAOEEffect. */
	void ResolveInstantSpell(
		AActor *Caster,
		AActor *Target,
		USkillDataBase *Skill,
		ESpellElement InElement,
		float FinalImpactRadius,
		int32 FinalDamage,
		bool bIsBrokenDarkness,
		const FSkillCastEntry *Entry = nullptr,
		int32 CastEntryIndex = INDEX_NONE);

	/** Entry-based delivery dispatch (D6 Stage 12) — the ONE spawn site both
	 *  trigger paths share (UCombatNotify Family=Cast AND the legacy
	 *  SpellRelease name-notify). Sizing comes from the ENTRY (Size = hitbox,
	 *  VisualScale = visual) × the infusion SpellSize multiplier — for migrated
	 *  content this reproduces the legacy BaseSize math exactly. Count>1
	 *  staggers via the burst chain. */
	void DispatchSpellCast(
		AActor *Caster,
		USkillDataBase *Skill,
		ESpellElement InElement,
		const FSkillCastEntry &Entry,
		float SpellSize,
		const TArray<AActor *> &ExplicitTargets,
		int32 Damage,
		int32 CastEntryIndex = INDEX_NONE);

	/** Burst stagger (Count>1): first spawn immediate, remainder queued and
	 *  spawned one per BurstInterval (the spike's SpawnNextBurst pattern;
	 *  multi-target bursts queue per target in dispatch order). */
	void SpawnNextBurstProjectile();

	// Burst chain state — one active burst per action lifecycle; cleared in
	// CancelAsyncAction and when the queue drains.
	FSkillCastEntry ActiveBurstEntry;

	/** Cast-entry index of the active burst — all Count projectiles share it (Option A). */
	int32 ActiveBurstCastEntryIndex = INDEX_NONE;

	UPROPERTY()
	USkillDataBase *ActiveBurstSpell = nullptr;

	TWeakObjectPtr<AActor> ActiveBurstCaster;
	TArray<TWeakObjectPtr<AActor>> BurstSpawnQueue;
	float ActiveBurstImpactRadius = 1.0f;
	float ActiveBurstVisualScale = 1.0f;
	int32 ActiveBurstDamage = 0;
	bool bActiveBurstIsBD = false;
	/** Resolved element for the active burst's continuation projectiles (Cluster 3c). */
	ESpellElement ActiveBurstElement = ESpellElement::None;
	FTimerHandle BurstTimerHandle;

	/** Pending telegraph timers (the "incoming!" cues scheduled backward from each notify's time at
	 *  montage start). Tracked so a cancelled/aborted action clears them — no stale tell fires for an
	 *  impact that never lands. Cleared in FinalizeAsyncAction + CancelAsyncAction. */
	TArray<FTimerHandle> TelegraphTimerHandles;

	/** Spawn support skill VFX (Self/Ally - no defense window) */
	void SpawnSupportSpellEffect(
		AActor *Caster,
		const TArray<AActor *> &Targets,
		USkillDataBase *Skill,
		ESpellElement InElement,
		float FinalVisualScale,
		bool bIsBrokenDarkness);

	/** Called when projectile impacts target */
	UFUNCTION()
	void OnProjectileImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage, int32 CastEntryIndex);

	/** Called when target dodged projectile */
	UFUNCTION()
	void OnProjectileDodged(AActor *Target, FVector ImpactLocation);
};