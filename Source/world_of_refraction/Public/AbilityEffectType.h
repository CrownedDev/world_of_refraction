// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityEffectType.generated.h"

UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
    None UMETA(DisplayName = "None"),
    DamageBuff UMETA(DisplayName = "Damage Buff"),
    DamageDebuff UMETA(DisplayName = "Damage Debuff"),
    DefenseBuff UMETA(DisplayName = "Defense Buff"),
    DefenseDebuff UMETA(DisplayName = "Defense Debuff"),
    SpeedBuff UMETA(DisplayName = "Speed Buff"),
    SpeedDebuff UMETA(DisplayName = "Speed Debuff"),
    EnergyRestore UMETA(DisplayName = "Energy Restore"),
    EnergyDrain UMETA(DisplayName = "Energy Drain"),
    HealthRestore UMETA(DisplayName = "Health Restore")
};