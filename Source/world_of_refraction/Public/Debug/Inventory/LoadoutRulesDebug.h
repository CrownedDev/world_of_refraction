// LoadoutRulesDebug.h
// Debug utilities for the one loadout rule set (FLoadoutRules, B-028). Console:
// wor.Loadout.CompareRules prints, per character in the world, what the merged pre-fight check
// reports that the old one didn't (and the reverse). Additive inspection only.
// wor.Loadout.TryAbility <name> tries a learned ability as a pick on each character's active
// loadout (B-159: refused with a reason, or assigned) until the loadout screen exists.

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

	/** B-302: the unified slot mirror (D-110) against the old getters for the active loadout, e.g.
	 *  "Hero_1 slots 3: weapon Sword = Sword, adapter Ember = Ember | match". */
	static FString GetCompareSlotsString(const ULoadoutComponent *Loadout);

	/** Logs GetCompareSlotsString for every character in the world and shows it on screen (wor.Loadout.CompareSlots). */
	UFUNCTION(BlueprintCallable, Category = "Debug|Loadout", meta = (WorldContext = "WorldContextObject"))
	static void CompareLoadoutSlots(const UObject *WorldContextObject);

	/** e.g. "Hero_1: 'Cleave' refused - Ability 'Cleave' requires weapon type Spear". */
	static FString TryAbilityPick(ULoadoutComponent *Loadout, const FString &AbilityName);
};
