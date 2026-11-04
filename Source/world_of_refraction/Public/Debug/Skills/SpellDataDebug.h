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

    /** B-356b: the school a spell shows, flagging a retired authored one: "Conjuration (authored Destruction, retired)". */
    UFUNCTION(BlueprintPure, Category = "Spell Debug")
    static FString GetSchoolString(const USpellData *Spell);

    /** B-356b: every spell asset whose school disagrees with what the AI reads from its effects: a damage-school
     *  (Conjuration) spell IsOffensiveSpell rejects, or a support-school spell it accepts. One line per spell;
     *  empty when all agree. Loads every USpellData asset: editor / test use only. */
    UFUNCTION(BlueprintCallable, Category = "Spell Debug")
    static FString GetSchoolEffectMismatchString();
};