// EquipmentRollDebug.h
// Debug surface for the per-instance roll system (U0–U4): shows, for a live
// actor's ACTIVE loadout, which path each equipped slot took — a resolved
// owned-instance roll (the U1 bridge carried it) vs the asset-Base fallback —
// plus the actual rolled StatBonus/ResistanceBonus values reaching combat and
// each instance's Pool/MaxPool charge-meter state.
//
// This is the □4/□5 proof tool: if a slot prints "INSTANCE ROLL", the bridge
// resolved a ref and the per-instance roll is live in combat.
//
// Reads via the SAME accessors the gameplay path uses (the active loadout's
// entries — what GetActiveStatBonus consumes — plus
// GetActivePrimaryEvolutionAttachment and the GetActive* aggregates), so the
// output cannot drift from what combat actually reads.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentRollDebug.generated.h"

/**
 * Debug utilities for the per-instance equipment roll system.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UEquipmentRollDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Full per-slot roll report for the actor's active loadout: per equipped
	 *  weapon/ring/evolution — the path marker (INSTANCE ROLL vs asset-Base
	 *  fallback), the instance identity (PersistentID where it exists), the
	 *  non-zero StatBonus/ResistanceBonus values, and Pool/MaxPool state —
	 *  plus the GetActiveStatBonus/GetActiveResistanceBonus aggregates combat
	 *  actually consumes. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Debug")
	static FString GetEquipmentRollString(AActor *Actor);

	/** Screen-print GetEquipmentRollString (line-split, reverse-ordered). */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	static void PrintEquipmentRoll(AActor *Actor, float Duration = 12.0f, FLinearColor TextColor = FLinearColor::Yellow);

	/** Log GetEquipmentRollString to the output log. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	static void LogEquipmentRoll(AActor *Actor);
};
