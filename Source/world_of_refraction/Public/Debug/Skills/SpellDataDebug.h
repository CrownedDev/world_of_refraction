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

    /** B-356b: the school a spell shows, flagging a retired authored one ("X (authored Y, retired)"; none since D-163). */
    UFUNCTION(BlueprintPure, Category = "Spell Debug")
    static FString GetSchoolString(const USpellData *Spell);

    /** B-374 (D-163, Q-374): the school a spell's main effect proposes, read from its effects the way the AI reads
     *  them (Q-356c): damage at an enemy → Destruction; a heal (not a drain) or a cleanse → Restoration; control at
     *  an enemy (stun, skip turn, silence, loss of control) → Manipulation; a debuff or status buildup at an enemy →
     *  Impairment; a buff → Enhancement. False when the effects propose nothing (an empty spell). */
    static bool ProposeSchool(const USpellData *Spell, ESpellSchool &OutSchool);

    /** B-374: the spell's shown school differs from the proposed one. Conjuration is never flagged: its identity
     *  is the conjured thing, which effects can't show (D-163; Q-374). A spell with no proposal is never flagged. */
    static bool IsSchoolMismatch(const USpellData *Spell);

    /** B-356b / B-374: every spell asset whose school disagrees with the school its effects propose
     *  (IsSchoolMismatch), plus every Conjuration spell whose effects propose Destruction as "check: conjured, or
     *  Destruction" (Conjuration was the damage school until D-163). First line: how many assets were scanned. Loads every USpellData asset: editor / test only. Re-tagging is an editor step. */
    UFUNCTION(BlueprintCallable, Category = "Spell Debug")
    static FString GetSchoolEffectMismatchString();
};