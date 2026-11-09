// ActionPipsDebug.h
// B-170a: inspection for the banked actions on UCharacterDataComponent (ActionPips.md §3.7).
// - GetActionsString: one combatant's actions / Max (action points and progress) / regain (Reflex share).
// - PrintActions: that line for every combatant in the world, then the current turn's spends (B-170c).
// Console: wor.Actions.Print; wor.Actions.AddPoints <N> (debug action points on every combatant, to see Max
// rise before any asset authors them); wor.Actions.Reset (every combatant to its fight-start actions).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionPipsDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UActionPipsDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "actions 2 / Max 3 (+75/100) | points 175 (gear 0 + ... ) | regain 2 (Reflex share 0.62)". */
	UFUNCTION(BlueprintPure, Category = "Debug|Actions")
	static FString GetActionsString(AActor *Actor);

	/** Logs and shows GetActionsString for every actor with a CharacterDataComponent. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Actions", meta = (WorldContext = "WorldContextObject"))
	static void PrintActions(const UObject *WorldContextObject);

	/** Adds Points debug action points to every combatant (negative takes them away), then prints. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Actions", meta = (WorldContext = "WorldContextObject"))
	static void AddDebugActionPoints(const UObject *WorldContextObject, int32 Points);

	/** Sets every combatant to its fight-start actions (the regain, capped at Max), then prints. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Actions", meta = (WorldContext = "WorldContextObject"))
	static void ResetActionsForFight(const UObject *WorldContextObject);
};
