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
	int32 CachedAttackSpeed = 0;

	UPROPERTY()
	int32 CachedMind = 0;

	UPROPERTY()
	int32 CachedBody = 0;

	UPROPERTY()
	int32 CachedSpirit = 0;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStarted, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnEnded, AActor*, Actor, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatEnded, int32, FinalTurnCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedChanged, AActor*, Actor);

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

	/** Initialize combat with two teams */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void InitializeCombat(const TArray<AActor*>& Team1, const TArray<AActor*>& Team2);

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
	AActor* GetCurrentActor() const;

	/** Preview next N turns */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	TArray<AActor*> PreviewTurnOrder(int32 NumTurns) const;

	/** Is combat active */
	UFUNCTION(BlueprintPure, Category = "Turn Manager")
	bool IsCombatActive() const { return bCombatActive; }

	// ========================================
	// SPEED CHANGES
	// ========================================

	/** Notify that actor's speed changed (recalculates ratios) */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorSpeedChanged(AActor* Actor);

	/** Notify that actor died */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorDied(AActor* Actor);

	/** Notify that actor was resurrected */
	UFUNCTION(BlueprintCallable, Category = "Turn Manager")
	void OnActorResurrected(AActor* Actor);

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
	FCombatantTurnDebt* GetNextCombatant();
	bool ShouldBreakTieInFavor(const FCombatantTurnDebt& A, const FCombatantTurnDebt& B) const;
	void CacheActorStats(FCombatantTurnDebt& Combatant);
};