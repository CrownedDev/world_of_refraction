// EUltimateType.h
// Categories of ultimate abilities

#pragma once

#include "CoreMinimal.h"
#include "EUltimateType.generated.h"

/**
 * Types of ultimate abilities in World of Refraction
 * Determines primary function and UI presentation
 */
UENUM(BlueprintType)
enum class EUltimateType : uint8
{
    Damage UMETA(DisplayName = "Damage (Single Target)"),
    DamageAOE UMETA(DisplayName = "Damage (Area of Effect)"),
    CrowdControl UMETA(DisplayName = "Crowd Control (Stun/Freeze/Silence)"),
    Buff UMETA(DisplayName = "Buff (Self Enhancement)"),
    Debuff UMETA(DisplayName = "Debuff (Enemy Weakening)"),
    Heal UMETA(DisplayName = "Heal (Restoration)"),
    Utility UMETA(DisplayName = "Utility (Unique Mechanics)")
};