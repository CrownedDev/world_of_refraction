// EStatusType.h
// Unified status types for both physical and elemental attacks
// Element-agnostic: Display names generated via StatusDisplayNames helper

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "EStatusType.generated.h"

/**
 * Unified status effect types - ELEMENT AGNOSTIC
 * Effects are generic mechanics, elements provide display names
 * Example: DOT + Fire = "Burn", DOT + Lightning = "Shocked"
 *
 * MIGRATION NOTE: Replaces both old EStatusType (13 values) and EStatusType (55+ values)
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
