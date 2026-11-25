// TrialGameMode.cpp

#include "Trial/TrialGameMode.h"


FTransform TrialReturn::GetReturnStand(const FTransform &Stand, int32 Index)
{
    if (Index <= 0)
    {
        return Stand;
    }
    const int32 Pair = (Index + 1) / 2;
    const float Side = (Index % 2 == 1) ? 1.f : -1.f;
    FTransform Out = Stand;
    Out.AddToTranslation(Stand.GetRotation().GetRightVector() * (Side * Pair * RETURN_SPACING_CM));
    return Out;
}
