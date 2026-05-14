// FPillarWeights.h
// Three-float pillar bias used by UEquipmentBonusGenerator to allocate
// substat capacity points across Mind / Body / Spirit. Values are
// normalised at consumption — only their RATIOS matter. Defaults are
// 1/1/1 (equal weight across pillars).

#pragma once

#include "CoreMinimal.h"
#include "FPillarWeights.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPillarWeights
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0"))
    float Mind = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0"))
    float Body = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = "0.0"))
    float Spirit = 1.0f;

    /** Sum of the three weights (≥ 0). */
    float Total() const { return Mind + Body + Spirit; }

    /** True when all three weights are zero — caller should treat as equal-split fallback. */
    bool IsEmpty() const { return Total() <= 0.0f; }
};
