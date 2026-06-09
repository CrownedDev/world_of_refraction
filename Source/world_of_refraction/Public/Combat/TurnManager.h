// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurnManager.generated.h"

class USkillEffectManager;

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
 * A bonus turn scheduled to fire after a delay (Emerald). TurnsRemaining counts down once
 * per global turn boundary (AdvanceToNextTurn); at 0 the actor is granted an extra turn via
 * the existing RequestExtraTurn debt-credit. Actor is a raw UPROPERTY ref (matching
 * FCombatantTurnDebt) — GC-tracked; a liveness check guards the fire, so a dead/invalid
 * actor's entry is silently dropped.
 */
USTRUCT()
struct FScheduledTurn
{
	GENERATED_BODY()

	UPROPERTY()
	AActor *Actor = nullptr;

	UPROPERTY()
	int32 TurnsRemaining = 0;
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

	/** Preview next N turns */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	TArray<AActor *> PreviewTurnOrder(int32 NumTurns) const;

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

	/** Grant an additional turn to the specified actor by crediting one unit of
	 *  TurnsOwed. The debt-based scheduler will pick this actor on the next
	 *  AdvanceToNextTurn call (or shortly after, depending on relative debt). */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void RequestExtraTurn(AActor *Actor);

	/** Schedule a bonus turn for Actor to fire after DelayTurns global turn boundaries
	 *  (Emerald's delayed grant). DelayTurns must be >= 1 — N==0 (immediate) is handled
	 *  caller-side via RequestExtraTurn directly; a <1 delay here is ignored + logged. On
	 *  expiry the scheduler calls RequestExtraTurn(Actor) if the actor is still a living
	 *  combatant, otherwise drops the entry. */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void ScheduleBonusTurn(AActor *Actor, int32 DelayTurns);

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

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	UPROPERTY()
	TArray<FCombatantTurnDebt> Combatants;

	/** Pending delayed bonus turns (Emerald). Decremented once per AdvanceToNextTurn;
	 *  fired via RequestExtraTurn at 0. Cleared on InitializeCombat / EndCombat. */
	UPROPERTY()
	TArray<FScheduledTurn> PendingTurns;

	UPROPERTY()
	AActor *CurrentActor;

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

	/** Add one round of debt to all combatants based on their SpeedRatio */
	void AccumulateDebtRound();

	/** Find combatant with highest net debt (TurnsOwed - TurnsTaken) */
	FCombatantTurnDebt *GetNextCombatant();

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