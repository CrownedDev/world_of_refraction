// EActionCameraPhase.h
// Action camera phase enum - sub-states during Action camera

#pragma once

#include "CoreMinimal.h"
#include "EActionCameraPhase.generated.h"

/**
 * Current phase within Action camera state
 * Action camera reframes based on what's happening
 */
UENUM(BlueprintType)
enum class EActionCameraPhase : uint8
{
    None        UMETA(DisplayName = "None"),
    Approach    UMETA(DisplayName = "Approach (Following Movement)"),
    Cast        UMETA(DisplayName = "Cast (Focus on Caster)"),
    Execution   UMETA(DisplayName = "Execution (Wide Shot)"),
    Defense     UMETA(DisplayName = "Defense (Hold Wide)"),
    Result      UMETA(DisplayName = "Result (Reactions)")
};
