// EUltimateCooldownType.h
// How ultimate cooldowns function

#pragma once

#include "CoreMinimal.h"
#include "EUltimateCooldownType.generated.h"

/**
 * Cooldown behavior for ultimates
 */
UENUM(BlueprintType)
enum class EUltimateCooldownType : uint8
{
    OncePerBattle UMETA(DisplayName = "Once Per Battle"),
    TurnBased UMETA(DisplayName = "Turn-Based Cooldown")
};