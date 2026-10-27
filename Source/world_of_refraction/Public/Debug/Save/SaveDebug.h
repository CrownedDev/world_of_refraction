// SaveDebug.h
// Inspection for the save keystone (B-090, Save/SaveSubsystem.h).
// - GetSaveString: what a save holds (party, wallet, pool counts).
// Console: wor.Save.Print (the slot as it stands now), wor.Save.Now (capture and write),
//          wor.Save.Delete (remove the slot; the next start is fresh).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Save/WoRSaveTypes.h"
#include "SaveDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API USaveDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FString GetSaveString(const FWoRSaveData &Data);

	/** Logs the live slot (after capturing the world's current state into it, without writing). */
	UFUNCTION(BlueprintCallable, Category = "Debug|Save", meta = (WorldContext = "WorldContextObject"))
	static void PrintSaveState(const UObject *WorldContextObject);
};
