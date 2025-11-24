// InfusionDisplayDataDebug.h
// Debug utilities for InfusionDisplayData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InfusionDisplayDataDebug.generated.h"

class UInfusionDisplayData;

UCLASS()
class WORLD_OF_REFRACTION_API UInfusionDisplayDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug|InfusionDisplay")
    static void LogInfusionDisplayStats(UInfusionDisplayData *InfusionDisplay);

    UFUNCTION(BlueprintPure, Category = "Debug|InfusionDisplay")
    static FString GetInfusionDisplayStatsString(UInfusionDisplayData *InfusionDisplay);
};