// BreakCalculatorDebug.h
// Debug utilities for crystal durability wear calculations

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemTier.h"
#include "BreakCalculator.h"
#include "BreakCalculatorDebug.generated.h"

/**
 * Debug utilities for Break Calculator
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBreakCalculatorDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ==================== DURABILITY WEAR (new model) ====================

    /** Log durability wear calculation for a crystal vs action */
    UFUNCTION(BlueprintCallable, Category = "Debug|Durability")
    static void LogDurabilityWear(
        EItemTier CrystalTier,
        EItemTier ActionTier,
        int32 InfusionLevel,
        bool bIsSpell);

    /** Log detailed wear calculation breakdown */
    UFUNCTION(BlueprintCallable, Category = "Debug|Durability")
    static void LogDurabilityWearDetailed(
        EItemTier CrystalTier,
        EItemTier ActionTier,
        int32 InfusionLevel,
        bool bIsSpell);

    /** Print wear matrix for a given crystal tier (all action tiers x infusion levels) */
    UFUNCTION(BlueprintCallable, Category = "Debug|Durability")
    static void PrintWearTable(EItemTier CrystalTier);

    /** Get formatted wear result string */
    UFUNCTION(BlueprintPure, Category = "Debug|Durability")
    static FString GetWearResultString(const FDurabilityWearResult &Result);
};