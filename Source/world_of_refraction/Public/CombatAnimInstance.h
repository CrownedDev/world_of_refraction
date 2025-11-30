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
};