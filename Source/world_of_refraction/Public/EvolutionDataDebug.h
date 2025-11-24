// EvolutionDataDebug.h
// Debug utilities for EvolutionData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EvolutionData.h"
#include "CharacterData.h"
#include "EvolutionDataDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UEvolutionDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug|Evolution")
    static void PrintEvolutionStats(UEvolutionData *Evolution, UCharacterData *Character = nullptr, float Duration = 10.0f);

    UFUNCTION(BlueprintCallable, Category = "Debug|Evolution")
    static void LogEvolutionStats(UEvolutionData *Evolution, UCharacterData *Character = nullptr);

    UFUNCTION(BlueprintPure, Category = "Debug|Evolution")
    static FString GetEvolutionStatsString(UEvolutionData *Evolution, UCharacterData *Character = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Debug|Evolution")
    static void CompareEvolutions(UEvolutionData *Evolution1, UEvolutionData *Evolution2, UCharacterData *Character = nullptr);
};