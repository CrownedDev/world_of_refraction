// BreakCalculatorDebug.h
// Debug utilities for Break Calculator

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemTier.h"
#include "BreakCalculator.h"
#include "BreakCalculatorDebug.generated.h"

class URingData;
class USpellData;

/**
 * Debug utilities for Break Calculator
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBreakCalculatorDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Log break calculation for tier matchup
    UFUNCTION(BlueprintCallable, Category = "Debug|Break")
    static void LogBreakChance(EItemTier EquipmentTier, EItemTier ActionTier, bool bInfused);

    // Log detailed break calculation
    UFUNCTION(BlueprintCallable, Category = "Debug|Break")
    static void LogBreakChanceDetailed(EItemTier EquipmentTier, EItemTier ActionTier, bool bInfused, float DurabilityPercent = 1.0f);

    // Log ring + spell break chance
    UFUNCTION(BlueprintCallable, Category = "Debug|Break")
    static void LogRingSpellBreak(URingData *Ring, USpellData *Spell, bool bInfused);

    // Print full break table for a given equipment tier
    UFUNCTION(BlueprintCallable, Category = "Debug|Break")
    static void PrintBreakTable(EItemTier EquipmentTier);

    // Get formatted break result string
    UFUNCTION(BlueprintPure, Category = "Debug|Break")
    static FString GetBreakResultString(const FBreakCalculationResult &Result);
};