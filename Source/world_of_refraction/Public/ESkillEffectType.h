// ESkillEffectType.h
// Unified status types for both physical and elemental attacks
// Element-agnostic: Display names generated via SkillEffectDisplayNames helper

#pragma once

#include "CoreMinimal.h"
#include "ESpellElement.h"
#include "ESkillEffectType.generated.h"

/**
 * Unified status effect types - ELEMENT AGNOSTIC
 * Effects are generic mechanics, elements provide display names
 * Example: DOT + Fire = "Burn", DOT + Lightning = "Shocked"
 *
 * MIGRATION NOTE: Replaces both old ESkillEffectType (13 values) and ESkillEffectType (55+ values)
 */
UENUM(BlueprintType)
enum class ESkillEffectType : uint8
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
    SpellDamageBuff UMETA(DisplayName = "Spell Damage Buff"),
    SpellDamageDebuff UMETA(DisplayName = "Spell Damage Debuff"),
    StatusMultiplierBuff UMETA(DisplayName = "Status Multiplier Buff"),
    StatusMultiplierDebuff UMETA(DisplayName = "Status Multiplier Debuff"),
    CritChanceBuff UMETA(DisplayName = "Crit Chance Buff"),
    CritChanceDebuff UMETA(DisplayName = "Crit Chance Debuff"),
    SpellSpeedBuff UMETA(DisplayName = "Spell Speed Buff"),
    SpellSpeedDebuff UMETA(DisplayName = "Spell Speed Debuff"),

    // ==================== BODY SUB-STATS ====================

    DefenseBuff UMETA(DisplayName = "Defense Buff"),
    ActionSpeedBuff UMETA(DisplayName = "Action Speed Buff"),
    ActionSpeedDebuff UMETA(DisplayName = "Action Speed Debuff"),
    RawDamageBuff UMETA(DisplayName = "Raw Damage Buff"),
    RawDamageDebuff UMETA(DisplayName = "Raw Damage Debuff"),

    // ==================== SPIRIT SUB-STATS ====================

    MaxEnergyBuff UMETA(DisplayName = "Max Energy Buff"),
    MaxEnergyDebuff UMETA(DisplayName = "Max Energy Debuff"),
    ResistanceBuff UMETA(DisplayName = "Resistance Buff"),
    ResistanceDebuff UMETA(DisplayName = "Resistance Debuff"),
    SpellSizeBuff UMETA(DisplayName = "Spell Size Buff"),
    SpellSizeDebuff UMETA(DisplayName = "Spell Size Debuff"),
    TurnSpeedBuff UMETA(DisplayName = "Turn Speed Buff"),
    TurnSpeedDebuff UMETA(DisplayName = "Turn Speed Debuff"),
    LuckBuff UMETA(DisplayName = "Luck Buff"),
    LuckDebuff UMETA(DisplayName = "Luck Debuff"),

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
    RemoveDefenseDebuff UMETA(DisplayName = "Remove Defense Debuff"),

    // ==================== BAR-CAP GATE EFFECTS (Session X) ====================
    // Appended (not mid-inserted) to preserve .uasset enum-by-value stamping.

    /** Stun - target can only Attack or Defend, blocks Ability/Spell/Item */
    Stun UMETA(DisplayName = "Stun (Attack/Defend Only)"),

    /** Block all healing - any heal source returns 0 */
    HealBlock UMETA(DisplayName = "Heal Block"),

    /** Cannot pay EP costs - blocks Spells, Abilities, infusion charge */
    Silenced UMETA(DisplayName = "Silenced (No EP Spend)"),

    /** Forces target to use a random skill from their loadout on a random enemy */
    RandomSkill UMETA(DisplayName = "Random Skill (Loss of Control)"),

    // ==================== PASSIVE LAYER (Phase 2) ====================
    // Appended (not mid-inserted) to preserve .uasset enum-by-value stamping.

    // Stat Modifiers (percent-based, passive layer)
    ModifyDamageDealt UMETA(DisplayName = "Damage Up"),
    ModifyDamageTaken UMETA(DisplayName = "Damage Taken Up"),
    ModifyHealing UMETA(DisplayName = "Healing Up"),
    ModifyCritChance UMETA(DisplayName = "Crit Chance Up"),
    ModifyCritDamage UMETA(DisplayName = "Crit Damage Up"),
    ModifyEnergyCost UMETA(DisplayName = "Energy Cost Up"),
    ModifyTurnSpeed UMETA(DisplayName = "Turn Speed Up"),
    ModifyStatusResist UMETA(DisplayName = "Status Resist Up"),

    // Resource Changes
    RestoreHPPercent UMETA(DisplayName = "Restore HP %"),
    RestoreEnergyPercent UMETA(DisplayName = "Restore Energy %"),
    DrainHP UMETA(DisplayName = "Drain HP"),
    DrainEnergy UMETA(DisplayName = "Drain Energy"),

    // Defensive
    DamageReflect UMETA(DisplayName = "Reflect"),
    Lifesteal UMETA(DisplayName = "Lifesteal"),
    AbsorbDamage UMETA(DisplayName = "Absorb"),
    Shield UMETA(DisplayName = "Shield"),

    // Counter
    CounterAttack UMETA(DisplayName = "Counter"),

    // Trigger-type immunities — block buildup when the resolved bar-cap trigger matches.
    // GrantDOTImmunity blocks any DOT regardless of element; per-element blocking goes
    // through the GrantXxxImmunity entries below.
    GrantDOTImmunity UMETA(DisplayName = "DOT Immunity"),
    GrantStunImmunity UMETA(DisplayName = "Stun Immunity"),
    GrantSilenceImmunity UMETA(DisplayName = "Silence Immunity"),
    GrantAllStatusImmunity UMETA(DisplayName = "All-Status Immunity"),

    // Per-element immunities — block buildup when the incoming Element matches.
    GrantFireImmunity UMETA(DisplayName = "Fire Immunity"),
    GrantWaterImmunity UMETA(DisplayName = "Water Immunity"),
    GrantEarthImmunity UMETA(DisplayName = "Earth Immunity"),
    GrantWindImmunity UMETA(DisplayName = "Wind Immunity"),
    GrantLightImmunity UMETA(DisplayName = "Light Immunity"),
    GrantDarknessImmunity UMETA(DisplayName = "Darkness Immunity"),
    GrantLightningImmunity UMETA(DisplayName = "Lightning Immunity"),
    GrantVoidImmunity UMETA(DisplayName = "Void Immunity"),
    GrantRealityImmunity UMETA(DisplayName = "Reality Immunity"),

    // Status Application on Trigger
    ApplyBurnToTarget UMETA(DisplayName = "Apply Burn"),
    ApplyChillToTarget UMETA(DisplayName = "Apply Chill"),
    ApplyStunToTarget UMETA(DisplayName = "Apply Stun"),
    CleanseSelf UMETA(DisplayName = "Cleanse Self"),
    CleanseAllies UMETA(DisplayName = "Cleanse Allies"),

    // Special Mechanics
    ExtraAction UMETA(DisplayName = "Extra Action"),
    GuaranteedCrit UMETA(DisplayName = "Guaranteed Crit"),
    IgnoreDefense UMETA(DisplayName = "Ignore Defense"),
    DoubleHit UMETA(DisplayName = "Double Hit"),
    Revive UMETA(DisplayName = "Revive")
};
