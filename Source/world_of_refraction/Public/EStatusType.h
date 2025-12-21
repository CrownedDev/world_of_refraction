// EStatusType.h
// Unified status types for both physical and elemental attacks
// Element-agnostic: Display names generated via StatusDisplayNames helper

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "EPhysicalDamageType.h"
#include "AbilityEffectType.h"
#include "EStatusType.generated.h"

/**
 * Unified status effect types - ELEMENT AGNOSTIC
 * Effects are generic mechanics, elements provide display names
 * Example: DOT + Fire = "Burn", DOT + Lightning = "Shocked"
 *
 * MIGRATION NOTE: Replaces both old EStatusType (13 values) and EAbilityEffectType (55+ values)
 */
UENUM(BlueprintType)
enum class EStatusType : uint8
{
    None UMETA(DisplayName = "None"),

    // ==================== CORE STATUS BAR TRIGGERS ====================

    /** Damage over time - display varies by element */
    DOT UMETA(DisplayName = "Damage Over Time"),

    /** Speed reduction - display varies by element */
    SpeedDebuff UMETA(DisplayName = "Speed Debuff"),

    /** Defense reduction - display varies by element */
    DefenseDebuff UMETA(DisplayName = "Defense Debuff"),

    /** Crit chance reduction - display varies by element */
    CritDebuff UMETA(DisplayName = "Crit Chance Debuff"),

    /** Energy manipulation - display varies by element */
    EnergyDebuff UMETA(DisplayName = "Energy Debuff"),

    /** Skip turn / stun - display varies by element */
    SkipTurn UMETA(DisplayName = "Skip Turn"),

    /** Random stat debuff - display varies by element */
    RandomDebuff UMETA(DisplayName = "Random Stat Debuff"),

    /** Burst damage (Reality/Raw mode) */
    BurstDamage UMETA(DisplayName = "Burst Damage"),

    // ==================== PILLAR STAT MODIFIERS ====================

    MindBuff UMETA(DisplayName = "Mind Buff (All Mind Sub-Stats)"),
    MindDebuff UMETA(DisplayName = "Mind Debuff (All Mind Sub-Stats)"),
    BodyBuff UMETA(DisplayName = "Body Buff (All Body Sub-Stats)"),
    BodyDebuff UMETA(DisplayName = "Body Debuff (All Body Sub-Stats)"),
    SpiritBuff UMETA(DisplayName = "Spirit Buff (All Spirit Sub-Stats)"),
    SpiritDebuff UMETA(DisplayName = "Spirit Debuff (All Spirit Sub-Stats)"),

    // ==================== MIND SUB-STATS ====================

    SpellCostBuff UMETA(DisplayName = "Spell Cost Buff"),
    SpellCostDebuff UMETA(DisplayName = "Spell Cost Debuff"),
    EffectDamageBuff UMETA(DisplayName = "Effect Damage Buff"),
    EffectDamageDebuff UMETA(DisplayName = "Effect Damage Debuff"),
    CritChanceBuff UMETA(DisplayName = "Crit Chance Buff"),
    CritChanceDebuff UMETA(DisplayName = "Crit Chance Debuff"),

    // ==================== BODY SUB-STATS ====================

    DefenseBuff UMETA(DisplayName = "Defense Buff"),
    AttackSpeedBuff UMETA(DisplayName = "Attack Speed Buff"),
    AttackSpeedDebuff UMETA(DisplayName = "Attack Speed Debuff"),
    RawDamageBuff UMETA(DisplayName = "Raw Damage Buff"),
    RawDamageDebuff UMETA(DisplayName = "Raw Damage Debuff"),

    // ==================== SPIRIT SUB-STATS ====================

    MaxEnergyBuff UMETA(DisplayName = "Max Energy Buff"),
    MaxEnergyDebuff UMETA(DisplayName = "Max Energy Debuff"),
    ResistanceBuff UMETA(DisplayName = "Resistance Buff"),
    ResistanceDebuff UMETA(DisplayName = "Resistance Debuff"),
    SpellSizeBuff UMETA(DisplayName = "Spell Size Buff"),
    SpellSizeDebuff UMETA(DisplayName = "Spell Size Debuff"),

    // ==================== LEGACY GENERIC ====================

    DamageBuff UMETA(DisplayName = "Damage Buff (Generic)"),
    DamageDebuff UMETA(DisplayName = "Damage Debuff (Generic)"),
    SpeedBuff UMETA(DisplayName = "Speed Buff (Generic)"),

    // ==================== UTILITY ====================

    EnergyRestore UMETA(DisplayName = "Energy Restore"),
    EnergyDrain UMETA(DisplayName = "Energy Drain"),
    HealthRestore UMETA(DisplayName = "Health Restore"),
    Heal UMETA(DisplayName = "Heal (Instant)"),

    // ==================== SPECIAL COMBAT ====================

    RetaliationDamage UMETA(DisplayName = "Retaliation Damage"),
    SelfDamage UMETA(DisplayName = "Self Damage"),

    // ==================== DEBUFF REMOVAL ====================

    Cleanse UMETA(DisplayName = "Cleanse (Remove All Debuffs)"),
    RemoveSpeedDebuff UMETA(DisplayName = "Remove Speed Debuff"),
    RemoveDamageDebuff UMETA(DisplayName = "Remove Damage Debuff"),
    RemoveDefenseDebuff UMETA(DisplayName = "Remove Defense Debuff")
};

/**
 * Helper functions for status type mapping
 */
namespace StatusTypeHelper
{
    /** Get default status type for an element - ELEMENT AGNOSTIC VERSION */
    inline EStatusType GetDefaultForElement(ESpellElement Element)
    {
        switch (Element)
        {
        case ESpellElement::Fire:
            return EStatusType::DOT;
        case ESpellElement::Water:
            return EStatusType::SpeedDebuff;
        case ESpellElement::Earth:
            return EStatusType::DefenseDebuff;
        case ESpellElement::Wind:
            return EStatusType::SkipTurn;
        case ESpellElement::Lightning:
            return EStatusType::DOT;
        case ESpellElement::Light:
            return EStatusType::CritDebuff;
        case ESpellElement::Darkness:
            return EStatusType::EnergyDebuff;
        case ESpellElement::Void:
            return EStatusType::RandomDebuff;
        case ESpellElement::Reality:
            return EStatusType::BurstDamage;
        case ESpellElement::Generic:
            return EStatusType::BurstDamage;
        default:
            return EStatusType::None;
        }
    }

    /** Get default status type for a physical damage type */
    inline EStatusType GetDefaultForPhysical(EPhysicalDamageType DamageType)
    {
        switch (DamageType)
        {
        case EPhysicalDamageType::Slash:
            return EStatusType::DOT;
        case EPhysicalDamageType::Pierce:
            return EStatusType::DefenseDebuff;
        case EPhysicalDamageType::Blunt:
            return EStatusType::SkipTurn;
        default:
            return EStatusType::None;
        }
    }

    /** Check if a status type causes skip turn */
    inline bool IsSkipTurnStatus(EStatusType Status)
    {
        return Status == EStatusType::SkipTurn;
    }

    /** Check if a status type is DOT */
    inline bool IsDOTStatus(EStatusType Status)
    {
        return Status == EStatusType::DOT;
    }

    /** Check if status is a buff */
    inline bool IsBuff(EStatusType Status)
    {
        switch (Status)
        {
        case EStatusType::MindBuff:
        case EStatusType::BodyBuff:
        case EStatusType::SpiritBuff:
        case EStatusType::SpellCostBuff:
        case EStatusType::EffectDamageBuff:
        case EStatusType::CritChanceBuff:
        case EStatusType::DefenseBuff:
        case EStatusType::AttackSpeedBuff:
        case EStatusType::RawDamageBuff:
        case EStatusType::MaxEnergyBuff:
        case EStatusType::ResistanceBuff:
        case EStatusType::SpellSizeBuff:
        case EStatusType::DamageBuff:
        case EStatusType::SpeedBuff:
        case EStatusType::HealthRestore:
        case EStatusType::EnergyRestore:
        case EStatusType::Heal:
            return true;
        default:
            return false;
        }
    }

    /** Check if status is a debuff */
    inline bool IsDebuff(EStatusType Status)
    {
        switch (Status)
        {
        case EStatusType::MindDebuff:
        case EStatusType::BodyDebuff:
        case EStatusType::SpiritDebuff:
        case EStatusType::SpellCostDebuff:
        case EStatusType::EffectDamageDebuff:
        case EStatusType::CritChanceDebuff:
        case EStatusType::DefenseDebuff:
        case EStatusType::AttackSpeedDebuff:
        case EStatusType::RawDamageDebuff:
        case EStatusType::MaxEnergyDebuff:
        case EStatusType::ResistanceDebuff:
        case EStatusType::SpellSizeDebuff:
        case EStatusType::DamageDebuff:
        case EStatusType::SpeedDebuff:
        case EStatusType::DOT:
        case EStatusType::SkipTurn:
        case EStatusType::EnergyDebuff:
        case EStatusType::CritDebuff:
        case EStatusType::RandomDebuff:
            return true;
        default:
            return false;
        }
    }

    /** Get status type from AbilityEffectType (COMPATIBILITY LAYER - for migration) */
    inline EStatusType GetFromAbilityEffect(EAbilityEffectType EffectType)
    {
        switch (EffectType)
        {
        // DOTs
        case EAbilityEffectType::BurnDOT:
        case EAbilityEffectType::BleedDOT:
        case EAbilityEffectType::PoisonDOT:
        case EAbilityEffectType::ElectrifiedDOT:
        case EAbilityEffectType::ChillDOT:
        case EAbilityEffectType::CorruptDOT:
            return EStatusType::DOT;

        // Speed debuffs
        case EAbilityEffectType::SpeedDebuff:
        case EAbilityEffectType::AttackSpeedDebuff:
            return EStatusType::SpeedDebuff;

        // Defense debuffs
        case EAbilityEffectType::DefenseDebuff:
            return EStatusType::DefenseDebuff;

        // Crit debuffs
        case EAbilityEffectType::CritChanceDebuff:
            return EStatusType::CritDebuff;

        // Energy debuffs
        case EAbilityEffectType::EnergyDrain:
            return EStatusType::EnergyDebuff;

        // Skip turn
        case EAbilityEffectType::Stun:
            return EStatusType::SkipTurn;

        // Damage debuffs
        case EAbilityEffectType::DamageDebuff:
        case EAbilityEffectType::RawDamageDebuff:
        case EAbilityEffectType::EffectDamageDebuff:
            return EStatusType::RandomDebuff;

        // Pillar buffs/debuffs
        case EAbilityEffectType::MindBuff:
            return EStatusType::MindBuff;
        case EAbilityEffectType::MindDebuff:
            return EStatusType::MindDebuff;
        case EAbilityEffectType::BodyBuff:
            return EStatusType::BodyBuff;
        case EAbilityEffectType::BodyDebuff:
            return EStatusType::BodyDebuff;
        case EAbilityEffectType::SpiritBuff:
            return EStatusType::SpiritBuff;
        case EAbilityEffectType::SpiritDebuff:
            return EStatusType::SpiritDebuff;

        // Mind sub-stats
        case EAbilityEffectType::SpellCostBuff:
            return EStatusType::SpellCostBuff;
        case EAbilityEffectType::SpellCostDebuff:
            return EStatusType::SpellCostDebuff;
        case EAbilityEffectType::EffectDamageBuff:
            return EStatusType::EffectDamageBuff;
        case EAbilityEffectType::CritChanceBuff:
            return EStatusType::CritChanceBuff;

        // Body sub-stats
        case EAbilityEffectType::DefenseBuff:
            return EStatusType::DefenseBuff;
        case EAbilityEffectType::AttackSpeedBuff:
            return EStatusType::AttackSpeedBuff;
        case EAbilityEffectType::RawDamageBuff:
            return EStatusType::RawDamageBuff;

        // Spirit sub-stats
        case EAbilityEffectType::MaxEnergyBuff:
            return EStatusType::MaxEnergyBuff;
        case EAbilityEffectType::MaxEnergyDebuff:
            return EStatusType::MaxEnergyDebuff;
        case EAbilityEffectType::ResistanceBuff:
            return EStatusType::ResistanceBuff;
        case EAbilityEffectType::ResistanceDebuff:
            return EStatusType::ResistanceDebuff;
        case EAbilityEffectType::SpellSizeBuff:
            return EStatusType::SpellSizeBuff;
        case EAbilityEffectType::SpellSizeDebuff:
            return EStatusType::SpellSizeDebuff;

        // Legacy generic
        case EAbilityEffectType::DamageBuff:
            return EStatusType::DamageBuff;
        case EAbilityEffectType::SpeedBuff:
            return EStatusType::SpeedBuff;

        // Utility
        case EAbilityEffectType::EnergyRestore:
            return EStatusType::EnergyRestore;
        case EAbilityEffectType::HealthRestore:
            return EStatusType::HealthRestore;

        // Special
        case EAbilityEffectType::RetaliationDamage:
            return EStatusType::RetaliationDamage;
        case EAbilityEffectType::SelfDamage:
            return EStatusType::SelfDamage;

        // Debuff removal
        case EAbilityEffectType::RemoveDebuffs:
            return EStatusType::Cleanse;
        case EAbilityEffectType::RemoveSpeedDebuff:
            return EStatusType::RemoveSpeedDebuff;
        case EAbilityEffectType::RemoveDamageDebuff:
            return EStatusType::RemoveDamageDebuff;
        case EAbilityEffectType::RemoveDefenseDebuff:
            return EStatusType::RemoveDefenseDebuff;

        default:
            return EStatusType::None;
        }
    }
}