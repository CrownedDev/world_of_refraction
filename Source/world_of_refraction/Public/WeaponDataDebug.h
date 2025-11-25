// WeaponDataDebug.h
// Debug utilities for WeaponData

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponDataDebug.generated.h"

class UWeaponData;

UCLASS()
class WORLD_OF_REFRACTION_API UWeaponDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug|Weapon")
    static void LogWeaponStats(UWeaponData *Weapon);

    UFUNCTION(BlueprintPure, Category = "Debug|Weapon")
    static FString GetWeaponStatsString(UWeaponData *Weapon);

    UFUNCTION(BlueprintCallable, Category = "Debug|Weapon")
    static void CompareWeapons(UWeaponData *Weapon1, UWeaponData *Weapon2);
};