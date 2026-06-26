// HubMovementLibrary.h
// Hub-only movement helpers. Maps the player's effective ActionSpeed stat to a
// hub sprint speed so faster characters sprint faster. Hub-only by design —
// combat is fixed-position and does not read MaxWalkSpeed for traversal.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HubMovementLibrary.generated.h"

class APawn;

UCLASS()
class WORLD_OF_REFRACTION_API UHubMovementLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Map the pawn's effective ActionSpeed multiplier [1.0, 2.0] to the hub sprint
     *  speed band [700, 1050]. Reads GetEffectiveActionSpeed() off the pawn's
     *  CharacterDataComponent (allocated ActionSpeed points + world-stat presets +
     *  equipment all folded in). Intended to be called live at sprint-press so it
     *  always reflects the current value. Returns the band minimum if the pawn is
     *  null or has no CharacterDataComponent. */
    UFUNCTION(BlueprintCallable, Category = "Hub|Movement")
    static float ComputeHubSprintSpeed(APawn *Pawn);
};
