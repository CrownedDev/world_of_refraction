// MovementData.cpp
// Approach data asset implementation

#include "MovementData.h"

FString UMovementData::GetMovementSummary() const
{
    FString Summary = FString::Printf(TEXT("%s (%s)"),
                                      *MovementName,
                                      *CombatMovementHelpers::GetMovementName(MovementType));

    if (SpeedMultiplier != 1.0f)
    {
        Summary += FString::Printf(TEXT(" | Speed: %.1fx"), GetEffectiveSpeedMultiplier());
    }

    if (MovementMontage)
    {
        Summary += FString::Printf(TEXT(" | Anim: %s"), *MovementMontage->GetName());
    }

    if (TrailVFX)
    {
        Summary += TEXT(" | Has Trail VFX");
    }

    if (ArrivalVFX)
    {
        Summary += TEXT(" | Has Arrival VFX");
    }

    return Summary;
}
