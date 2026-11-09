// WoRAbilityDebug.h
// Inspection for the skill abilities (B-043b, GAS/Abilities/WoRSkillAbilities.h).
// - GetAbilitiesString: Actor's granted skill specs, one line each: the source skill, its Ability.Shape.* tags,
//   active or idle, the running action, whether its impacts route through Event.Combat.Impact (B-043c) and how
//   many it has resolved, and the plain cast's EP cost (what CheckCost reads with no action).
// - GetArmedRitualsString (B-043g): an orchestrator's armed rituals (payload, action, caster) and how each fire will
//   run: through its skill's ability or the old call, cost-free either way (paid at arm).
// - PrintAbilities: logs both for every combatant / orchestrator in the world. Console: wor.Abilities.Print.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRAbilityDebug.generated.h"

class ACombatOrchestrator;

UCLASS()
class WORLD_OF_REFRACTION_API UWoRAbilityDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A header (the switch, the ASC, the loadout skill count) and one line per granted skill spec. */
	UFUNCTION(BlueprintPure, Category = "Debug|GAS")
	static FString GetAbilitiesString(AActor *Actor);

	/** B-043g: the orchestrator's armed rituals, one line each, with the route its fire will take. Not a UFUNCTION:
	 *  ACombatOrchestrator's rituals are C++-only (FDeferredActivation). */
	static FString GetArmedRitualsString(const ACombatOrchestrator *Orchestrator);

	/** Logs GetAbilitiesString for every combatant and GetArmedRitualsString for every orchestrator; returns the
	 *  number of skill specs found. */
	UFUNCTION(BlueprintCallable, Category = "Debug|GAS", meta = (WorldContext = "WorldContextObject"))
	static int32 PrintAbilities(const UObject *WorldContextObject);
};
