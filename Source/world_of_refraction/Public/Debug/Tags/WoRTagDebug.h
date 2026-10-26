// WoRTagDebug.h
// Inspection for the GAS stage 1 tags (B-040, Tags/WoRGameplayTags.h).
// - GetTagMapString: every enum value and its tag, with a round-trip check.
// - GetActorTagsString: the Status / Immunity tags an actor's effects give it.
// - GetTagMismatchString: where a tag query disagrees with the matching enum query (empty = agree).
// - CompareTags: runs GetTagMismatchString on every combatant in the world.
//   Console: wor.DumpTags, wor.CompareTags.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WoRTagDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UWoRTagDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One "Enum -> Tag" line per element, class and tagged effect type; "ROUND-TRIP FAIL" marks a broken map. */
	UFUNCTION(BlueprintPure, Category = "Debug|Tags")
	static FString GetTagMapString();

	/** e.g. "Status.Stun, Immunity.DOT", or "no tags". */
	UFUNCTION(BlueprintPure, Category = "Debug|Tags")
	static FString GetActorTagsString(AActor *Actor);

	/** One line per tag query that disagrees with its enum query on Actor; empty when they all agree. */
	UFUNCTION(BlueprintPure, Category = "Debug|Tags")
	static FString GetTagMismatchString(AActor *Actor);

	/** Logs every combatant's tags and any mismatch; returns how many combatants disagree. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Tags", meta = (WorldContext = "WorldContextObject"))
	static int32 CompareTags(const UObject *WorldContextObject);
};
