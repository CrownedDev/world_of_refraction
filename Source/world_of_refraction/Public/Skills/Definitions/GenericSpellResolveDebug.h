// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GenericSpellResolveDebug.generated.h"

class AActor;
class USpellData;

/**
 * Debug utilities for the GenericSpellInherit resolve-at-cast feature.
 *
 * Inspects the full resolve chain for a Generic (polymorphic) spell:
 *   authored element -> resolved source/pool element -> display name -> colour,
 * plus the Broken Darkness Model-B active-pool rotation. Additive — touches no
 * existing system. Call the statics from a debug Blueprint / widget.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UGenericSpellResolveDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Log + on-screen the full resolve for one spell cast by Caster:
     *  authored -> resolved -> name -> colour (+ BD state / active pool). */
    UFUNCTION(BlueprintCallable, Category = "GenericSpell Debug")
    static void PrintResolvedElement(AActor *Caster, USpellData *Spell, float Duration = 15.0f);

    /** Same as PrintResolvedElement, returned as an FString for UI / inspection. */
    UFUNCTION(BlueprintPure, Category = "GenericSpell Debug")
    static FString GetResolveString(AActor *Caster, USpellData *Spell);

    /** Dump the Broken Darkness Model-B pool state for Caster: the single active
     *  pool (GetActivePool) and each pool (Darkness/InnateSpells + every
     *  BDSpellPools entry) with spell count and an ACTIVE/dormant marker, so the
     *  rotation is inspectable. Null-guards the caster; prints "not BD" otherwise. */
    UFUNCTION(BlueprintCallable, Category = "GenericSpell Debug")
    static void PrintBDPools(AActor *Caster, float Duration = 15.0f);
};
