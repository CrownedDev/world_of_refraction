// CharacterFormDebug.h
// Debug utilities for the character-form meters (UCharacterFormSubsystem, B-279): every known form with
// its progress. Console: wor.Forms.Print. Additive inspection only.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterFormDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UCharacterFormDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** UCharacterFormSubsystem::GetFormsString, or a no-game-instance note. */
	UFUNCTION(BlueprintPure, Category = "Debug|Character", meta = (WorldContext = "WorldContextObject"))
	static FString GetFormsString(const UObject *WorldContextObject);

	/** Logs GetFormsString and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Character", meta = (WorldContext = "WorldContextObject"))
	static void PrintForms(const UObject *WorldContextObject);
};
