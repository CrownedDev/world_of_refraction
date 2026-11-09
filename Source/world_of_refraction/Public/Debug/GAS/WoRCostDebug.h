// WoRCostDebug.h
// Inspection for skill costs paid as GameplayEffects (B-043a, GAS/WoRActionCosts.h).
// - GetCostString: which way Actor's costs are paid and why, its EP / HP beside the Vitals attributes,
//   and a probe that the cost specs carry minus the amount.
// - PrintCosts: logs it for every combatant in the world. Console: wor.Costs.Print.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRCostDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoRCostDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line: the cost path (GAS or the old calls) and its reasons, EP / HP and their attributes, the spec probe. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetCostString(AActor *Actor);

	/** Logs GetCostString for every combatant; returns how many pay through GAS. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 PrintCosts(const UObject *WorldContextObject);
};
