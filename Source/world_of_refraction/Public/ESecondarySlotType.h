// ESecondarySlotType.h
#pragma once

#include "CoreMinimal.h"
#include "ESecondarySlotType.generated.h"

UENUM(BlueprintType)
enum class ESecondarySlotType : uint8
{
    None UMETA(DisplayName = "None"),
    Weapon UMETA(DisplayName = "Weapon"),
    Ring UMETA(DisplayName = "Ring")
};