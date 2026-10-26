// PartyDisplayWidgetDebug.h
// Debug utilities for UPartyDisplayWidget (Arc 1.5b Cluster 3): snapshot the
// strip's card states without clicking through the UI — per the per-system
// debug-tooling rule. Callable from BP graphs or the editor's Blueprint
// debugger; PrintPartyDisplayState mirrors the string on screen.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PartyDisplayWidgetDebug.generated.h"

class UPartyDisplayWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UPartyDisplayWidgetDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Formatted snapshot: panel count, per-slot EPanelDisplayMode
	 *  (Vacant/Roster/Live), and the display name shown in each non-vacant
	 *  slot (cached on the display — the panel's data accessor is private).
	 *  Null-safe. */
	UFUNCTION(BlueprintPure, Category = "Party Debug")
	static FString GetPartyDisplayStateString(const UPartyDisplayWidget *Display);

	/** Log + on-screen print of GetPartyDisplayStateString. */
	UFUNCTION(BlueprintCallable, Category = "Party Debug")
	static void PrintPartyDisplayState(const UPartyDisplayWidget *Display, float Duration = 8.0f);
};
