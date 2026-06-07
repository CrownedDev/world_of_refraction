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
     *  pointer; inventory factories read this when building runtime entries. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attached Item")
    FAttachedItem AttachedItem;

    /** Default spells for this equipment — copied to inventory entry when obtained.
     *  Lost if crystal is removed; spell vendor reassigns them. Hidden in the
     *  editor when the attachment is a whetstone (whetstones carry abilities,
     *  not spells); the hidden value is inert and never read for a whetstone. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attached Item",
              meta = (TitleProperty = "Name", EditCondition = "!IsWeaponStoneAttached", EditConditionHides))
    TArray<USpellData *> DefaultSpells;

    /** Default abilities seeded onto a whetstone-attached weapon's whetstone slots.
     *  Parallels DefaultSpells; only meaningful when the attachment is a whetstone,
     *  so the editor shows it only then. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attached Item",
              meta = (DisplayName = "Extra Abilities", EditCondition = "IsWeaponStoneAttached", EditConditionHides))
    TArray<UAbilityData *> DefaultAbilities;

    // ==================== INFUSION ====================

    /** When true, this equipment cannot serve as an infusion source and
     *  any action wielded through it is rejected if infusion is requested.
     *  ORed with the action-level immunity at the infusion gate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bImmuneToInfusion = false;

    /** Generic lock for the attachment's provided skills (spells for crystals,
     *  abilities for whetstones). When true, the player cannot unfuse the
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
    // Roll template for per-instance stat bonuses is the field-wise sum of
    // the two layers below, exposed via GetCombinedStatBonus(). The
    // inventory entry copies that sum at CreateFromWeapon / CreateFromRing
    // time.

    /** Designer-authored baseline. Never touched by the generator.
     *  Positive or negative; per-field clamp -21..21 (substats) /
     *  -15..15 (pillar percent) inherited from FEquipmentStatBonus. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
    FEquipmentStatBonus BaseStatBonus;

    /** Generator-rolled values. Rerollable. Zero-sum per roll. Read-only
     *  in editor — only modified by Roll/Reroll buttons. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bonuses")
    FEquipmentStatBonus GeneratedStatBonus;

    /** Field-wise sum of BaseStatBonus + GeneratedStatBonus. Read by
     *  inventory factories and any code needing the asset's final
     *  per-instance roll template. */
    FEquipmentStatBonus GetCombinedStatBonus() const;

    // ==================== EFFECTS ====================
    // Equipment-level skill effects (passives + triggered). Applied via
    // USkillEffectManager::ApplyEquipmentEffects at combat start. Distinct
    // from evolution crystal effects on UEvolutionItemData::Effects — those flow
    // through their own application path.

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    TArray<FSkillEffect> Effects;

    UFUNCTION(BlueprintPure, Category = "Effects")
    int32 GetEffectCount() const { return Effects.Num(); }

    UFUNCTION(BlueprintPure, Category = "Effects")
    TArray<FSkillEffect> GetAlwaysActiveEffects() const;

    UFUNCTION(BlueprintPure, Category = "Effects")
    TArray<FSkillEffect> GetTriggeredEffects() const;

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FWorldStatRequirements Requirements;

    // ==================== CRYSTAL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    bool HasCrystal() const { return AttachedItem.Kind != EAttachedItemKind::None; }

    /** Editor-only discriminator for the contextual EditConditions on
     *  DefaultSpells / DefaultAbilities. EditCondition cannot reach the nested
     *  AttachedItem.Kind path, so it resolves this UFUNCTION instead. */
    UFUNCTION()
    bool IsWeaponStoneAttached() const { return AttachedItem.Kind == EAttachedItemKind::WeaponStone; }

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

    UPROPERTY(EditAnywhere, Category = "Bonuses",
        meta=(ClampMin="0"))
    int32 SubstatPoints = 0;
    // Set to tier budget max to unlock Roll Substat Points

    UPROPERTY(EditAnywhere, Category = "Bonuses",
        meta=(ClampMin="0.0"))
    float PillarPoints = 0.0f;
    // Set to tier budget max to unlock Roll Pillar Points

    UFUNCTION(CallInEditor, Category = "Bonuses")
    virtual void RollSubstatPoints() override;
    // Only fires when SubstatPoints >= tier substat budget
    // Distributes points into stats using zero-sum broken-stick
    // Resets SubstatPoints to 0 after roll
    // Logs warning if insufficient points

    UFUNCTION(CallInEditor, Category = "Bonuses")
    virtual void RollPillarPoints() override;
    // Only fires when PillarPoints >= tier pillar budget
    // Distributes points into pillar percent fields
    // Resets PillarPoints to 0 after roll
    // Logs warning if insufficient points

    UFUNCTION(CallInEditor, Category = "Bonuses")
    virtual void ClearAllBonuses() override;
    // Resets all stat fields and both point fields to 0

    // IEquipmentGenerator: target the generator layer (BaseStatBonus is the
    // designer baseline and stays untouched).
    virtual FEquipmentStatBonus& GetEditableStatBonus() override { return GeneratedStatBonus; }
    virtual EItemTier GetGeneratorTier() const override { return Tier; }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
#endif
};
