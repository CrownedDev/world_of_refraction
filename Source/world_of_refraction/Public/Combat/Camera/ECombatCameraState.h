// ECombatCameraState.h
// Combat camera state enum

#pragma once

#include "CoreMinimal.h"
#include "ECombatCameraState.generated.h"

/**
 * Current camera state during combat
 */
UENUM(BlueprintType)
enum class ECombatCameraState : uint8
{
    None        UMETA(DisplayName = "None (Not in Combat)"),
    Home        UMETA(DisplayName = "Home (Team View)"),
    Character   UMETA(DisplayName = "Character Focus"),
    Selection   UMETA(DisplayName = "Target Selection"),
    Action      UMETA(DisplayName = "Action Execution")
};
