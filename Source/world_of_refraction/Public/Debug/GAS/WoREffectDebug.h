// WoREffectDebug.h
// Inspection for the GAS effect mirror (B-042c, GAS/WoREffectMirrorComponent.h).
// - GetEffectMismatchString: for every ported effect type, where the mirrored GEs' "has it" or
//   total (overall and per element) differs from the stored enum-path effects (empty = equal).
// - CompareEffects: runs it on every mirrored combatant in the world.
//   Console: wor.CompareEffects.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoREffectDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoREffectDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per ported type whose GAS read differs from the enum path; empty when all match. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetEffectMismatchString(AActor *Actor);

	/** Logs every mirrored combatant's mismatches (with its mirror); returns how many differ. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 CompareEffects(const UObject *WorldContextObject);
};
