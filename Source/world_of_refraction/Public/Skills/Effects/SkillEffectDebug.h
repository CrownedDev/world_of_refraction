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

    /** Cluster-B parity proof. Migrates a copy of Effect from its legacy flat fields
     *  via the SAME FSkillEffect::MigrateLegacyToNew PostSerialize runs, then asserts
     *  every classifier (IsValid/IsBuff/IsDebuff/IsRestore/IsDrain/IsConditional/
     *  IsConditionalEffect) returns identical legacy-vs-new results, and that the
     *  Conditions[]/Payloads[] counts match the drain rule. Returns false + a diff
     *  report in OutReport on any mismatch; OutReport = "OK …" on success. */
    WORLD_OF_REFRACTION_API bool CompareEffect(const FSkillEffect &Effect, FString &OutReport);
}
