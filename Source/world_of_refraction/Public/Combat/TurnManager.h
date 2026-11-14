// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/WoRFightSubsystem.h"
#include "Combat/TurnRotation.h"
#include "TurnManager.generated.h"

namespace TurnManagerTeams
{
	/** InitializeCombat's team numbering. */
	constexpr int32 LOCAL_TEAM_INDEX = 0;
	constexpr int32 OPPOSING_TEAM_INDEX = 1;
}

class USkillEffectManager;
struct FWoRStatLayers;

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
	int32 TurnsTaken = 0;

	/** The base turn speed (B-042v), read by GetEffectiveTurnSpeed for the GAS turn speed. B-170h2: it no longer
	 *  orders turns (the fixed rotation does); the Turn Speed family goes with B-170h3. */
	UPROPERTY()
	int32 CachedSpeed = 0;

	/** First strike (B-185): this combatant's side was struck first, so it waits until the
	 *  other side has taken its opening round. Cleared for everyone when that round is done. */
	UPROPERTY()
	bool bWaitsForOpening = false;

	/** B-341 (D-133, the turn belt): this combatant's next turns removed by SkipNextTurn. Each
	 *  skip removes the next Normal or Bonus turn it would take (not a ritual's Execution turn);
	 *  a removed turn is used up without happening. Cleared on death. */
	UPROPERTY()
	int32 TurnsToSkip = 0;
};

/**
 * A pinned bonus turn (Emerald). Pinned bonus: fires at its slot regardless of debt;
 * every turn taken (normal or another fired entry) decrements it. TurnsRemaining is the delay
 * in turns until fire; at <= 0 the entry preempts the debt pick in AdvanceSimState
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

	/** B-341 (D-133): granted by GrantBonusTurn. Lands immediately after the current turn: it sits at
	 *  the front of the queue, behind only earlier land-next grants. */
	UPROPERTY()
	bool bLandsNext = false;
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
/** B-291: a new round began (every living combatant was owed its next turn). Listeners count rounds themselves
 *  (the balance simulator's recorder); per-fight state stays off this subsystem (D-064). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoundAdded);

/**
 * TurnManager - per-fight world subsystem (B-336)
 * Orders turns by the fixed rotation (B-170b, ActionPips.md §3.2: the sides alternate, the leading side first;
 * FTurnRotation), with pinned turns (bonus, a ritual's Execution turn) fired ahead of the next seat.
 * B-170h2: the old speed timeline (debt per round, the 2:1 cap, the tie-breaks, the Wind yield) is deleted.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UTurnManager : public UWoRFightSubsystem
{
	GENERATED_BODY()

public:
	UTurnManager();

	// ========================================
	// COMBAT CONTROL
	// ========================================

	/** Initialize combat with two teams. OpeningTeam (B-185 first strike): that team (0 = local
	 *  party, 1 = opposing) takes its whole opening round before the other side moves;
	 *  -1 (INDEX_NONE) = no advantage. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void InitializeCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, int32 OpeningTeam = -1);

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

	/** The speed turn order runs Actor at (B-042v): its cached base x the attached stone x the net buff, rounded,
	 *  at least 1. The cached base is its combatant entry's, or worked out now when Actor isn't in the fight. */
	int32 GetEffectiveTurnSpeed(const AActor *Actor) const;

	/** GetEffectiveTurnSpeed's layers, before the round (FWoRStatLayers::ForTurnSpeed), for the GAS turn speed.
	 *  False when the speed takes a fallback instead (no cached base, no skill effect manager). */
	bool GetTurnSpeedLayers(const AActor *Actor, FWoRStatLayers &OutLayers) const;

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
	/** B-228: the global turn count (the number OnTurnStarted carries). */
	int32 GetTurnCount() const { return GlobalTurnCount; }

	// ========================================
	// SPEED CHANGES
	// ========================================

	/** Notify that actor's speed changed: its base turn speed is read again and OnSpeedChanged broadcast (the GAS
	 *  attribute sync listens). Speed no longer moves turns (B-170h2); the callers go with B-170h3. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorSpeedChanged(AActor *Actor);

	/** Notify that actor died */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorDied(AActor *Actor);

	/** Notify that actor was resurrected */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorResurrected(AActor *Actor);

	/** OBSOLETE (B-341, B-170h2): a no-op. Extra Action grants through GrantBonusTurn (D-133); the turn debt it
	 *  credited went with the speed timeline (B-170h2). Kept for Blueprint callers until B-170h3. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void RequestExtraTurn(AActor *Actor, bool bIsBonusTurn = false);

	/** OBSOLETE (B-170h2): a no-op. The fixed rotation has no place delays: Wind's break takes an action instead
	 *  (B-170d). Kept for Blueprint callers until B-170h3. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void DelayNextTurn(AActor *Actor, int32 Places);

	// ---- The turn belt (B-341, D-133; the deckbuilder's §7c rule) ----
	// One bonus-turn rule: a bonus turn lands immediately after the current turn; a skip removes
	// the holder's next turn; death purges the holder's bonus turns and skips, and a revive
	// re-enters the holder from the next round. Emerald and Extra Action both grant through here.

	/** Count bonus turns for Actor, landing immediately after the current turn (ahead of the speed
	 *  pick and of ready pinned turns; behind earlier land-next grants). A bonus turn is a real extra
	 *  turn (D-052) and passes the 2:1 cap (Q-297). No-op outside combat or for a dead Actor. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void GrantBonusTurn(AActor *Actor, int32 Count = 1);

	/** Remove Actor's next Count turns (Normal or Bonus; a ritual's Execution turn still fires).
	 *  A removed turn is used up without happening: it doesn't count toward the 2:1 cap or burn
	 *  pinned delays. No-op outside combat. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void SkipNextTurn(AActor *Actor, int32 Count = 1);

	/** Skips Actor still has waiting (0 when not in the fight). Debug and specs. */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	int32 GetTurnsToSkip(const AActor *Actor) const;

	/** OBSOLETE (B-341): Emerald grants through GrantBonusTurn (lands next) since D-133; kept for
	 *  the specs that pin the delayed path until PIE.
	 *  Schedule Count pinned bonus turn(s) for Actor, firing after DelayTurns turns
	 *  (Emerald). DelayTurns >= 0 is valid: 0 fires on the very next scheduler step (the
	 *  S-rank immediate case, Count=2 → two back-to-back). Pinned: the fire preempts the
	 *  rotation's next seat; every turn taken burns the delay. A dead/invalid grantee's
	 *  entry is dropped at fire time without emitting a turn. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void ScheduleBonusTurn(AActor *Actor, int32 DelayTurns, int32 Count = 1);

	/** Schedule an Execution Turn (W-A) — an inserted special turn that fires a deferred
	 *  ritual. Mirrors ScheduleBonusTurn: a pinned entry firing after DelayTurns turns,
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

	/** B-291: broadcast when the live turn order adds a round (never for the belt preview). */
	UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
	FOnRoundAdded OnRoundAdded;

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
	 *  Console trigger is wor.Turns.PrintBelt (in the .cpp) — a subsystem
	 *  is not on the engine's Exec dispatch chain. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
	void DebugPrintBelt();

	/** B-170b: "Mode: fixed rotation" with the round and the rest of it, or "Mode: speed timeline". */
	FString GetTurnModeString() const;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	UPROPERTY()
	TArray<FCombatantTurnDebt> Combatants;

	/** Pinned pending bonus turns (Emerald). Delay burns down on every turn taken;
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

	/** B-170b: the fixed rotation's live state. RebuildBelt steps a copy. */
	FTurnRotation Rotation;

	// ========================================
	// INTERNAL METHODS
	// ========================================

	/** The single scheduler step both the live path and the belt fill share (Cluster 2a:
	 *  this IS the live step — AdvanceToNextTurn passes real state, RebuildBelt scratch).
	 *  Advances ONE turn on the passed state: a ready pinned turn first, else the rotation's next
	 *  seat; TurnsTaken++, bonus-consume -> OutEntry.bIsBonusTurn, every pending entry's delay
	 *  burns one. Returns false if no living combatant remains. const: touches only the passed refs. */
	bool AdvanceSimState(TArray<FCombatantTurnDebt> &State,
						 TArray<FScheduledTurn> &Pending,
						 FTurnRotation &RotationState,
						 FPreviewTurnEntry &OutEntry,
						 bool *bOutRoundAdded = nullptr) const;

	/** Refill TurnBelt (up to TURN_BELT_HORIZON) from a scratch copy of live state. */
	void RebuildBelt();

	/** Cache actor stats from CharacterDataComponent */
	void CacheActorStats(FCombatantTurnDebt &Combatant);

	/** The cached turn speed CacheActorStats stores for Actor: the Spirit stat capped, its gear multiplying past
	 *  the cap up to the ceiling, rounded (a fallback without character data). */
	int32 ComputeCachedTurnSpeed(const AActor *Actor) const;

	bool BuildTurnSpeedLayers(const AActor *Actor, int32 CachedSpeed, FWoRStatLayers &OutLayers) const;
	int32 EffectiveTurnSpeedOf(const AActor *Actor, int32 CachedSpeed) const;
	const FCombatantTurnDebt *FindCombatantEntry(const AActor *Actor) const;

	/** Lazy-acquired SkillEffectManager pointer used by BuildTurnSpeedLayers to
	 *  fold ModifyTurnSpeed / TurnSpeedBuff / TurnSpeedDebuff into the effective
	 *  speed. Matches the const_cast pattern used by other subsystems. */
	UPROPERTY()
	mutable USkillEffectManager *SkillEffectManagerRef = nullptr;

	USkillEffectManager *GetSkillEffectManager() const;
};