// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Skills/SpellDataDebug.h"
#include "Engine/Engine.h"

void USpellDataDebug::PrintSpellStats(USpellData *Spell, UCharacterData *Character, float Duration)
{
    if (!Spell || !Character)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Spell or Character is NULL"));
        }
        return;
    }

    FString StatsString = GetSpellStatsString(Spell, Character);

    if (GEngine)
    {
        TArray<FString> Lines;
        StatsString.ParseIntoArray(Lines, TEXT("\n"));

        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Magenta, Lines[i]);
        }
    }
}

void USpellDataDebug::LogSpellStats(USpellData *Spell, UCharacterData *Character)
{
    if (!Spell || !Character)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Spell or Character is NULL"));
        return;
    }

    FString StatsString = GetSpellStatsString(Spell, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString USpellDataDebug::GetSpellStatsString(USpellData *Spell, UCharacterData *Character)
{
    if (!Spell || !Character)
    {
        return TEXT("ERROR: Invalid Spell or Character Data");
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("SPELL: %s\n"), *Spell->GetDisplayName(Character));
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->Name);
    Output += TEXT("===================================\n\n");

    // Element & School
    // Element & School
    FString ElementName = UEnum::GetValueAsString(Spell->Element);
    ElementName.RemoveFromStart(TEXT("ESpellElement::"));

    FString SchoolName = UEnum::GetValueAsString(Spell->School);
    SchoolName.RemoveFromStart(TEXT("ESpellSchool::"));

    Output += TEXT("SPELL TYPE:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *ElementName);
    Output += FString::Printf(TEXT("  School:  %s\n"), *SchoolName);

    // Check if character can cast
    // NOTE: As of Reality element unlock, CanCharacterCast always returns
    // true for valid characters. This branch is effectively unreachable —
    // kept for historical reference / future per-spell gates.
    bool bCanCast = Spell->CanCharacterCast(Character);
    if (!bCanCast)
    {
        Output += TEXT("⚠️  CANNOT CAST (legacy check unreachable)\n");
        Output += TEXT("===================================\n");
        return Output;
    }

    // Requirements
    Output += TEXT("REQUIREMENTS:\n");
    if (Spell->Requirements.HasRequirements())
    {
        Output += Spell->Requirements.GetRequirementsSummary(Character);
    }
    else
    {
        Output += TEXT("  None\n");
    }

    Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character));
    Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character));

    // D8: 0 = fires this turn; N = arms now, fires N turns ahead (8b/8c mechanism).
    if (Spell->ActivationDelay > 0)
    {
        Output += FString::Printf(TEXT("  Activation Delay: %d turn(s)\n"), Spell->ActivationDelay);
    }

    int32 Buildup = Spell->CalculateStatusBuildup(Character);
    if (Buildup > 0)
    {
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Buildup);
        Output += FString::Printf(TEXT("  Triggers Status: %s\n\n"),
                                  Buildup >= CombatConstants::STATUS_EFFECT_THRESHOLD ? TEXT("YES!") : TEXT("No"));
    }
    else
    {
        Output += TEXT("  Status Buildup: None\n\n");
    }

    // Effects
    const TArray<FSkillEffect> All = Spell->GetAllEffects();
    if (All.Num() > 0)
    {
        Output += TEXT("EFFECTS:\n");
        for (int32 i = 0; i < All.Num(); ++i)
        {
            const FSkillEffect &Effect = All[i];
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

void USpellDataDebug::CompareSpellEffectiveness(USpellData *Spell, UCharacterData *Character1, UCharacterData *Character2)
{
    if (!Spell || !Character1 || !Character2)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: Missing data for comparison"));
        }
        return;
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("SPELL COMPARISON: %s\n"), *Spell->Name);
    Output += TEXT("===================================\n\n");

    // Character 1
    Output += FString::Printf(TEXT("%s:\n"), *Character1->Name);
    if (Spell->CanCharacterCast(Character1))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character1));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character1));
    }
    else
    {
        Output += TEXT("  Cannot cast (legacy check unreachable)\n");
    }
    Output += TEXT("\n");

    // Character 2
    Output += FString::Printf(TEXT("%s:\n"), *Character2->Name);
    if (Spell->CanCharacterCast(Character2))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character2));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character2));
    }
    else
    {
        Output += TEXT("  Cannot cast (legacy check unreachable)\n");
    }

    Output += TEXT("===================================\n");

    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);

    if (GEngine)
    {
        TArray<FString> Lines;
        Output.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Magenta, Lines[i]);
        }
    }
}