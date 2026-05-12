// WeaponData.h
// Weapon data asset - combines attack, abilities, stance, and infusion display

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "EWeaponType.h"
#include "EPhysicalDamageType.h"
#include "WorldStatRequirements.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "ItemTier.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "WeaponData.generated.h"

// Forward declarations
class UWeaponAttackData;
class UAbilityData;
class UStanceData;
class UItemData;
class USpellData;

/**
 * Weapon Data Asset
 * Defines weapon properties, attack, abilities, stance, and infusion visuals
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString WeaponName = TEXT("Unnamed Weapon");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EWeaponType WeaponType = EWeaponType::Sword;

    /** What physical damage this weapon delivers. Drives the bar-cap
     *  trigger when no elemental infusion is active. Every weapon must
     *  declare one — None is rejected by validation. A staff = Impact,
     *  a dagger = Pierce, a sword = Slash. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::Slash;

    /** Tier of the weapon. Used as the action tier for abilities/attacks performed
     *  with this weapon — feeds BreakCalculator when a slotted crystal is used as
     *  the infusion source (Phase 4d). All abilities and attacks on this weapon
     *  inherit this tier; per-ability tier overrides are not supported. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== COMBAT ====================

    // Attack used when this weapon is equipped (replaces base attack)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    UWeaponAttackData *WeaponAttack = nullptr;

    // Default abilities for this weapon (can be customized unless locked)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (TitleProperty = "AbilityName"))
    TArray<UAbilityData *> PresetAbilities;

    // If true, abilities cannot be customized (used for conjured weapons)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bAbilitiesLocked = false;

    // ==================== TIER & CRYSTAL ====================

    /** Refined crystal slotted into weapon - determines element */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
    UItemData *SlottedCrystal = nullptr;

    // ==================== CRYSTAL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasCrystal() const { return SlottedCrystal != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsEvolved() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    ESpellElement GetWeaponElement() const;

    // Default spells for this weapon - copied to inventory entry when weapon obtained
    // Lost if crystal is removed; spell vendor reassigns them
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (TitleProperty = "SpellName"))
    TArray<USpellData *> DefaultSpells;

    // ==================== WORLD STAT REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FWorldStatRequirements Requirements;

    // ==================== STAT BONUSES (APPLIED WHILE EQUIPPED) ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusAttack = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusDefense = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusMagicPower = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    float BonusCritChance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses", meta = (ClampMin = "0.0"))
    float BonusCritDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusMaxHP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Bonuses")
    int32 BonusMaxMP = 0;

    // ==================== ANIMATION ====================

    // Idle stance when this weapon is equipped
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UStanceData *WeaponStance = nullptr;

    // ==================== EQUIP ANIMATIONS ====================

    /** Animation when drawing this weapon */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *DrawMontage = nullptr;

    /** Animation when sheathing this weapon */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *SheatheMontage = nullptr;

    /** Optional parry animation override (used if character has bUseWeaponParryAnimation) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
    UAnimMontage *ParryMontageOverride = nullptr;

    // ==================== DEFENSE ====================

    /** Weapon-specific parry animation (used when character's bUseWeaponParryAnimation is true) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
    UAnimMontage *ParryMontage = nullptr;

    // ==================== INFUSION (GENERIC ONLY) ====================

    // Status buildup multiplier when abilities are infused (higher = faster status)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion",
              meta = (EditCondition = "bCanBeInfused", ClampMin = "0.0", ClampMax = "2.0"))
    float InfusionStatusMultiplier = 1.0f;

    // ==================== MESH ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMesh *WeaponStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    USkeletalMesh *WeaponSkeletalMesh = nullptr;

    // ==================== PRESENTATION ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon = nullptr;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool MeetsRequirements(const UCharacterData *Character) const
    {
        return Requirements.MeetsRequirements(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetRequirementsSummary(const UCharacterData *Character) const
    {
        return Requirements.GetRequirementsSummary(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetDisplayName() const { return WeaponName; }

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    FRotator MeshRotation = FRotator::ZeroRotator;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};