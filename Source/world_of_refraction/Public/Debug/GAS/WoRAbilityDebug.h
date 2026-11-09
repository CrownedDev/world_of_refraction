// WoRAbilityDebug.h
// Inspection for the skill abilities (B-043b, GAS/Abilities/WoRSkillAbilities.h).
// - GetAbilitiesString: Actor's granted skill specs, one line each: the source skill, its Ability.Shape.* tags,
//   active or idle, the running action, whether its impacts route through Event.Combat.Impact (B-043c) and how
//   many it has resolved, and the plain cast's EP cost (what CheckCost reads with no action).
// - PrintAbilities: logs it for every combatant in the world. Console: wor.Abilities.Print.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRAbilityDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoRAbilityDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A header (the switch, the ASC, the loadout skill count) and one line per granted skill spec. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAbilitiesString(AActor *Actor);

	/** Logs GetAbilitiesString for every combatant; returns the number of skill specs found. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 PrintAbilities(const UObject *WorldContextObject);
};
