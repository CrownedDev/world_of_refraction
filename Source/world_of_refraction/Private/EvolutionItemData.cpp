// EvolutionItemData.cpp
// Implementation of UEvolutionItemData. Renamed from ItemData.cpp in commit 3
// of the crystal/evolution refactor sequence.

#include "EvolutionItemData.h"
#include "ItemConstants.h"
#include "SpellData.h"
#include "DurabilityConstants.h"
#include "CombatConstants.h"
#include "SkillTriggerUtils.h"
#include "CrystalTypeHelpers.h"
#include "CrystalDescription.h"

FString UEvolutionItemData::GetFullItemName() const
{
    return FString::Printf(TEXT("%s (%s)"), *GetCrystalName(), *GetTierString());
}

FString UEvolutionItemData::GetTierName() const
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return TEXT("F-Tier");
    case EItemTier::E_Tier:
        return TEXT("E-Tier");
    case EItemTier::D_Tier:
        return TEXT("D-Tier");
    case EItemTier::C_Tier:
        return TEXT("C-Tier");
    case EItemTier::B_Tier:
        return TEXT("B-Tier");
    case EItemTier::A_Tier:
        return TEXT("A-Tier");
    case EItemTier::S_Tier:
        return TEXT("S-Tier");
    default:
        return TEXT("Unknown");
    }
}

FString UEvolutionItemData::GetTierString() const
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return TEXT("F");
    case EItemTier::E_Tier:
        return TEXT("E");
    case EItemTier::D_Tier:
        return TEXT("D");
    case EItemTier::C_Tier:
        return TEXT("C");
    case EItemTier::B_Tier:
        return TEXT("B");
    case EItemTier::A_Tier:
        return TEXT("A");
    case EItemTier::S_Tier:
        return TEXT("S");
    default:
        return TEXT("?");
    }
}

FString UEvolutionItemData::GetCrystalName() const
{
    switch (CrystalType)
    {
    case ECrystalType::Garnet:
        return TEXT("Garnet");
    case ECrystalType::Sapphire:
        return TEXT("Sapphire");
    case ECrystalType::Citrine:
        return TEXT("Citrine");
    case ECrystalType::Emerald:
        return TEXT("Emerald");
    case ECrystalType::Amber:
        return TEXT("Amber");
    case ECrystalType::Opal:
        return TEXT("Opal");
    case ECrystalType::Onyx:
        return TEXT("Onyx");
    case ECrystalType::Amethyst:
        return TEXT("Amethyst");
    case ECrystalType::Iolite:
        return TEXT("Iolite");
    case ECrystalType::Quartz:
        return TEXT("Quartz");
    default:
        return TEXT("Unknown Crystal");
    }
}

int32 UEvolutionItemData::GetTierValue() const
{
    return static_cast<int32>(Tier);
}

ESpellElement UEvolutionItemData::GetAssociatedElement() const
{
    return CrystalTypeHelpers::GetElement(CrystalType);
}

// ==================== TARGETING ====================

ETargetType UEvolutionItemData::GetItemTargetType() const
{
    // All crystals can target anyone for tactical flexibility.
    return ETargetType::SingleAnyone;
}

// ==================== STAT MODIFIER FUNCTIONS ====================
// All read from BaseStatBonus (the new canonical authoring surface). The prior
// inverted bIsEvolutionCrystal guards have been corrected — these are
// evolution-only concepts and now return non-empty results when the crystal
// is an evolution crystal with non-zero modifiers.

bool UEvolutionItemData::HasStatModifiers() const
{
    return BaseStatBonus.BonusMindModifierPercent != 0.0f ||
           BaseStatBonus.BonusBodyModifierPercent != 0.0f ||
           BaseStatBonus.BonusSpiritModifierPercent != 0.0f ||
           BaseStatBonus.BonusRawDamage        != 0 ||
           BaseStatBonus.BonusSpellDamage      != 0 ||
           BaseStatBonus.BonusEfficiency       != 0 ||
           BaseStatBonus.BonusStatusMultiplier != 0 ||
           BaseStatBonus.BonusCritChance       != 0.0f ||
           BaseStatBonus.BonusSpellSpeed       != 0 ||
           BaseStatBonus.BonusDefense          != 0 ||
           BaseStatBonus.BonusActionSpeed      != 0 ||
           BaseStatBonus.BonusMaxHP            != 0 ||
           BaseStatBonus.BonusMaxEnergy        != 0 ||
           BaseStatBonus.BonusResistance       != 0 ||
           BaseStatBonus.BonusTurnSpeed        != 0 ||
           BaseStatBonus.BonusLuck             != 0;
}

FString UEvolutionItemData::GetStatModifierSummary() const
{
    TArray<FString> Modifiers;

    if (BaseStatBonus.BonusMindModifierPercent != 0.0f)
    {
        const FString Sign = BaseStatBonus.BonusMindModifierPercent > 0 ? TEXT("+") : TEXT("");
        Modifiers.Add(FString::Printf(TEXT("Mind %s%.0f%%"), *Sign, BaseStatBonus.BonusMindModifierPercent));
    }
    if (BaseStatBonus.BonusBodyModifierPercent != 0.0f)
    {
        const FString Sign = BaseStatBonus.BonusBodyModifierPercent > 0 ? TEXT("+") : TEXT("");
        Modifiers.Add(FString::Printf(TEXT("Body %s%.0f%%"), *Sign, BaseStatBonus.BonusBodyModifierPercent));
    }
    if (BaseStatBonus.BonusSpiritModifierPercent != 0.0f)
    {
        const FString Sign = BaseStatBonus.BonusSpiritModifierPercent > 0 ? TEXT("+") : TEXT("");
        Modifiers.Add(FString::Printf(TEXT("Spirit %s%.0f%%"), *Sign, BaseStatBonus.BonusSpiritModifierPercent));
    }

    if (Modifiers.Num() == 0)
    {
        return TEXT("No stat changes");
    }

    return FString::Join(Modifiers, TEXT(", "));
}

float UEvolutionItemData::GetMindModifierPercent() const
{
    return BaseStatBonus.BonusMindModifierPercent;
}

float UEvolutionItemData::GetBodyModifierPercent() const
{
    return BaseStatBonus.BonusBodyModifierPercent;
}

float UEvolutionItemData::GetSpiritModifierPercent() const
{
    return BaseStatBonus.BonusSpiritModifierPercent;
}

// ==================== EVOLUTION HELPER FUNCTIONS ====================

FString UEvolutionItemData::GetEvolutionTypeName() const
{
    switch (EvolutionType)
    {
    case EEvolutionType::Positive:
        return TEXT("Positive");
    case EEvolutionType::Cursed:
        return TEXT("Cursed");
    case EEvolutionType::Balanced:
        return TEXT("Balanced");
    case EEvolutionType::Specialist:
        return TEXT("Specialist");
    default:
        return TEXT("Unknown");
    }
}

FString UEvolutionItemData::GetEvolutionStatSummary() const
{
    // SubStats authoring mode has been removed — the 11 substat percent fields
    // were dropped in the BaseStatBonus migration. Only the three pillar percent
    // fields remain, sourced from BaseStatBonus.
    return GetStatModifierSummary();
}

// ==================== STAT CALCULATION ====================

FActionStatModifiers UEvolutionItemData::GetInfusionStatModifiers(float InfusionMultiplier) const
{
    FActionStatModifiers Out;

    // Map BaseStatBonus int fields 1:1 onto FActionStatModifiers, scaled by the
    // infusion magnitude (L1 = 0.5, L2 = 1.0). Int values are interpreted as
    // percentages at this boundary — numerically equivalent to the old SubStats
    // authoring path, just integer-typed.
    //
    // NOT applied here:
    //  - Pillar percent (BonusMind/Body/SpiritModifierPercent) — character-
    //    persistent via UCharacterDataComponent::ApplyCrystalPillarModifier.
    //  - Effects — character-only via a separate system.
    Out.Efficiency       = BaseStatBonus.BonusEfficiency        * InfusionMultiplier;
    Out.SpellDamage      = BaseStatBonus.BonusSpellDamage       * InfusionMultiplier;
    Out.StatusMultiplier = BaseStatBonus.BonusStatusMultiplier  * InfusionMultiplier;
    Out.CritChance       = BaseStatBonus.BonusCritChance        * InfusionMultiplier;
    Out.SpellSpeed       = BaseStatBonus.BonusSpellSpeed        * InfusionMultiplier;
    Out.Defense          = BaseStatBonus.BonusDefense           * InfusionMultiplier;
    Out.ActionSpeed      = BaseStatBonus.BonusActionSpeed       * InfusionMultiplier;
    Out.RawDamage        = BaseStatBonus.BonusRawDamage         * InfusionMultiplier;
    Out.Resistance       = BaseStatBonus.BonusResistance        * InfusionMultiplier;
    Out.TurnSpeed        = BaseStatBonus.BonusTurnSpeed         * InfusionMultiplier;
    Out.Luck             = BaseStatBonus.BonusLuck              * InfusionMultiplier;

    return Out;
}

// ==================== PASSIVE HELPER FUNCTIONS ====================

TArray<FSkillEffect> UEvolutionItemData::GetAlwaysActiveEffects() const
{
    TArray<FSkillEffect> Result;

    // Effects is authored only on evolution crystals (EditCondition gated
    // in the header). Non-evolution crystals have an empty Effects array,
    // so the loop yields an empty Result for them.
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsAlwaysActive())
        {
            Result.Add(Effect);
        }
    }

    return Result;
}

TArray<FSkillEffect> UEvolutionItemData::GetTriggeredEffects() const
{
    TArray<FSkillEffect> Result;

    for (const FSkillEffect &Effect : Effects)
    {
        if (!Effect.IsAlwaysActive())
        {
            Result.Add(Effect);
        }
    }

    return Result;
}
// ==================== DURABILITY ====================

void UEvolutionItemData::PostInitProperties()
{
    Super::PostInitProperties();

    // Skip during CDO construction and on unrefined crystals (consumables don't track durability)
    if (HasAnyFlags(RF_ClassDefaultObject) || !bIsRefined)
    {
        return;
    }

    // Per-instance durability lives on the runtime attachment (FRuntimeAttachedItem);
    // this asset only carries the design-time MaxDurability seed.
    // Auto-compute MaxDurability from tier if designer left it at 0.
    if (MaxDurability == 0)
    {
        MaxDurability = DurabilityConstants::GetMaxDurabilityForTier(Tier);
    }
}

void UEvolutionItemData::PostLoad()
{
    Super::PostLoad();

    // Migration for assets saved before Phase 2a (durability auto-init):
    // initialise MaxDurability from tier if loaded with MaxDurability == 0.
    // Immune and evolution crystals also need a positive MaxDurability so
    // UI scaling does not divide by zero.
    if (bIsRefined && MaxDurability == 0)
    {
        const int32 TierMax = DurabilityConstants::GetMaxDurabilityForTier(Tier);
        if (TierMax > 0)
        {
            MaxDurability = TierMax;
        }
    }

    // Migration: copy legacy pillar percent fields into BaseStatBonus.
    // Triggered only when BaseStatBonus is still default-zero — once an asset is
    // re-saved through the new authoring surface, legacy values are no longer
    // the source of truth. Legacy fields are scheduled for removal after the
    // content team confirms every crystal has been re-saved.
    if (bIsEvolutionCrystal)
    {
        if (BaseStatBonus.BonusMindModifierPercent == 0.0f && MindModifierPercent != 0.0f)
        {
            BaseStatBonus.BonusMindModifierPercent = MindModifierPercent;
        }
        if (BaseStatBonus.BonusBodyModifierPercent == 0.0f && BodyModifierPercent != 0.0f)
        {
            BaseStatBonus.BonusBodyModifierPercent = BodyModifierPercent;
        }
        if (BaseStatBonus.BonusSpiritModifierPercent == 0.0f && SpiritModifierPercent != 0.0f)
        {
            BaseStatBonus.BonusSpiritModifierPercent = SpiritModifierPercent;
        }
    }
}

// ==================== EDITOR FUNCTIONS ====================

#if WITH_EDITOR
FString UEvolutionItemData::GenerateDescription() const
{
    // Description tracks crystal identity only — the shared "A {tier} {name}
    // crystal." sentence. Effect text is a separate concern: items go through
    // CrystalDescription::GetItemEffectText, evolution crystals through
    // UEvolutionItemData::GetEvolutionEffectText. Neither is composed into
    // Description here.
    return CrystalDescription::GetCrystalText(FCrystalId{CrystalType, Tier});
}

FString UEvolutionItemData::GetEvolutionEffectText() const
{
    // Parallel to CrystalDescription::GetItemEffectText — produces the "effect
    // half" sentence for an evolution crystal. Standalone getter; not written
    // into Description (Description is the shared crystal-identity sentence).
    // Returns a self-contained sentence ending in "." to match item effect text.
    TArray<FString> EffectNames;
    for (const FSkillEffect &Effect : Effects)
    {
        if (!Effect.EffectName.IsEmpty())
        {
            EffectNames.Add(Effect.EffectName);
        }
    }

    TArray<FString> StatTokens;
    auto AddPillar = [&StatTokens](const TCHAR *Name, float Value)
    {
        if (Value != 0.0f)
        {
            const FString Sign = Value > 0.0f ? TEXT("+") : TEXT("");
            StatTokens.Add(FString::Printf(TEXT("%s %s%.0f%%"), Name, *Sign, Value));
        }
    };
    AddPillar(TEXT("Mind"),   BaseStatBonus.BonusMindModifierPercent);
    AddPillar(TEXT("Body"),   BaseStatBonus.BonusBodyModifierPercent);
    AddPillar(TEXT("Spirit"), BaseStatBonus.BonusSpiritModifierPercent);

    const bool bHasEffects = EffectNames.Num() > 0;
    const bool bHasStats   = StatTokens.Num()  > 0;

    if (!bHasEffects && !bHasStats)
    {
        return TEXT("Configure stats, spells, and effects.");
    }

    FString Body;
    if (bHasEffects)
    {
        Body = FString::Join(EffectNames, TEXT(", "));
    }
    if (bHasEffects && bHasStats)
    {
        Body += TEXT(" and ");
    }
    if (bHasStats)
    {
        Body += FString::Join(StatTokens, TEXT(", "));
    }

    return FString::Printf(TEXT("Grants %s."), *Body);
}

void UEvolutionItemData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    UObject::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const FName CrystalTypeProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, CrystalType);
    static const FName TierProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, Tier);
    static const FName EvolutionProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, bIsEvolutionCrystal);

    const bool bTypeOrTierChanged =
        PropertyName == CrystalTypeProperty ||
        PropertyName == TierProperty;

    // Description = crystal identity sentence only, for BOTH item and evolution
    // branches. Effect text lives in separate getters (CrystalDescription::
    // GetItemEffectText / UEvolutionItemData::GetEvolutionEffectText) and is
    // not composed into Description. Regenerate on first fill or Type/Tier
    // change so the sentence tracks identity.
    if (bIsEvolutionCrystal)
    {
        // Evolution: ItemName stays user-authored; Description tracks identity.
        if (Description.IsEmpty() || bTypeOrTierChanged)
        {
            Description = GenerateDescription();
        }
    }
    else
    {
        // Non-evolution: ItemName and Description are both derived from Type/Tier.
        if (ItemName.IsEmpty() || bTypeOrTierChanged)
        {
            ItemName = GetFullItemName();
        }
        if (Description.IsEmpty() || bTypeOrTierChanged)
        {
            Description = GenerateDescription();
        }
    }

    // Update all display values for editor viewing
    DisplayElement = GetAssociatedElement();

    // Re-init durability when designer changes Tier or the Evolution flag.
    if (PropertyName == TierProperty ||
        PropertyName == EvolutionProperty)
    {
        // Always derive MaxDurability from tier — including for unbreakable/evolution
        // crystals. Needed for non-zero UI display and to avoid divide-by-zero.
        const int32 TierMax = DurabilityConstants::GetMaxDurabilityForTier(Tier);
        if (TierMax > 0)
        {
            MaxDurability = TierMax;
        }
    }

    // Quartz is consumable-only — it cannot be refined or made an evolution
    // crystal. If the designer switches CrystalType to Quartz with either flag
    // set, force the flags off (item-system-redesign).
    if (PropertyName == CrystalTypeProperty && CrystalType == ECrystalType::Quartz)
    {
        if (bIsRefined)
        {
            bIsRefined = false;
            UE_LOG(LogTemp, Warning, TEXT("[ItemData] Quartz is consumable-only — cleared bIsRefined on %s"), *GetName());
        }
        if (bIsEvolutionCrystal)
        {
            bIsEvolutionCrystal = false;
            UE_LOG(LogTemp, Warning, TEXT("[ItemData] Quartz is consumable-only — cleared bIsEvolutionCrystal on %s"), *GetName());
        }
    }
}

void UEvolutionItemData::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    // Refresh threshold-visibility flags on every effect so EditCondition gating
    // for ConditionThreshold / SecondaryThreshold / TargetThreshold reacts live
    // to in-editor edits of the matching ESkillTrigger field. Effects is empty
    // for non-evolution crystals — the loop is a no-op in that case.
    for (FSkillEffect &Effect : Effects)
    {
        Effect.bConditionUsesThreshold          = SkillTriggerUtils::IsThresholdTrigger(Effect.Condition);
        Effect.bSecondaryConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(Effect.SecondaryCondition);
        Effect.bTargetConditionUsesThreshold    = SkillTriggerUtils::IsThresholdTrigger(Effect.TargetCondition);
    }
}

EDataValidationResult UEvolutionItemData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Quartz is consumable-only (item-system-redesign) — refined or evolution
    // Quartz is an invalid authoring state.
    if (CrystalType == ECrystalType::Quartz)
    {
        if (bIsRefined)
        {
            Context.AddError(FText::FromString(TEXT("Quartz crystals cannot be refined — they are consumable only")));
            Result = EDataValidationResult::Invalid;
        }
        if (bIsEvolutionCrystal)
        {
            Context.AddError(FText::FromString(TEXT("Quartz crystals cannot be evolution crystals — they are consumable only")));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Evolution crystal BaseStatBonus range warnings — the embedded FEquipmentStatBonus
    // struct's UPROPERTY meta clamps are baked at ClampMin=0 (intended for
    // weapons/rings) and can't be overridden at the embedding site. Crystals
    // permit negative values down to CRYSTAL_BONUS_MIN; we surface out-of-range
    // authoring here as warnings rather than enforcing in the editor UI.
    if (bIsEvolutionCrystal)
    {
        auto WarnIntOutOfRange = [&](const TCHAR *FieldName, int32 Value)
        {
            if (Value < CombatConstants::CRYSTAL_BONUS_MIN || Value > CombatConstants::CRYSTAL_BONUS_MAX)
            {
                Context.AddWarning(FText::FromString(FString::Printf(
                    TEXT("BaseStatBonus.%s = %d is outside [%d, %d] — clamp not enforced for crystals; please correct manually."),
                    FieldName, Value,
                    CombatConstants::CRYSTAL_BONUS_MIN, CombatConstants::CRYSTAL_BONUS_MAX)));
            }
        };
        WarnIntOutOfRange(TEXT("BonusRawDamage"),        BaseStatBonus.BonusRawDamage);
        WarnIntOutOfRange(TEXT("BonusSpellDamage"),      BaseStatBonus.BonusSpellDamage);
        WarnIntOutOfRange(TEXT("BonusEfficiency"),       BaseStatBonus.BonusEfficiency);
        WarnIntOutOfRange(TEXT("BonusStatusMultiplier"), BaseStatBonus.BonusStatusMultiplier);
        WarnIntOutOfRange(TEXT("BonusSpellSpeed"),       BaseStatBonus.BonusSpellSpeed);
        WarnIntOutOfRange(TEXT("BonusDefense"),          BaseStatBonus.BonusDefense);
        WarnIntOutOfRange(TEXT("BonusActionSpeed"),      BaseStatBonus.BonusActionSpeed);
        WarnIntOutOfRange(TEXT("BonusMaxHP"),            BaseStatBonus.BonusMaxHP);
        WarnIntOutOfRange(TEXT("BonusMaxEnergy"),        BaseStatBonus.BonusMaxEnergy);
        WarnIntOutOfRange(TEXT("BonusResistance"),       BaseStatBonus.BonusResistance);
        WarnIntOutOfRange(TEXT("BonusTurnSpeed"),        BaseStatBonus.BonusTurnSpeed);
        WarnIntOutOfRange(TEXT("BonusLuck"),             BaseStatBonus.BonusLuck);

        // BonusCritChance is float; cast for the int-domain comparison.
        if (BaseStatBonus.BonusCritChance < CombatConstants::CRYSTAL_BONUS_MIN ||
            BaseStatBonus.BonusCritChance > CombatConstants::CRYSTAL_BONUS_MAX)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("BaseStatBonus.BonusCritChance = %.2f is outside [%d, %d]."),
                BaseStatBonus.BonusCritChance,
                CombatConstants::CRYSTAL_BONUS_MIN, CombatConstants::CRYSTAL_BONUS_MAX)));
        }

        auto WarnPillarOutOfRange = [&](const TCHAR *FieldName, float Value)
        {
            if (Value < CombatConstants::PILLAR_MODIFIER_MIN || Value > CombatConstants::PILLAR_MODIFIER_MAX)
            {
                Context.AddWarning(FText::FromString(FString::Printf(
                    TEXT("BaseStatBonus.%s = %.2f is outside [%.1f, %.1f]."),
                    FieldName, Value,
                    CombatConstants::PILLAR_MODIFIER_MIN, CombatConstants::PILLAR_MODIFIER_MAX)));
            }
        };
        WarnPillarOutOfRange(TEXT("BonusMindModifierPercent"),   BaseStatBonus.BonusMindModifierPercent);
        WarnPillarOutOfRange(TEXT("BonusBodyModifierPercent"),   BaseStatBonus.BonusBodyModifierPercent);
        WarnPillarOutOfRange(TEXT("BonusSpiritModifierPercent"), BaseStatBonus.BonusSpiritModifierPercent);
    }

    return Result;
}
#endif