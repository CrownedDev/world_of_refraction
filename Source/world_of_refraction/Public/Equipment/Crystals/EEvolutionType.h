// EEvolutionType.h
// Categories of evolution defining their stat trade-off patterns

#pragma once

#include "CoreMinimal.h"
#include "EEvolutionType.generated.h"

UENUM(BlueprintType)
enum class EEvolutionType : uint8
{
    Positive UMETA(DisplayName = "Positive (Pure Upgrade)"),
    Cursed UMETA(DisplayName = "Cursed (High Power, High Cost)"),
    Balanced UMETA(DisplayName = "Balanced (Trade-offs)"),
    Specialist UMETA(DisplayName = "Specialist (Focused)")
};