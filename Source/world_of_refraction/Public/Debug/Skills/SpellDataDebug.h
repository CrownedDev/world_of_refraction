// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills/Definitions/SpellData.h"
#include "Character/CharacterData.h"
#include "SpellDataDebug.generated.h"

/**
 * Debug utilities for SpellData
 */
UCLASS()
class WORLD_OF_REFRACTION_API USpellDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Print spell stats for a specific character
    UFUNCTION(BlueprintCallable, Category = "Spell Debug")
    static void PrintSpellStats(USpellData *Spell, UCharacterData *Character, float Duration = 15.0f);

    // Print to log only
    UFUNCTION(BlueprintCallable, Category = "Spell Debug")
    static void LogSpellStats(USpellData *Spell, UCharacterData *Character);

    // Get formatted stats as string
    UFUNCTION(BlueprintPure, Category = "Spell Debug")
    static FString GetSpellStatsString(USpellData *Spell, UCharacterData *Character);

    // Compare spell effectiveness between two characters
    UFUNCTION(BlueprintCallable, Category = "Spell Debug")
    static void CompareSpellEffectiveness(USpellData *Spell, UCharacterData *Character1, UCharacterData *Character2);
};