// ItemData.h
// Primary data asset for items in World of Refraction
// Defines properties, effects, and mechanics for crystals
//
// CRYSTAL STATES (orthogonal flags — any combination is valid):
// - bIsRefined:           Whether crystal has been cut for slotting (false = consumable item)
// - bIsEvolutionCrystal:  Whether crystal grants evolution when slotted/applied
//
// Common combinations:
// - !refined, !evolution: Standard consumable elemental crystal (e.g. unrefined Garnet)
// -  refined, !evolution: Slottable elemental crystal (slot on weapon/ring for element)
// -  refined,  evolution: Slottable evolution crystal (grants evolution when slotted)
// - !refined,  evolution: Evolution crystal in inventory, awaiting refinement

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CrystalType.h"
#include "ItemTier.h"
#include "ItemEffectType.h"
#include "ESpellElement.h"
#include "DurabilityConstants.h"
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

/** Broadcast when a refined crystal's durability hits 0. Single-param: the broken crystal. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrystalBroken, UItemData *, BrokenCrystal);

/**
 * Primary data asset for items (crystals)
 * Each item is defined by crystal type + tier + refined/evolution flags.
 * See ItemData.h header comment for crystal state combinations.
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

        /** Whether crystal has been refined (cut) for slotting onto equipment.
         *  Unrefined crystals are consumable items; refined crystals slot into weapons/rings. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System")
        bool bIsRefined = false;

        /** Whether this is an Evolution crystal (grants evolution status when slotted).
         *  Independent of bIsRefined: Evolution crystals can be unrefined (in inventory)
         *  or refined (slotted on weapon/ring/character). */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System")
        bool bIsEvolutionCrystal = false;

        /** Evolution type (only for Evolution category crystals) */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System",
                  meta = (EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        EEvolutionType EvolutionType = EEvolutionType::Balanced;

        /** Stat modifier mode - Pillar (Mind/Body/Spirit) or SubStats (9 individual stats) */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System",
                  meta = (EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        EStatModifierMode StatModifierMode = EStatModifierMode::Pillar;

        // ==================== SUB-STAT MODIFIERS (Evolution only, SubStats mode) ====================

        /** Mind sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float CostReductionModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float TurnSpeedModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Mind",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float CritChanceModifierPercent = 0.0f;

        /** Body sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float DefenseModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float AttackSpeedModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Body",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float RawDamageModifierPercent = 0.0f;

        /** Spirit sub-stats */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float EffectDamageModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float ResistanceModifierPercent = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System|SubStats|Spirit",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal && StatModifierMode == EStatModifierMode::SubStats",
                          EditConditionHides))
        float SpellSizeModifierPercent = 0.0f;

        // ==================== DURABILITY ====================
        // Only meaningful for refined crystals. Unrefined crystals are consumables
        // and don't track durability. Evolution crystals are immune (set bImmuneToBreaking).
        // See DurabilityConstants.h for tier defaults and wear values.

        /** Maximum durability. If left at 0, auto-computed from Tier in PostInitProperties.
         *  Per-tier defaults: F=30 E=40 D=50 C=60 B=70 A=80 S=100 */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides, ClampMin = "0"))
        int32 MaxDurability = 0;

        /** Current durability. Transient runtime state — NOT saved on the asset.
         *  Initialised to MaxDurability in PostInitProperties. */
        UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Durability",
                  Transient, meta = (EditCondition = "bIsRefined", EditConditionHides))
        int32 CurrentDurability = 0;

        /** Crystal cannot break (durability is cosmetic). Auto-set true for Evolution crystals
         *  in PostInitProperties; can be manually set true for special cases. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides))
        bool bImmuneToBreaking = false;

        /** Fired when this crystal's durability reaches 0 from ApplyWear.
         *  Only fires for refined, non-immune crystals. */
        UPROPERTY(BlueprintAssignable, Category = "Durability")
        FOnCrystalBroken OnCrystalBroken;

        // ---- Durability helpers ----

        /** Returns CurrentDurability / MaxDurability. 0.0 if not refined or MaxDurability is 0. */
        UFUNCTION(BlueprintPure, Category = "Crystal|Durability")
        float GetDurabilityPercent() const;

        /** Reduce CurrentDurability by Amount, clamped at 0. Fires OnCrystalBroken if it hits 0.
         *  No-op (with verbose log) for unrefined or immune crystals. */
        UFUNCTION(BlueprintCallable, Category = "Crystal|Durability")
        void ApplyWear(int32 Amount);

        /** Restore Amount to CurrentDurability, clamped at MaxDurability.
         *  Used by combat-end auto-repair (+REPAIR_PER_BATTLE per battle). */
        UFUNCTION(BlueprintCallable, Category = "Crystal|Durability")
        void RepairBetweenCombats(int32 Amount);

        /** True if CurrentDurability <= 0 AND crystal is refined AND not immune. */
        UFUNCTION(BlueprintPure, Category = "Crystal|Durability")
        bool IsBroken() const;

        /** Reset CurrentDurability to MaxDurability. Called at combat start. */
        UFUNCTION(BlueprintCallable, Category = "Crystal|Durability")
        void ResetDurability();

        // ==================== STAT MODIFIERS (Evolution only) ====================

        /** Mind stat modifier percentage (affects Cost Reduction, Turn Speed, Crit Chance) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        float MindModifierPercent = 0.0f;

        /** Body stat modifier percentage (affects Defense, Attack Speed, Raw Damage) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        float BodyModifierPercent = 0.0f;

        /** Spirit stat modifier percentage (affects Effect Damage, Resistance, Spell Size) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (ClampMin = "-50", ClampMax = "50",
                          EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        float SpiritModifierPercent = 0.0f;

        // ==================== PASSIVE EFFECTS (Evolution only) ====================

        /** Passive effects granted by this evolution crystal */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passives|Evolution",
                  meta = (EditCondition = "bIsEvolutionCrystal", EditConditionHides))
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
        bool GrantsEvolution() const { return bIsEvolutionCrystal; }

        /** Check if crystal is a refined item crystal (non-evolution slottable) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool IsRefinedItemCrystal() const { return !bIsEvolutionCrystal && bIsRefined; }

        /** Check if crystal is a refined evolution crystal (slottable, grants evolution) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool IsRefinedEvolutionCrystal() const { return bIsEvolutionCrystal && bIsRefined; }

        /** Check if item effects are active (unrefined Item category only) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool HasItemEffects() const { return !bIsEvolutionCrystal && !bIsRefined; }

        /** Check if can be applied to character directly (unrefined Evolution only) */
        UFUNCTION(BlueprintPure, Category = "Item|Category")
        bool CanApplyToCharacter() const { return bIsEvolutionCrystal && !bIsRefined; }

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

        // ==================== LIFECYCLE ====================

        virtual void PostInitProperties() override;

        /** Migration hook: initialise durability from tier for assets saved before Phase 2a.
         *  Old assets loaded with MaxDurability == 0 get auto-fixed here. */
        virtual void PostLoad() override;
#endif
};
