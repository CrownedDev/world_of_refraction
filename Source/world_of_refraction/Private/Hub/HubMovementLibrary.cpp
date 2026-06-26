// HubMovementLibrary.cpp

#include "Hub/HubMovementLibrary.h"
#include "Character/CharacterDataComponent.h"
#include "GameFramework/Pawn.h"

namespace HubMovement
{
    // GetEffectiveActionSpeed() returns a pure x1.0-x2.0 multiplier (x1.0 at zero
    // gear, capped at STAT_MODIFIER_MAX). Map that band linearly to the sprint band.
    constexpr float ACTIONSPEED_MULT_MIN = 1.0f;
    constexpr float ACTIONSPEED_MULT_MAX = 2.0f;
    constexpr float SPRINT_SPEED_MIN = 800.0f;  // at multiplier 1.0
    constexpr float SPRINT_SPEED_MAX = 1800.0f; // at multiplier 2.0
}

float UHubMovementLibrary::ComputeHubSprintSpeed(APawn *Pawn)
{
    using namespace HubMovement;

    if (!Pawn)
    {
        return SPRINT_SPEED_MIN;
    }

    UCharacterDataComponent *Comp = Pawn->FindComponentByClass<UCharacterDataComponent>();
    if (!Comp)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[HubMovement] %s has no CharacterDataComponent; sprint -> %.0f (band min)"),
               *Pawn->GetName(), SPRINT_SPEED_MIN);
        return SPRINT_SPEED_MIN;
    }

    const float Mult = FMath::Clamp(Comp->GetEffectiveActionSpeed(), ACTIONSPEED_MULT_MIN, ACTIONSPEED_MULT_MAX);
    const float Alpha = (Mult - ACTIONSPEED_MULT_MIN) / (ACTIONSPEED_MULT_MAX - ACTIONSPEED_MULT_MIN);
    const float SprintSpeed = FMath::Lerp(SPRINT_SPEED_MIN, SPRINT_SPEED_MAX, Alpha);

    UE_LOG(LogTemp, Verbose, TEXT("[HubMovement] %s ActionSpeed x%.3f -> sprint %.0f"),
           *Pawn->GetName(), Mult, SprintSpeed);

    return SprintSpeed;
}
