// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurnManager.generated.h"

class USkillEffectManager;

/**
 * The KIND of a scheduled/current turn. Normal = ordinary debt-picked rotation turn.
 * Bonus = Emerald pinned bonus turn (the original FScheduledTurn use). Execution = an
 * inserted "Execution Turn" carrying a deferred ritual fire (W-A slot; W-B wires the
 * fire). Extensible: Interrupt / Reaction later.
 */
UENUM(BlueprintType)
enum class ETurnType : uint8
{
	Normal,
	Bonus,
	Execution
};

/**
 * Turn debt tracking for individual combatants
 */
USTRUCT(BlueprintType)
struct FCombatantTurnDebt
{
	GENERATED_BODY()

	UPROPERTY()
	AActor *Actor = nullptr;

	UPROPERTY()
	int32 TeamIndex = 0;

	UPROPERTY()
	int32 PositionInTeam = 0;

	UPROPERTY()
	float TurnsOwed = 0.0f;

	UPROPERTY()
	int32 TurnsTaken = 0;

	UPROPERTY()
	float SpeedRatio = 1.0f;

	// Cached stats for tie-breaking
	UPROPERTY()
	int32 CachedSpeed = 0;

	UPROPERTY()
	int32 CachedActionSpeed = 0;

	UPROPERTY()
	int32 CachedMind = 0;

	UPROPERTY()
	int32 CachedBody = 0;

	UPROPERTY()
	int32 CachedSpirit = 0;
};

/**
 * A pinned bonus turn (Emerald). Pinned bonus: fires at its slot regardless of debt;
 * decremented ONLY by normal turns, not by other bonus turns. TurnsRemaining is the delay
 * in NORMAL turns until fire; at <= 0 the entry preempts the debt pick in AdvanceSimState
 * and emits Count back-to-back bonus turns. Actor is a raw UPROPERTY ref (matching
 * FCombatantTurnDebt) — GC-tracked; a liveness check guards the fire, so a dead/invalid
 * actor's entry is dropped without emitting a turn.
 */
USTRUCT()
struct FScheduledTurn
{
	GENERATED_BODY()

	UPROPERTY()
	AActor *Actor = nullptr;

	UPROPERTY()
	int32 TurnsRemaining = 0;

	UPROPERTY()
	int32 Count = 1; // bonus turns this entry grants when it fires (S-rank=2)

	/** What kind of pinned turn this is. Defaults to Bonus so existing Emerald entries
	 *  (built field-by-field in ScheduleBonusTurn) keep byte-identical behavior. Execution
	 *  entries (ScheduleExecutionTurn) set this — the pinned-fire scan speed-races only
	 *  among ready Execution entries. */
	UPROPERTY()
	ETurnType Type = ETurnType::Bonus;

	/** Opaque payload handle. Bonus leaves it INDEX_NONE; Execution uses it in W-B to look
	 *  up the orchestrator's frozen FDeferredActivation (caster + FireAction) at fire time.
	 *  TurnManager never dereferences it — it only carries the token. */
	UPROPERTY()
	int32 PayloadId = INDEX_NONE;

	/** Frozen spell speed (caster CalculateSpellSpeed at arm). Drives the fire-time
	 *  selection among directly-adjacent ready Execution entries — higher fires first.
	 *  Unused by Bonus (0). TurnManager does NO CharacterData lookup; the caller supplies it. */
	UPROPERTY()
	float SortKey = 0.0f;
};

/**
 * One slot of a turn-order preview. Actor is the combatant at that future slot;
 * bIsBonusTurn marks a slot produced by a scheduled bonus turn (Emerald) rather than a
 * normal debt-driven turn, so the UI can render it distinctly. False for every slot when
 * no bonus turns are pending (the common case) — the preview is then identical to the
 * plain actor order.
 */
USTRUCT(BlueprintType)
struct FPreviewTurnEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Turn Manager")
	AActor *Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Turn Manager")
	bool bIsBonusTurn = false;

	// RESERVED: hard-pinned positional turns (feel pass). Always false until then —
	// no producer sets it; appended at the END so existing BP reads stay layout-safe.
	UPROPERTY(BlueprintReadOnly, Category = "Turn Manager")
	bool bIsForced = false;

	/** The kind of turn at this slot — Normal (debt pick), Bonus (Emerald), or Execution
	 *  (inserted ritual fire). Stamped by AdvanceSimState and carried through the belt, so
	 *  the turn-order widget can label "Ritual Turn" / "Emerald Turn" vs the character name.
	 *  Appended at the END so existing BP reads stay layout-safe. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn Manager")
	ETurnType Type = ETurnType::Normal;

	/** The scheduled entry's opaque payload handle, carried out of AdvanceSimState the same way
	 *  Type is (W-B). Meaningful only for an Execution slot — the orchestrator looks up its frozen
	 *  FDeferredActivation by it; INDEX_NONE for Normal/Bonus slots. Appended at the END so existing
	 *  BP reads stay layout-safe. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn Manager")
	int32 PayloadId = INDEX_NONE;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStarted, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatEnded, int32, FinalTurnCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedChanged, AActor *, Actor);

/**
 * TurnManager - GameInstanceSubsystem
 * Manages turn order using debt-based system with speed ratios
 *
 * Algorithm: Debt accumulates per ROUND, not per turn.
 * - SpeedRatio = ActorSpeed / SlowestSpeed (slowest = 1.0)
 * - Each round adds SpeedRatio to TurnsOwed for all combatants
 * - Actor with highest (TurnsOwed - TurnsTaken) goes next
 * - New round starts when no combatant has positive net debt
 */
UCLASS()
class WORLD_OF_REFRACTION_API UTurnManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UTurnManager();

	// ========================================
	// COMBAT CONTROL
	// ========================================

	/** Initialize combat with two teams */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void InitializeCombat(const TArray<AActor *> &Team1, const TArray<AActor *> &Team2);

	/** End combat */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void EndCombat();

	/** Advance to next turn */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void AdvanceToNextTurn();

	// ========================================
	// QUERY
	// ========================================

	/** Get current actor */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	AActor *GetCurrentActor() const;

	/** True when the CURRENT turn is a bonus turn being taken (Emerald). Per-turn transient:
	 *  recomputed every AdvanceToNextTurn at the consume point, so it always describes the
	 *  current turn (never a stale prior value). Lets the turn-order strip's current-actor
	 *  slot show the bonus tint — the immediate/self-target bonus IS the current turn, so it
	 *  never appears in the upcoming-slots preview. Upcoming bonuses use PreviewTurnOrder. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	bool GetCurrentTurnIsBonus() const { return bCurrentTurnIsBonus; }

	/** The kind of the CURRENT turn (Normal / Bonus / Execution). Set every AdvanceToNextTurn
	 *  from the picked entry. W-B branches on this (Execution → run the ritual fire instead of
	 *  requesting an action); the turn-order UI reads it for labels. BlueprintPure const getter,
	 *  mirroring GetCurrentTurnIsBonus. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	ETurnType GetCurrentTurnType() const { return CurrentTurnType; }

	/** The payload handle of the CURRENT turn — the orchestrator's key to the frozen ritual when
	 *  this is an Execution turn (W-B). INDEX_NONE for Normal/Bonus turns. Set every AdvanceToNextTurn
	 *  from the picked entry, mirroring GetCurrentTurnType. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	int32 GetCurrentTurnPayloadId() const { return CurrentTurnPayloadId; }

	/** Preview next N turns. Each entry is the combatant at that slot + a bonus-turn flag
	 *  (scheduled Emerald bonus turns appear inline at their future slot, flagged).
	 *  Cluster 2a: a slice of TurnBelt — no forward sim; N beyond the belt horizon
	 *  returns what's available. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	TArray<FPreviewTurnEntry> PreviewTurnOrder(int32 NumTurns) const;

	/** Is combat active */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	bool IsCombatActive() const { return bCombatActive; }

	// ========================================
	// SPEED CHANGES
	// ========================================

	/** Notify that actor's speed changed (recalculates ratios) */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorSpeedChanged(AActor *Actor);

	/** Notify that actor died */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorDied(AActor *Actor);

	/** Notify that actor was resurrected */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorResurrected(AActor *Actor);

	/** Grant an additional NON-bonus turn by crediting one unit of TurnsOwed (debt-soft) —
	 *  the ExtraAction skill effect path. Cluster 2b: the bonus path is RETIRED — Emerald
	 *  bonuses are pinned entries via ScheduleBonusTurn. bIsBonusTurn is kept only so
	 *  existing call sites compile; passing true is a warn-and-ignore no-op. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void RequestExtraTurn(AActor *Actor, bool bIsBonusTurn = false);

	/** Schedule Count pinned bonus turn(s) for Actor, firing after DelayTurns NORMAL turns
	 *  (Emerald). DelayTurns >= 0 is valid: 0 fires on the very next scheduler step (the
	 *  S-rank immediate case, Count=2 → two back-to-back). Pinned: the fire preempts the
	 *  debt pick at its slot; only normal turns burn the delay. A dead/invalid grantee's
	 *  entry is dropped at fire time without emitting a turn. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void ScheduleBonusTurn(AActor *Actor, int32 DelayTurns, int32 Count = 1);

	/** Schedule an Execution Turn (W-A) — an inserted special turn that fires a deferred
	 *  ritual. Mirrors ScheduleBonusTurn: a pinned entry firing after DelayTurns NORMAL turns,
	 *  preempting the debt pick at its slot. Type=Execution, Count=1. PayloadId is the
	 *  orchestrator's handle to the frozen FDeferredActivation (looked up at fire in W-B).
	 *  SpellSpeed is the caster's frozen CalculateSpellSpeed — drives the fire-time selection
	 *  among directly-adjacent ready Execution entries (faster fires first). TurnManager does
	 *  NO CharacterData lookup; the caller supplies SpellSpeed. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void ScheduleExecutionTurn(AActor *Caster, int32 DelayTurns, int32 PayloadId, float SpellSpeed);

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
	FOnTurnStarted OnTurnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
	FOnCombatEnded OnCombatEnded;

	UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
	FOnSpeedChanged OnSpeedChanged;

	// ========================================
	// TEAM HELPERS
	// ========================================

	/** Get all actors on a specific team */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	TArray<AActor *> GetTeamMembers(int32 TeamIndex) const;

	/** Get the team index for an actor (-1 if not in combat) */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	int32 GetActorTeam(AActor *Actor) const;

	// ========================================
	// DEBUG TOOLS
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
	void DebugPrintTurnOrder();

	/** Formatted snapshot of the pinned bonus-turn queue (Emerald) — each scheduled
	 *  actor + its remaining normal-turn delay + count. For log/screen inspection. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager|Debug")
	FString GetPendingTurnsString() const;

	/** Dump the bonus-turn queue to the log + on-screen (mirrors DebugPrintTurnOrder).
	 *  Inspect the scheduler mid-combat without constructing the Emerald-use path. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
	void DebugPrintPendingTurns();

	/** Log TurnBelt in DebugPrintTurnOrder's slot format so output diffs by eye.
	 *  Console trigger is wor.PrintBelt (in the .cpp) — a UGameInstanceSubsystem
	 *  is not on the engine's Exec dispatch chain. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
	void DebugPrintBelt();

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	UPROPERTY()
	TArray<FCombatantTurnDebt> Combatants;

	/** Pinned pending bonus turns (Emerald). Delay burns down on NORMAL turns only;
	 *  ready entries (TurnsRemaining <= 0) fire from AdvanceSimState's pinned-fire step,
	 *  preempting the debt pick. Cleared on InitializeCombat / EndCombat. */
	UPROPERTY()
	TArray<FScheduledTurn> PendingTurns;

	/** Materialized upcoming turns (excludes the current turn). Filled by RebuildBelt
	 *  running AdvanceSimState forward on scratch state; rebuilt every AdvanceToNextTurn
	 *  before OnTurnStarted fires. PreviewTurnOrder is a slice of this. */
	// ONLINE: server owns this rebuild and replicates TurnBelt + CurrentActor; clients
	// consume belt[0] instead of recomputing. Not implemented (single-player).
	UPROPERTY()
	TArray<FPreviewTurnEntry> TurnBelt;

	UPROPERTY()
	AActor *CurrentActor;

	/** Whether the current turn is a bonus turn being taken (Emerald). Set every
	 *  AdvanceToNextTurn at the consume point — true only when the picked combatant had an
	 *  untaken bonus turn (which this pick consumes), false otherwise. Surfaced via
	 *  GetCurrentTurnIsBonus() for the current-actor slot. Reset on InitializeCombat. */
	UPROPERTY()
	bool bCurrentTurnIsBonus = false;

	/** The kind of the current turn (Normal / Bonus / Execution). Set every AdvanceToNextTurn
	 *  from the picked entry's Type — Normal for a debt pick, the pinned entry's Type when
	 *  pinned-fired. Surfaced via GetCurrentTurnType(); reset to Normal on Init/EndCombat. */
	UPROPERTY()
	ETurnType CurrentTurnType = ETurnType::Normal;

	/** The payload handle of the current turn (Execution → the orchestrator's frozen-ritual key;
	 *  INDEX_NONE otherwise). Set every AdvanceToNextTurn from the picked entry's PayloadId; reset to
	 *  INDEX_NONE on Init/EndCombat alongside CurrentTurnType. Surfaced via GetCurrentTurnPayloadId(). */
	UPROPERTY()
	int32 CurrentTurnPayloadId = INDEX_NONE;

	UPROPERTY()
	AActor *PreviousActor;

	UPROPERTY()
	int32 GlobalTurnCount;

	UPROPERTY()
	bool bCombatActive;

	// ========================================
	// INTERNAL METHODS
	// ========================================

	/** Calculate speed ratios relative to slowest combatant (does NOT add debt) */
	void CalculateSpeedRatios();

	/** The single scheduler step both the live path and the belt fill share (Cluster 2a:
	 *  this IS the live step — AdvanceToNextTurn passes real state, RebuildBelt scratch).
	 *  Advances ONE turn on the passed state: round-check+accumulate, select+tiebreak,
	 *  TurnsTaken++, bonus-consume -> OutEntry.bIsBonusTurn, pending-countdown -> debt
	 *  credit. Returns false if no living combatant remains. const: touches only the
	 *  passed refs. */
	bool AdvanceSimState(TArray<FCombatantTurnDebt> &State,
						 TArray<FScheduledTurn> &Pending,
						 FPreviewTurnEntry &OutEntry) const;

	/** Refill TurnBelt (up to TURN_BELT_HORIZON) from a scratch copy of live state. */
	void RebuildBelt();

	/** Determine tie-breaker winner between two combatants */
	bool ShouldBreakTieInFavor(const FCombatantTurnDebt &A, const FCombatantTurnDebt &B) const;

	/** Cache actor stats from CharacterDataComponent */
	void CacheActorStats(FCombatantTurnDebt &Combatant);

	/** Lazy-acquired SkillEffectManager pointer used by CalculateSpeedRatios to
	 *  fold ModifyTurnSpeed / TurnSpeedBuff / TurnSpeedDebuff into the cached
	 *  speed value. Matches the const_cast pattern used by other subsystems. */
	UPROPERTY()
	mutable USkillEffectManager *SkillEffectManagerRef = nullptr;

	USkillEffectManager *GetSkillEffectManager() const;
};