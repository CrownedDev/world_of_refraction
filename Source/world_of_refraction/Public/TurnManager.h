// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurnManager.generated.h"

class UCharacterData;

/**
 * Turn debt tracking for individual combatants
 * Stores speed, debt accumulation, and alive state
 */
USTRUCT(BlueprintType)
struct FCombatantTurnDebt
{
    GENERATED_BODY()

    /** Actor participating in combat */
    UPROPERTY()
    AActor *Actor = nullptr;

    /** Cached speed value (WorldBody + TurnSpeed substat) */
    UPROPERTY()
    int32 Speed = 0;

    /** Accumulated turn debt (how many turns owed) */
    UPROPERTY()
    float TurnsOwed = 0.0f;

    /** Turns already taken */
    UPROPERTY()
    int32 TurnsTaken = 0;

    /** Is this combatant alive and able to act? */
    UPROPERTY()
    bool bIsAlive = true;

    /** For tie-breaking: Attack speed (WorldBody + AttackSpeed substat) */
    UPROPERTY()
    int32 AttackSpeed = 0;

    /** For tie-breaking: Total stats (Mind + Body + Spirit) */
    UPROPERTY()
    int32 TotalStats = 0;

    /** For tie-breaking: Individual world stats */
    UPROPERTY()
    int32 WorldBody = 0;

    UPROPERTY()
    int32 WorldMind = 0;

    UPROPERTY()
    int32 WorldSpirit = 0;

    /** For tie-breaking: Team index (0 or 1) */
    UPROPERTY()
    int32 TeamIndex = 0;

    /** For tie-breaking: Position in team array */
    UPROPERTY()
    int32 ArrayPosition = 0;

    FCombatantTurnDebt()
        : Actor(nullptr), Speed(0), TurnsOwed(0.0f), TurnsTaken(0), bIsAlive(true), AttackSpeed(0), TotalStats(0), WorldBody(0), WorldMind(0), WorldSpirit(0), TeamIndex(0), ArrayPosition(0)
    {
    }
};

/**
 * Delegate signatures for turn events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStarted, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnEnded, AActor *, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedChanged, AActor *, AffectedActor);

/**
 * TurnManager - GameInstanceSubsystem
 *
 * Manages turn order using turn debt system with speed-based ratios.
 * Handles tie-breaking through 7-level cascade.
 * Provides events for combat flow coordination.
 *
 * Design: Single responsibility - ONLY manages whose turn it is.
 * Does NOT execute actions, apply damage, or manage status effects.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UTurnManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize combat with two teams */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void InitializeCombat(const TArray<AActor *> &Team1, const TArray<AActor *> &Team2);

    /** Clear all combat state */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void EndCombat();

    // ========================================
    // TURN CONTROL
    // ========================================

    /** Advance to next combatant's turn */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void AdvanceToNextTurn();

    /** Manually end current turn (called by orchestrator after action completes) */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void EndCurrentTurn();

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get current actor whose turn it is */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    AActor *GetCurrentActor() const { return CurrentActor; }

    /** Get global turn count (increments each time any actor takes a turn) */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    int32 GetGlobalTurnCount() const { return GlobalTurnCount; }

    /** Check if combat is active */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    bool IsCombatActive() const { return bCombatActive; }

    /** Get all combatants (for UI/debug) */
    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    TArray<AActor *> GetAllCombatants() const;

    /** Get turn order preview (next N actors) */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    TArray<AActor *> GetTurnOrderPreview(int32 NumTurns = 5);

    // ========================================
    // DYNAMIC UPDATES
    // ========================================

    /** Notify that an actor's speed changed (buff/debuff) - recalculates debt */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorSpeedChanged(AActor *AffectedActor);

    /** Notify that an actor died - skip their turns */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorDied(AActor *DeadActor);

    /** Notify that an actor was resurrected - resume their turns */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorResurrected(AActor *RessurectedActor);

    // ========================================
    // EVENTS
    // ========================================

    /** Broadcast when a turn starts */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnTurnStarted OnTurnStarted;

    /** Broadcast when a turn ends */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnTurnEnded OnTurnEnded;

    /** Broadcast when combat ends */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnCombatEnded OnCombatEnded;

    /** Broadcast when any actor's speed changes */
    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnSpeedChanged OnSpeedChanged;

    // ========================================
    // DEBUG TOOLS
    // ========================================

    /** Print current turn order to log */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
    void DebugPrintTurnOrder();

    /** Print detailed debt information */
    UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
    void DebugPrintDebtDetails();

    /** Context menu: Show turn manager state */
    UFUNCTION(CallInEditor, Category = "Turn Manager|Debug")
    void Debug_ShowState();

private:
    // ========================================
    // CORE SYSTEMS
    // ========================================

    /** Calculate turn debt for all combatants based on speed ratios */
    void CalculateTurnDebts();

    /** Find combatant with highest net debt (owed - taken) */
    FCombatantTurnDebt *FindNextCombatant();

    /** Resolve ties using 7-level cascade */
    FCombatantTurnDebt *ResolveTie(TArray<FCombatantTurnDebt *> &TiedCombatants);

    /** Extract speed and stats from actor's CharacterData */
    void CacheActorStats(FCombatantTurnDebt &Combatant);

    /** Find combatant entry by actor */
    FCombatantTurnDebt *FindCombatantByActor(AActor *Actor);

    // ========================================
    // STATE
    // ========================================

    /** All combatants participating in combat */
    UPROPERTY()
    TArray<FCombatantTurnDebt> Combatants;

    /** Current actor whose turn it is */
    UPROPERTY()
    AActor *CurrentActor = nullptr;

    /** Previous actor who just finished their turn */
    UPROPERTY()
    AActor *PreviousActor = nullptr;

    /** Global turn counter (increments with each turn) */
    UPROPERTY()
    int32 GlobalTurnCount = 0;

    /** Is combat currently active? */
    UPROPERTY()
    bool bCombatActive = false;
};