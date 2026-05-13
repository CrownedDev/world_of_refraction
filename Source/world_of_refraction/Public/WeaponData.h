// WeaponData.h
// Weapon data asset - combines attack, abilities, stance, and infusion display

#pragma once

#include "CoreMinimal.h"
#include "EquipmentDataBase.h"
#include "EWeaponType.h"
#include "EPhysicalDamageType.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "WeaponData.generated.h"

// Forward declarations
class UWeaponAttackData;
class UAbilityData;
class UStanceData;

/**
 * Weapon Data Asset
 * Defines weapon properties, attack, abilities, stance, and infusion visuals
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponData : public UEquipmentDataBase
{
    GENERATED_BODY()

public:
    // ==================== WEAPON ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType WeaponType = EWeaponType::Sword;

    /** What physical damage this weapon delivers. Drives the bar-cap
     *  trigger when no elemental infusion is active. Every weapon must
     *  declare one — None is rejected by validation. A staff = Impact,
     *  a dagger = Pierce, a sword = Slash. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::Slash;

    // Attack used when this weapon is equipped (replaces base attack)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UWeaponAttackData *WeaponAttack = nullptr;

    // Default abilities for this weapon (can be customized unless locked)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (TitleProperty = "Name"))
    TArray<UAbilityData *> PresetAbilities;

    // If true, abilities cannot be customized (used for conjured weapons)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bAbilitiesLocked = false;

    // Idle stance when this weapon is equipped
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStanceData *WeaponStance = nullptr;

    // ==================== ANIMATIONS ====================

    /** Animation when drawing this weapon */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *DrawMontage = nullptr;

    /** Animation when sheathing this weapon */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *SheatheMontage = nullptr;

    /** Weapon-specific parry animation (used when character's bUseWeaponParryAnimation is true) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *ParryMontage = nullptr;

    // ==================== MESH ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMesh *WeaponStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    USkeletalMesh *WeaponSkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    FRotator MeshRotation = FRotator::ZeroRotator;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetWeaponTypeName() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetAbilityCount() const { return PresetAbilities.Num(); }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsConjuredWeapon() const { return bAbilitiesLocked; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasAttack() const { return WeaponAttack != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasStance() const { return WeaponStance != nullptr; }

    virtual int32 GetMaxSpells() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
