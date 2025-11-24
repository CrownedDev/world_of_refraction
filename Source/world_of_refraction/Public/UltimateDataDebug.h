// UltimateDataDebug.h
// Debug utilities for UltimateData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UltimateData.h"
#include "CharacterData.h"
#include "RefractionElement.h"
#include "UltimateDataDebug.generated.h"

/**
 * Debug function library for UltimateData
 */
UCLASS()
class WORLD_OF_REFRACTION_API UUltimateDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Print ultimate stats to screen
    UFUNCTION(BlueprintCallable, Category = "Debug|Ultimate")
    static void PrintUltimateStats(UUltimateData* Ultimate, UCharacterData* Character = nullptr, float Duration = 10.0f);

    // Print ultimate stats to log
    UFUNCTION(BlueprintCallable, Category = "Debug|Ultimate")
    static void LogUltimateStats(UUltimateData* Ultimate, UCharacterData* Character = nullptr);

    // Get formatted stats string
    UFUNCTION(BlueprintPure, Category = "Debug|Ultimate")
    static FString GetUltimateStatsString(UUltimateData* Ultimate, UCharacterData* Character = nullptr);

    // Log all ultimates of a specific element
    UFUNCTION(BlueprintCallable, Category = "Debug|Ultimate")
    static void LogAllUltimatesByElement(ERefractionElement Element);

    // Compare two ultimates side by side
    UFUNCTION(BlueprintCallable, Category = "Debug|Ultimate")
    static void CompareUltimates(UUltimateData* Ultimate1, UUltimateData* Ultimate2, UCharacterData* Character = nullptr);
};