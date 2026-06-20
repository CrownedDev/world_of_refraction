// EquipmentDataBase.h
// Shared base for UWeaponData and URingData. Owns identity, crystal slot,
// default spells, requirements, stat-bonus roll template, and icon —
// fields and behaviour confirmed identical between weapon and ring assets
// before extraction.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Equipment/FAttachedItem.h"
#include "Equipment/IEquipmentGenerator.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "EquipmentDataBase.generated.h"

class UEvolutionItemData;
class USpellData;
class UAbilityData;
class UEffectDefinition;
class UCharacterData;
class UTexture2D;

/**
 * UEquipmentDataBase
 * Common base for weapon and ring data assets. Subclasses extend with
 * type-specific data (weapon attack, abilities, stance vs. ring mesh,
 * spell locking, etc).
 *
 * NOTE (UE5.7): Details-panel category ordering across base/derived class
 * boundaries is not controllable via `meta = (DisplayAfter = ...)` — that
 * hint only sorts within a single class. Base categories always render
 * before derived categories. Accepted as a cosmetic limitation; fixing it
 * would require an editor-only module with `IDetailCustomization`.
 */
UCLASS(Abstract, BlueprintType)
class WORLD_OF_REFRACTION_API UEquipmentDataBase : public UPrimaryDataAsset, public IEquipmentGenerator
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Name = TEXT("Unnamed Equipment");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    UTexture2D *Icon = nullptr;

    // ==================== CRYSTAL ====================

    /** Design-time attachment slot. Refined (Type+Tier) or Evolution (asset
     *  pointer), discriminated by Kind. Replaces the legacy SlottedCrystal
     *  pointer; inventory factories read this when building runtime entries.
     *  ShowOnlyInnerProperties collapses the duplicate "Attached Item" member
     *  row under the same-named category (inner EditConditions on Kind still
     *  resolve within the struct). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attached Item",
              meta = (ShowOnlyInnerProperties))
    FAttachedItem AttachedItem;

    /** Default spells for this equipment — copied to inventory entry when obtained.
     *  Lost if crystal is removed; spell vendor reassigns them. Hidden in the
     *  editor when the attachment is a augment stone (augment stones carry abilities,
     *  not spells); the hidden value is inert and never read for a augment stone. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attached Item",
              meta = (TitleProperty = "Name", EditCondition = "!IsAugmentStoneAttached", EditConditionHides))
    TArray<USpellData *> DefaultSpells;

    /** Default abilities seeded onto a augment-stone-attached weapon's ability slots.
     *  Parallels DefaultSpells; only meaningful when the attachment is a augment stone,
     *  so the editor shows it only then. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (DisplayName = "Extra Abilities", EditCondition = "IsAugmentStoneAttached", EditConditionHides))
    TArray<UAbilityData *> DefaultAbilities;

    // ==================== INFUSION ====================

    /** When true, this equipment cannot serve as an infusion source and
     *  any action wielded through it is rejected if infusion is requested.
     *  ORed with the action-level immunity at the infusion gate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bImmuneToInfusion = false;

    /** Generic lock for the attachment's provided skills (spells for crystals,
     *  abilities for augment stones). When true, the player cannot unfuse the
     *  attachment or reassign its skills at a vendor. Independent of bCanBreak
     *  (which is on UEvolutionItemData and gates wear); a crystal can still break
     *  via the standard wear path when its bCanBreak is true. No runtime code
     *  consumes this flag yet — the vendor system will use it when built. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attached Item")
    bool bLockSkills = false;

    /** Status buildup multiplier when this equipment is the infusion source
     *  (higher = faster status). Applied at the infusion DOT gate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion",
              meta = (EditCondition = "!bImmuneToInfusion", ClampMin = "0.0", ClampMax = "2.0"))
    float InfusionStatusMultiplier = 1.0f;

    // ==================== BONUSES ====================
    // Two layers: BaseStatBonus (authored, the real shipped stats — copied to
    // the inventory entry at CreateFrom* time) and GeneratedStatBonus (designer
    // PREVIEW, gameplay-inert as of U4). Toggle-ON acquisitions replace the
    // instance copy with Base + a fresh per-instance roll (ApplyPickupRoll).

    /** Designer-authored baseline. Never touched by the generator.
     *  Positive or negative; per-field clamp -21..21 (substats) /
     *  -15..15 (pillar percent) inherited from FEquipmentStatBonus. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
    FEquipmentStatBonus BaseStatBonus;

    /** DESIGNER PREVIEW (simulated sample) — a rollable example so designers can
     *  eyeball the spread. From U2 on, the REAL roll happens per-instance at
     *  acquisition (into the inventory entry); this asset layer is NOT the gameplay
     *  source for rolled instances. Read-only in editor (Roll/Reroll buttons). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview Roll",
              meta = (DisplayName = "Bonuses"))
    FEquipmentStatBonus GeneratedStatBonus;

    /** Field-wise sum of BaseStatBonus + GeneratedStatBonus. PREVIEW-side combine
     *  only — no gameplay caller as of U4 (CreateFrom* copies Base only); kept for
     *  preview/editor tooling that wants "what would this preview total look like". */
    FEquipmentStatBonus GetCombinedStatBonus() const;

    // ==================== RESISTANCE ROLL ====================
    // Per-category status-buildup resistance (9 elements + 3 physical), an OWN
    // zero-sum pool separate from the stat substats above. BaseResistance
    // (authored) is copied to the inventory entry at CreateFrom* time;
    // GeneratedResistance is designer PREVIEW (gameplay-inert as of U4) — real
    // rolls are per-instance at acquisition.

    /** Designer-authored baseline resistance. Permanent per-category +resist/-weak. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistance")
    FResistanceBonus BaseResistance;

    /** DESIGNER PREVIEW (simulated sample) — written by the Roll Resistance button
     *  so designers can eyeball a roll. From U2 on, the REAL roll is per-instance at
     *  acquisition; this asset layer is NOT the gameplay source for rolled instances. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview Roll",
              meta = (DisplayName = "Resistance"))
    FResistanceBonus GeneratedResistance;

    /** Field-wise sum of BaseResistance + GeneratedResistance. PREVIEW-side combine
     *  only — no gameplay caller as of U4 (CreateFrom* copies Base only); kept for
     *  preview/editor tooling. */
    FResistanceBonus GetCombinedResistance() const;

    // ==================== GENERATION (per-instance roll) ====================
    // Controls the per-instance roll at acquisition (U2). Inert in U0 — no
    // acquisition path reads these yet.

    /** When true, a fresh instance of this item ROLLS its StatBonus/ResistanceBonus
     *  at acquisition (from the tier budget or the overrides below). When false, the
     *  instance ships authored Base only (fixed/designed item). Default false =
     *  migration-safe; designers opt in. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
    bool bRandomGenerateOnPickup = false;

    /** Per-asset MaxPool override for the stat roll. 0 = use the tier-derived budget
     *  (the sentinel; named constant added where consumed in U2). >0 overrides it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0"))
    int32 StatMaxPoolOverride = 0;

    /** Per-asset MaxPool override for the resistance roll. 0 = use the tier budget. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0"))
    int32 ResistanceMaxPoolOverride = 0;

    // ==================== EFFECTS ====================
    // Equipment-level skill effects (starting + conditional). The STARTING subset
    // (no condition) is applied once at combat start via
    // USkillEffectManager::ApplyEquipmentEffects (sourced through
    // ULoadoutComponent::GetActiveEffects). Conditional effects await their trigger.

    /** Referenced effect bundles (author once as a UEffectDefinition, point several
     *  items at the same asset). The item's effects come entirely from these bundles;
     *  resolved into the by-value Starting/Conditional lists each gather (live link).
     *  Null/unloaded entries are skipped. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    TArray<TObjectPtr<UEffectDefinition>> ReferencedEffects;

    /** Flattened effect count across the referenced bundles (editor/UI info). */
    UFUNCTION(BlueprintPure, Category = "Effects")
    int32 GetEffectCount() const;

    /** Non-conditional effects — applied once at combat start. */
    UFUNCTION(BlueprintPure, Category = "Effects")
    TArray<FSkillEffect> GetStartingEffects() const;

    /** Condition-gated effects — never auto-applied at combat start. */
    UFUNCTION(BlueprintPure, Category = "Effects")
    TArray<FSkillEffect> GetConditionalEffects() const;

    // ==================== REQUIREMENTS ====================

    // ShowOnlyInnerProperties: inline the struct's fields under the category
    // header — without it the panel shows "Requirements" twice (category +
    // identically-named member row).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ShowOnlyInnerProperties))
    FWorldStatRequirements Requirements;

    // ==================== CRYSTAL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    bool HasCrystal() const { return AttachedItem.Kind != EAttachedItemKind::None; }

    /** Editor-only discriminator for the contextual EditConditions on
     *  DefaultSpells / DefaultAbilities. EditCondition cannot reach the nested
     *  AttachedItem.Kind path, so it resolves this UFUNCTION instead. */
    UFUNCTION()
    bool IsAugmentStoneAttached() const { return AttachedItem.Kind == EAttachedItemKind::AugmentStone; }

    /** Supplies the dropdown grey-out set for AttachedItem.CrystalType via
     *  meta=(GetRestrictedEnumValues). Resolved on this owning asset (not the
     *  struct), so it can read AttachedItem.Kind — the same reason
     *  IsAugmentStoneAttached above works for EditCondition. Returns the
     *  wrong-kind enum value names (short form, e.g. "Garnet"/"DamageStone")
     *  to restrict: stones when a gem crystal is attached, gems when a weapon
     *  stone is attached, none otherwise. Editor affordance only — the hard
     *  guarantee is the IsDataValid backstop. */
    UFUNCTION()
    TArray<FString> GetRestrictedCrystalTypes() const;

    /** Grey-out set for AttachedItem.FusionHalfAType. Half A is augment-stone only,
     *  so this restricts every gem (IsGemType) — fixed, no sibling read. Same owning-
     *  asset resolution + short-name shape as GetRestrictedCrystalTypes. */
    UFUNCTION()
    TArray<FString> GetRestrictedFusionHalfATypes() const;

    /** Grey-out set for AttachedItem.FusionHalfBType. Reads sibling
     *  AttachedItem.bFusionHalfBIsCrystal: crystal -> restrict stones (IsAugmentStoneType),
     *  stone -> restrict gems (IsGemType). Mirrors how GetRestrictedCrystalTypes keys
     *  off AttachedItem.Kind. */
    UFUNCTION()
    TArray<FString> GetRestrictedFusionHalfBTypes() const;

    /** Grey-out set for AttachedItem.FusionBonusStat. Restricts every ESubStat NOT
     *  in the wired six (RawDamage, Defense, CritDamage, TurnSpeed, StatusMultiplier,
     *  Efficiency) — the only stats a read-site actually queries — plus None, so a
     *  fusion bonus can only target a stat it will actually affect. Same owning-asset
     *  resolution + short-name shape as GetRestrictedCrystalTypes. */
    UFUNCTION()
    TArray<FString> GetRestrictedFusionBonusStats() const;

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    bool IsEvolved() const;

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    ESpellElement GetCrystalElement() const;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool MeetsRequirements(const UCharacterData *Character) const
    {
        return Requirements.MeetsRequirements(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Equipment")
    FString GetRequirementsSummary(const UCharacterData *Character) const
    {
        return Requirements.GetRequirementsSummary(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Equipment")
    FString GetDisplayName() const { return Name; }

    /** Cap on DefaultSpells, used by IsDataValid warning. Subclasses override. */
    UFUNCTION(BlueprintPure, Category = "Equipment|Spells")
    virtual int32 GetMaxSpells() const { return TNumericLimits<int32>::Max(); }

    // ==================== GENERATOR (CallInEditor) ====================
    // Editor-side roll buttons. SubstatPoints and PillarPoints act as the
    // pending pool — fill to the tier budget, then hit Roll to consume them
    // into GeneratedStatBonus (BaseStatBonus is never touched). Modify()
    // wraps the writes so they hit the transaction stack (undo / redo /
    // asset-dirty propagation).

    UPROPERTY(EditAnywhere, Category = "Preview Roll",
        meta=(ClampMin="0"))
    int32 SubstatPoints = 0;
    // Set to tier budget max to unlock Roll Substat Points

    UPROPERTY(EditAnywhere, Category = "Preview Roll",
        meta=(ClampMin="0.0"))
    float PillarPoints = 0.0f;
    // Set to tier budget max to unlock Roll Pillar Points

    UFUNCTION(CallInEditor, Category = "Preview Roll")
    virtual void RollSubstatPoints() override;
    // Only fires when SubstatPoints >= tier substat budget
    // Distributes points into stats using zero-sum broken-stick
    // Resets SubstatPoints to 0 after roll
    // Logs warning if insufficient points

    UFUNCTION(CallInEditor, Category = "Preview Roll")
    virtual void RollPillarPoints() override;
    // Only fires when PillarPoints >= tier pillar budget
    // Distributes points into pillar percent fields
    // Resets PillarPoints to 0 after roll
    // Logs warning if insufficient points

    UFUNCTION(CallInEditor, Category = "Preview Roll")
    virtual void ClearAllBonuses() override;
    // Resets the GENERATED (preview) layers + both pending point fields to 0;
    // authored BaseStatBonus / BaseResistance are preserved — preview-only, so
    // it lives in Preview Roll with the rolls it clears.

    // Roll a fresh GeneratedResistance from the tier budget (own zero-sum pool).
    // No point-gate — resistance has no pending-pool surface; one click = one roll.
    UFUNCTION(CallInEditor, Category = "Preview Roll")
    virtual void RollResistance() override;

    // IEquipmentGenerator: target the generator layer (BaseStatBonus is the
    // designer baseline and stays untouched).
    virtual FEquipmentStatBonus& GetEditableStatBonus() override { return GeneratedStatBonus; }
    virtual FResistanceBonus& GetEditableResistance() override { return GeneratedResistance; }
    virtual EItemTier GetGeneratorTier() const override { return Tier; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
