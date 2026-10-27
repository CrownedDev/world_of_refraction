// DamageMathDebug.h
// Inspection for the damage maths core (B-219, DamageMath.h): what UDamageCalculator gathers from
// two actors and what WoRDamageMath::Compute makes of it, crit off so no battle-random draw is used.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DamageMathDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UDamageMathDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The gathered inputs and the result for BaseDamage from Attacker to Defender (crit off). */
	UFUNCTION(BlueprintPure, Category = "Debug|Damage", meta = (WorldContext = "WorldContext"))
	static FString GetDamageMathString(const UObject *WorldContext, AActor *Attacker, AActor *Defender, int32 BaseDamage,
									   bool bIsSpell);

	/** Logs GetDamageMathString and shows its result line on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Damage", meta = (WorldContext = "WorldContext"))
	static void PrintDamageMath(const UObject *WorldContext, AActor *Attacker, AActor *Defender, int32 BaseDamage,
								bool bIsSpell);
};
