// EAbilityExecutionType.h
// Defines how an ability executes - movement behavior and delivery method

#pragma once

#include "CoreMinimal.h"
#include "EAbilityExecutionType.generated.h"

/**
 * Ability Execution Type
 * Determines movement behavior, animation style, and delivery method
 */
UENUM(BlueprintType)
enum class EAbilityExecutionType : uint8
{
    /** Approach target, execute at close range (e.g., Power Strike, Heavy Slash) */
    Melee UMETA(DisplayName = "Melee (Approach Target)"),

    /** Stay in place, launch projectile at target (e.g., Javelin Toss, Ki Blast) */
    Ranged UMETA(DisplayName = "Ranged (Projectile)"),

    /** Stay in place, effect on self or allies, no projectile (e.g., War Cry, Defend) */
    SelfCast UMETA(DisplayName = "Self Cast (Support)"),

    /** Stay in place, direct effect on target, no projectile (e.g., Intimidate, Curse) */
    TargetCast UMETA(DisplayName = "Target Cast (Direct Effect)")
};

/**
 * Helper namespace for EAbilityExecutionType utilities
 */
namespace AbilityExecutionTypeHelper
{
    /** Does this execution type require approaching the target? */
    inline bool RequiresApproach(EAbilityExecutionType Type)
    {
        return Type == EAbilityExecutionType::Melee;
    }

    /** Does this execution type spawn a projectile? */
    inline bool SpawnsProjectile(EAbilityExecutionType Type)
    {
        return Type == EAbilityExecutionType::Ranged;
    }

    /** Does this execution type keep the user stationary? */
    inline bool IsStationary(EAbilityExecutionType Type)
    {
        return Type != EAbilityExecutionType::Melee;
    }

    /** Is this a support-style ability (no direct attack on enemy)? */
    inline bool IsSupportStyle(EAbilityExecutionType Type)
    {
        return Type == EAbilityExecutionType::SelfCast;
    }

    /** Get display name for UI */
    inline FString GetDisplayName(EAbilityExecutionType Type)
    {
        switch (Type)
        {
        case EAbilityExecutionType::Melee:
            return TEXT("Melee");
        case EAbilityExecutionType::Ranged:
            return TEXT("Ranged");
        case EAbilityExecutionType::SelfCast:
            return TEXT("Support");
        case EAbilityExecutionType::TargetCast:
            return TEXT("Target");
        default:
            return TEXT("Unknown");
        }
    }
}
