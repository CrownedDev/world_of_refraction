// SkillDataBase.h
// Shared base class for all skill-shaped data assets (abilities, spells, weapon attacks).
// Carries fields that every skill has in common.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Loadout/LoadoutConstants.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SkillDataBase.generated.h"

/**
 * USkillDataBase
 * Truly-shared fields for abilities, spells, and weapon attacks.
 * Subclasses extend with asset-specific data (cost, requirements, delivery, etc.).
 */
UCLASS(Abstract, BlueprintType)
class WORLD_OF_REFRACTION_API USkillDataBase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Name = TEXT("Unnamed Skill");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== COMBAT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1"))
    int32 HitCount = 1;

    /** Raw mode: folds StatusBuildup into damage at the orchestrator boundary; status bar doesn't move. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bIsRawMode = false;

    /** Per-hit status buildup amount. Disabled in raw mode. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat",
              meta = (EditCondition = "!bIsRawMode", EditConditionHides, ClampMin = "0"))
    int32 StatusBuildup = 0;

    /** If true, orchestrator rejects this skill when an infusion source is selected. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bImmuneToInfusion = false;

    // ==================== EFFECTS ====================

    /**
     * Effects applied by this skill (max LoadoutConstants::MAX_SKILL_EFFECTS).
     * Each effect carries its own target, condition, and timing.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects",
              meta = (TitleProperty = "EffectType"))
    TArray<FSkillEffect> Effects;

    // ==================== EFFECT HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    TArray<FSkillEffect> GetEffectsForCondition(ESkillTrigger Condition) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDrainEffect() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasBuffEffects() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDebuffEffects() const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
#endif
};
