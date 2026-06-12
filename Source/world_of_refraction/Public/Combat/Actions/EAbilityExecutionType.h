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
    /** Does this execution type require approaching the target?
     *  Legacy approach path — unhook is runner-gated (D4). */
    inline bool RequiresApproach(EAbilityExecutionType Type)
    {
        return Type == EAbilityExecutionType::Melee;
    }

    /** Get display name for UI (serves the D3 descriptive-tag filtering) */
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
