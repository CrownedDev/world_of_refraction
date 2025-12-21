// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilityDataDebug.h"
#include "Engine/Engine.h"

void UAbilityDataDebug::PrintAbilityStats(UAbilityData *Ability, UCharacterData *Character, float Duration)
{
    if (!Ability || !Character)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Ability or Character is NULL"));
        }
        return;
    }

    FString StatsString = GetAbilityStatsString(Ability, Character);

    if (GEngine)
    {
        TArray<FString> Lines;
        StatsString.ParseIntoArray(Lines, TEXT("\n"));

        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Cyan, Lines[i]);
        }
    }
}
void UAbilityDataDebug::LogAbilityStats(UAbilityData *Ability, UCharacterData *Character)
{
    if (!Ability || !Character)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Ability or Character is NULL"));
        return;
    }

    FString StatsString = GetAbilityStatsString(Ability, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString UAbilityDataDebug::GetAbilityStatsString(UAbilityData *Ability, UCharacterData *Character)
{
    if (!Ability || !Character)
    {
        return TEXT("ERROR: Invalid Ability or Character Data");
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("ABILITY: %s\n"), *Ability->AbilityName);
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->CharacterName);
    Output += TEXT("===================================\n\n");

    // Base Stats
    Output += TEXT("BASE ABILITY STATS:\n");
    Output += FString::Printf(TEXT("  Base Damage: %d\n"), Ability->BaseDamage);
    Output += FString::Printf(TEXT("  Base Energy: %d\n"), Ability->BaseEnergyCost);
    Output += FString::Printf(TEXT("  Hit Count:   %d\n"), Ability->HitCount);
    Output += FString::Printf(TEXT("  Can Infuse:  %s\n\n"), Ability->bCanBeInfused ? TEXT("Yes") : TEXT("No"));

    // Requirements
    Output += TEXT("REQUIREMENTS:\n");
    if (Ability->Requirements.HasRequirements())
    {
        Output += Ability->Requirements.GetRequirementsSummary(Character);
    }
    else
    {
        Output += TEXT("  None\n");
    }

    // Normal Use
    Output += TEXT("NORMAL USE:\n");
    Output += FString::Printf(TEXT("  Damage: %d\n"), Ability->CalculateNormalDamage(Character));
    Output += FString::Printf(TEXT("  Energy: %d\n\n"), Ability->CalculateNormalEnergyCost(Character));

    // Infused Use
    if (Ability->bCanBeInfused)

    {
        FString ElementName = UEnum::GetValueAsString(Character->InnateElement);
        ElementName.RemoveFromStart(TEXT("ESpellElement::"));

        Output += FString::Printf(TEXT("INFUSED USE (%s):\n"), *ElementName);
        Output += FString::Printf(TEXT("  Damage: %d (%.0f%% penalty)\n"),
                                  Ability->CalculateInfusedDamage(Character),
                                  CombatConstants::INFUSION_DAMAGE_PENALTY * 100.0f);
        Output += FString::Printf(TEXT("  Energy: %d (%.0f%% more)\n"),
                                  Ability->CalculateInfusedEnergyCost(Character),
                                  (CombatConstants::INFUSION_ENERGY_MULTIPLIER - 1.0f) * 100.0f);
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"),
                                  Ability->CalculateStatusBuildup(Character));
        Output += FString::Printf(TEXT("  Triggers Status: %s\n\n"),
                                  Ability->CalculateStatusBuildup(Character) >= CombatConstants::STATUS_EFFECT_THRESHOLD ? TEXT("YES!") : TEXT("No"));
    }
    else
    {
        Output += TEXT("INFUSION: Not Available\n\n");
    }
    // Effects
    if (Ability->EffectType != EStatusType::None)
    {
        FString EffectTypeName = UEnum::GetValueAsString(Ability->EffectType);
        EffectTypeName.RemoveFromStart(TEXT("EStatusType::"));

        Output += TEXT("EFFECTS:\n");
        Output += FString::Printf(TEXT("  Type: %s\n"), *EffectTypeName);

        // Show magnitude for percentage-based effects
        if (Ability->EffectMagnitude > 0.0f)
        {
            Output += FString::Printf(TEXT("  Magnitude: %.0f%%\n"), Ability->EffectMagnitude * 100.0f);
        }

        // Show value for flat effects
        if (Ability->EffectValue != 0)
        {
            Output += FString::Printf(TEXT("  Value: %d\n"), Ability->EffectValue);
        }

        // Show duration
        if (Ability->EffectDuration > 0)
        {
            Output += FString::Printf(TEXT("  Duration: %d turn%s\n"),
                                      Ability->EffectDuration,
                                      Ability->EffectDuration == 1 ? TEXT("") : TEXT("s"));
        }
        else if (Ability->EffectType == EStatusType::EnergyRestore ||
                 Ability->EffectType == EStatusType::HealthRestore)
        {
            Output += TEXT("  Duration: Instant\n");
        }

        Output += TEXT("\n");
    }
    else
    {
        Output += TEXT("EFFECTS: None\n\n");
    }

    Output += TEXT("===================================\n");

    return Output;
}

void UAbilityDataDebug::CompareAbilityEffectiveness(UAbilityData *Ability, UCharacterData *Character1, UCharacterData *Character2)
{
    if (!Ability || !Character1 || !Character2)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: Missing data for comparison"));
        }
        return;
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("ABILITY COMPARISON: %s\n"), *Ability->AbilityName);
    Output += TEXT("===================================\n\n");

    // Character 1
    Output += FString::Printf(TEXT("%s:\n"), *Character1->CharacterName);
    Output += FString::Printf(TEXT("  Normal Damage: %d\n"), Ability->CalculateNormalDamage(Character1));
    Output += FString::Printf(TEXT("  Normal Energy: %d\n"), Ability->CalculateNormalEnergyCost(Character1));
    if (Ability->bCanBeInfused)
    {
        Output += FString::Printf(TEXT("  Infused Damage: %d\n"), Ability->CalculateInfusedDamage(Character1));
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Ability->CalculateStatusBuildup(Character1));
    }
    Output += TEXT("\n");

    // Character 2
    Output += FString::Printf(TEXT("%s:\n"), *Character2->CharacterName);
    Output += FString::Printf(TEXT("  Normal Damage: %d\n"), Ability->CalculateNormalDamage(Character2));
    Output += FString::Printf(TEXT("  Normal Energy: %d\n"), Ability->CalculateNormalEnergyCost(Character2));
    if (Ability->bCanBeInfused)
    {
        Output += FString::Printf(TEXT("  Infused Damage: %d\n"), Ability->CalculateInfusedDamage(Character2));
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Ability->CalculateStatusBuildup(Character2));
    }

    Output += TEXT("===================================\n");

    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);

    if (GEngine)
    {
        TArray<FString> Lines;
        Output.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, Lines[i]);
        }
    }
}