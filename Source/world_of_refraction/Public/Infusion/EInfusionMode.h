#pragma once
#include "CoreMinimal.h"
#include "EInfusionMode.generated.h"

/** How an infusion splits its charge bonus between damage and status. Read from the equipment/character
 *  that owns the infusion source (weapon for raw/weapon-crystal, character for innate, ring for ring
 *  sources, evolution for primary-slot evolution). Append-only — never reorder/remove. */
UENUM(BlueprintType)
enum class EInfusionMode : uint8
{
    Physical = 0  UMETA(DisplayName = "Physical"),   // damage gets the bigger bonus
    Status   = 1  UMETA(DisplayName = "Status"),     // status gets the bigger bonus
    Balanced = 2  UMETA(DisplayName = "Balanced")    // the middle to both (default)
};

/** Debug/inspection string for an infusion mode. */
inline FString GetInfusionModeString(EInfusionMode Mode)
{
    switch (Mode)
    {
    case EInfusionMode::Physical: return TEXT("Physical");
    case EInfusionMode::Status:   return TEXT("Status");
    case EInfusionMode::Balanced: return TEXT("Balanced");
    default:                      return TEXT("Unknown");
    }
}
