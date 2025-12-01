// WeaponData.h
// Weapon data asset - combines attack, abilities, stance, and infusion display

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "EWeaponType.h"
#include "WorldStatRequirements.h"
#include "EInfusionDisplayLocation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "WeaponData.generated.h"

// Forward declarations
class UWeaponAttackData;
class UAbilityData;
class UStanceData;
class UWeaponInfusionDisplayData;
class UItemData;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

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

    // ==================== DURABILITY ====================

    /** Maximum weapon durability */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "1"))
    int32 MaxDurability = 100;

    /** Current durability (runtime - not saved on asset) */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Durability", Transient)
    int32 CurrentDurability = 100;

    /** Is the crystal disabled due to durability loss? */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Durability", Transient)
    bool bCrystalDisabled = false;

    // ==================== DURABILITY HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    float GetDurabilityPercent() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    bool IsCrystalFunctional() const;

    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void ApplyDurabilityDamage(int32 Damage);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void RepairDurability(float Percent);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void ResetDurability();

    /** Get damage weapon takes when slotted crystal breaks */
    UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
    static int32 GetCrystalBreakDamage(EItemTier CrystalTier);

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

    // ==================== INFUSION (GENERIC ONLY) ====================

    // Can Generic characters infuse this weapon with abilities?
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bCanBeInfused = true;

    // Status buildup multiplier when abilities are infused (higher = faster status)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion",
              meta = (EditCondition = "bCanBeInfused", ClampMin = "0.0", ClampMax = "2.0"))
    float InfusionStatusMultiplier = 1.0f;

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

    // ==================== DISPLAY ====================

    /** Filter for infusion display assets */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    EInfusionDisplayLocation InfusionDisplayFilter = EInfusionDisplayLocation::Weapon;

    // Infusion visual effect for this weapon
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    UWeaponInfusionDisplayData *InfusionDisplay = nullptr;

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

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasInfusionDisplay() const { return InfusionDisplay != nullptr; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    FRotator MeshRotation = FRotator::ZeroRotator;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};