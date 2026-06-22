// RequirementGapDebug.h
// Debug readout for the requirement-gap system (per-pillar world-level proficiency).
// Mirrors USpellDataDebug / UTierPowerDebug: BlueprintFunctionLibrary statics (log +
// on-screen), callable from a debug Blueprint/widget and directly in C++. Reuses the
// SAME runtime helper (USkillDataBase::GetRequirementGapPillarPercents) so the readout
// cannot drift from live behaviour. Additive inspection only — touches no live system.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RequirementGapDebug.generated.h"

class USkillDataBase;
class UCharacterData;

/**
 * Debug utilities for the per-pillar requirement-gap scaling (TierGapConstants.h
 * GetRequirementGapMultiplier + USkillDataBase::GetRequirementGapPillarPercents).
 */
UCLASS()
class WORLD_OF_REFRACTION_API URequirementGapDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Per-pillar requirement-gap breakdown for Skill cast by Character: required level,
     *  char level, gap (Required - CharLevel), the ladder multiplier, and the resulting
     *  pillar percent — the delta AddPillarPercent applies to every substat in that pillar.
     *  The percents come from the LIVE USkillDataBase::GetRequirementGapPillarPercents and
     *  are displayed verbatim (never recomputed here), so the readout can't drift. */
    UFUNCTION(BlueprintPure, Category = "Debug|RequirementGap")
    static FString GetRequirementGapString(const USkillDataBase *Skill, const UCharacterData *Character);

    /** Log GetRequirementGapString via UE_LOG + on-screen (line by line, mirroring
     *  USpellDataDebug::PrintSpellStats). */
    UFUNCTION(BlueprintCallable, Category = "Debug|RequirementGap")
    static void PrintRequirementGap(const USkillDataBase *Skill, const UCharacterData *Character, float Duration = 15.0f);
};
