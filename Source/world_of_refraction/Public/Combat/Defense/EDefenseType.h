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
	/** Guard (D-084, B-168): a block on normal timing, a parry on Perfect timing. */
	Block UMETA(DisplayName = "Guard"),
	/** Legacy: a Parry press is a Guard press (GuardRules::Normalize). */
	Parry UMETA(DisplayName = "Parry (legacy: Guard)"),
	Dodge UMETA(DisplayName = "Dodge")
};
