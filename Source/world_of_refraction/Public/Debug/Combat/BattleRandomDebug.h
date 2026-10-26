// BattleRandomDebug.h
// Debug utilities for the per-battle random stream (UBattleRandomSubsystem, B-010):
// the seed in use and how many draws the fight has taken. Console: wor.Battle.PrintRandom.
// Replay a fight with wor.Battle.Seed <seed>. Additive inspection only.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BattleRandomDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UBattleRandomDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "Battle seed 123456, 42 draws (replay: wor.Battle.Seed 123456)", or a no-world note. */
	UFUNCTION(BlueprintPure, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static FString GetBattleRandomString(const UObject *WorldContextObject);

	/** Logs GetBattleRandomString and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static void PrintBattleRandomState(const UObject *WorldContextObject);
};
