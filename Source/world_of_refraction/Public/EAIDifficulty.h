// EAIDifficulty.h
// AI difficulty levels for combat

#pragma once

#include "CoreMinimal.h"
#include "EAIDifficulty.generated.h"

/**
 * AI difficulty tiers affecting decision quality and reaction times
 */
UENUM(BlueprintType)
enum class EAIDifficulty : uint8
{
    Easy UMETA(DisplayName = "Easy (Tutorial Friendly)"),
    Medium UMETA(DisplayName = "Medium (Competent)"),
    Hard UMETA(DisplayName = "Hard (Optimal Play)"),
    Expert UMETA(DisplayName = "Expert (Perfect Execution)")
};