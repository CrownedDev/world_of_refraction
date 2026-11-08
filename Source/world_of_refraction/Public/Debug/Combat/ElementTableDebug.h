// ElementTableDebug.h
// Debug utilities for the element table (ElementTable, B-271): every row, and a comparison of each
// row against the old per-element functions. Console: wor.Elements.Print, wor.Elements.Compare.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ElementTableDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UElementTableDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per row: type, element, physical, gem, essence, break trigger, immunity, BD flags. */
	UFUNCTION(BlueprintPure, Category = "Debug|Combat")
	static FString GetElementTableString();

	/** Every place the table disagrees with an old per-element function; empty when they all agree. */
	static TArray<FString> CompareElements();

	/** Logs CompareElements (or that every row agrees) and shows the count on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat")
	static void PrintCompareElements();
};
