// EvolutionItemData.h
// Primary data asset for evolution crystals in World of Refraction. Item and
// refined crystals live in the FCrystalId / CrystalEffectTable count-based
// system (no asset); only evolution crystals are asset-backed.
//
// REFINEMENT STATE:
// - !bIsRefined: Evolution crystal in inventory, awaiting refinement
// -  bIsRefined: Slottable evolution crystal (slotted onto weapon/ring)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Skills/Effects/FSkillEffect.h"
#include "NiagaraSystem.h"
#include "Equipment/Crystals/EEvolutionType.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Combat/TargetType.h"
#include "EvolutionItemData.generated.h"

class USpellData;

// Spell slot constants
namespace CrystalSpellConstants
{
        constexpr int32 MAX_SPELL_SLOTS = 6;
        constexpr int32 DEFAULT_LOCKED_SPELLS = 2;
}

/**
 * Primary data asset for evolution crystals. Identity is (CrystalType, Tier);
 * bIsRefined distinguishes a slottable evolution crystal from an unrefined one
 * in inventory. See the file header for the refinement-state combinations.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEvolutionItemData : public UPrimaryDataAsset
{
        GENERATED_BODY()

public:
        // ==================== IDENTITY ====================

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
        ECrystalType CrystalType = ECrystalType::Garnet;

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
        ESpellElement DisplayElement;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
        EItemTier Tier = EItemTier::F_Tier;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
        FString ItemName;

        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
        FString Description;

        /** Authored description shown when the crystal is revealed (slotting / Mind).
         *  Distinct from auto-generated Description. Reveal logic not yet implemented. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
                  meta = (MultiLine = true))
        FString RevealedDescription;

        /** Whether crystal has been refined (cut) for slotting onto equipment.
         *  Unrefined crystals are consumable items; refined crystals slot into weapons/rings. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System")
        bool bIsRefined = false;

        /** Evolution type */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal System")
        EEvolutionType EvolutionType = EEvolutionType::Balanced;

        // ==================== DURABILITY ====================
        // Only meaningful for refined crystals. Unrefined crystals are consumables
        // and don't track durability. Breaking is opt-in via bCanBreak — default
        // false means the crystal never wears down (durability shown is cosmetic).
        // See DurabilityConstants.h for tier defaults and wear values.

        /** Maximum durability. If left at 0, auto-computed from Tier in PostInitProperties.
         *  Per-tier defaults: F=30 E=40 D=50 C=60 B=70 A=80 S=100 */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides, ClampMin = "0"))
        int32 MaxDurability = 0;

        /** When true, this crystal can wear down and break. Default false means the
         *  crystal is permanent (durability shown is cosmetic). Designers opt-in per
         *  item — evolution crystals are unbreakable unless explicitly set true. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability",
                  meta = (EditCondition = "bIsRefined", EditConditionHides))
        bool bCanBreak = false;

        // ==================== STAT BONUS (Evolution only) ====================
        // Authoring surface for evolution crystal stat modifiers. Replaces the
        // legacy MindModifierPercent / BodyModifierPercent / SpiritModifierPercent
        // pillar percent fields and the entire SubStats authoring mode.
        //
        // READ PATHS:
        //  - Pillar percent (BonusMindModifierPercent etc.): consumed directly by
        //    UCharacterDataComponent::ApplyEvolutionPillarModifier — NOT aggregated
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
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution")
        FEquipmentStatBonus BaseStatBonus;

        // ==================== RESISTANCE (Evolution only) ====================
        // Per-category status-buildup resistance granted while this evolution is
        // slotted. PERMANENT equipped bonus (always-on whenever slotted) — read by
        // ULoadoutComponent::GetActiveResistanceBonus as asset BaseResistance +
        // the slotted FEvolutionAttachment's per-instance GeneratedResistance
        // (U3c). Deliberately distinct from the infusion-scaled BaseStatBonus
        // above (evolution's offensive stats stay on their own conditional path).
        // The asset's GeneratedResistance below is designer PREVIEW only.

        /** Designer-authored baseline resistance. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Evolution")
        FResistanceBonus BaseResistance;

        /** DESIGNER PREVIEW (simulated sample) — written by the Roll Resistance button
         *  so designers can eyeball a roll. From U3 on, the REAL roll is per-instance
         *  (on FEvolutionInventoryEntry/FEvolutionAttachment) at acquisition; this asset
         *  layer is NOT the gameplay source for rolled instances. */
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview Roll",
                  meta = (DisplayName = "Resistance"))
        FResistanceBonus GeneratedResistance;

        /** Roll a fresh GeneratedResistance from this crystal's Tier budget (own
         *  zero-sum pool). Standalone CallInEditor button — UEvolutionItemData does
         *  NOT implement IEquipmentGenerator (it has no stat roll), so this avoids
         *  the full interface burden while giving the same one-click roll.
         *  PREVIEW only (writes the preview layer above), per U0's reframe. */
        UFUNCTION(CallInEditor, Category = "Preview Roll")
        void RollResistance();

        // ==================== GENERATION (per-instance roll) ====================
        // Controls the per-instance roll at acquisition (U3). Inert in U0.

        /** When true, a fresh owned instance ROLLS its Generated stat/resistance at
         *  acquisition (AddInstance). When false, the instance ships authored Base
         *  only. Default false = migration-safe; designers opt in. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
        bool bRandomGenerateOnPickup = false;

        /** Per-asset MaxPool override for the stat roll. 0 = use the tier budget. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0"))
        int32 StatMaxPoolOverride = 0;

        /** Per-asset MaxPool override for the resistance roll. 0 = use the tier budget. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0"))
        int32 ResistanceMaxPoolOverride = 0;

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
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Evolution")
        TArray<FSkillEffect> Effects;

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

        // ==================== STAT MODIFIER FUNCTIONS ====================

        /** Check if crystal has any stat modifiers */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        bool HasStatModifiers() const;

        /** Get stat modifier summary string */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        FString GetStatModifierSummary() const;

        /** Get Mind modifier */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetMindModifierPercent() const;

        /** Get Body modifier */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetBodyModifierPercent() const;

        /** Get Spirit modifier */
        UFUNCTION(BlueprintPure, Category = "Item|Stats")
        float GetSpiritModifierPercent() const;

        // ==================== EFFECT HELPER FUNCTIONS ====================

        /** Get effect count */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        int32 GetEffectCount() const { return Effects.Num(); }

        /** Get starting effects (no condition — applied once at combat start) */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        TArray<FSkillEffect> GetStartingEffects() const;

        /** Get conditional effects (condition-gated — never auto-applied at start) */
        UFUNCTION(BlueprintPure, Category = "Item|Effects")
        TArray<FSkillEffect> GetConditionalEffects() const;

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
         *     flow through UCharacterDataComponent::ApplyEvolutionPillarModifier as
         *     character-persistent modifiers, not per-action.
         *   - Effects — character-only via a separate system. */
        UFUNCTION(BlueprintPure, Category = "Item|Evolution|Stats")
        FActionStatModifiers GetInfusionStatModifiers(float InfusionMultiplier) const;

        /** The shared int-substat → FActionStatModifiers mapping behind
         *  GetInfusionStatModifiers (which delegates with BaseStatBonus). Exposed
         *  so the per-instance GeneratedStatBonus (on the slotted attachment) is
         *  infusion-scaled IDENTICALLY to Base — rolled ints inherit the
         *  infusion-conditional split (U3c). Pillar percents not applied here. */
        static FActionStatModifiers MapToInfusionModifiers(const FEquipmentStatBonus &Bonus, float InfusionMultiplier);

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

        /** Target type for combat targeting UI — all crystals target anyone. */
        UFUNCTION(BlueprintPure, Category = "Item|Targeting")
        ETargetType GetItemTargetType() const;

        // ==================== LIFECYCLE ====================
        // Deliberately OUTSIDE WITH_EDITOR — the PostLoad migrations (durability
        // auto-init, legacy pillar → BaseStatBonus) must run in packaged builds
        // too, so assets not re-saved in-editor still load correct values.

        virtual void PostInitProperties() override;

        /** Migration hook: initialise durability from tier for assets saved before Phase 2a
         *  (old assets loaded with MaxDurability == 0 get auto-fixed), and copy legacy
         *  pillar percent fields into BaseStatBonus. */
        virtual void PostLoad() override;

        // ==================== EDITOR SUPPORT ====================

#if WITH_EDITOR
        FString GenerateDescription() const;
        FString GetEvolutionEffectText() const;
        virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
        virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
        virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
