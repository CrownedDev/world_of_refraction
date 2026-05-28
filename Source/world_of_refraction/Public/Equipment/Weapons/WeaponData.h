// WeaponData.h
// Weapon data asset - combines attack, abilities, stance, and infusion display

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Equipment/Weapons/EWeaponWieldMode.h"
#include "Combat/Damage/EPhysicalDamageType.h"
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
    // Inherited BaseStatBonus / GeneratedStatBonus (Category = "Bonuses") are
    // referred to as "Mastery" in design docs for weapons — the in-editor
    // category cannot vary per subclass, so the header label stays generic.

    // ==================== WEAPON ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType WeaponType = EWeaponType::Sword;

    /** Drives mesh-spawn behaviour AND ability dual-gating. See EWeaponWieldMode. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponWieldMode WieldMode = EWeaponWieldMode::Single;

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

    /** Weapon-specific parry animation (used when the active weapon's
     *  FWeaponLoadoutEntry::bUseWeaponParryAnimation is true) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *ParryMontage = nullptr;

    // ==================== MESH ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMesh *WeaponStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    USkeletalMesh *WeaponSkeletalMesh = nullptr;

    /** Rotation applied to the right-hand mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    FRotator MeshRotation = FRotator::ZeroRotator;

    /** Distinct left-hand static mesh for any non-Single WieldMode (Dual / OffHandShield).
     *  Must be assigned explicitly — there is no reuse of the right-hand mesh.
     *  If both LeftHand* fields are null, no left-hand mesh spawns. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh",
              meta = (EditCondition = "WieldMode != EWeaponWieldMode::Single", EditConditionHides))
    UStaticMesh *LeftHandStaticMesh = nullptr;

    /** Distinct left-hand skeletal mesh for any non-Single WieldMode (Dual / OffHandShield).
     *  Must be assigned explicitly — there is no reuse of the right-hand mesh.
     *  If both LeftHand* fields are null, no left-hand mesh spawns. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh",
              meta = (EditCondition = "WieldMode != EWeaponWieldMode::Single", EditConditionHides))
    USkeletalMesh *LeftHandSkeletalMesh = nullptr;

    /** Rotation applied to the left-hand mesh in Dual / OffHandShield modes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh",
              meta = (EditCondition = "WieldMode != EWeaponWieldMode::Single", EditConditionHides))
    FRotator LeftMeshRotation = FRotator::ZeroRotator;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetWeaponTypeName() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetAbilityCount() const { return PresetAbilities.Num(); }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsConjuredWeapon() const { return bAbilitiesLocked; }

    /** True if this weapon spawns a left-hand mesh — i.e. WieldMode is Dual or OffHandShield. */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsDualWielded() const { return WieldMode != EWeaponWieldMode::Single; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasAttack() const { return WeaponAttack != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasStance() const { return WeaponStance != nullptr; }

    virtual int32 GetMaxSpells() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
