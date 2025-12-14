// ItemData.h
// Primary data asset for items in World of Refraction
// Defines properties, effects, and mechanics for crystals
//
// CRYSTAL CATEGORIES:
// - Item: Consumable in combat, item effects active
// - Refined: Slottable on weapons/rings, 6 customizable spell slots, no item effects
// - Evolution: Slottable on weapons/rings/characters, locked+custom spells, stat modifiers, no item effects

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "ItemEffectType.h"
#include "SpellElement.h"
#include "ECrystalCategory.h"
#include "PassiveEffect.h"
#include "NiagaraSystem.h"
#include "EEvolutionType.h"
#include "EStatModifierMode.h"
#include "ItemData.generated.h"

class USpellData;

// Spell slot constants
namespace CrystalSpellConstants
{
        constexpr int32 MAX_SPELL_SLOTS = 6;
        constexpr int32 DEFAULT_LOCKED_SPELLS = 2;
}

/**
 * Primary data asset for items (crystals)
 * Each item is defined by crystal type + tier + category combination
 * Category determines usage: Item (consumable), Refined (slottable), Evolution (grants evolution)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UItemData : public UPrimaryDataAsset
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

        /** Crystal category - determines usage (replaces bIsRefined) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
        ECrystalCategory Category = ECrystalCategory::Item;

        /** Whether crystal has been refined (cut) for slotting onto equipment */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System")
        bool bIsRefined = false;

        /** Evolution type (only for Evolution category crystals) */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System",
                  meta = (EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        EEvolutionType EvolutionType = EEvolutionType::Balanced;

        /** Stat modifier mode - Pillar (Mind/Body/Spirit) or SubStats (9 individual stats) */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System",
                  meta = (EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        EStatModifierMode StatModifierMode = EStatModifierMode::Pillar;

        // ==================== SUB-STAT MODIFIERS (Evolution only, SubStats mode) ====================

        /** Mind sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float CostReductionModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float TurnSpeedModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float CritChanceModifierPercent = 0.0f;

        /** Body sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float DefenseModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float AttackSpeedModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float RawDamageModifierPercent = 0.0f;

        /** Spirit sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float EffectDamageModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float ResistanceModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float SpellSizeModifierPercent = 0.0f;

        // ==================== SPELLS (Refined/Evolution only) ====================

        /** Spells available on this crystal (max 6) - only for Refined/Evolution */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells",
                  meta = (EditCondition = "bIsRefined || Category == ECrystalCategory::Evolution", EditConditionHides))
        TArray<USpellData *> Spells;

        /** Number of locked spells (0-6) - first N spells are unchangeable (Evolution only) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells",
                  meta = (ClampMin = "0", ClampMax = "6",
                          EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        int32 LockedSpellCount = CrystalSpellConstants::DEFAULT_LOCKED_SPELLS;

        // ==================== STAT MODIFIERS (Evolution only) ====================

        /** Mind stat modifier percentage (affects Cost Reduction, Turn Speed, Crit Chance) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        float MindModifierPercent = 0.0f;

        /** Body stat modifier percentage (affects Defense, Attack Speed, Raw Damage) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        float BodyModifierPercent = 0.0f;

        /** Spirit stat modifier percentage (affects Effect Damage, Resistance, Spell Size) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        float SpiritModifierPercent = 0.0f;

        // ==================== PASSIVE EFFECTS (Evolution only) ====================

        /** Passive effects granted by this evolution crystal */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passives|Evolution",
                  meta = (EditCondition = "Category == ECrystalCategory::Evolution", EditConditionHides))
        TArray<FPassiveEffect> PassiveEffects;

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

        // ==================== VISUAL/AUDIO ====================

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
        UTexture2D *Icon;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
        FLinearColor TierColor;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
        UNiagaraSystem *UseEffect;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
        USoundBase *UseSound;

        // ==================== CATEGORY HELPER FUNCTIONS ====================

        /** Check if crystal can be slotted on weapons/rings (must be refined) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool CanBeSlotted() const { return bIsRefined; }

        /** Check if crystal can have spells (must be refined) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool CanHaveSpells() const { return bIsRefined; }

        /** Check if crystal has been refined (cut for slotting) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool IsRefined() const { return bIsRefined; }

        /** Check if crystal grants evolution status */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool GrantsEvolution() const { return Category == ECrystalCategory::Evolution; }

        /** Check if crystal is a refined item crystal (non-evolution slottable) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool IsRefinedItemCrystal() const { return Category == ECrystalCategory::Item && bIsRefined; }

        /** Check if crystal is a refined evolution crystal (slottable, grants evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool IsRefinedEvolutionCrystal() const { return Category == ECrystalCategory::Evolution && bIsRefined; }

        /** Check if item effects are active (unrefined Item category only) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool HasItemEffects() const { return Category == ECrystalCategory::Item && !bIsRefined; }

        /** Check if can be applied to character directly (unrefined Evolution only) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool CanApplyToCharacter() const { return Category == ECrystalCategory::Evolution && !bIsRefined; }
        // ==================== SPELL HELPER FUNCTIONS ====================

        /** Get all spells on this crystal */
        UFUNCTION(BlueprintPure, Category = "Item|Spells")
        const TArray<USpellData *> &GetSpells() const { return Spells; }

        /** Get locked spell count (0 for non-Evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Spells")
        int32 GetLockedSpellCount() const;

        /** Get customizable spell slot count */
        UFUNCTION(BlueprintPure, Category = "Item|Spells")
        int32 GetCustomSpellSlots() const;

        /** Get total spell capacity */
        UFUNCTION(BlueprintPure, Category = "Item|Spells")
        int32 GetTotalSpellSlots() const;

        // ==================== STAT MODIFIER FUNCTIONS ====================

        /** Check if crystal has any stat modifiers */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        bool HasStatModifiers() const;

        /** Get stat modifier summary string */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        FString GetStatModifierSummary() const;

        /** Get Mind modifier (returns 0 if not Evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetMindModifierPercent() const;

        /** Get Body modifier (returns 0 if not Evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetBodyModifierPercent() const;

        /** Get Spirit modifier (returns 0 if not Evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetSpiritModifierPercent() const;

        // ==================== PASSIVE HELPER FUNCTIONS ====================

        /** Get passive effect count */
        UFUNCTION(BlueprintPure, Category = "Item|Passives")
        int32 GetPassiveCount() const { return PassiveEffects.Num(); }

        /** Get always-active passives */
        UFUNCTION(BlueprintPure, Category = "Item|Passives")
        TArray<FPassiveEffect> GetAlwaysActivePassives() const;

        /** Get triggered passives (not always-active) */
        UFUNCTION(BlueprintPure, Category = "Item|Passives")
        TArray<FPassiveEffect> GetTriggeredPassives() const;

        // ==================== EVOLUTION HELPER FUNCTIONS ====================

        /** Get evolution type name for display */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution")
        FString GetEvolutionTypeName() const;

        /** Get stat modifier summary string */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution")
        FString GetEvolutionStatSummary() const;

        // ==================== STAT CALCULATION (Evolution only) ====================

        /** Calculate modified Mind stat */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|Stats")
        float CalculateModifiedMind(float BaseMind) const;

        /** Calculate modified Body stat */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|Stats")
        float CalculateModifiedBody(float BaseBody) const;

        /** Calculate modified Spirit stat */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|Stats")
        float CalculateModifiedSpirit(float BaseSpirit) const;

        // ==================== SUB-STAT GETTERS (Evolution only) ====================

        /** Get cost reduction modifier (uses Mind in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetCostReductionModifier() const;

        /** Get turn speed modifier (uses Mind in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetTurnSpeedModifier() const;

        /** Get crit chance modifier (uses Mind in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetCritChanceModifier() const;

        /** Get defense modifier (uses Body in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetDefenseModifier() const;

        /** Get attack speed modifier (uses Body in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetAttackSpeedModifier() const;

        /** Get raw damage modifier (uses Body in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetRawDamageModifier() const;

        /** Get effect damage modifier (uses Spirit in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetEffectDamageModifier() const;

        /** Get resistance modifier (uses Spirit in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetResistanceModifier() const;

        /** Get Spell Size modifier (uses Spirit in Pillar mode) */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|SubStats")
        float GetSpellSizeModifier() const;

        // ==================== EXISTING UTILITY FUNCTIONS ====================

        UFUNCTION(BlueprintPure, Category = "Item")
        FString GetFullItemName() const;

        UFUNCTION(BlueprintPure, Category = "Item")
        FString GetTierName() const;

        UFUNCTION(BlueprintPure, Category = "Item")
        FString GetTierString() const;

        UFUNCTION(BlueprintPure, Category = "Item")
        FString GetCrystalName() const;

        UFUNCTION(BlueprintPure, Category = "Item")
        int32 GetTierValue() const;

        UFUNCTION(BlueprintPure, Category = "Item")
        ESpellElement GetAssociatedElement() const;

        UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
        float GetGenericResistanceBonus() const;

        UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
        int32 GetGenericResistanceDuration() const;

        UFUNCTION(BlueprintPure, Category = "Item|Bonuses")
        int32 GetBrokenDarknessEnergyBonus() const;

        // ==================== COMPUTED EFFECT VALUES ====================

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        EItemEffectType GetPrimaryEffectType() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetDamageValue() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetEnergyValue() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetSelfDamage() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetBuffPercentage() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetBuffDuration() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetSilencePercentage() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetSilenceDuration() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetDebuffsToRemove() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool GetGrantsImmunity() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetImmunityDuration() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool GetRevealsHP() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool GetRevealsStats() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetTransformThreshold() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool HasSecondaryEffect() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetSecondaryDamagePerTurn() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetSecondaryDuration() const;

        // ==================== EDITOR SUPPORT ====================

#if WITH_EDITOR
        FString GenerateDescription() const;
        FString GenerateEvolutionDescription() const; // ADD THIS
        virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
        virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
