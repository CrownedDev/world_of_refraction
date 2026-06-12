// DamageSplitDebug.h
// Debug utilities for the per-hit DamageSplit table (D1)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DamageSplitDebug.generated.h"

class USkillDataBase;

/**
 * Debug utilities for DamageSplit resolution (SkillDataBase.h). Both functions
 * call the shared ResolveDamageSplit, so the output is exactly what executes.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UDamageSplitDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Log the resolved per-hit percent table for a skill, marking authored vs derived hits. */
    UFUNCTION(BlueprintCallable, Category = "Debug|Damage")
    static void PrintDamageSplitTable(const USkillDataBase *Skill);

    /** Formatted one-line summary of a skill's resolved per-hit split. */
    UFUNCTION(BlueprintPure, Category = "Debug|Damage")
    static FString GetDamageSplitString(const USkillDataBase *Skill);
};
