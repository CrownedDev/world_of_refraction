// LoadoutRulesDebug.h
// Debug utilities for the one loadout rule set (FLoadoutRules, B-028). Console:
// wor.Loadout.CompareSlots prints the unified slot mirror against the old getters.
// wor.Loadout.PrintFindings prints what the rules report on each character's active loadout.
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
	/** Logs what the loadout rules report on every character's active loadout and shows it on screen
	 *  (wor.Loadout.PrintFindings), e.g. "Hero_1 loadout 0: 1 finding(s): Ability 'Cleave' requires weapon type Spear".
	 *  TODO: the name is from the old-against-new comparison (B-028); rename to PrintLoadoutFindings once the editor's
	 *  Find in Blueprints shows no "Compare Loadout Rules" node (B-172 kept it so no Blueprint can break). */
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
