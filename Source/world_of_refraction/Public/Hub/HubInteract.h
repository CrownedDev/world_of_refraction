// HubInteract.h
// B-158 (D-076): the hub / trial "E" interaction, shared by AWoRHubController (the one hub
// controller) and the retired AExplorationPlayerController: gather the in-range interactables
// (trial door, merchant, recruit, map feature), nearest first, and interact with the first.
// World of Refraction - Hub

#pragma once

#include "CoreMinimal.h"

class APawn;

namespace HubInteract
{
    /** One candidate per interactable kind; the nearest wins. */
    struct FInteractCandidate
    {
        FString Label;
        float DistSquared = TNumericLimits<float>::Max();
        TFunction<void()> Interact;
    };

    /** Every interactable in range of the pawn, nearest first. Empty without a pawn. */
    WORLD_OF_REFRACTION_API TArray<FInteractCandidate> GatherCandidates(APawn *Pawn);

    /** Interact with the nearest candidate. False when nothing is in range. LogTag names the caller. */
    WORLD_OF_REFRACTION_API bool InteractWithNearest(APawn *Pawn, const TCHAR *LogTag);

    /** Debug: what E would hit right now, with distances. */
    WORLD_OF_REFRACTION_API FString GetInteractionStateString(APawn *Pawn, const TCHAR *LogTag);
}
