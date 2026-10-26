// StatModifierDebug.h
// Inspection for the stat-modifier table (B-020) and the composed stats it feeds.
// - GetStatModifierString: each sub-stat's net transient modifier on an actor.
// - GetStatsSnapshotString: one line per composed stat getter (the values a refactor must keep).
// - CompareStats: snapshots every combatant in the world and lists what changed since the last
//   call. Console: wor.CompareStats (run once, change something, run again), wor.PrintStatModifiers.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StatModifierDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UStatModifierDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "Luck +15.0%, Defense -5.0%", or "no active stat modifiers". */
	UFUNCTION(BlueprintPure, Category = "Debug|Stats")
	static FString GetStatModifierString(AActor *Actor);

	/** "Name=value" lines for every composed stat getter, in a fixed order. */
	UFUNCTION(BlueprintPure, Category = "Debug|Stats")
	static FString GetStatsSnapshotString(AActor *Actor);

	/** Snapshots every actor with a CharacterDataComponent; logs the lines that changed since the
	 *  previous call (or "first snapshot"). Snapshots are kept per actor name for the editor session. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Stats", meta = (WorldContext = "WorldContextObject"))
	static void CompareStats(const UObject *WorldContextObject);

	/** Logs and shows GetStatModifierString for every combatant in the world. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Stats", meta = (WorldContext = "WorldContextObject"))
	static void PrintStatModifiers(const UObject *WorldContextObject);
};
