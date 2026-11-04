// SkillCastDebug.h
// Debug utilities for the skill Cast array (D6)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillCastDebug.generated.h"

class USkillDataBase;

/**
 * Debug utilities for CastArray (SkillDataBase.h). Base-typed so all
 * three skill types (spell / ability / attack) print. On a migrated spell the
 * entry should show Size = BaseSize × HitboxRatio and Trail = the old SpellVFX.
 */
UCLASS()
class WORLD_OF_REFRACTION_API USkillCastDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Print the Cast array to screen + log, one line per entry. */
    UFUNCTION(BlueprintCallable, Category = "Debug|Cast")
    static void PrintCastArray(const USkillDataBase *Skill, float Duration = 15.0f);

    /** Formatted Cast array, one line per entry:
     *  "[N] <Delivery> '<Label>' Size=<f> VisualScale=<f> Speed=<f> Trail=<asset> Count=<n>" */
    UFUNCTION(BlueprintPure, Category = "Debug|Cast")
    static FString GetCastArrayString(const USkillDataBase *Skill);

    /** B-367 (D-161): the skill's interrupt points, "Interrupt points: hit 2, hit 4, cast [1]" (or "none"): the hit
     *  entries (DamageSplit, 1-based hit number) and cast entries (CastArray index) marked bInterruptable. A Perfect
     *  Guard or Perfect Dodge by every target on one ends the skill there. PrintCastArray prints it too. */
    UFUNCTION(BlueprintPure, Category = "Debug|Cast")
    static FString GetInterruptPointsString(const USkillDataBase *Skill);
};
