// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterData.h"
#include "CharacterDataDebug.generated.h"

/**
 * Debug utilities for CharacterData
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCharacterDataDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Print all character stats to screen
	UFUNCTION(BlueprintCallable, Category = "Character Debug")
	static void PrintCharacterStats(UCharacterData* Character, float Duration = 10.0f, FLinearColor TextColor = FLinearColor::Green);

	// Print stats to log only (no screen print)
	UFUNCTION(BlueprintCallable, Category = "Character Debug")
	static void LogCharacterStats(UCharacterData* Character);

	// Get formatted stats as string (for UI display)
	UFUNCTION(BlueprintPure, Category = "Character Debug")
	static FString GetCharacterStatsString(UCharacterData* Character);
};