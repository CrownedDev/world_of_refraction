// InfusionDisplayDataDebug.cpp
// Debug utilities implementation

#include "InfusionDisplayDataDebug.h"
#include "InfusionDisplayData.h"

void UInfusionDisplayDataDebug::LogInfusionDisplayStats(UInfusionDisplayData *InfusionDisplay)
{
    if (!InfusionDisplay)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: InfusionDisplay is NULL"));
        return;
    }

    FString Output = GetInfusionDisplayStatsString(InfusionDisplay);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
}

FString UInfusionDisplayDataDebug::GetInfusionDisplayStatsString(UInfusionDisplayData *InfusionDisplay)
{
    if (!InfusionDisplay)
    {
        return TEXT("ERROR: InfusionDisplay is NULL");
    }

    FString Output;

    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("INFUSION DISPLAY: %s\n"), *InfusionDisplay->DisplayName);
    Output += TEXT("==========================================\n");

    if (!InfusionDisplay->Description.IsEmpty())
    {
        Output += FString::Printf(TEXT("Description: %s\n"), *InfusionDisplay->Description);
    }

    // Display location
    Output += FString::Printf(TEXT("Location: %s\n"),
                              *UEnum::GetValueAsString(InfusionDisplay->DisplayLocation));

    Output += FString::Printf(TEXT("VFX: %s\n"),
                              InfusionDisplay->VFXSystem ? *InfusionDisplay->VFXSystem->GetName() : TEXT("None"));

    if (InfusionDisplay->bOverrideElementColor)
    {
        Output += FString::Printf(TEXT("Color Override: R=%.2f G=%.2f B=%.2f\n"),
                                  InfusionDisplay->ColorOverride.R,
                                  InfusionDisplay->ColorOverride.G,
                                  InfusionDisplay->ColorOverride.B);
    }
    else
    {
        Output += TEXT("Color: Uses element default\n");
    }

    Output += FString::Printf(TEXT("Intensity: %.2fx\n"), InfusionDisplay->EffectIntensity);

    Output += TEXT("==========================================\n");

    return Output;
}