// WoRAttributeDebug.h
// Inspection for the GAS stage 2 attributes (B-041, GAS/WoRAttributeSets.h).
// - GetAttributesString: every mirrored attribute's current value on an actor.
// - GetAttributeMismatchString: where an attribute differs from its source getter (empty = equal); for
//   a composed stat (B-042q), also its base vs the capped stat and its one stat-sources effect; for a
//   derived stat (B-042s), its one derived-stat effect.
// - CompareAttributes: runs GetAttributeMismatchString on every combatant in the world.
//   Console: wor.DumpAttributes, wor.CompareAttributes.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRAttributeDebug.generated.h"

class UAbilitySystemComponent;
struct FGameplayAttribute;

UCLASS()
class WORLD_OF_REFRACTION_API UWoRAttributeDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One "Attribute=value" line per mirrored attribute, or why there are none. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAttributesString(AActor *Actor);

	/** One line per attribute whose value differs from its source getter, or whose composed base or
	 *  stat-sources effect is wrong; empty when all match. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAttributeMismatchString(AActor *Actor);

	/** Logs every combatant's mismatches; returns how many combatants differ. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 CompareAttributes(const UObject *WorldContextObject);

private:
	/** A composed stat's base vs its capped stat, and its one stat-sources effect (B-042q); a derived
	 *  stat's one derived-stat effect (B-042s). */
	static void AddComposedMismatches(const AActor *Actor, const UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute,
									  TArray<FString> &Mismatches);
};
