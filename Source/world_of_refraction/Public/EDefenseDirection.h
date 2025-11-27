// EDefenseDirection.h
// Dodge direction - limited to left/right movement only

#pragma once

#include "CoreMinimal.h"
#include "EDefenseDirection.generated.h"

/**
 * Direction of dodge movement
 * Limited to left/right only (no forward/back)
 */
UENUM(BlueprintType)
enum class EDefenseDirection : uint8
{
	None UMETA(DisplayName = "None"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};
