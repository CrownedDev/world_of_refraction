// EFirstStrike.h
// B-185 (Enemies §2, Q-185): which side struck first on the map. That side takes its whole
// opening round before the other side moves (Crown, 2026-10-26).

#pragma once

#include "CoreMinimal.h"
#include "EFirstStrike.generated.h"

UENUM(BlueprintType)
enum class EFirstStrike : uint8
{
	None UMETA(DisplayName = "No first strike"),
	Party UMETA(DisplayName = "Party strikes first"),
	Enemy UMETA(DisplayName = "Enemy strikes first")
};
