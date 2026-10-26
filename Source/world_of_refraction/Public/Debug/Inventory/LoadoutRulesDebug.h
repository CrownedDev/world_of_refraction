// LoadoutRulesDebug.h
// Debug utilities for the one loadout rule set (FLoadoutRules, B-028). Console:
// wor.Loadout.CompareRules prints, per character in the world, what the merged pre-fight check
// reports that the old one didn't (and the reverse). Additive inspection only.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LoadoutRulesDebug.generated.h"

class ULoadoutComponent;

UCLASS()
class WORLD_OF_REFRACTION_API ULoadoutRulesDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "Hero_1 loadout 0: 3 findings (old 1) | new: Ability 'Cleave' requires weapon type ... | gone: none". */
	static FString GetCompareRulesString(const ULoadoutComponent *Loadout);

	/** Logs GetCompareRulesString for every character in the world and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Loadout", meta = (WorldContext = "WorldContextObject"))
	static void CompareLoadoutRules(const UObject *WorldContextObject);
};
