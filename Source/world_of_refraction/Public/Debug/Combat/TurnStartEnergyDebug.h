// TurnStartEnergyDebug.h
// Inspection for the turn-start Energy refill (B-173, D-089, Combat/Mechanics/TurnStartEnergy.h).
// - GetEnergyString: an actor's EP, what its next turn start offers, and which gate (if any) refuses it.
// - PrintEnergy: logs it for every combatant in the world. Console: wor.Energy.Print.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TurnStartEnergyDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UTurnStartEnergyDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line: EP now, the turn-start offer (share and minimum), and the gate that refuses it, if any. */
	UFUNCTION(BlueprintPure, Category = "Debug|Combat")
	static FString GetEnergyString(AActor *Actor);

	/** Logs GetEnergyString for every combatant; returns how many would gain EP at their next turn start. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static int32 PrintEnergy(const UObject *WorldContextObject);
};
