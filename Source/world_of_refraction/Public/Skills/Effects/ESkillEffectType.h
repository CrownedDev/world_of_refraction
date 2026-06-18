// ESkillEffectType.h
// Unified status types for both physical and elemental attacks
// Element-agnostic: Display names generated via SkillEffectDisplayNames helper

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
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
    // TODO: spell-only counterpart (SpellDamageBuff) pending — see forward list. DamageBuff=all,
    //       RawDamageBuff=physical, SpellDamageBuff=spell would be the symmetric trio.
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
    // ModifyDamageTaken was split into ReduceDamageTaken (buff) / IncreaseDamageTaken
    // (debuff) — see item-system-redesign Phase 1. ReduceDamageTaken keeps the old
    // enum slot so the ModifyDamageTaken redirect resolves by name; both use a
    // POSITIVE magnitude (percent). IncreaseDamageTaken is appended at the end.
    ReduceDamageTaken UMETA(DisplayName = "Reduce Damage Taken"),
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
    ReflectPhysicalDamage UMETA(DisplayName = "Reflect Physical"),
    ReflectSpellDamage UMETA(DisplayName = "Reflect Spell"),
    Lifesteal UMETA(DisplayName = "Lifesteal"),
    AbsorbDamage UMETA(DisplayName = "Absorb"),

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
    Revive UMETA(DisplayName = "Revive"),

    // ==================== ITEM SYSTEM REDESIGN (Phase 1) ====================
    // Appended (not mid-inserted) to preserve .uasset enum-by-value stamping.
    // Debuff half of the ModifyDamageTaken split; positive magnitude = % increase.
    IncreaseDamageTaken UMETA(DisplayName = "Increase Damage Taken"),

    // ==================== STATUS BAR MANIPULATION (sweep-4) ====================
    // Effect-system gauge manipulators. Element comes from the resolved cast
    // element of the source skill (spell's Element, or infused-source's element
    // if the cast was infused). Value is the absolute buildup amount. Both
    // effect types fire instant on apply (Duration stays as an authoring field
    // but the handlers do not persist).

    /** Builds up the target's status-bar gauge in the resolved cast element.
     *  Routes through UStatusBuildupManager::AddStatusBuildup so StatusMultiplier
     *  amplification and target resistance apply as normal. Classified as a debuff. */
    StatusIncrease UMETA(DisplayName = "Status Increase (Build Gauge)"),

    /** Reduces the target's status-bar gauge by a flat amount. Routes through
     *  UStatusBuildupManager::ReduceStatusBuildupByAmount (parallel to AddStatusBuildup;
     *  the existing fraction-based ReduceStatusBuildup stays for Quartz items).
     *  Classified as a buff (reducing your own buildup is beneficial). */
    StatusDecrease UMETA(DisplayName = "Status Decrease (Reduce Gauge)"),

    // ==================== EFFICIENCY (transient — H0) ====================
    // Appended (not mid-inserted) to preserve enum-by-value stamping. Transient
    // Mind-substat Efficiency buff/debuff — summed via GetTotalStatModifier and folded
    // into GetEffectiveEfficiencyMultiplier, so it reaches BD drain / EP cost / durability.
    EfficiencyBuff UMETA(DisplayName = "Efficiency Buff"),
    EfficiencyDebuff UMETA(DisplayName = "Efficiency Debuff"),

    // ==================== POOL — MAX HP (transient — P2a) ====================
    // Appended (not mid-inserted) to preserve enum-by-value stamping. Transient MaxHP
    // buff/debuff — summed via GetTotalStatModifier and folded into RecomputeMaxPools
    // (P2b), raising/lowering the MaxHP ceiling. MaxEnergyBuff/Debuff already exist above.
    MaxHPBuff UMETA(DisplayName = "Max HP Buff"),
    MaxHPDebuff UMETA(DisplayName = "Max HP Debuff"),

    // ==================== CRIT DAMAGE DEBUFF (transient — 5f-C) ====================
    // Appended (not mid-inserted) to preserve enum-by-value stamping. The debuff side of the
    // crit-DAMAGE consumable: ModifyCritDamage ("Crit Damage Up") is the BUFF, this is its paired
    // debuff. GetCritDamageMultiplier reads (Max(0,ModifyCritDamage) − CritDamageDebuff) and floors the
    // final multiplier at CRIT_DMG_BASE (1.0) — a debuff drags crit damage toward a normal hit, never below.
    CritDamageDebuff UMETA(DisplayName = "Crit Damage Debuff"),

    // ==================== REFLEX (transient — Cluster B-4) ====================
    // Appended (not mid-inserted) to preserve enum-by-value stamping. Transient Body-substat
    // Reflex buff/debuff — summed via GetTotalStatModifier and read by the defense-window calc
    // (B-5) as (ReflexBuff − ReflexDebuff), widening/narrowing the defender's input window.
    ReflexBuff UMETA(DisplayName = "Reflex Buff"),
    ReflexDebuff UMETA(DisplayName = "Reflex Debuff")
};

/**
 * Single source of truth for buff/debuff classification, shared by FSkillEffect and
 * FActiveSkillEffect (both delegate their IsBuff/IsDebuff here, passing their own
 * magnitude field). Reads the enum only — does NOT affect enum order/serialization.
 *
 * `Magnitude` is consulted ONLY for sign-aware types (currently just ModifyStatusResist:
 * >0 = buff, <0 = debuff, exactly 0 = neither). Every other type is pure type->bool and
 * ignores Magnitude. Callers pass the SIGN-PRESERVING field for their struct
 * (FSkillEffect::Magnitude, FActiveSkillEffect::EffectValue = Value!=0 ? Value : Magnitude*100).
 */
namespace SkillEffectClassification
{
    inline bool IsBuff(ESkillEffectType Type, float Magnitude)
    {
        switch (Type)
        {
        case ESkillEffectType::MindBuff:
        case ESkillEffectType::BodyBuff:
        case ESkillEffectType::SpiritBuff:
        case ESkillEffectType::DamageBuff:
        case ESkillEffectType::DefenseBuff:
        case ESkillEffectType::SpeedBuff:
        case ESkillEffectType::CritChanceBuff:
        case ESkillEffectType::SpellSpeedBuff:
        case ESkillEffectType::ActionSpeedBuff:
        case ESkillEffectType::RawDamageBuff:
        case ESkillEffectType::SpellDamageBuff:
        case ESkillEffectType::StatusMultiplierBuff:
        case ESkillEffectType::EfficiencyBuff:
        case ESkillEffectType::SpellCostBuff:
        case ESkillEffectType::ResistanceBuff:
        case ESkillEffectType::SpellSizeBuff:
        case ESkillEffectType::MaxEnergyBuff:
        case ESkillEffectType::MaxHPBuff:
        case ESkillEffectType::TurnSpeedBuff:
        case ESkillEffectType::LuckBuff:
        case ESkillEffectType::ReflexBuff:
        case ESkillEffectType::HealthRestore:
        case ESkillEffectType::EnergyRestore:
        // Phase 2 passive-layer buffs
        case ESkillEffectType::ModifyDamageDealt:
        case ESkillEffectType::ReduceDamageTaken:
        case ESkillEffectType::ModifyHealing:
        case ESkillEffectType::ModifyCritChance:
        case ESkillEffectType::ModifyCritDamage:
        case ESkillEffectType::RestoreHPPercent:
        case ESkillEffectType::RestoreEnergyPercent:
        case ESkillEffectType::DamageReflect:
        case ESkillEffectType::ReflectPhysicalDamage:
        case ESkillEffectType::ReflectSpellDamage:
        case ESkillEffectType::Lifesteal:
        case ESkillEffectType::AbsorbDamage:
        case ESkillEffectType::GrantDOTImmunity:
        case ESkillEffectType::GrantStunImmunity:
        case ESkillEffectType::GrantSilenceImmunity:
        case ESkillEffectType::GrantAllStatusImmunity:
        case ESkillEffectType::GrantFireImmunity:
        case ESkillEffectType::GrantWaterImmunity:
        case ESkillEffectType::GrantEarthImmunity:
        case ESkillEffectType::GrantWindImmunity:
        case ESkillEffectType::GrantLightImmunity:
        case ESkillEffectType::GrantDarknessImmunity:
        case ESkillEffectType::GrantLightningImmunity:
        case ESkillEffectType::GrantVoidImmunity:
        case ESkillEffectType::GrantRealityImmunity:
        case ESkillEffectType::CleanseSelf:
        case ESkillEffectType::CleanseAllies:
        case ESkillEffectType::ExtraAction:
        case ESkillEffectType::GuaranteedCrit:
        case ESkillEffectType::IgnoreDefense:
        case ESkillEffectType::DoubleHit:
        case ESkillEffectType::Revive:
        // sweep-4: status-bar manipulation buff (reduces target's gauge)
        case ESkillEffectType::StatusDecrease:
            return true;
        // ModifyStatusResist is sign-aware: +magnitude raises status resistance (a buff),
        // -magnitude lowers it (a debuff), exactly 0 is neither.
        case ESkillEffectType::ModifyStatusResist:
            return Magnitude > 0.0f;
        default:
            return false;
        }
    }

    inline bool IsDebuff(ESkillEffectType Type, float Magnitude)
    {
        switch (Type)
        {
        case ESkillEffectType::MindDebuff:
        case ESkillEffectType::BodyDebuff:
        case ESkillEffectType::SpiritDebuff:
        case ESkillEffectType::DamageDebuff:
        case ESkillEffectType::DefenseDebuff:
        case ESkillEffectType::SpeedDebuff:
        case ESkillEffectType::CritDebuff:
        case ESkillEffectType::CritChanceDebuff:
        case ESkillEffectType::SpellSpeedDebuff:
        case ESkillEffectType::EnergyDebuff:
        case ESkillEffectType::ActionSpeedDebuff:
        case ESkillEffectType::RawDamageDebuff:
        case ESkillEffectType::SpellDamageDebuff:
        case ESkillEffectType::StatusMultiplierDebuff:
        case ESkillEffectType::EfficiencyDebuff:
        case ESkillEffectType::SpellCostDebuff:
        case ESkillEffectType::ResistanceDebuff:
        case ESkillEffectType::SpellSizeDebuff:
        case ESkillEffectType::MaxEnergyDebuff:
        case ESkillEffectType::MaxHPDebuff:
        case ESkillEffectType::TurnSpeedDebuff:
        case ESkillEffectType::LuckDebuff:
        case ESkillEffectType::ReflexDebuff:
        case ESkillEffectType::CritDamageDebuff:
        case ESkillEffectType::DOT:
        case ESkillEffectType::SkipTurn:
        case ESkillEffectType::RandomDebuff:
        case ESkillEffectType::Stun:
        case ESkillEffectType::HealBlock:
        case ESkillEffectType::Silenced:
        case ESkillEffectType::RandomSkill:
        case ESkillEffectType::SelfDamage:
        // Phase 2 passive-layer debuffs
        case ESkillEffectType::IncreaseDamageTaken:
        case ESkillEffectType::ModifyEnergyCost:
        case ESkillEffectType::ModifyTurnSpeed:
        case ESkillEffectType::DrainHP:
        case ESkillEffectType::DrainEnergy:
        case ESkillEffectType::ApplyBurnToTarget:
        case ESkillEffectType::ApplyChillToTarget:
        case ESkillEffectType::ApplyStunToTarget:
        // sweep-4: status-bar manipulation debuff (builds target's gauge)
        case ESkillEffectType::StatusIncrease:
            return true;
        // ModifyStatusResist sign-aware (see IsBuff): -magnitude = a resistance debuff.
        case ESkillEffectType::ModifyStatusResist:
            return Magnitude < 0.0f;
        default:
            return false;
        }
    }
}
