// BaseAttackDataDebug.h
// Debug utilities for BaseAttackData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BaseAttackDataDebug.generated.h"

class UBaseAttackData;

UCLASS()
class WORLD_OF_REFRACTION_API UBaseAttackDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Print attack stats to screen
    UFUNCTION(BlueprintCallable, Category = "Debug|BaseAttack")
    static void PrintAttackStats(UBaseAttackData* Attack, float Duration = 5.0f);

    // Log attack stats to console
    UFUNCTION(BlueprintCallable, Category = "Debug|BaseAttack")
    static void LogAttackStats(UBaseAttackData* Attack);

    // Get formatted stats string
    UFUNCTION(BlueprintPure, Category = "Debug|BaseAttack")
    static FString GetAttackStatsString(UBaseAttackData* Attack);

    // Compare two attacks
    UFUNCTION(BlueprintCallable, Category = "Debug|BaseAttack")
    static void CompareAttacks(UBaseAttackData* Attack1, UBaseAttackData* Attack2);
};