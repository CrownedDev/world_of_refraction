// EStatScalingType.h
// Which stat (if any) scales ability effects

#pragma once

#include "CoreMinimal.h"
#include "EStatScalingType.generated.h"

UENUM(BlueprintType)
enum class EStatScalingType : uint8
{
    None    UMETA(DisplayName = "None (Fixed Values)"),
    Body    UMETA(DisplayName = "Body (Raw Damage Multiplier)"),
    Spirit  UMETA(DisplayName = "Spirit (Effect Damage Multiplier)"),
    Mind    UMETA(DisplayName = "Mind (Bonus Crit Chance + Crit Damage)")
};