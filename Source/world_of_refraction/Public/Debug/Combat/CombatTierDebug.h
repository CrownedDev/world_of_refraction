// CombatTierDebug.h
// Debug utilities for Combat Tier (B-325, D-118, WoRCombatTier): each combatant's tier, the tiers it averaged
// and the fallback. Console: wor.Combat.PrintTiers. Additive inspection only.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTierDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UCombatTierDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "Goblin: Tier C from 3 (C C B), tier-power HP x2.20", or "... no gear or skills, fallback D". */
	UFUNCTION(BlueprintPure, Category = "Debug|Combat")
	static FString GetCombatTierString(const AActor *Actor);

	/** Logs GetCombatTierString for every actor with character data in the world, and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static void PrintCombatTiers(const UObject *WorldContextObject);
};
