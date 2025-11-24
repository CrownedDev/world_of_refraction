// ECharacterInfusionDisplayType.h
// Types of character infusion effects (excludes weapon)

#pragma once

#include "CoreMinimal.h"
#include "ECharacterInfusionDisplayType.generated.h"

UENUM(BlueprintType)
enum class ECharacterInfusionDisplayType : uint8
{
    Body UMETA(DisplayName = "Body Effect"),
    Aura UMETA(DisplayName = "Aura Effect")
};