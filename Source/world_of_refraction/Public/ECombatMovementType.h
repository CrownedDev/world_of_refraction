// ECombatMovementType.h
// Defines how a character moves during combat actions

#pragma once

#include "CoreMinimal.h"
#include "ECombatMovementType.generated.h"

/**
 * Combat Movement Type
 * Determines movement behavior when executing attacks, abilities, or spells
 */
UENUM(BlueprintType)
enum class ECombatMovementType : uint8
{
    /** No movement - stay at grid position (ranged attacks, most spells) */
    None UMETA(DisplayName = "None (Ranged)"),

    /** Walk/run directly to target at ActionSpeed */
    Direct UMETA(DisplayName = "Direct"),

    /** Fast dash to target at 2x ActionSpeed */
    Dash UMETA(DisplayName = "Dash"),

    /** Instant teleport - no travel time */
    Teleport UMETA(DisplayName = "Teleport")
};

/**
 * Helper functions for ECombatMovementType
 */
namespace CombatMovementHelpers
{
    /** Get display name for movement type */
    inline FString GetMovementName(ECombatMovementType Type)
    {
        switch (Type)
        {
        case ECombatMovementType::None:
            return TEXT("Ranged");
        case ECombatMovementType::Direct:
            return TEXT("Direct");
        case ECombatMovementType::Dash:
            return TEXT("Dash");
        case ECombatMovementType::Teleport:
            return TEXT("Teleport");
        default:
            return TEXT("Unknown");
        }
    }

    /** Get speed multiplier for movement type */
    inline float GetSpeedMultiplier(ECombatMovementType Type)
    {
        switch (Type)
        {
        case ECombatMovementType::None:
            return 0.0f;
        case ECombatMovementType::Direct:
            return 1.0f;
        case ECombatMovementType::Dash:
            return 2.0f;
        case ECombatMovementType::Teleport:
            return 0.0f;
        default:
            return 1.0f;
        }
    }

    /** Does this movement type require physical movement? */
    inline bool RequiresMovement(ECombatMovementType Type)
    {
        return Type == ECombatMovementType::Direct || Type == ECombatMovementType::Dash;
    }

    /** Is this movement instant (no travel time)? */
    inline bool IsInstant(ECombatMovementType Type)
    {
        return Type == ECombatMovementType::None || Type == ECombatMovementType::Teleport;
    }
}