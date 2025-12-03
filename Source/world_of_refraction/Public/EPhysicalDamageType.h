// EPhysicalDamageType.h
// Physical damage types for Generic character weapon infusion

#pragma once

#include "CoreMinimal.h"
#include "EPhysicalDamageType.generated.h"

UENUM(BlueprintType)
enum class EPhysicalDamageType : uint8
{
    Slash,  // → Bleed
    Pierce, // → Armor Break
    Blunt   // → Stun
};