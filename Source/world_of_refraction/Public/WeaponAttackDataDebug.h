// WeaponAttackDataDebug.h
// Debug utilities for weapon attacks

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponAttackDataDebug.generated.h"

class UWeaponAttackData;

UCLASS()
class WORLD_OF_REFRACTION_API UWeaponAttackDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug")
    static void PrintAttackStats(UWeaponAttackData *Attack, float Duration = 5.0f);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    static void LogAttackStats(UWeaponAttackData *Attack);

    UFUNCTION(BlueprintPure, Category = "Debug")
    static FString GetAttackStatsString(UWeaponAttackData *Attack);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    static void CompareAttacks(UWeaponAttackData *Attack1, UWeaponAttackData *Attack2);
};