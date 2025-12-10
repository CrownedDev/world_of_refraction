// ApproachData.cpp
// Approach data asset implementation

#include "ApproachData.h"

FString UApproachData::GetApproachSummary() const
{
    FString Summary = FString::Printf(TEXT("%s (%s)"),
                                      *ApproachName,
                                      *CombatApproachHelpers::GetApproachName(ApproachType));

    if (SpeedMultiplier != 1.0f)
    {
        Summary += FString::Printf(TEXT(" | Speed: %.1fx"), GetEffectiveSpeedMultiplier());
    }

    if (ApproachMontage)
    {
        Summary += FString::Printf(TEXT(" | Anim: %s"), *ApproachMontage->GetName());
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
