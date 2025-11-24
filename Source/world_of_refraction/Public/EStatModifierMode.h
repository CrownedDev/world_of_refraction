// EStatModifierMode.h
// Toggle between pillar-level or sub-stat-level modifiers

#pragma once

#include "CoreMinimal.h"
#include "EStatModifierMode.generated.h"

UENUM(BlueprintType)
enum class EStatModifierMode : uint8
{
    Pillar UMETA(DisplayName = "Pillar (All sub-stats in pillar)"),
    SubStats UMETA(DisplayName = "Sub-Stats (Granular control)")
};