// EvolutionItemData.h
// Primary data asset for crystals in World of Refraction. Renamed from
// ItemData.h in commit 3 of the crystal/evolution refactor sequence; class
// will be narrowed to evolution-only in later commits as the refined and
// item-crystal asset paths are deleted.
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
#include "FSkillEffect.h"
#include "NiagaraSystem.h"
#include "EEvolutionType.h"
#include "ActionStatModifiers.h"
#include "FEquipmentStatBonus.h"
#include "TargetType.h"
#include "EvolutionItemData.generated.h"

class USpellData;

// Spell slot constants
namespace CrystalSpellConstants
{
        constexpr int32 MAX_SPELL_SLOTS = 6;
        constexpr int32 DEFAULT_LOCKED_SPELLS = 2;
}

/**
 * Primary data asset for items (crystals)
 * Each item is defined by crystal type + tier + refined/evolution flags.
 * See ItemData.h header comment for crystal state combinations.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEvolutionItemData : public UPrimaryDataAsset
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

        // ==================== DURABILITY ====================
        // Only meaningful for refined crystals. Unrefined crystals are consumables
        // and don't track durability. Evolution crystals are immune (set bImmuneToBreaking).
        // See DurabilityConstants.h for tier defaults and wear values.

        /** Maximum durability. If left at 0, auto-computed from Tier in PostInitProperties.
         *  Per-tier defaults: F=30 E=40 D=50 C=60 B=70 A=80 S=100 */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides, ClampMin = "0"))
        int32 MaxDurability = 0;

        /** Crystal cannot break (durability is cosmetic). Auto-set true for Evolution crystals
         *  in PostInitProperties; can be manually set true for special cases. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides))
        bool bImmuneToBreaking = false;

        // ==================== STAT BONUS (Evolution only) ====================
        // Authoring surface for evolution crystal stat modifiers. Replaces the
        // legacy MindModifierPercent / BodyModifierPercent / SpiritModifierPercent
        // pillar percent fields and the entire SubStats authoring mode.
        //
        // READ PATHS:
        //  - Pillar percent (BonusMindModifierPercent etc.): consumed directly by
        //    UCharacterDataComponent::ApplyCrystalPillarModifier — NOT aggregated
        //    by ULoadoutComponent::GetActiveStatBonus.
        //  - Int substat fields (BonusRawDamage etc.): consumed by
        //    UEvolutionItemData::GetInfusionStatModifiers when the crystal is infusing,
        //    scaled by the infusion magnitude (L1=0.5, L2=1.0).
        //
        // CLAMP NOTE: The struct's int fields have UPROPERTY meta ClampMin=0
        // (designed for weapons/rings). Crystals support negative values down
        // to CombatConstants::CRYSTAL_BONUS_MIN; the UI clamp can't be
        // overridden at the embedding site, so out-of-range values are caught
        // by UEvolutionItemData::IsDataValid warnings rather than UI enforcement.
        // BaseStatBonus (Category = "Stats|Evolution") is referred to as "Traits"
        // in design docs for evolution crystals — the in-editor category cannot
        // vary per subclass, so the header label stays generic.
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution",
                  meta = (EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        FEquipmentStatBonus BaseStatBonus;

        // ==================== LEGACY PILLAR FIELDS (Deprecated) ====================
        // Kept for one transition release to support PostLoad migration of
        // existing crystal assets. Values are copied into BaseStatBonus on load.
        // Scheduled for removal in a follow-up commit after content team
        // confirms all assets have been re-saved.

        UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BaseStatBonus.BonusMindModifierPercent instead — PostLoad migrates legacy values."))
        float MindModifierPercent = 0.0f;

        UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BaseStatBonus.BonusBodyModifierPercent instead — PostLoad migrates legacy values."))
        float BodyModifierPercent = 0.0f;

        UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BaseStatBonus.BonusSpiritModifierPercent instead — PostLoad migrates legacy values."))
        float SpiritModifierPercent = 0.0f;

        // ==================== EFFECTS (Evolution only) ====================

        /** Skill effects granted by this evolution crystal (passives + triggered) */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Evolution",
                  meta = (EditCondition = "bIsEvolutionCrystal", EditConditionHides))
        TArray<FSkillEffect> Effects;

        // ==================== COMPUTED VALUES (DISPLAY ONLY) ====================
        // These are computed and displayed for reference - not editable

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Identity")
        ESpellElement DisplayElement;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
        EItemEffectType DisplayEffectType;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
        float DisplayBuffPercentage;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
        float DisplaySilencePercentage;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
        bool DisplayRevealsHP;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Computed Values|Effect")
        bool DisplayRevealsStats;

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

        // ==================== EFFECT HELPER FUNCTIONS ====================

        /** Get effect count */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetEffectCount() const { return Effects.Num(); }

        /** Get always-active effects (Condition == Always, no target condition) */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        TArray<FSkillEffect> GetAlwaysActiveEffects() const;

        /** Get triggered effects (anything not always-active) */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        TArray<FSkillEffect> GetTriggeredEffects() const;

        // ==================== EVOLUTION HELPER FUNCTIONS ====================

        /** Get evolution type name for display */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution")
        FString GetEvolutionTypeName() const;

        /** Get stat modifier summary string */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution")
        FString GetEvolutionStatSummary() const;

        // ==================== STAT CALCULATION (Evolution only) ====================

        /** Compute per-action stat modifiers when this Evolution crystal is INFUSING.
         *  Maps BaseStatBonus int fields 1:1 onto FActionStatModifiers, scaled by
         *  InfusionMultiplier (L1 = 0.5f, L2 = 1.0f).
         *
         *  DOES NOT apply:
         *   - Pillar percent fields (BonusMind/Body/SpiritModifierPercent) — these
         *     flow through UCharacterDataComponent::ApplyCrystalPillarModifier as
         *     character-persistent modifiers, not per-action.
         *   - Effects — character-only via a separate system.
         *
         *  Returns a zeroed struct when bIsEvolutionCrystal is false. */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|Stats")
        FActionStatModifiers GetInfusionStatModifiers(float InfusionMultiplier) const;

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
        int32 GetBrokenDarknessEnergyBonus() const;

        /** Target type for combat targeting UI — all crystals target anyone. */
        UFUNCTION(BlueprintPure, Category = "Item|Targeting")
        ETargetType GetItemTargetType() const;

        // ==================== COMPUTED EFFECT VALUES ====================

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        EItemEffectType GetPrimaryEffectType() const;

        /** Garnet DOT — burn damage per turn as a percent of target MaxHP */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetDOTDamagePercent() const;

        /** Garnet DOT — burn duration in turns */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetDOTDuration() const;

        /** Sapphire heal — restored HP as a percent of target MaxHP */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetHealPercent() const;

        // ==================== PHASE 2 REDESIGN GETTERS ====================

        /** Citrine — EP restored as a percent of target MaxEP */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetEPRestorePercent() const;

        /** Emerald — turn-speed buff percent */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetSpeedBuffPercent() const;

        /** Phase 2 shared buff duration (Emerald/Amber/Opal) in turns */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetCrystalDuration() const;

        /** Opal — crit-chance buff/debuff percent */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetCritBuffPercent() const;

        /** Amethyst — chance to roll a buff (vs debuff), percent */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetBuffChancePercent() const;

        /** Amethyst — gamble effect magnitude percent */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetGambleMagnitudePercent() const;

        /** Amethyst — gamble effect duration in turns */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetGambleDuration() const;

        /** Iolite — number of effects to remove (99 = remove all) */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetEffectsToRemoveCount() const;

        /** Quartz — fraction of the status bar cleared, percent */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetStatusClearPercent() const;

        /** Quartz — elemental resistance duration in turns */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetResistanceDuration() const;

        /** Shared status-bar buildup percent for status-building crystals
         *  (Garnet/Citrine/Onyx/Amethyst): 10/15/20/30/40/50/60 per tier. */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetElementalBuildupPercent() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetBuffPercentage() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        float GetSilencePercentage() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool GetRevealsHP() const;

        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        bool GetRevealsStats() const;

        // ==================== EDITOR SUPPORT ====================

#if WITH_EDITOR
        FString GenerateDescription() const;
        FString GenerateEvolutionDescription() const; // ADD THIS
        virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
        virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
        virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;

        // ==================== LIFECYCLE ====================

        virtual void PostInitProperties() override;

        /** Migration hook: initialise durability from tier for assets saved before Phase 2a.
         *  Old assets loaded with MaxDurability == 0 get auto-fixed here. */
        virtual void PostLoad() override;
#endif
};
