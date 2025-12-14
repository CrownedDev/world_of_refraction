// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityEffectType.generated.h"

UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
    None UMETA(DisplayName = "None"),

    // Pillar buffs/debuffs (all sub-stats)
    MindBuff UMETA(DisplayName = "Mind Buff (All Sub-Stats)"),
    MindDebuff UMETA(DisplayName = "Mind Debuff (All Sub-Stats)"),
    BodyBuff UMETA(DisplayName = "Body Buff (All Sub-Stats)"),
    BodyDebuff UMETA(DisplayName = "Body Debuff (All Sub-Stats)"),
    SpiritBuff UMETA(DisplayName = "Spirit Buff (All Sub-Stats)"),
    SpiritDebuff UMETA(DisplayName = "Spirit Debuff (All Sub-Stats)"),

    // Mind sub-stat buffs/debuffs
    SpellCostBuff UMETA(DisplayName = "Spell Cost Buff (Cost Reduction)"),
    SpellCostDebuff UMETA(DisplayName = "Spell Cost Debuff (Cost Increase)"),
    EffectDamageBuff UMETA(DisplayName = "Effect Damage Buff"),
    EffectDamageDebuff UMETA(DisplayName = "Effect Damage Debuff"),
    CritChanceBuff UMETA(DisplayName = "Crit Chance Buff"),
    CritChanceDebuff UMETA(DisplayName = "Crit Chance Debuff"),

    // Body sub-stat buffs/debuffs
    DefenseBuff UMETA(DisplayName = "Defense Buff"),
    DefenseDebuff UMETA(DisplayName = "Defense Debuff"),
    AttackSpeedBuff UMETA(DisplayName = "Attack Speed Buff"),
    AttackSpeedDebuff UMETA(DisplayName = "Attack Speed Debuff"),
    RawDamageBuff UMETA(DisplayName = "Raw Damage Buff"),
    RawDamageDebuff UMETA(DisplayName = "Raw Damage Debuff"),

    // Spirit sub-stat buffs/debuffs
    MaxEnergyBuff UMETA(DisplayName = "Max Energy Buff"),
    MaxEnergyDebuff UMETA(DisplayName = "Max Energy Debuff"),
    ResistanceBuff UMETA(DisplayName = "Resistance Buff (Status Effect)"),
    ResistanceDebuff UMETA(DisplayName = "Resistance Debuff (Status Effect)"),
    SpellSizeBuff UMETA(DisplayName = "Spell Size Buff"),
    SpellSizeDebuff UMETA(DisplayName = "Spell Size Debuff"),

    // Legacy individual buffs/debuffs (kept for compatibility)
    DamageBuff UMETA(DisplayName = "Damage Buff (Generic)"),
    DamageDebuff UMETA(DisplayName = "Damage Debuff (Generic)"),
    SpeedBuff UMETA(DisplayName = "Speed Buff (Generic)"),
    SpeedDebuff UMETA(DisplayName = "Speed Debuff (Generic)"),

    // Utility
    EnergyRestore UMETA(DisplayName = "Energy Restore"),
    EnergyDrain UMETA(DisplayName = "Energy Drain"),
    HealthRestore UMETA(DisplayName = "Health Restore"),

    // Special combat effects
    RetaliationDamage UMETA(DisplayName = "Retaliation Damage (Elemental Aura)"),
    SelfDamage UMETA(DisplayName = "Self Damage (Recoil)"),

    // Debuff removal
    RemoveDebuffs UMETA(DisplayName = "Remove All Debuffs"),
    RemoveSpeedDebuff UMETA(DisplayName = "Remove Speed Debuff"),
    RemoveDamageDebuff UMETA(DisplayName = "Remove Damage Debuff"),
    RemoveDefenseDebuff UMETA(DisplayName = "Remove Defense Debuff"),

    // DOT effects
    BurnDOT UMETA(DisplayName = "Burn (Fire DOT)"),
    ChillDOT UMETA(DisplayName = "Chill (Water DOT)"),
    PoisonDOT UMETA(DisplayName = "Poison (Earth DOT)"),
    ElectrifiedDOT UMETA(DisplayName = "Electrified (Lightning DOT)"),
    BleedDOT UMETA(DisplayName = "Bleed (Darkness DOT)"),
    CorruptDOT UMETA(DisplayName = "Corrupt (Void DOT)")
};