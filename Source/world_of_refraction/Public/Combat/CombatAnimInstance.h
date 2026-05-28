// CombatAnimInstance.h
// Custom AnimInstance that manages combat stance montages

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatAnimInstance.generated.h"

class UCharacterDataComponent;
class UCharacterData;
class UAnimMontage;
class UStanceData;

UCLASS()
class WORLD_OF_REFRACTION_API UCombatAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UCombatAnimInstance();

    // ==================== STATE ====================

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsArmed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    UCharacterDataComponent *CharacterDataComponent = nullptr;

    // ==================== MONTAGE CONTROL ====================

    /** Play the current stance idle montage (looping) */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void PlayStanceMontage();

    /** Stop the current stance montage */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopStanceMontage();

    /** Called after an action montage finishes to resume stance */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResumeStanceMontage();

    // ==================== ACTION MONTAGE (Unified) ====================

    /** Play any action montage (attack, ability, spell), auto-resumes stance when done */
    UFUNCTION(BlueprintCallable, Category = "Combat Animation")
    void PlayActionMontage(UAnimMontage *Montage, float PlayRate = 1.0f);

    /** Check if currently playing any action */
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsPlayingAction() const { return bIsPlayingAction; }

    /** Delegate broadcast when action montage ends */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionMontageEnded, UAnimMontage *, Montage, bool, bInterrupted);

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnActionMontageEnded OnActionMontageEnded;

    /** Broadcast when animation notify fires during action montage */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionNotify, FName, NotifyName);

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnActionNotify OnActionNotify;

    // ==================== MOVEMENT MONTAGE ====================

    /** Play a movement montage (approach/return), blocks stance updates */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void PlayMovementMontage(UAnimMontage *MovementMontage);

    /** Stop current movement montage */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopMovementMontage();

    /** Check if playing movement */
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsPlayingMovement() const { return bIsPlayingMovement; }

    // ==================== DEBUG ====================

protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    void CacheReferences();
    void UpdateCombatState();
    UStanceData *GetDesiredStance() const;

    /** Currently playing stance montage */
    UPROPERTY()
    UAnimMontage *CurrentStanceMontage = nullptr;

    /** Last stance we applied (to detect changes) */
    UPROPERTY()
    UStanceData *LastAppliedStance = nullptr;

    // ==================== ACTION MONTAGE (Unified) ====================

    /** Called when any action montage ends */
    UFUNCTION()
    void OnActionMontageEndedInternal(UAnimMontage *Montage, bool bInterrupted);

    bool bIsPlayingAction = false;

    UPROPERTY()
    UAnimMontage *CurrentActionMontage = nullptr;

    bool bIsPlayingMovement = false;

    UPROPERTY()
    UAnimMontage *CurrentMovementMontage = nullptr;

    /** Handle montage notify events */
    UFUNCTION()
    void HandleMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload);
};