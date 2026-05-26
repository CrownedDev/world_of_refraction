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
#include "CrystalIdentity.h"
#include "CrystalEffectTable.h"

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

int32 UEvolutionItemData::GetBrokenDarknessEnergyBonus() const
{
    return CrystalEffectTable::GetBrokenDarknessEnergyBonus(FCrystalId{CrystalType, Tier});
}

// ==================== TARGETING ====================

ETargetType UEvolutionItemData::GetItemTargetType() const
{
    // All crystals can target anyone for tactical flexibility.
    return ETargetType::SingleAnyone;
}

// ==================== EFFECT TYPE ====================

EItemEffectType UEvolutionItemData::GetPrimaryEffectType() const
{
    return CrystalIdentity::GetPrimaryEffectType(FCrystalId{CrystalType, Tier});
}

// ==================== GARNET DOT ====================
// Phase 2 redesign — Garnet is a percentage-based fire DOT (no instant damage).
// Damage per turn is a percent of the target's MaxHP; see UItemExecutor::ExecuteDamageEffect.

float UEvolutionItemData::GetDOTDamagePercent() const
{
    return CrystalEffectTable::GetDOTDamagePercent(FCrystalId{CrystalType, Tier});
}

int32 UEvolutionItemData::GetDOTDuration() const
{
    return CrystalEffectTable::GetDOTDuration(FCrystalId{CrystalType, Tier});
}

// ==================== SAPPHIRE HEAL ====================
// Phase 2 redesign — Sapphire heals a percent of the target's MaxHP.
// S-tier additionally revives a dead target at 30% MaxHP (see ExecuteHealingEffect).

float UEvolutionItemData::GetHealPercent() const
{
    return CrystalEffectTable::GetHealPercent(FCrystalId{CrystalType, Tier});
}

// ==================== PHASE 2 REDESIGN GETTERS ====================
// Percentage / duration tables for the Phase 2 crystal redesign. Each guards on
// its owning crystal type and returns 0 otherwise.

float UEvolutionItemData::GetEPRestorePercent() const
{
    return CrystalEffectTable::GetEPRestorePercent(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetSpeedBuffPercent() const
{
    return CrystalEffectTable::GetSpeedBuffPercent(FCrystalId{CrystalType, Tier});
}

int32 UEvolutionItemData::GetCrystalDuration() const
{
    return CrystalEffectTable::GetCrystalDuration(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetCritBuffPercent() const
{
    return CrystalEffectTable::GetCritBuffPercent(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetBuffChancePercent() const
{
    return CrystalEffectTable::GetBuffChancePercent(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetGambleMagnitudePercent() const
{
    return CrystalEffectTable::GetGambleMagnitudePercent(FCrystalId{CrystalType, Tier});
}

int32 UEvolutionItemData::GetGambleDuration() const
{
    return CrystalEffectTable::GetGambleDuration(FCrystalId{CrystalType, Tier});
}

int32 UEvolutionItemData::GetEffectsToRemoveCount() const
{
    return CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetStatusClearPercent() const
{
    return CrystalEffectTable::GetStatusClearPercent(FCrystalId{CrystalType, Tier});
}

int32 UEvolutionItemData::GetResistanceDuration() const
{
    return CrystalEffectTable::GetResistanceDuration(FCrystalId{CrystalType, Tier});
}

float UEvolutionItemData::GetElementalBuildupPercent() const
{
    return CrystalEffectTable::GetElementalBuildupPercent(FCrystalId{CrystalType, Tier});
}

// ==================== BUFF VALUES ====================

float UEvolutionItemData::GetBuffPercentage() const
{
    return CrystalEffectTable::GetBuffPercentage(FCrystalId{CrystalType, Tier});
}

// ==================== SILENCE (Onyx) ====================

float UEvolutionItemData::GetSilencePercentage() const
{
    return CrystalEffectTable::GetSilencePercentage(FCrystalId{CrystalType, Tier});
}

// ==================== OPAL REVEALS ====================

bool UEvolutionItemData::GetRevealsHP() const
{
    return CrystalEffectTable::GetRevealsHP(FCrystalId{CrystalType, Tier});
}

bool UEvolutionItemData::GetRevealsStats() const
{
    return CrystalEffectTable::GetRevealsStats(FCrystalId{CrystalType, Tier});
}

// ==================== STAT MODIFIER FUNCTIONS ====================
// All read from BaseStatBonus (the new canonical authoring surface). The prior
// inverted bIsEvolutionCrystal guards have been corrected — these are
// evolution-only concepts and now return non-empty results when the crystal
// is an evolution crystal with non-zero modifiers.

bool UEvolutionItemData::HasStatModifiers() const
{
    if (!bIsEvolutionCrystal)
    {
        return false;
    }
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
    if (!bIsEvolutionCrystal)
    {
        return TEXT("");
    }

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
    return bIsEvolutionCrystal ? BaseStatBonus.BonusMindModifierPercent : 0.0f;
}

float UEvolutionItemData::GetBodyModifierPercent() const
{
    return bIsEvolutionCrystal ? BaseStatBonus.BonusBodyModifierPercent : 0.0f;
}

float UEvolutionItemData::GetSpiritModifierPercent() const
{
    return bIsEvolutionCrystal ? BaseStatBonus.BonusSpiritModifierPercent : 0.0f;
}

// ==================== EVOLUTION HELPER FUNCTIONS ====================

FString UEvolutionItemData::GetEvolutionTypeName() const
{
    if (!bIsEvolutionCrystal)
    {
        return TEXT("N/A");
    }

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
    if (!bIsEvolutionCrystal)
    {
        return TEXT("N/A");
    }

    // SubStats authoring mode has been removed — the 11 substat percent fields
    // were dropped in the BaseStatBonus migration. Only the three pillar percent
    // fields remain, sourced from BaseStatBonus.
    return GetStatModifierSummary();
}

// ==================== STAT CALCULATION ====================

FActionStatModifiers UEvolutionItemData::GetInfusionStatModifiers(float InfusionMultiplier) const
{
    FActionStatModifiers Out;

    if (!bIsEvolutionCrystal)
    {
        return Out;
    }

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
    // in the header). Returns filtered list for evolution crystals and
    // empty for anything else.
    if (!bIsEvolutionCrystal)
    {
        return Result;
    }

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

    if (!bIsEvolutionCrystal)
    {
        return Result;
    }

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
    FString BaseName = GetCrystalName();
    FString TierDesc;

    // Get tier descriptor
    switch (Tier)
    {
    case EItemTier::F_Tier:
        TierDesc = TEXT("crude");
        break;
    case EItemTier::E_Tier:
        TierDesc = TEXT("common");
        break;
    case EItemTier::D_Tier:
        TierDesc = TEXT("refined");
        break;
    case EItemTier::C_Tier:
        TierDesc = TEXT("quality");
        break;
    case EItemTier::B_Tier:
        TierDesc = TEXT("exceptional");
        break;
    case EItemTier::A_Tier:
        TierDesc = TEXT("masterwork");
        break;
    case EItemTier::S_Tier:
        TierDesc = TEXT("legendary");
        break;
    default:
        TierDesc = TEXT("unknown");
        break;
    }

    // Generate description based on crystal type
    FString Effect;
    switch (CrystalType)
    {
    case ECrystalType::Garnet:
        Effect = FString::Printf(TEXT("Applies a fire burn dealing %.0f%% of target's max HP per turn for %d turns"),
                                 GetDOTDamagePercent(), GetDOTDuration());
        break;

    case ECrystalType::Sapphire:
        if (Tier == EItemTier::S_Tier)
        {
            Effect = TEXT("Revives fallen ally at 30% HP, or heals for 60% max HP");
        }
        else
        {
            Effect = FString::Printf(TEXT("Restores %.0f%% of target's max HP"), GetHealPercent());
        }
        break;

    case ECrystalType::Citrine:
        Effect = FString::Printf(TEXT("Restores %.0f%% of the target's max energy; overloads the user with Lightning status buildup"),
                                 GetEPRestorePercent());
        break;

    case ECrystalType::Emerald:
        if (Tier == EItemTier::S_Tier)
        {
            Effect = TEXT("Grants the target an extra turn");
        }
        else
        {
            Effect = FString::Printf(TEXT("Increases turn speed by %.0f%% for %d turns"),
                                     GetSpeedBuffPercent(), GetCrystalDuration());
        }
        break;

    case ECrystalType::Amber:
        Effect = FString::Printf(TEXT("Buffs an ally's defense (or debuffs an enemy's) by %.0f%% for %d turns"),
                                 GetBuffPercentage(), GetCrystalDuration());
        break;

    case ECrystalType::Opal:
        Effect = FString::Printf(TEXT("Buffs an ally's crit chance (or debuffs an enemy's) by %.0f%% for %d turns"),
                                 GetCritBuffPercent(), GetCrystalDuration());
        break;

    case ECrystalType::Onyx:
        if (Tier == EItemTier::S_Tier)
        {
            Effect = TEXT("Completely silences target for 1 turn");
        }
        else
        {
            Effect = FString::Printf(TEXT("Drains %.0f%% of the target's energy on use"),
                                     GetSilencePercentage());
        }
        break;

    case ECrystalType::Amethyst:
        Effect = FString::Printf(TEXT("%.0f%% chance of a random buff (else a random debuff) at %.0f%% magnitude for %d turns"),
                                 GetBuffChancePercent(), GetGambleMagnitudePercent(), GetGambleDuration());
        break;

    case ECrystalType::Iolite:
        if (GetEffectsToRemoveCount() >= 99)
        {
            Effect = TEXT("Removes all debuffs from an ally, or all buffs from an enemy");
        }
        else
        {
            Effect = FString::Printf(TEXT("Removes up to %d debuff(s) from an ally, or %d buff(s) from an enemy"),
                                     GetEffectsToRemoveCount(), GetEffectsToRemoveCount());
        }
        break;

    case ECrystalType::Quartz:
        Effect = FString::Printf(TEXT("Clears %.0f%% of the target's status bar and grants matching elemental resistance for %d turns"),
                                 GetStatusClearPercent(), GetResistanceDuration());
        break;

    default:
        Effect = TEXT("Unknown effect");
        break;
    }

    return FString::Printf(TEXT("A %s %s crystal. %s."), *TierDesc, *BaseName.ToLower(), *Effect);
}

FString UEvolutionItemData::GenerateEvolutionDescription() const
{
    TArray<FString> Parts;

    // Stat modifiers
    FString StatSummary = GetStatModifierSummary();
    if (!StatSummary.IsEmpty() && StatSummary != TEXT("No stat changes"))
    {
        Parts.Add(StatSummary);
    }

    // Effects - names only
    if (Effects.Num() > 0)
    {
        TArray<FString> EffectNames;
        for (const FSkillEffect &Effect : Effects)
        {
            if (!Effect.EffectName.IsEmpty())
            {
                EffectNames.Add(Effect.EffectName);
            }
        }

        if (EffectNames.Num() > 0)
        {
            Parts.Add(TEXT("Effects: ") + FString::Join(EffectNames, TEXT(", ")));
        }
    }

    if (Parts.Num() == 0)
    {
        return TEXT("Configure stats, spells, and effects");
    }

    return FString::Join(Parts, TEXT(". ")) + TEXT(".");
}

void UEvolutionItemData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    UObject::PostEditChangeProperty(PropertyChangedEvent);

    // Evolution crystals: Don't auto-generate name/description (user provides unique values)
    if (bIsEvolutionCrystal)
    {
        // Only auto-generate description if empty, based on stats/spells
        if (Description.IsEmpty())
        {
            Description = GenerateEvolutionDescription();
        }
        // Leave ItemName alone for manual entry
    }
    else
    {
        // Non-evolution crystals: Auto-generate name and description
        ItemName = GetFullItemName();
        Description = GenerateDescription();
    }

    // Update all display values for editor viewing
    DisplayElement = GetAssociatedElement();
    DisplayEffectType = GetPrimaryEffectType();
    DisplayBuffPercentage = GetBuffPercentage();
    DisplaySilencePercentage = GetSilencePercentage();
    DisplayRevealsHP = GetRevealsHP();
    DisplayRevealsStats = GetRevealsStats();
    DisplayBDEnergy = GetBrokenDarknessEnergyBonus();

    // Re-init durability when designer changes Tier or the Evolution flag.
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const FName TierProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, Tier);
    static const FName EvolutionProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, bIsEvolutionCrystal);

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
    static const FName CrystalTypeProperty = GET_MEMBER_NAME_CHECKED(UEvolutionItemData, CrystalType);
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