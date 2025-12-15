// MovementData.h
// Movement approach data asset - defines how characters approach targets in combat

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ECombatMovementType.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimMontage.h"
#include "MovementData.generated.h"

/**
 * Approach Data Asset
 * Defines movement behavior, animation, VFX, and audio for combat approach
 *
 * Note: This only handles Movement. Return is always instant (jump back).
 * Ranged actions (spells, etc.) don't need MovementData - their cast animation handles it.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UMovementData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString MovementName = TEXT("Unnamed Approach");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== BEHAVIOR ====================

    /** Movement type - Direct (walk/run), Dash (fast), or Teleport (instant) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
    ECombatMovementType MovementType = ECombatMovementType::Direct;

    /** Extra speed multiplier on top of character's MovementSpeed stat */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float SpeedMultiplier = 1.0f;

    // ==================== ANIMATION ====================

    /** Animation during approach (walk, dash, etc.) - loops until arrival */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *MovementMontage = nullptr;

    /** Animation on arrival (for teleport appear effect) - plays once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *ArrivalMontage = nullptr;

    // ==================== VFX ====================

    /** VFX during movement (trail, dust, shadow, etc.) - spawned and attached */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem *TrailVFX = nullptr;

    /** VFX on arrival (smoke puff, flash, impact, etc.) - spawned once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
    UNiagaraSystem *ArrivalVFX = nullptr;

    /** VFX on departure for teleport (vanish effect) - spawned once */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (EditCondition = "MovementType == ECombatMovementType::Teleport"))
    UNiagaraSystem *DepartureVFX = nullptr;

    // ==================== AUDIO ====================

    /** Sound during movement (footsteps, whoosh, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    USoundBase *MovementSound = nullptr;

    /** Sound on arrival (land, appear, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
    USoundBase *ArrivalSound = nullptr;

    /** Sound on departure for teleport (vanish sound) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (EditCondition = "MovementType == ECombatMovementType::Teleport"))
    USoundBase *DepartureSound = nullptr;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Movement")
    FString GetDisplayName() const { return MovementName; }

    UFUNCTION(BlueprintPure, Category = "Movement")
    bool IsTeleport() const { return MovementType == ECombatMovementType::Teleport; }

    UFUNCTION(BlueprintPure, Category = "Movement")
    bool IsDash() const { return MovementType == ECombatMovementType::Dash; }

    UFUNCTION(BlueprintPure, Category = "Movement")
    bool RequiresMovement() const { return CombatMovementHelpers::RequiresMovement(MovementType); }

    /** Get effective speed multiplier (MovementType multiplier * asset multiplier) */
    UFUNCTION(BlueprintPure, Category = "Movement")
    float GetEffectiveSpeedMultiplier() const
    {
        return CombatMovementHelpers::GetSpeedMultiplier(MovementType) * SpeedMultiplier;
    }

    UFUNCTION(BlueprintPure, Category = "Movement")
    FString GetMovementSummary() const;
};
