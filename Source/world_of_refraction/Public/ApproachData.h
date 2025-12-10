// ApproachData.h
// Movement approach data asset - defines how characters approach targets in combat

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ECombatApproachType.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimMontage.h"
#include "ApproachData.generated.h"

/**
 * Approach Data Asset
 * Defines movement behavior, animation, VFX, and audio for combat approach
 * 
 * Note: This only handles APPROACH movement. Return is always instant (jump back).
 * Ranged actions (spells, etc.) don't need ApproachData - their cast animation handles it.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UApproachData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString ApproachName = TEXT("Unnamed Approach");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== BEHAVIOR ====================

    /** Movement type - Direct (walk/run), Dash (fast), or Teleport (instant) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
    ECombatApproachType ApproachType = ECombatApproachType::Direct;

    /** Extra speed multiplier on top of character's MovementSpeed stat */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float SpeedMultiplier = 1.0f;

    // ==================== ANIMATION ====================

    /** Animation during approach (walk, dash, etc.) - loops until arrival */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* ApproachMontage = nullptr;

    /** Animation on arrival (for teleport appear effect) - plays once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* ArrivalMontage = nullptr;

    // ==================== VFX ====================

    /** VFX during movement (trail, dust, shadow, etc.) - spawned and attached */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* TrailVFX = nullptr;

    /** VFX on arrival (smoke puff, flash, impact, etc.) - spawned once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem* ArrivalVFX = nullptr;

    /** VFX on departure for teleport (vanish effect) - spawned once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (EditCondition = "ApproachType == ECombatApproachType::Teleport"))
    UNiagaraSystem* DepartureVFX = nullptr;

    // ==================== AUDIO ====================

    /** Sound during movement (footsteps, whoosh, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    USoundBase* MovementSound = nullptr;

    /** Sound on arrival (land, appear, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    USoundBase* ArrivalSound = nullptr;

    /** Sound on departure for teleport (vanish sound) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (EditCondition = "ApproachType == ECombatApproachType::Teleport"))
    USoundBase* DepartureSound = nullptr;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Approach")
    FString GetDisplayName() const { return ApproachName; }

    UFUNCTION(BlueprintPure, Category = "Approach")
    bool IsTeleport() const { return ApproachType == ECombatApproachType::Teleport; }

    UFUNCTION(BlueprintPure, Category = "Approach")
    bool IsDash() const { return ApproachType == ECombatApproachType::Dash; }

    UFUNCTION(BlueprintPure, Category = "Approach")
    bool RequiresMovement() const { return CombatApproachHelpers::RequiresMovement(ApproachType); }

    /** Get effective speed multiplier (ApproachType multiplier * asset multiplier) */
    UFUNCTION(BlueprintPure, Category = "Approach")
    float GetEffectiveSpeedMultiplier() const
    {
        return CombatApproachHelpers::GetSpeedMultiplier(ApproachType) * SpeedMultiplier;
    }

    UFUNCTION(BlueprintPure, Category = "Approach")
    FString GetApproachSummary() const;
};
