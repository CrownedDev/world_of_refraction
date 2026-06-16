// SkillCastDebug.h
// Debug utilities for the skill Cast array (D6)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillCastDebug.generated.h"

class UCastableSkillDataBase;

/**
 * Debug utilities for CastArray (CastableSkillDataBase.h). Base-typed so all
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
    static void PrintCastArray(const UCastableSkillDataBase *Skill, float Duration = 15.0f);

    /** Formatted Cast array, one line per entry:
     *  "[N] <Delivery> '<Label>' Size=<f> VisualScale=<f> Speed=<f> Trail=<asset> Count=<n>" */
    UFUNCTION(BlueprintPure, Category = "Debug|Cast")
    static FString GetCastArrayString(const UCastableSkillDataBase *Skill);
};
