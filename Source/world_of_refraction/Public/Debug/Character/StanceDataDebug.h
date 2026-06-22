// StanceDataDebug.h
// Debug utilities for StanceData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StanceDataDebug.generated.h"

class UStanceData;

UCLASS()
class WORLD_OF_REFRACTION_API UStanceDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug|Stance")
    static void LogStanceStats(UStanceData *Stance);

    UFUNCTION(BlueprintPure, Category = "Debug|Stance")
    static FString GetStanceStatsString(UStanceData *Stance);
};