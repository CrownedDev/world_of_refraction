// ItemData.h
// Primary data asset for items in World of Refraction
// Defines properties, effects, and mechanics for consumable crystals

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "ItemEffectType.h"
#include "SpellElement.h"
#include "ItemData.generated.h"
class UEvolutionData;
/**
 * Primary data asset for items (crystals)
 * Each item is defined by crystal type + tier combination
 * Contains all effect values and mechanics for that specific item
 */
UCLASS(BlueprintType)
class UItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ECrystalType CrystalType = ECrystalType::Garnet;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::F_Tier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description;

    /** Whether crystal is refined (for slotting into weapons/rings) or raw (consumable) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    bool bIsRefined = false;

    // ==================== EVOLUTION (EvolutionCrystal only) ====================

    /** Evolution data - only visible for EvolutionCrystal type */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
              meta = (EditCondition = "CrystalType == ECrystalType::EvolutionCrystal", EditConditionHides))
    UEvolutionData *Evolution = nullptr;

    // ==================== COMPUTED VALUES (DISPLAY ONLY) ====================
    // These are computed and displayed for reference - not editable

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Identity")
    ESpellElement DisplayElement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    EItemEffectType DisplayEffectType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    float DisplayDamageValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplayEnergyValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplaySelfDamage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    float DisplayBuffPercentage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplayBuffDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    float DisplaySilencePercentage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplaySilenceDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplayDebuffsToRemove;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    bool DisplayGrantsImmunity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplayImmunityDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    bool DisplayRevealsHP;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    bool DisplayRevealsStats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
    int32 DisplayTransformThreshold;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Secondary")
    bool DisplayHasSecondary;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Secondary")
    int32 DisplaySecondaryDamage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Secondary")
    int32 DisplaySecondaryDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Bonuses")
    float DisplayGenericResistance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Bonuses")
    int32 DisplayGenericDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Bonuses")
    int32 DisplayBDEnergy;

    // ==================== COMPUTED EFFECT VALUES ====================
    // All effect values are computed based on Crystal Type + Tier
    // See GetDamageValue(), GetEnergyValue(), etc.

    // ==================== VISUAL/AUDIO ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D *Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FLinearColor TierColor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UParticleSystem *UseEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    USoundBase *UseSound;

    // ==================== UTILITY FUNCTIONS ====================

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetFullItemName() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetTierName() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetTierString() const; // Returns "F", "S", etc.

    UFUNCTION(BlueprintPure, Category = "Item")
    FString GetCrystalName() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetTierValue() const;

    // Get associated element based on crystal type
    UFUNCTION(BlueprintPure, Category = "Item")
    ESpellElement GetAssociatedElement() const;

    // Get Generic character resistance bonus (constant)
    UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
    float GetGenericResistanceBonus() const;

    UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
    int32 GetGenericResistanceDuration() const;

    // Get Broken Darkness energy bonus based on tier
    UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
    int32 GetBrokenDarknessEnergyBonus() const;

    // ==================== COMPUTED EFFECT VALUES ====================

    // Get primary effect type (derived from crystal type)
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    EItemEffectType GetPrimaryEffectType() const;

    // Damage/Healing values
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    float GetDamageValue() const;

    // Energy restore values
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetEnergyValue() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetSelfDamage() const;

    // Buff values
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    float GetBuffPercentage() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetBuffDuration() const;

    // Silence values
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    float GetSilencePercentage() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetSilenceDuration() const;

    // Cleanse properties
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetDebuffsToRemove() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    bool GetGrantsImmunity() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetImmunityDuration() const;

    // Opal reveal flags
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    bool GetRevealsHP() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    bool GetRevealsStats() const;

    // Quartz transform threshold
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetTransformThreshold() const;

    // Secondary effects (S-tier only)
    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    bool HasSecondaryEffect() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetSecondaryDamagePerTurn() const;

    UFUNCTION(BlueprintPure, Category = "Item|Effects")
    int32 GetSecondaryDuration() const;

#if WITH_EDITOR
    // Editor-only: Auto-configure properties when changed
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;

    // Generate description based on crystal type and tier
    FString GenerateDescription() const;

    // Validate item configuration
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};