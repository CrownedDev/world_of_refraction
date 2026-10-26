// StoneRowDebug.h
// Inspection for the stone row table (B-021): every stat stone's attached sub-stat, consumable
// item effect, buff/debuff pair and F..S base percent. Console: wor.PrintStoneRows.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StoneRowDebug.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API UStoneRowDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per row, e.g. "LuckStone: stat Luck, item BuffLuck, LuckBuff / LuckDebuff, 3-15%". */
	UFUNCTION(BlueprintPure, Category = "Debug|Items")
	static FString GetStoneRowsString();

	/** Logs GetStoneRowsString. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Items")
	static void PrintStoneRows();
};
