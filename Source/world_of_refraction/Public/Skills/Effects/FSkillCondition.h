// FSkillCondition.h
// One entry in a skill effect's condition group: a trigger + optional threshold,
// combined with the previous entry via Combine (AND / OR). May be evaluated against
// the source's state or the target's state (bTargetSide).
//
// Cluster A: additive new type for the FSkillEffect reshape. Nothing reads it yet.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillTrigger.h"
#include "Skills/Effects/ECondCombine.h"
#include "FSkillCondition.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillCondition
{
    GENERATED_BODY()

    /** When this condition is satisfied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ESkillTrigger Trigger = ESkillTrigger::Always;

    /** HP or Energy % threshold (0..100) for threshold triggers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float Threshold = 30.0f;

    /** How this condition combines with the previous entry in the group (AND / OR). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ECondCombine Combine = ECondCombine::And;

    /** True = evaluated against the TARGET's state rather than the source's. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    bool bTargetSide = false;

    /** Auto-synced (later cluster) — true when Trigger uses a threshold value.
     *  Drives EditCondition gating for Threshold. Do not edit manually. */
    UPROPERTY()
    bool bUsesThreshold = false;
};
