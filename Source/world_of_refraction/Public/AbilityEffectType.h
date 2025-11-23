// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityEffectType.generated.h"

UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
    None UMETA(DisplayName = "None"),

    // Damage over time
    BurnDOT UMETA(DisplayName = "Burn (Damage Over Time)"),
    ChillDOT UMETA(DisplayName = "Chill (Damage Over Time)"),
    ShockDOT UMETA(DisplayName = "Shock (Damage Over Time)"),
    // Or generic:
    DamageOverTime UMETA(DisplayName = "Damage Over Time"),

    // Individual stat buffs/debuffs
    DamageBuff UMETA(DisplayName = "Damage Buff"),
    DamageDebuff UMETA(DisplayName = "Damage Debuff"),
    DefenseBuff UMETA(DisplayName = "Defense Buff"),
    DefenseDebuff UMETA(DisplayName = "Defense Debuff"),
    SpeedBuff UMETA(DisplayName = "Speed Buff"),
    SpeedDebuff UMETA(DisplayName = "Speed Debuff"),

    // Pillar buffs/debuffs (all sub-stats)
    MindBuff UMETA(DisplayName = "Mind Buff (All Sub-Stats)"),
    MindDebuff UMETA(DisplayName = "Mind Debuff (All Sub-Stats)"),
    BodyBuff UMETA(DisplayName = "Body Buff (All Sub-Stats)"),
    BodyDebuff UMETA(DisplayName = "Body Debuff (All Sub-Stats)"),
    SpiritBuff UMETA(DisplayName = "Spirit Buff (All Sub-Stats)"),
    SpiritDebuff UMETA(DisplayName = "Spirit Debuff (All Sub-Stats)"),

    // Utility
    EnergyRestore UMETA(DisplayName = "Energy Restore"),
    EnergyDrain UMETA(DisplayName = "Energy Drain"),
    HealthRestore UMETA(DisplayName = "Health Restore"),

    // Special combat effects
    RetaliationDamage UMETA(DisplayName = "Retaliation Damage (Elemental Aura)"),
    SelfDamage UMETA(DisplayName = "Self Damage (Recoil)"),
};