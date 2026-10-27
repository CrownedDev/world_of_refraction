// CombatTuningDebug.h
// Inspection for the combat tuning data (B-218, CombatTuningData.h).
// - GetActiveTuningString: which asset is live (or the C++ defaults) and every value it gives.
// - CompareCombatTuning: every ladder key and single value against the C++ constants.
//   Console: wor.PrintCombatTuning, wor.CompareCombatTuning.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTuningDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UCombatTuningDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Debug|Tuning")
	static FString GetActiveTuningString();

	/** Logs and shows every value; returns how many differ from the C++ constants. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Tuning")
	static int32 CompareCombatTuning();

	UFUNCTION(BlueprintCallable, Category = "Debug|Tuning")
	static void PrintCombatTuning();
};
