// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "AI/EAIDifficulty.h"
#include "AI/AIDecisionManager.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Skills/Definitions/SkillVFXEntry.h"
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
	int32 LocalPartySurvivors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OpposingPartySurvivors = 0;

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

// SPIKE RETIRED (Stage 12 SC8): the FSpike* structs are deleted — the
// production equivalents are FSkillVFXEntry / FSkillCastEntry on the skill
// data (EVFXAttach lives in SkillVFXEntry.h, hoisted Stage 10A).

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatResultReady, const FCombatResult &, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActorTurnStarted, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionRequested, AActor *, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionExecuted, AActor *, Actor, const FActionResult &, Result);

/** Fired at combat end on a PLAYER WIN when the encounter earned World Stat Points (§7 C3).
 *  Carries the pending pool; the deferred 5-pick-3 draft UI binds this to let the player allocate.
 *  INERT until that UI exists (like OnInventoryChanged was) — C3 only FIRES it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldStatDraftReady, int32, PoolAmount);

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

	/** Start combat between two parties. LocalParty is the local player's side
	 *  (TeamIndex 0), OpposingParty is whatever opposes them (TeamIndex 1).
	 *  Perspective-based: under PvP both sides are player parties, each client
	 *  seeing itself as LocalParty. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, EAIDifficulty Difficulty = EAIDifficulty::Medium);

	// Blueprint event fired when combat starts - override in BP to create HUD
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|UI")
	void OnCombatStartedUI(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

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
	const TArray<AActor *> &GetLocalParty() const { return LocalPartyCombatants; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<AActor *> &GetOpposingParty() const { return OpposingPartyCombatants; }

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

	/** §7 C3 — fired at combat end on a player WIN when PendingWorldStatPool > 0. The 5-pick-3
	 *  draft UI binds this (deferred); nothing consumes it yet. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnWorldStatDraftReady OnWorldStatDraftReady;

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

	// SPIKE RETIRED (Stage 12 SC8) — the Spike* montages/arrays/handlers are
	// deleted; Crown resaves BP_CombatOrchestrator once so UE drops the
	// orphaned Spike* property data.

	/** Override actor for debug commands (if set, uses this instead of CurrentActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	AActor *DebugOverrideActor = nullptr;

	/** Helper to get actor for debug commands */
	AActor *GetDebugActor() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DebugPrintCombatState();

	// ==================== WORLD STATS (§7 C3 — earn pool) ====================

	/** Current encounter's accumulated World Stat Point pool (filled by enemy kills; reset at
	 *  combat start). Read by the draft event / debug. */
	UFUNCTION(BlueprintPure, Category = "Combat|WorldStats")
	int32 GetPendingWorldStatPool() const { return PendingWorldStatPool; }

	/** DEBUG-ONLY: dump the entire pending pool into the player team's Mind via AddEarnedWorldStat,
	 *  then clear the pool. NOT the real draft (that's the deferred 5-pick-3 UI on
	 *  OnWorldStatDraftReady) — this just proves the pool→grant pipe end-to-end in PIE. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat|WorldStats")
	void DebugApplyPendingWorldStats();

	/** DEBUG: print the pending pool + each player-team member's live Mind/Body/Spirit. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Combat|WorldStats")
	void DebugPrintWorldStats() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	int32 DebugDamageAmount = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	float DebugStatusBuildupAmount = 25.0f;

	// Change functions to no parameters
	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugDamageLocalParty();

	UFUNCTION(CallInEditor, Category = "Combat|Debug")
	void DebugDamageOpposingParty();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Combat|Debug")
	void DebugSpendEPLocalParty();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Combat|Debug")
	void DebugSpendEPOpposingParty();

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
	TArray<AActor *> LocalPartyCombatants;

	UPROPERTY()
	TArray<AActor *> OpposingPartyCombatants;

	/** §7 C3 — World Stat Points earned THIS encounter (enemy kills, caliber-scaled). Reset at
	 *  combat start; fed by OnCombatantDied; surfaced via OnWorldStatDraftReady on a player win. */
	UPROPERTY()
	int32 PendingWorldStatPool = 0;

	/** Death listener bound per-combatant in StartCombat (both teams), unbound at combat end —
	 *  mirrors TurnManager's OnDied lifecycle. Enemy (OpposingParty) deaths add caliber WSP to the pool;
	 *  player (LocalParty) deaths are ignored. UFUNCTION so it can bind to the dynamic OnDied delegate. */
	UFUNCTION()
	void OnCombatantDied(AActor *Victim);

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

	/** Armed deferred activations (W-B). Keyed by an orchestrator-owned PayloadId
	 *  handed to TurnManager::ScheduleExecutionTurn at arm; looked up by the same
	 *  handle when the scheduled Execution turn fires. Lives HERE, not on
	 *  TurnManager — its AdvanceSimState is replayed 16x per belt-preview rebuild.
	 *  The countdown is OWNED BY TurnManager now (delay burns on Normal turns in
	 *  AdvanceSimState); this map only holds the frozen intent. Cleared on every
	 *  combat-end path. */
	UPROPERTY()
	TMap<int32, FDeferredActivation> DeferredPayloads;

	/** Monotonic source for DeferredPayloads keys / ScheduleExecutionTurn handles. */
	int32 NextPayloadId = 0;

	/** Bound to ActionExecutor::OnActionDeferredArmed — stashes the frozen ritual
	 *  in DeferredPayloads and schedules an Execution turn (W-B). */
	UFUNCTION()
	void HandleActionDeferredArmed(AActor *Caster, const FAction &Action, int32 DelayTurns);

	/** Execution-turn fire (Option 2): looks up the frozen FDeferredActivation by
	 *  the current turn's PayloadId, sets bIsDeferredFire, and runs the ritual
	 *  through the NORMAL SubmitAction path — completion routes through
	 *  HandleAsyncActionCompleted (return → finalize → single AdvanceToNextTurn),
	 *  same as any normal turn. There is no parallel fire/completion path. */
	void FireScheduledExecution();

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