// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurnManager.generated.h"

/**
 * Turn debt tracking for individual combatants
 */
USTRUCT(BlueprintType)
struct FCombatantTurnDebt
{
    GENERATED_BODY()

    UPROPERTY()
    AActor* Actor = nullptr;

    UPROPERTY()
    int32 Speed = 0;

    UPROPERTY()
    float TurnsOwed = 0.0f;

    UPROPERTY()
    int32 TurnsTaken = 0;

    // Tie-breaking stats
    UPROPERTY()
    int32 AttackSpeed = 0;

    UPROPERTY()
    int32 TotalStats = 0;

    UPROPERTY()
    int32 WorldBody = 0;

    UPROPERTY()
    int32 WorldMind = 0;

    UPROPERTY()
    int32 WorldSpirit = 0;

    UPROPERTY()
    int32 TeamIndex = 0;

    UPROPERTY()
    int32 PositionInTeam = 0;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStarted, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnEnded, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpeedChanged, AActor*, Actor, int32, NewSpeed);

/**
 * TurnManager - GameInstanceSubsystem
 * Manages turn order using debt-based system with speed ratios
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

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void InitializeCombat(const TArray<AActor*>& Team1, const TArray<AActor*>& Team2);

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void EndCombat();

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void AdvanceToNextTurn();

    // ========================================
    // STATE QUERIES
    // ========================================

    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    AActor* GetCurrentActor() const;

    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    TArray<AActor*> GetAllCombatants() const;

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    TArray<AActor*> GetTurnOrderPreview(int32 NumTurns = 5);

    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    bool IsCombatActive() const { return bCombatActive; }

    UFUNCTION(BlueprintPure, Category = "Turn Manager")
    int32 GetGlobalTurnCount() const { return GlobalTurnCount; }

    // ========================================
    // DYNAMIC UPDATES
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorSpeedChanged(AActor* AffectedActor);

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorDied(AActor* DeadActor);

    UFUNCTION(BlueprintCallable, Category = "Turn Manager")
    void OnActorResurrected(AActor* ResurrectedActor);

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnTurnStarted OnTurnStarted;

    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnTurnEnded OnTurnEnded;

    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnCombatEnded OnCombatEnded;

    UPROPERTY(BlueprintAssignable, Category = "Turn Manager|Events")
    FOnSpeedChanged OnSpeedChanged;

    // ========================================
    // DEBUG TOOLS
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
    void DebugPrintTurnOrder();

    UFUNCTION(BlueprintCallable, Category = "Turn Manager|Debug")
    void DebugPrintDebtDetails();

    UFUNCTION(Exec, Category = "Turn Manager|Debug")
    void Debug_ShowState();

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    UPROPERTY()
    TArray<FCombatantTurnDebt> Combatants;

    UPROPERTY()
    AActor* CurrentActor;

    UPROPERTY()
    AActor* PreviousActor;

    UPROPERTY()
    int32 GlobalTurnCount;

    UPROPERTY()
    bool bCombatActive;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    void CalculateTurnDebts();
    FCombatantTurnDebt* FindNextCombatant();
    FCombatantTurnDebt* ResolveTie(TArray<FCombatantTurnDebt*>& TiedCombatants);
    void CacheActorStats(FCombatantTurnDebt& Combatant);
    FCombatantTurnDebt* FindCombatantByActor(AActor* Actor);
};