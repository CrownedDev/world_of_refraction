// ItemData.cpp
// Implementation of ItemData functions

#include "ItemData.h"
#include "ItemConstants.h"
#include "SpellData.h"
#include "DurabilityConstants.h"
#include "CombatConstants.h"
#include "SkillTriggerUtils.h"

FString UItemData::GetFullItemName() const
{
    return FString::Printf(TEXT("%s (%s)"), *GetCrystalName(), *GetTierString());
}

FString UItemData::GetTierName() const
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

FString UItemData::GetTierString() const
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

FString UItemData::GetCrystalName() const
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

int32 UItemData::GetTierValue() const
{
    return static_cast<int32>(Tier);
}

ESpellElement UItemData::GetAssociatedElement() const
{
    switch (CrystalType)
    {
    case ECrystalType::Garnet:
        return ESpellElement::Fire;
    case ECrystalType::Sapphire:
        return ESpellElement::Water;
    case ECrystalType::Citrine:
        return ESpellElement::Lightning;
    case ECrystalType::Emerald:
        return ESpellElement::Wind;
    case ECrystalType::Amber:
        return ESpellElement::Earth;
    case ECrystalType::Opal:
        return ESpellElement::Light;
    case ECrystalType::Onyx:
        return ESpellElement::Darkness;
    case ECrystalType::Amethyst:
        return ESpellElement::Void;
    case ECrystalType::Iolite:
        return ESpellElement::Reality;
    case ECrystalType::Quartz:
        return ESpellElement::Generic;
    default:
        return ESpellElement::Generic;
    }
}

float UItemData::GetGenericResistanceBonus() const
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return ItemConstants::GENERIC_RESISTANCE_F;
    case EItemTier::E_Tier:
        return ItemConstants::GENERIC_RESISTANCE_E;
    case EItemTier::D_Tier:
        return ItemConstants::GENERIC_RESISTANCE_D;
    case EItemTier::C_Tier:
        return ItemConstants::GENERIC_RESISTANCE_C;
    case EItemTier::B_Tier:
        return ItemConstants::GENERIC_RESISTANCE_B;
    case EItemTier::A_Tier:
        return ItemConstants::GENERIC_RESISTANCE_A;
    case EItemTier::S_Tier:
        return ItemConstants::GENERIC_RESISTANCE_S;
    default:
        return 0.0f;
    }
}

int32 UItemData::GetGenericResistanceDuration() const
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return ItemConstants::GENERIC_DURATION_F;
    case EItemTier::E_Tier:
        return ItemConstants::GENERIC_DURATION_E;
    case EItemTier::D_Tier:
        return ItemConstants::GENERIC_DURATION_D;
    case EItemTier::C_Tier:
        return ItemConstants::GENERIC_DURATION_C;
    case EItemTier::B_Tier:
        return ItemConstants::GENERIC_DURATION_B;
    case EItemTier::A_Tier:
        return ItemConstants::GENERIC_DURATION_A;
    case EItemTier::S_Tier:
        return ItemConstants::GENERIC_DURATION_S;
    default:
        return 0;
    }
}

int32 UItemData::GetBrokenDarknessEnergyBonus() const
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return ItemConstants::BD_ENERGY_F;
    case EItemTier::E_Tier:
        return ItemConstants::BD_ENERGY_E;
    case EItemTier::D_Tier:
        return ItemConstants::BD_ENERGY_D;
    case EItemTier::C_Tier:
        return ItemConstants::BD_ENERGY_C;
    case EItemTier::B_Tier:
        return ItemConstants::BD_ENERGY_B;
    case EItemTier::A_Tier:
        return ItemConstants::BD_ENERGY_A;
    case EItemTier::S_Tier:
        return ItemConstants::BD_ENERGY_S;
    default:
        return 0;
    }
}

// ==================== EFFECT TYPE ====================

EItemEffectType UItemData::GetPrimaryEffectType() const
{
    switch (CrystalType)
    {
    case ECrystalType::Garnet:
        return EItemEffectType::Damage;
    case ECrystalType::Sapphire:
        return EItemEffectType::Healing;
    case ECrystalType::Citrine:
        return EItemEffectType::EnergyRestore;
    case ECrystalType::Emerald:
        return EItemEffectType::BuffSpeed;
    case ECrystalType::Amber:
        return EItemEffectType::BuffDefense;
    case ECrystalType::Opal:
        return EItemEffectType::BuffCrit;
    case ECrystalType::Onyx:
        return EItemEffectType::Silence;
    case ECrystalType::Amethyst:
        return EItemEffectType::Gamble;
    case ECrystalType::Iolite:
        return EItemEffectType::Cleanse;
    case ECrystalType::Quartz:
        return EItemEffectType::Transform;
    default:
        return EItemEffectType::Damage; // Default to damage
    }
}

// ==================== DAMAGE/HEALING VALUES ====================

float UItemData::GetDamageValue() const
{
    // Garnet (Fire Damage) and Sapphire (Water Healing) use same values
    if (CrystalType == ECrystalType::Garnet || CrystalType == ECrystalType::Sapphire)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 60.0f;
        case EItemTier::E_Tier:
            return 75.0f;
        case EItemTier::D_Tier:
            return 95.0f;
        case EItemTier::C_Tier:
            return 120.0f;
        case EItemTier::B_Tier:
            return 150.0f;
        case EItemTier::A_Tier:
            return 180.0f;
        case EItemTier::S_Tier:
            return 220.0f;
        default:
            return 0.0f;
        }
    }
    return 0.0f;
}

// ==================== ENERGY VALUES ====================

int32 UItemData::GetEnergyValue() const
{
    if (CrystalType == ECrystalType::Citrine)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 20;
        case EItemTier::E_Tier:
            return 25;
        case EItemTier::D_Tier:
            return 35;
        case EItemTier::C_Tier:
            return 45;
        case EItemTier::B_Tier:
            return 60;
        case EItemTier::A_Tier:
            return 80;
        case EItemTier::S_Tier:
            return 100;
        default:
            return 0;
        }
    }
    return 0;
}

int32 UItemData::GetSelfDamage() const
{
    if (CrystalType == ECrystalType::Citrine)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 10;
        case EItemTier::E_Tier:
            return 10;
        case EItemTier::D_Tier:
            return 10;
        case EItemTier::C_Tier:
            return 15;
        case EItemTier::B_Tier:
            return 15;
        case EItemTier::A_Tier:
            return 20;
        case EItemTier::S_Tier:
            return 25;
        default:
            return 0;
        }
    }
    return 0;
}

// ==================== BUFF VALUES ====================

float UItemData::GetBuffPercentage() const
{
    switch (CrystalType)
    {
    case ECrystalType::Emerald: // Attack Speed
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 10.0f;
        case EItemTier::E_Tier:
            return 15.0f;
        case EItemTier::D_Tier:
            return 20.0f;
        case EItemTier::C_Tier:
            return 25.0f;
        case EItemTier::B_Tier:
            return 30.0f;
        case EItemTier::A_Tier:
            return 35.0f;
        case EItemTier::S_Tier:
            return 40.0f;
        default:
            return 0.0f;
        }

    case ECrystalType::Amber: // Defense
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 15.0f;
        case EItemTier::E_Tier:
            return 20.0f;
        case EItemTier::D_Tier:
            return 25.0f;
        case EItemTier::C_Tier:
            return 30.0f;
        case EItemTier::B_Tier:
            return 35.0f;
        case EItemTier::A_Tier:
            return 40.0f;
        case EItemTier::S_Tier:
            return 50.0f;
        default:
            return 0.0f;
        }

    case ECrystalType::Opal: // Crit Chance
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 5.0f;
        case EItemTier::E_Tier:
            return 8.0f;
        case EItemTier::D_Tier:
            return 10.0f;
        case EItemTier::C_Tier:
            return 12.0f;
        case EItemTier::B_Tier:
            return 15.0f;
        case EItemTier::A_Tier:
            return 18.0f;
        case EItemTier::S_Tier:
            return 20.0f;
        default:
            return 0.0f;
        }

    default:
        return 0.0f;
    }
}

int32 UItemData::GetBuffDuration() const
{
    switch (CrystalType)
    {
    case ECrystalType::Emerald: // Attack Speed
    case ECrystalType::Amber:   // Defense
    case ECrystalType::Opal:    // Crit
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 3;
        case EItemTier::E_Tier:
            return 3;
        case EItemTier::D_Tier:
            return 4;
        case EItemTier::C_Tier:
            return 4;
        case EItemTier::B_Tier:
            return 5;
        case EItemTier::A_Tier:
            return 5;
        case EItemTier::S_Tier:
            return 6;
        default:
            return 0;
        }

    default:
        return 0;
    }
}

// ==================== SILENCE (Onyx) ====================

float UItemData::GetSilencePercentage() const
{
    if (CrystalType == ECrystalType::Onyx)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 15.0f;
        case EItemTier::E_Tier:
            return 30.0f;
        case EItemTier::D_Tier:
            return 30.0f;
        case EItemTier::C_Tier:
            return 50.0f;
        case EItemTier::B_Tier:
            return 70.0f;
        case EItemTier::A_Tier:
            return 70.0f;
        case EItemTier::S_Tier:
            return 100.0f;
        default:
            return 0.0f;
        }
    }
    return 0.0f;
}

int32 UItemData::GetSilenceDuration() const
{
    if (CrystalType == ECrystalType::Onyx)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 1;
        case EItemTier::E_Tier:
            return 1;
        case EItemTier::D_Tier:
            return 2;
        case EItemTier::C_Tier:
            return 2;
        case EItemTier::B_Tier:
            return 2;
        case EItemTier::A_Tier:
            return 3;
        case EItemTier::S_Tier:
            return 1;
        default:
            return 0;
        }
    }
    return 0;
}

// ==================== CLEANSE (Iolite) ====================

int32 UItemData::GetDebuffsToRemove() const
{
    if (CrystalType == ECrystalType::Iolite)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 1;
        case EItemTier::E_Tier:
            return 1;
        case EItemTier::D_Tier:
            return 2;
        case EItemTier::C_Tier:
            return 2;
        case EItemTier::B_Tier:
            return 3;
        case EItemTier::A_Tier:
            return 3;
        case EItemTier::S_Tier:
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

bool UItemData::GetGrantsImmunity() const
{
    if (CrystalType == ECrystalType::Iolite)
    {
        // F through A grant immunity, S doesn't need it (removes ALL)
        return Tier >= EItemTier::F_Tier && Tier <= EItemTier::A_Tier;
    }
    return false;
}

int32 UItemData::GetImmunityDuration() const
{
    if (CrystalType == ECrystalType::Iolite)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 1; // NEW
        case EItemTier::E_Tier:
            return 2; // NEW
        case EItemTier::D_Tier:
            return 1; // NEW
        case EItemTier::C_Tier:
            return 2; // NEW
        case EItemTier::B_Tier:
            return 2; // CHANGED: was 1
        case EItemTier::A_Tier:
            return 3; // CHANGED: was 2
        case EItemTier::S_Tier:
            return 0; // CHANGED: was 3 (no immunity needed - all gone!)
        default:
            return 0;
        }
    }
    return 0;
}

// ==================== OPAL REVEALS ====================

bool UItemData::GetRevealsHP() const
{
    return CrystalType == ECrystalType::Opal && Tier == EItemTier::S_Tier;
}

bool UItemData::GetRevealsStats() const
{
    return CrystalType == ECrystalType::Opal && Tier == EItemTier::S_Tier;
}

// ==================== QUARTZ TRANSFORM ====================

int32 UItemData::GetTransformThreshold() const
{
    if (CrystalType == ECrystalType::Quartz)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier:
            return 200;
        case EItemTier::E_Tier:
            return 250;
        case EItemTier::D_Tier:
            return 300;
        case EItemTier::C_Tier:
            return 400;
        case EItemTier::B_Tier:
            return 500;
        case EItemTier::A_Tier:
            return 600;
        case EItemTier::S_Tier:
            return 750;
        default:
            return 0;
        }
    }
    return 0;
}

// ==================== SECONDARY EFFECTS ====================

bool UItemData::HasSecondaryEffect() const
{
    // Only Garnet S-Tier has burn DOT
    return CrystalType == ECrystalType::Garnet && Tier == EItemTier::S_Tier;
}

int32 UItemData::GetSecondaryDamagePerTurn() const
{
    if (HasSecondaryEffect())
    {
        return 15; // Garnet S burn damage
    }
    return 0;
}

int32 UItemData::GetSecondaryDuration() const
{
    if (HasSecondaryEffect())
    {
        return 3; // Garnet S burn duration
    }
    return 0;
}

// ==================== STAT MODIFIER FUNCTIONS ====================
// All read from BaseStatBonus (the new canonical authoring surface). The prior
// inverted bIsEvolutionCrystal guards have been corrected — these are
// evolution-only concepts and now return non-empty results when the crystal
// is an evolution crystal with non-zero modifiers.

bool UItemData::HasStatModifiers() const
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

FString UItemData::GetStatModifierSummary() const
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

float UItemData::GetMindModifierPercent() const
{
    return bIsEvolutionCrystal ? BaseStatBonus.BonusMindModifierPercent : 0.0f;
}

float UItemData::GetBodyModifierPercent() const
{
    return bIsEvolutionCrystal ? BaseStatBonus.BonusBodyModifierPercent : 0.0f;
}

float UItemData::GetSpiritModifierPercent() const
{
    return bIsEvolutionCrystal ? BaseStatBonus.BonusSpiritModifierPercent : 0.0f;
}

// ==================== EVOLUTION HELPER FUNCTIONS ====================

FString UItemData::GetEvolutionTypeName() const
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

FString UItemData::GetEvolutionStatSummary() const
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

FActionStatModifiers UItemData::GetInfusionStatModifiers(float InfusionMultiplier) const
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

TArray<FSkillEffect> UItemData::GetAlwaysActiveEffects() const
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

TArray<FSkillEffect> UItemData::GetTriggeredEffects() const
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

void UItemData::PostInitProperties()
{
    Super::PostInitProperties();

    // Skip during CDO construction and on unrefined crystals (consumables don't track durability)
    if (HasAnyFlags(RF_ClassDefaultObject) || !bIsRefined)
    {
        return;
    }

    // Evolution crystals are immune to breaking by design. Per-instance
    // durability lives on FCrystalInventoryEntry; this asset only carries
    // the design-time MaxDurability seed.
    if (bIsEvolutionCrystal)
    {
        bImmuneToBreaking = true;
    }

    // Auto-compute MaxDurability from tier if designer left it at 0.
    if (MaxDurability == 0)
    {
        MaxDurability = DurabilityConstants::GetMaxDurabilityForTier(Tier);
    }
}

void UItemData::PostLoad()
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
FString UItemData::GenerateDescription() const
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
        Effect = FString::Printf(TEXT("Deals %.0f fire damage"), GetDamageValue());
        if (HasSecondaryEffect())
        {
            Effect += FString::Printf(TEXT(" and applies burn (%d/turn for %d turns)"),
                                      GetSecondaryDamagePerTurn(), GetSecondaryDuration());
        }
        break;

    case ECrystalType::Sapphire:
        Effect = FString::Printf(TEXT("Restores %.0f HP"), GetDamageValue());
        break;

    case ECrystalType::Citrine:
        Effect = FString::Printf(TEXT("Restores %d energy"), GetEnergyValue());
        if (GetSelfDamage() > 0)
        {
            Effect += FString::Printf(TEXT(" (costs %d HP)"), GetSelfDamage());
        }
        break;

    case ECrystalType::Emerald:
        Effect = FString::Printf(TEXT("Increases attack speed by %.0f%% for %d turns"),
                                 GetBuffPercentage(), GetBuffDuration());
        break;

    case ECrystalType::Amber:
        Effect = FString::Printf(TEXT("Reduces incoming damage by %.0f%% for %d turns"),
                                 GetBuffPercentage(), GetBuffDuration());
        break;

    case ECrystalType::Opal:
        Effect = FString::Printf(TEXT("Increases crit chance by %.0f%% for %d turns"),
                                 GetBuffPercentage(), GetBuffDuration());
        if (GetRevealsHP() || GetRevealsStats())
        {
            Effect += TEXT(" and reveals enemy HP and stats");
        }
        break;

    case ECrystalType::Onyx:
        if (GetSilencePercentage() >= 100.0f)
        {
            Effect = FString::Printf(TEXT("Completely silences target for %d turns"), GetSilenceDuration());
        }
        else
        {
            Effect = FString::Printf(TEXT("Locks %.0f%% of target's energy for %d turns"),
                                     GetSilencePercentage(), GetSilenceDuration());
        }
        break;

    case ECrystalType::Amethyst:
        Effect = TEXT("Random effect - high risk, high reward!");
        break;

    case ECrystalType::Iolite:
        if (GetDebuffsToRemove() == 0)
        {
            Effect = TEXT("Removes all debuffs");
        }
        else
        {
            Effect = FString::Printf(TEXT("Removes %d debuff(s)"), GetDebuffsToRemove());
        }
        if (GetGrantsImmunity())
        {
            Effect += FString::Printf(TEXT(" and grants immunity for %d turns"), GetImmunityDuration());
        }
        break;

    case ECrystalType::Quartz:
        Effect = FString::Printf(TEXT("Passively absorbs elemental damage (transforms at %d damage)"),
                                 GetTransformThreshold());
        break;

    default:
        Effect = TEXT("Unknown effect");
        break;
    }

    return FString::Printf(TEXT("A %s %s crystal. %s."), *TierDesc, *BaseName.ToLower(), *Effect);
}

FString UItemData::GenerateEvolutionDescription() const
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

void UItemData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
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
    DisplayDamageValue = GetDamageValue();
    DisplayEnergyValue = GetEnergyValue();
    DisplaySelfDamage = GetSelfDamage();
    DisplayBuffPercentage = GetBuffPercentage();
    DisplayBuffDuration = GetBuffDuration();
    DisplaySilencePercentage = GetSilencePercentage();
    DisplaySilenceDuration = GetSilenceDuration();
    DisplayDebuffsToRemove = GetDebuffsToRemove();
    DisplayGrantsImmunity = GetGrantsImmunity();
    DisplayImmunityDuration = GetImmunityDuration();
    DisplayRevealsHP = GetRevealsHP();
    DisplayRevealsStats = GetRevealsStats();
    DisplayTransformThreshold = GetTransformThreshold();
    DisplayHasSecondary = HasSecondaryEffect();
    DisplaySecondaryDamage = GetSecondaryDamagePerTurn();
    DisplaySecondaryDuration = GetSecondaryDuration();
    DisplayGenericResistance = GetGenericResistanceBonus();
    DisplayGenericDuration = GetGenericResistanceDuration();
    DisplayBDEnergy = GetBrokenDarknessEnergyBonus();

    // Re-init durability when designer changes Tier (or evolution/immune flags
    // that determine whether the crystal uses durability at all).
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const FName TierProperty = GET_MEMBER_NAME_CHECKED(UItemData, Tier);
    static const FName EvolutionProperty = GET_MEMBER_NAME_CHECKED(UItemData, bIsEvolutionCrystal);
    static const FName ImmuneProperty = GET_MEMBER_NAME_CHECKED(UItemData, bImmuneToBreaking);

    if (PropertyName == TierProperty ||
        PropertyName == EvolutionProperty ||
        PropertyName == ImmuneProperty)
    {
        // Evolution flag implies immunity (matches PostInitProperties).
        if (bIsEvolutionCrystal)
        {
            bImmuneToBreaking = true;
        }

        // Always derive MaxDurability from tier — including for immune/evolution
        // crystals. Needed for non-zero UI display and to avoid divide-by-zero.
        const int32 TierMax = DurabilityConstants::GetMaxDurabilityForTier(Tier);
        if (TierMax > 0)
        {
            MaxDurability = TierMax;
        }
    }
}

void UItemData::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
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

EDataValidationResult UItemData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Basic validation - values are computed, so just check they make sense
    if (GetDamageValue() < 0.0f)
    {
        Context.AddError(FText::FromString(TEXT("Computed damage value is negative")));
        Result = EDataValidationResult::Invalid;
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