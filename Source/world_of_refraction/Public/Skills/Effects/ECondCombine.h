// ECondCombine.h
// How a skill condition combines with the previous entry in its group (AND / OR).

#pragma once

#include "CoreMinimal.h"
#include "ECondCombine.generated.h"

UENUM(BlueprintType)
enum class ECondCombine : uint8
{
    And UMETA(DisplayName = "And"),
    Or UMETA(DisplayName = "Or")
};
