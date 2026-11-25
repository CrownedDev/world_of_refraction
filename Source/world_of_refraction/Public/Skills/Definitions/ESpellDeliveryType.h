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
    
    /** Spawns at target location - large radius, must block */
    AOE           UMETA(DisplayName = "Area of Effect"),
    
    /** Immediate effect - no travel time, tiny defense window */
    Instant       UMETA(DisplayName = "Instant")
};

/**
 * B-607 (D-275): how a Projectile cast reaches several targets. Each target's hit lands when what is sent at it
 * reaches it (WoRMultiTargetDelivery::GetArrivalSeconds).
 */
UENUM(BlueprintType)
enum class EMultiTargetDelivery : uint8
{
    /** A copy flies straight at each target, all at once (Slash Volley). */
    OnePerTarget  UMETA(DisplayName = "One Per Target"),

    /** One wide wave sweeps out toward the targets and hits each as its front reaches them (Fan Slash). */
    OneBigWave    UMETA(DisplayName = "One Big Wave")
};
