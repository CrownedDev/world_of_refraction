// ESpellDeliveryType.h
// Defines how a spell travels from caster to target

#pragma once

#include "CoreMinimal.h"
#include "ESpellDeliveryType.generated.h"

/**
 * Spell Delivery Type
 * Determines movement behavior and defense options
 */
UENUM(BlueprintType)
enum class ESpellDeliveryType : uint8
{
    /** Travels toward target's position at cast time - dodgeable by moving */
    Projectile    UMETA(DisplayName = "Projectile"),
    
    /** Tracks target actor - harder to dodge, requires timing */
    Homing        UMETA(DisplayName = "Homing"),
    
    /** Spawns at target location - large radius, must block */
    AOE           UMETA(DisplayName = "Area of Effect"),
    
    /** Immediate effect - no travel time, tiny defense window */
    Instant       UMETA(DisplayName = "Instant")
};
