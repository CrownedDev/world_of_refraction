// CombatCameraDebug.h
// Debug utilities for the combat camera (UCombatCameraDirector, B-156). Console:
// wor.Camera (state), wor.Camera.Style 0|1|2 (centred-wide / over-the-shoulder / tactical),
// wor.Camera.MovementOff 0|1.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatCameraDebug.generated.h"

class UCombatCameraDirector;

UCLASS()
class WORLD_OF_REFRACTION_API UCombatCameraDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "In combat | mode Turn | viewing Hero_1 | style CentredWide | movement on | sequence none | 4 cameras". */
	static FString GetCameraStateString(const UCombatCameraDirector *Director);

	/** The director on the world's combat orchestrator, or null. */
	static UCombatCameraDirector *FindDirector(const UObject *WorldContextObject);

	/** Logs GetCameraStateString and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static void PrintCameraState(const UObject *WorldContextObject);
};
