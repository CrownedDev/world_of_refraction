// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilityDataDebug.h"
#include "FSkillEffect.h"
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
    Output += FString::Printf(TEXT("ABILITY: %s\n"), *Ability->Name);
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->Name);
    Output += TEXT("===================================\n\n");

    // Base Stats
    Output += TEXT("BASE ABILITY STATS:\n");
    Output += FString::Printf(TEXT("  Base Damage: %d\n"), Ability->BaseDamage);
    Output += FString::Printf(TEXT("  Base Energy: %d\n"), Ability->BaseEnergyCost);
    Output += FString::Printf(TEXT("  Hit Count:   %d\n"), Ability->HitCount);
    Output += FString::Printf(TEXT("  Immune To Infusion:  %s\n\n"), Ability->bImmuneToInfusion ? TEXT("Yes") : TEXT("No"));

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
    Output += FString::Printf(TEXT("  Damage: %d\n"), Ability->CalculateDamage(Character, /*bIsInfused=*/false));
    Output += FString::Printf(TEXT("  Energy: %d\n\n"), Ability->CalculateNormalEnergyCost(Character));

    // Infused Use
    if (!Ability->bImmuneToInfusion)

    {
        FString ElementName = UEnum::GetValueAsString(Character->InnateElement);
        ElementName.RemoveFromStart(TEXT("ESpellElement::"));

        Output += FString::Printf(TEXT("INFUSED USE (%s):\n"), *ElementName);
        Output += FString::Printf(TEXT("  Damage: %d\n"),
                                  Ability->CalculateDamage(Character, /*bIsInfused=*/true));
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
    if (Ability->Effects.Num() > 0)
    {
        Output += TEXT("EFFECTS:\n");
        for (int32 i = 0; i < Ability->Effects.Num(); ++i)
        {
            const FSkillEffect &Effect = Ability->Effects[i];
            Output += FString::Printf(TEXT("  [%d] %s\n"), i + 1, *Effect.GetDescription());
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
    Output += FString::Printf(TEXT("ABILITY COMPARISON: %s\n"), *Ability->Name);
    Output += TEXT("===================================\n\n");

    // Character 1
    Output += FString::Printf(TEXT("%s:\n"), *Character1->Name);
    Output += FString::Printf(TEXT("  Normal Damage: %d\n"), Ability->CalculateDamage(Character1, /*bIsInfused=*/false));
    Output += FString::Printf(TEXT("  Normal Energy: %d\n"), Ability->CalculateNormalEnergyCost(Character1));
    if (!Ability->bImmuneToInfusion)
    {
        Output += FString::Printf(TEXT("  Infused Damage: %d\n"), Ability->CalculateDamage(Character1, /*bIsInfused=*/true));
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Ability->CalculateStatusBuildup(Character1));
    }
    Output += TEXT("\n");

    // Character 2
    Output += FString::Printf(TEXT("%s:\n"), *Character2->Name);
    Output += FString::Printf(TEXT("  Normal Damage: %d\n"), Ability->CalculateDamage(Character2, /*bIsInfused=*/false));
    Output += FString::Printf(TEXT("  Normal Energy: %d\n"), Ability->CalculateNormalEnergyCost(Character2));
    if (!Ability->bImmuneToInfusion)
    {
        Output += FString::Printf(TEXT("  Infused Damage: %d\n"), Ability->CalculateDamage(Character2, /*bIsInfused=*/true));
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