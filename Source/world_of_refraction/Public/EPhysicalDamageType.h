// EPhysicalDamageType.h
// Physical damage types for Generic character weapon infusion

#pragma once

#include "CoreMinimal.h"
#include "EPhysicalDamageType.generated.h"

UENUM(BlueprintType)
enum class EPhysicalDamageType : uint8
{
    Slash UMETA(DisplayName = "Slash (Bleed)"),
    Pierce UMETA(DisplayName = "Pierce (Armor Break)"),
    Blunt UMETA(DisplayName = "Blunt (Stun)")
};