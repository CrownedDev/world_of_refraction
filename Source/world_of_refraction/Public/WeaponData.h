// WeaponData.h
// Weapon data asset - combines attack, abilities, stance, and infusion display

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "EWeaponType.h"
#include "ItemTier.h"
#include "WorldStatRequirements.h"

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

    /** Weapon tier - affects break chance calculations */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
    EItemTier Tier = EItemTier::E_Tier;

    /** Refined crystal slotted into weapon - determines element */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
    UItemData *SlottedCrystal = nullptr;

    // ==================== TIER & CRYSTAL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetTierString() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasCrystal() const { return SlottedCrystal != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsEvolved() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    ESpellElement GetWeaponElement() const;

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

    // Infusion visual effect for this weapon
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
    UWeaponInfusionDisplayData *InfusionDisplay = nullptr;

    // ==================== MESH ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
    USkeletalMesh *WeaponMesh = nullptr;

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
#if WITH_EDITOR
    EDataValidationResult UWeaponData::IsDataValid(FDataValidationContext& Context) const
    {
        EDataValidationResult Result = Super::IsDataValid(Context);

        // Name validation
        if (WeaponName.IsEmpty() || WeaponName == TEXT("Unnamed Weapon"))
        {
            Context.AddError(FText::FromString(TEXT("Weapon must have a unique name")));
            Result = EDataValidationResult::Invalid;
        }

        // Attack validation
        if (WeaponAttack == nullptr)
        {
            Context.AddWarning(FText::FromString(TEXT("No weapon attack assigned")));
        }

        // Abilities validation
        if (PresetAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Weapon cannot have more than %d abilities"),
                LoadoutConstants::MAX_WEAPON_ABILITIES)));
            Result = EDataValidationResult::Invalid;
        }

        // Stance validation
        if (WeaponStance == nullptr)
        {
            Context.AddWarning(FText::FromString(TEXT("No weapon stance assigned")));
        }

        // Crystal validation
        if (SlottedCrystal)
        {
            if (!SlottedCrystal->bIsRefined)
            {
                Context.AddWarning(FText::FromString(TEXT("Slotted crystal is not refined")));
            }
        }

        return Result;
    }
#endif
};