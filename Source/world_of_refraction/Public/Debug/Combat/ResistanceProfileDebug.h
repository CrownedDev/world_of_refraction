// ResistanceProfileDebug.h
// On-demand inspection for the class / innate-element resistance table. Prints a
// character's FULL resolved 12-value profile (9 elements + Slash/Pierce/Impact),
// the selection arm that fired (BD / Generic / Resonator / Caster-element), and
// the BD aliasing — without launching into the buildup path.
//
// Two views, because the row a character actually uses can diverge from its
// design-time row:
//   - DESIGN-TIME  (UCharacterData)  — InnateElement-based; what the asset
//                                       getters return. BD = character-created
//                                       (bBrokenDarknessInnate; InnateElement = Darkness).
//   - RUNTIME      (AActor)          — IsBrokenDarkness()-based via the actor's
//                                       CharacterDataComponent; reflects a live
//                                       BD transform.
//
// Strategic, not per-tick — call it on demand from a debug Blueprint / console.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "ResistanceProfileDebug.generated.h"

class UCharacterData;

/**
 * Debug utilities for the ClassInnateResistanceTable.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UResistanceProfileDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Design-time profile string for a data asset (InnateElement-based — matches
	 *  UCharacterData's asset getters). Footer notes that a live BD transform may
	 *  differ from this design-time row. */
	UFUNCTION(BlueprintPure, Category = "Resistances|Debug")
	static FString GetResistanceProfileString(UCharacterData *Character);

	/** Runtime profile string for a live actor (IsBrokenDarkness()-based via its
	 *  CharacterDataComponent — the row the buildup path actually resolves). */
	UFUNCTION(BlueprintPure, Category = "Resistances|Debug")
	static FString GetActorResistanceProfileString(AActor *Actor);

	/** Screen-print the RUNTIME profile for a live actor (mirrors
	 *  UCharacterDataDebug::PrintCharacterStats — line-split, reverse-ordered). */
	UFUNCTION(BlueprintCallable, Category = "Resistances|Debug")
	static void PrintResistanceProfile(AActor *Actor, float Duration = 10.0f, FLinearColor TextColor = FLinearColor::Yellow);

	/** Log the DESIGN-TIME profile for a data asset to the output log. */
	UFUNCTION(BlueprintCallable, Category = "Resistances|Debug")
	static void LogResistanceProfile(UCharacterData *Character);

	/** Combined DEFENDER-side resistance for a live actor vs an incoming
	 *  (Element, PhysicalType): the unified GetTotalStatusResistance total PLUS the
	 *  per-source breakdown (all 6 sources), the pre-final-clamp sum, and the
	 *  post-clamp total — so the clamp's effect and each source's contribution are
	 *  visible together. Every term is read via the SAME function the getter uses,
	 *  and cross-checked against GetTotalStatusResistance (MATCH/MISMATCH). Runtime
	 *  (reflects live gear / effects / BD-transform). */
	UFUNCTION(BlueprintCallable, Category = "Resistances|Debug")
	static FString GetCombinedResistanceString(AActor *Actor, ESpellElement Element, EPhysicalDamageType PhysicalType);

	/** Screen-print GetCombinedResistanceString (line-split, reverse-ordered). */
	UFUNCTION(BlueprintCallable, Category = "Resistances|Debug")
	static void PrintCombinedResistance(AActor *Actor, ESpellElement Element, EPhysicalDamageType PhysicalType,
										float Duration = 10.0f, FLinearColor TextColor = FLinearColor::Yellow);
};
