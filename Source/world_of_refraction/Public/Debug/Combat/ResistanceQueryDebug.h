// ResistanceQueryDebug.h
// Inspection for the per-damage-type resistance query (B-216, ResistanceQuery.h).
// - GetResistanceTypesString: an actor's resistance per damage type and the two buildup layers.
// - GetLegacyTotalStatusResistance: the buildup resistance composed the pre-B-216 way (the eight
//   sources gathered per hit), kept as the reference until the new path is proven in PIE.
// - CompareResistance: every combatant x every (element, physical type) hit, new vs legacy.
//   Console: wor.CompareResistance, wor.PrintResistanceTypes.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "ResistanceQueryDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UResistanceQueryDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per damage type (percent), then StatusResistance and StatusResistBonus. */
	UFUNCTION(BlueprintPure, Category = "Debug|Resistance")
	static FString GetResistanceTypesString(AActor *Actor);

	/** The pre-B-216 composition of UStatusBuildupManager::GetTotalStatusResistance. */
	UFUNCTION(BlueprintPure, Category = "Debug|Resistance")
	static float GetLegacyTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType);

	/** Compares new vs legacy for every combatant and every element (None included; Generic left out, Q-216) x
	 *  physical type (None included). Logs each difference; returns how many differ. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Resistance", meta = (WorldContext = "WorldContextObject"))
	static int32 CompareResistance(const UObject *WorldContextObject);

	/** Logs and shows GetResistanceTypesString for every combatant in the world. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Resistance", meta = (WorldContext = "WorldContextObject"))
	static void PrintResistanceTypes(const UObject *WorldContextObject);

	/** Same comparison for one actor (the spec uses it). */
	static int32 CompareActorResistance(AActor *Actor, bool bLogDifferences);
};
