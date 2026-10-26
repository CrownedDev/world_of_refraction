// WoRAttributeDebug.h
// Inspection for the GAS stage 2 attributes (B-041, GAS/WoRAttributeSets.h).
// - GetAttributesString: every mirrored attribute's current value on an actor.
// - GetAttributeMismatchString: where an attribute differs from its source getter (empty = equal).
// - CompareAttributes: runs GetAttributeMismatchString on every combatant in the world.
//   Console: wor.DumpAttributes, wor.CompareAttributes.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRAttributeDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoRAttributeDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One "Attribute=value" line per mirrored attribute, or why there are none. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAttributesString(AActor *Actor);

	/** One line per attribute whose value differs from its source getter; empty when all match. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAttributeMismatchString(AActor *Actor);

	/** Logs every combatant's mismatches; returns how many combatants differ. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 CompareAttributes(const UObject *WorldContextObject);
};
