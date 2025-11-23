#pragma once
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilityData.h"
#include "CharacterData.h"
#include "AbilityDataDebug.generated.h"

/**
 * Debug utilities for AbilityData
 */
UCLASS()
class WORLD_OF_REFRACTION_API UAbilityDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Print ability stats for a specific character
    UFUNCTION(BlueprintCallable, Category = "Ability Debug")
    static void PrintAbilityStats(UAbilityData* Ability, UCharacterData* Character, float Duration = 10.0f);

    // Print to log only
    UFUNCTION(BlueprintCallable, Category = "Ability Debug")
    static void LogAbilityStats(UAbilityData *Ability, UCharacterData *Character);

    // Get formatted stats as string
    UFUNCTION(BlueprintPure, Category = "Ability Debug")
    static FString GetAbilityStatsString(UAbilityData *Ability, UCharacterData *Character);

    // Compare ability effectiveness between two characters
    UFUNCTION(BlueprintCallable, Category = "Ability Debug")
    static void CompareAbilityEffectiveness(UAbilityData *Ability, UCharacterData *Character1, UCharacterData *Character2);
};