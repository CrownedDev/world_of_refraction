// SkillEffectDebug.h
// Inspection-only helpers for FSkillEffect during the dynamic-effect reshape.
// DescribeEffect prints the legacy flat fields alongside the new Conditions[] /
// Payloads[] arrays so migration can be eyeballed without launching PIE.
//
// Cluster A: no behaviour, inspection only. UFUNCTION-free namespace by design.

#pragma once

#include "CoreMinimal.h"

struct FSkillEffect;

namespace SkillEffectDebug
{
    /** Multi-line dump: legacy flat condition/payload fields beside the new
     *  Conditions[] and Payloads[] arrays, side by side. */
    WORLD_OF_REFRACTION_API FString DescribeEffect(const FSkillEffect &Effect);

    /** UE_LOG wrapper around DescribeEffect (LogTemp, Display). */
    WORLD_OF_REFRACTION_API void LogEffect(const FSkillEffect &Effect);
}
