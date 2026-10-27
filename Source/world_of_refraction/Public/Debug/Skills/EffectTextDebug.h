// EffectTextDebug.h
// Inspection for the generated descriptions (B-045, Skills/Effects/EffectText.h).
// - GetEffectTextString: an effect list's tree and short form, with each condition's lighting.
// - GetActorEffectTextString: an actor's active gear effects, lit against its state now.
//   Console: wor.DescribeEffects (every combatant in the world).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills/Effects/EffectText.h"
#include "EffectTextDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UEffectTextDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The tree (condition lines tagged [held] / [not held] once lit), then the short form. */
	static FString GetEffectTextString(const TArray<FEffectTextLine> &Lines, const TArray<FText> &ShortForm);

	/** The actor's active gear effects as text, lit by USkillEffectManager::IsConditionHeld. */
	UFUNCTION(BlueprintPure, Category = "Debug|Effects")
	static FString GetActorEffectTextString(AActor *Actor);

	/** Logs every combatant's gear text; returns how many combatants were described. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Effects", meta = (WorldContext = "WorldContextObject"))
	static int32 DescribeEffects(const UObject *WorldContextObject);
};
