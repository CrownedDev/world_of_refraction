#pragma once
#include "CoreMinimal.h"
#include "EBreakability.generated.h"

/** Who can break an evolution crystal. Append-only once shipped — never reorder/remove. */
UENUM(BlueprintType)
enum class EBreakability : uint8
{
    Unbreakable  = 0  UMETA(DisplayName = "Unbreakable"),   // no one — not even Broken Darkness
    Breakable    = 1  UMETA(DisplayName = "Breakable"),     // anyone can break it
    BDBreakable  = 2  UMETA(DisplayName = "BD-Breakable")   // only Broken Darkness / Reality
};

/** Human-readable name for logging / debug inspection. */
inline FString GetBreakabilityString(EBreakability Breakability)
{
    switch (Breakability)
    {
    case EBreakability::Unbreakable: return TEXT("Unbreakable");
    case EBreakability::Breakable:   return TEXT("Breakable");
    case EBreakability::BDBreakable: return TEXT("BDBreakable");
    default:                         return TEXT("Unknown");
    }
}
