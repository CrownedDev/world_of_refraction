// EInfusionDisplayLocation.h
// Determines where infusion VFX spawns on character

#pragma once

#include "CoreMinimal.h"
#include "EInfusionDisplayLocation.generated.h"

/**
 * Where the infusion VFX displays on the character
 * Set per-asset to categorize infusion display styles
 */
UENUM(BlueprintType)
enum class EInfusionDisplayLocation : uint8
{
    /** VFX attached to weapon/hand socket */
    Weapon UMETA(DisplayName = "Weapon Glow"),

    /** VFX attached to body/pelvis socket */
    Body UMETA(DisplayName = "Body Glow"),

    /** VFX floating around character */
    Aura UMETA(DisplayName = "Aura/Orbs")
};