// ECombatApproachType.h
// Defines how a character approaches their target during combat actions

#pragma once

#include "CoreMinimal.h"
#include "ECombatApproachType.generated.h"

/**
 * Combat Approach Type
 * Determines movement behavior when executing attacks, abilities, or spells
 */
UENUM(BlueprintType)
enum class ECombatApproachType : uint8
{
    /** No movement - stay at grid position (ranged attacks, most spells) */
    None UMETA(DisplayName = "None (Ranged)"),

    /** Walk/run directly to target at MovementSpeed */
    Direct UMETA(DisplayName = "Direct"),

    /** Fast dash to target at 2x MovementSpeed */
    Dash UMETA(DisplayName = "Dash"),

    /** Instant teleport - no travel time */
    Teleport UMETA(DisplayName = "Teleport")
};

/**
 * Helper functions for ECombatApproachType
 */
namespace CombatApproachHelpers
{
    /** Get display name for approach type */
    inline FString GetApproachName(ECombatApproachType Type)
    {
        switch (Type)
        {
        case ECombatApproachType::None:
            return TEXT("Ranged");
        case ECombatApproachType::Direct:
            return TEXT("Direct");
        case ECombatApproachType::Dash:
            return TEXT("Dash");
        case ECombatApproachType::Teleport:
            return TEXT("Teleport");
        default:
            return TEXT("Unknown");
        }
    }

    /** Get speed multiplier for approach type */
    inline float GetSpeedMultiplier(ECombatApproachType Type)
    {
        switch (Type)
        {
        case ECombatApproachType::None:
            return 0.0f; // No movement
        case ECombatApproachType::Direct:
            return 1.0f; // Normal speed
        case ECombatApproachType::Dash:
            return 2.0f; // Double speed
        case ECombatApproachType::Teleport:
            return 0.0f; // Instant (handled specially)
        default:
            return 1.0f;
        }
    }

    /** Does this approach type require physical movement? */
    inline bool RequiresMovement(ECombatApproachType Type)
    {
        return Type == ECombatApproachType::Direct || Type == ECombatApproachType::Dash;
    }

    /** Is this approach instant (no travel time)? */
    inline bool IsInstant(ECombatApproachType Type)
    {
        return Type == ECombatApproachType::None || Type == ECombatApproachType::Teleport;
    }
}