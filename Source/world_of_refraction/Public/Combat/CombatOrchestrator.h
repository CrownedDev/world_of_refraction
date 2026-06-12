// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "AI/EAIDifficulty.h"
#include "AI/AIDecisionManager.h"
#include "Combat/Camera/CombatCameraManager.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "CombatOrchestrator.generated.h"

class UTurnManager;
class UCharacterDataComponent;
class USkillEffectManager;
class UActionExecutor;
class UAnimMontage;

/**
 * Combat state enum
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	Idle,		  // No combat active
	Initializing, // Setting up combatants
	InProgress,	  // Combat active, processing turns
	Victory,	  // Player team won
	Defeat,		  // Enemy team won
	Draw		  // Both teams eliminated (edge case)
};

/**
 * Result of a completed combat
 */
USTRUCT(BlueprintType)
struct FCombatResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECombatState FinalState = ECombatState::Idle;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalTurns = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Team0Survivors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Team1Survivors = 0;

	UPROPERTY(BlueprintReadOnly)
	AActor *LastActorStanding = nullptr;
};

/**
 * One armed deferred activation (D8): a skill cast with ActivationDelay > 0.
 * Costs were paid at arm; the FAction is the frozen intent (stats resolve at
 * fire). Fired FIFO at turn start by the Stage 8c hook.
 */
USTRUCT()
struct FDeferredActivation
{
	GENERATED_BODY()

	UPROPERTY()
	FAction Action;

	UPROPERTY()
	AActor *Caster = nullptr;

	UPROPERTY()
	int32 TurnsRemaining = 0;
};

class UNiagaraSystem;
class USpellData;
class ASkillProjectile;

/** SPIKE (throwaway): where a notify-driven VFX/SFX entry spawns. */
UENUM(BlueprintType)
enum class EVFXAttach : uint8
{
	Caster       UMETA(DisplayName = "Caster"),
	Target       UMETA(DisplayName = "Target"),
	ImpactPoint  UMETA(DisplayName = "Impact Point"),
	CasterSocket UMETA(DisplayName = "Caster Socket"),
	World        UMETA(DisplayName = "World Location")
};

/** SPIKE (throwaway): crude VFX library entry — array index matches the "VFX<N>" notify. */
USTRUCT(BlueprintType)
struct FSpikeVFXEntry
{
	GENERATED_BODY()

	/** Human note, unused by code. */
	UPROPERTY(EditAnywhere)
	FString Label;

	UPROPERTY(EditAnywhere)
	UNiagaraSystem *VFX = nullptr;

	UPROPERTY(EditAnywhere)
	EVFXAttach Attach = EVFXAttach::Target;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "Attach==EVFXAttach::CasterSocket"))
	FName SocketName;
};

class USoundBase;

/** SPIKE (throwaway): crude SFX library entry — array index matches the "SFX<N>" notify. */
USTRUCT(BlueprintType)
struct FSpikeSFXEntry
{
	GENERATED_BODY()

	/** Human note, unused by code. */
	UPROPERTY(EditAnywhere)
	FString Label;

	UPROPERTY(EditAnywhere)
	USoundBase *Sound = nullptr;

	UPROPERTY(EditAnywhere)
	EVFXAttach Attach = EVFXAttach::Target;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "Attach==EVFXAttach::CasterSocket"))
	FName SocketName;
};

/** SPIKE (throwaway): notify-driven spell cast — array index matches the "Cast<N>" notify.
 *  Projectile/Homing/Beam spawn the real ASkillProjectile; AOE/Instant fake-resolve with VFX. */
USTRUCT(BlueprintType)
struct FSpikeCastEntry
{
	GENERATED_BODY()

	/** Human note, unused by code. */
	UPROPERTY(EditAnywhere)
	FString Label;

	UPROPERTY(EditAnywhere)
	ESpellDeliveryType Delivery = ESpellDeliveryType::Projectile;

	/** Required for Projectile/Homing/Beam. */
	UPROPERTY(EditAnywhere)
	TSubclassOf<ASkillProjectile> ProjectileClass;

	/** Optional — drives Init sizing + projectile VFX when set. */
	UPROPERTY(EditAnywhere)
	USpellData *Spell = nullptr;

	/** For AOE/Instant (no projectile actor). */
	UPROPERTY(EditAnywhere)
	UNiagaraSystem *AOEVFX = nullptr;

	/** Barrage = >1, staggered by BurstInterval. */
	UPROPERTY(EditAnywhere)
	int32 Count = 1;

	UPROPERTY(EditAnywhere)
	float BurstInterval = 0.15f;

	UPROPERTY(EditAnywhere)
	int32 TestDamage = 50;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatResultReady, const FCombatResult &, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActorTurnStarted, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionRequested, AActor *, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionExecuted, AActor *, Actor, const FActionResult &, Result);

/**
 * CombatOrchestrator - Coordinates all combat subsystems
 *
 * Responsibilities:
 * - Initialize/end combat via TurnManager
 * - Listen to turn events and coordinate responses
 * - Process status effects at turn boundaries (via SkillEffectManager)
 * - Accept and execute actions from UI/AI (via ActionExecutor)
 * - Check win conditions
 *
 * Integrated Systems:
 * - TurnManager (turn order, speed changes)
 * - SkillEffectManager (start/end of turn processing)
 * - ActionExecutor (validate and execute actions)
 *
 * Future integrations:
 * - BattleUIManager (show action menu for players)
 * - AIDecisionManager (make decisions for AI)
 */
UCLASS()
class WORLD_OF_REFRACTION_API ACombatOrchestrator : public AActor
{
	GENERATED_BODY()

public:
	ACombatOrchestrator();

	// ========================================
	// COMBAT CONTROL
	// ========================================

	/** Start combat between two teams. Team 0 = players, Team 1 = enemies (by convention) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartCombat(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1, EAIDifficulty Difficulty = EAIDifficulty::Medium);

	// Blueprint event fired when combat starts - override in BP to create HUD
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|UI")
	void OnCombatStartedUI(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1);

	/** Force end combat (e.g., flee, cutscene interrupt) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ForceEndCombat(ECombatState ForcedState = ECombatState::Idle);

	// ========================================
	// ACTION SUBMISSION
	// ========================================

	/**
	 * Submit an action for the current actor (synchronous)
	 * Validates, executes via ActionExecutor, then ends turn
	 * @param Action The action to execute
	 * @return True if action was valid and executed
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	bool SubmitAction(const FAction &Action);

	/**
	 * Submit action asynchronously (for defense windows, animations)
	 * Turn ends when async execution completes
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void SubmitActionAsync(const FAction &Action);

	/**
	 * Validate an action without executing
	 * @param Action The action to validate
	 * @return Validation result with error message if invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	FActionValidationResult ValidateAction(const FAction &Action) const;

	/** Called when current actor's action completes (internal - advances turn) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnActionCompleted();

	// ========================================
	// QUERY
	// ========================================

	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor *GetCurrentActor() const { return CurrentActor; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetCurrentTurnNumber() const { return CurrentTurnNumber; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor *> &GetTeam0() const { return Team0Combatants; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor *> &GetTeam1() const { return Team1Combatants; }

	/** Check if it's currently this actor's turn */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsActorsTurn(AActor *Actor) const { return CurrentActor == Actor && CombatState == ECombatState::InProgress; }

	/** Check if actor is AI-controlled */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsActorAIControlled(AActor *Actor) const;

	/** Get living enemies for an actor */
	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	TArray<AActor *> GetLivingEnemies(AActor *ForActor) const;

	/** Get living allies for an actor */
	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	TArray<AActor *> GetLivingAllies(AActor *ForActor) const;

	/** Get current AI difficulty */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	EAIDifficulty GetCombatDifficulty() const { return CombatDifficulty; }

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnCombatResultReady OnCombatResultReady;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActorTurnStarted OnActorTurnStarted;

	/** Broadcast when waiting for action input (UI should show action menu) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActionRequested OnActionRequested;

	/** Broadcast when action execution completes (for UI feedback) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActionExecuted OnActionExecuted;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** If true, auto-advances turn after delay (for testing without UI/AI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bAutoAdvanceTurns = false;

	/** Automatically start combat on BeginPlay using tagged actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bAutoStartCombat = false;

	/** Delay before auto-advancing (simulates action time) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug", meta = (EditCondition = "bAutoAdvanceTurns"))
	float AutoAdvanceDelay = 1.0f;

	/** AI difficulty level for this combat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|AI")
	EAIDifficulty CombatDifficulty = EAIDifficulty::Medium;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	// --- SPIKE (throwaway: fused-montage warp test) ---
	/** Approach + strike (warps to target). */
	UPROPERTY(EditAnywhere, Category = "Spike")
	UAnimMontage *SpikeMontage = nullptr;

	/** Warps back to origin. Optional — loop ends after the strike if unset. */
	UPROPERTY(EditAnywhere, Category = "Spike")
	UAnimMontage *SpikeReturnMontage = nullptr;

	/** Crude VFX library — entry [N] fires on a "VFX<N>" notify (bare "VFX" = 0). */
	UPROPERTY(EditAnywhere, Category = "Spike")
	TArray<FSpikeVFXEntry> SpikeVFXArray;

	/** Crude SFX library — entry [N] fires on an "SFX<N>" notify (bare "SFX" = 0). */
	UPROPERTY(EditAnywhere, Category = "Spike")
	TArray<FSpikeSFXEntry> SpikeSFXArray;

	/** Crude cast library — entry [N] fires on a "Cast<N>" notify (bare "Cast" = 0). */
	UPROPERTY(EditAnywhere, Category = "Spike")
	TArray<FSpikeCastEntry> SpikeCastArray;

	// Diagnostic: warp this far in front of the actor instead of the enemy.
	// 0 = warp to the actual enemy (real behavior). >0 = fixed-distance probe.
	UPROPERTY(EditAnywhere, Category = "Spike")
	float SpikeWarpDistanceOverride = 300.0f;

	UFUNCTION(CallInEditor, Category = "Spike")
	void DebugWarpSpike();

	UFUNCTION(CallInEditor, Category = "Spike")
	void DebugWarpSpikeReset();

	/** Montage notify re-broadcast handler — fake hit + VFX, no damage. */
	UFUNCTION()
	void OnSpikeNotify(FName NotifyName);

	/** Chains approach-end -> return, return-end -> loop complete. */
	UFUNCTION()
	void OnSpikeMontageEnded(UAnimMontage *Montage, bool bInterrupted);

	void PlaySpikeReturn();
	void SpikeUnbind();

	/** Cast-entry dispatch — branches on Delivery (actor vs fake AOE resolve). */
	void SpikeExecuteCast(const FSpikeCastEntry &Entry);

	/** Spawns one real ASkillProjectile at the caster, aimed at the spike target. */
	void SpikeSpawnProjectile(const FSpikeCastEntry &Entry);

	/** Burst timer chain — mirrors SkillProjectileTestActor::SpawnNextBurst. */
	void SpikeSpawnNextBurst();

	/** Resolves an entry's attach mode to a world location.
	 *  Returns false when the entry must be skipped (Target mode, no target). */
	bool ResolveSpikeAttachLocation(EVFXAttach Attach, FName SocketName, FName Tag, FVector &OutLoc) const;

	/** Projectile impact handler — fake hit log only, no damage. */
	UFUNCTION()
	void OnSpikeCastImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage);

	/** One burst chain at a time — last Cast entry with Count>1 wins. */
	UPROPERTY(Transient)
	FSpikeCastEntry SpikeBurstEntry;
	int32 SpikeBurstRemaining = 0;
	FTimerHandle SpikeBurstTimerHandle;

	/** Pre-warp pose cache so the reset button can undo root motion. */
	UPROPERTY(Transient)
	AActor *SpikeCachedActor = nullptr;
	UPROPERTY(Transient)
	AActor *SpikeCachedTarget = nullptr;
	FVector SpikeCachedLoc = FVector::ZeroVector;
	FRotator SpikeCachedRot = FRotator::ZeroRotator;

	/** True end-of-approach pose, captured at montage-end before the deferred return tick. */
	FVector SpikeApproachEndLoc = FVector::ZeroVector;
	FRotator SpikeApproachEndRot = FRotator::ZeroRotator;
	// --- END SPIKE ---

	/** Override actor for debug commands (if set, uses this instead of CurrentActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	AActor *DebugOverrideActor = nullptr;

	/** Helper to get actor for debug commands */
	AActor *GetDebugActor() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugPrintCombatState();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	int32 DebugDamageAmount = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	float DebugStatusBuildupAmount = 25.0f;

	// Change functions to no parameters
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugDamageTeam0();

	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugDamageTeam1();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Combat|Debug")
	void DebugSpendEPTeam0();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Combat|Debug")
	void DebugSpendEPTeam1();

	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugApplyStatusBuildup();

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugKillActor(AActor *Actor);

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugHealAllTeam(int32 TeamIndex);

	/** Execute a test action (basic attack on random enemy) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugExecuteTestAction();

	/** Test attack with movement - current actor attacks first enemy */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestAttackMovement();

	/** Execute ability with movement - tests full ability pipeline */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestAbilityMovement();

	/** Execute spell with movement - tests full spell pipeline */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestSpellMovement();

	/** Trigger an async attack via ExecuteActionAsync (active weapon's WeaponAttack vs opposing team[0]) */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteAsyncAttack();

	/** Trigger an async spell via ExecuteActionAsync (first available spell vs opposing team[0]) */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteAsyncSpell();

	/** Test spell from PRIMARY slot specifically */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestPrimarySpell();

	/** Test spell from SECONDARY slot specifically */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestSecondarySpell();

	/** Trigger an async ability via ExecuteActionAsync (first available ability vs opposing team[0]) */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugExecuteAsyncAbility();

	/** Draw grid visualization with actor positions */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugDrawCombatGrid(float Duration = 5.0f);

	/** Start combat using tagged actors in level and snap to grid positions */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug", meta = (CallInEditor = "true"))
	void DebugStartCombatWithLevelActors();

	/** Test item on enemy (Garnet damage) - tests target facing + animation */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestItemOnEnemy();

	/** Test item on self (Sapphire heal) - tests self animation */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestItemOnSelf();

	/** Test item on ally (Sapphire heal) - tests ally facing + animation */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugTestItemOnAlly();

	// ========================================
	// CRYSTAL WEAR DEBUG (forwards to UCrystalManager WOR_* methods)
	// ========================================
	// CallInEditor must be parameterless; each button is a fixed scenario.
	// All forwarders resolve the active combatant via TurnManager inside
	// the WOR_ methods — no orchestrator-side state needed.

	/** Print the active combatant's equipped primary weapon crystal state. */
	UFUNCTION(CallInEditor, Category = "Debug|CrystalWear")
	void DebugCrystalState();

	/** Print the active combatant's substat-modified wear prediction table (F->S). */
	UFUNCTION(CallInEditor, Category = "Debug|CrystalWear")
	void DebugWearTable();

	/** Sim worst case: S-tier action, L2 infused, on the active combatant's crystal. */
	UFUNCTION(CallInEditor, Category = "Debug|CrystalWear")
	void DebugSimCast_S_L2();

	/** Sim light case: matched-tier action (== crystal tier), L1 infused — isolates infusion-only wear. */
	UFUNCTION(CallInEditor, Category = "Debug|CrystalWear")
	void DebugSimCast_Matched_L1();

	UPROPERTY()
	ACombatCameraManager *CameraManager;

	// Helper to find camera manager
	ACombatCameraManager *FindCameraManager();

	/** Manually advance to next turn (debug - use when bAutoAdvanceTurns is false) */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugManualAdvanceTurn();

	/** Toggle auto-advance turns on/off */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugToggleAutoAdvance();

	/** Select target by index and trigger Selection camera (0, 1, 2 for enemies) */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugSelectTarget(int32 EnemyIndex);

	/** Execute attack on selected target with full camera flow */
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugAttackSelectedTarget();

	/** Currently selected target for debug */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	int32 DebugSelectedTargetIndex = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	UPROPERTY()
	ECombatState CombatState;

	UPROPERTY()
	TArray<AActor *> Team0Combatants;

	UPROPERTY()
	TArray<AActor *> Team1Combatants;

	UPROPERTY()
	AActor *CurrentActor;

	UPROPERTY()
	int32 CurrentTurnNumber;

	UPROPERTY()
	UTurnManager *TurnManagerRef;

	UPROPERTY()
	USkillEffectManager *SkillEffectManagerRef;

	UPROPERTY()
	UActionExecutor *ActionExecutorRef;

	FTimerHandle AutoAdvanceTimerHandle;

	UPROPERTY()
	UAIDecisionManager *AIDecisionManagerRef = nullptr;

	/** Track if we're waiting for async action to complete */
	bool bWaitingForAsyncAction = false;

	/** Armed deferred activations (D8). Lives HERE, not on TurnManager — its
	 *  AdvanceSimState is replayed 16x per belt-preview rebuild, so a queue
	 *  there would fire during preview. Fired once, for real, FIFO, in
	 *  HandleTurnStarted (Stage 8c). Cleared on every combat-end path. */
	UPROPERTY()
	TArray<FDeferredActivation> DeferredActivations;

	/** Bound to ActionExecutor::OnActionDeferredArmed — registers an armed
	 *  skill in the queue. */
	UFUNCTION()
	void HandleActionDeferredArmed(AActor *Caster, const FAction &Action, int32 DelayTurns);

	/** Entries due THIS turn, moved out of DeferredActivations by
	 *  TickDeferredActivations and consumed FIFO by the fire chain. */
	UPROPERTY()
	TArray<FDeferredActivation> DueDeferredFires;

	/** Caster of the deferred fire currently executing — for the
	 *  OnActionExecuted UI broadcast (the firing actor is the ritual's caster,
	 *  not necessarily the turn owner). */
	UPROPERTY()
	AActor *CurrentDeferredFireCaster = nullptr;

	/** Turn-start tick (8c): decrement EVERY queued entry (global countdown),
	 *  move due entries (TurnsRemaining <= 0) into DueDeferredFires, FIFO. */
	void TickDeferredActivations();

	/** Fire the next due ritual via ExecuteActionAsync with bIsDeferredFire set
	 *  (cost-free — paid at arm). Only drops casters ALREADY gone at turn start
	 *  (removed in a prior turn) — rituals fire before this turn's DoT, so an
	 *  about-to-die caster still fires. Drained (or empty) → the normal turn
	 *  proceeds (ProceedWithTurnStart). Async-chained via
	 *  HandleDeferredFireCompleted — never blocks. */
	void FireNextDeferredActivation();

	/** Completion of one deferred fire: broadcast for UI, divert to the normal
	 *  end-of-combat flow if the ritual ended the fight, else chain the next. */
	void HandleDeferredFireCompleted(const FActionResult &Result);

	/** Normal turn-start flow AFTER rituals fire: start-of-turn effects (DoT),
	 *  BD overflow, death check, turn broadcast, then RequestActionFromActor.
	 *  Split from HandleTurnStarted (8c rev) so the deferred-fire chain can
	 *  resume it once the due list drains. */
	void ProceedWithTurnStart();

	// ========================================
	// TURN MANAGER EVENT HANDLERS
	// ========================================

	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleCombatEnded(int32 FinalTurnCount);

	// ========================================
	// INTERNAL METHODS
	// ========================================

	void SetCombatState(ECombatState NewState);
	/** Prepare all combatants' loadouts for battle */
	void PrepareAllLoadoutsForBattle();

	/** Consume used items from all combatants' inventories after battle */
	void ConsumeAllUsedItems();
	void BindTurnManagerEvents();
	void UnbindTurnManagerEvents();

	// Status effect processing (delegates to SkillEffectManager)
	void ProcessStartOfTurnEffects(AActor *Actor);
	void ProcessEndOfTurnEffects(AActor *Actor);

	// Action request (broadcasts to UI/AI)
	void RequestActionFromActor(AActor *Actor);

	// Async action callback
	void HandleAsyncActionCompleted(const FActionResult &Result);

	// Win condition
	ECombatState CheckWinCondition();
	int32 CountLivingMembers(const TArray<AActor *> &Team);
	bool IsActorAlive(AActor *Actor) const;

	// Team helpers
	int32 GetActorTeamIndex(AActor *Actor) const;
	TArray<AActor *> GetEnemyTeam(AActor *Actor) const;

	// Result building
	FCombatResult BuildCombatResult();

	/** Apply +REPAIR_PER_BATTLE to every refined, non-immune, non-broken crystal
	 *  on every active combatant's equipped rings. Fired in the win-condition cleanup
	 *  inside OnActionCompleted. NOT called from ForceEndCombat (aborts don't count
	 *  as completed battles). */
	void ApplyBetweenCombatRepair();

	/** Phase B — Crystal destruction at combat end.
	 *  Iterates every active combatant's rings and weapons; for any broken crystal,
	 *  clears the slotted crystal reference (preserves the ring/weapon, removes the
	 *  destroyed crystal). Runs BEFORE ApplyBetweenCombatRepair so destroyed crystals
	 *  aren't candidates for repair.
	 *  No UI delegate — destruction is silent; player learns via empty crystal slot
	 *  in next combat's UI (capability queries return no spells / no element). */
	void ApplyBetweenCombatCrystalDestruction();
	// ========================================
	// BROKEN DARKNESS HELPERS
	// ========================================

	/** Get BrokenDarknessManager from an actor */
	UBrokenDarknessManager *GetBrokenDarknessManager(AActor *Actor) const;

	/** Get all combatants within range of an actor (excludes the actor itself) */
	TArray<AActor *> GetCombatantsInRange(AActor *Origin, float Range);

	/** Process BD overflow effects for an actor (aura damage, self-damage, drain) */
	void ProcessBrokenDarknessOverflow(AActor *Actor);
};