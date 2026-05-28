// EDefenseType.h
// Defense action types for real-time defense system

#pragma once

#include "CoreMinimal.h"
#include "EDefenseType.generated.h"

/**
 * Type of defense action chosen by defender
 */
UENUM(BlueprintType)
enum class EDefenseType : uint8
{
	None UMETA(DisplayName = "None"),
	Block UMETA(DisplayName = "Block"),
	Parry UMETA(DisplayName = "Parry"),
	Dodge UMETA(DisplayName = "Dodge")
};
