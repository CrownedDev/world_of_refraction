// Fill out your copyright notice in the Description page of Project Settings.

#include "SpellDataDebug.h"
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
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->CharacterName);
    Output += TEXT("===================================\n\n");

    // Element & School
    FString ElementName;
    if (Spell->bIsUniversalSpell)
    {
        // Show caster's element for universal spells
        ElementName = UEnum::GetValueAsString(Character->InnateElement);
        ElementName.RemoveFromStart(TEXT("ESpellElement::"));
    }
    else
    {
        // Show spell's element for element-specific spells
        ElementName = UEnum::GetValueAsString(Spell->Element);
        ElementName.RemoveFromStart(TEXT("ESpellElement::"));
    }

    FString SchoolName = UEnum::GetValueAsString(Spell->School);
    SchoolName.RemoveFromStart(TEXT("ESpellSchool::"));

    Output += TEXT("SPELL TYPE:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *ElementName);
    Output += FString::Printf(TEXT("  School:  %s\n"), *SchoolName);

    // Check if character can cast
    bool bCanCast = Spell->CanCharacterCast(Character);
    if (!bCanCast)
    {
        FString CharElementName = UEnum::GetValueAsString(Character->InnateElement);
        CharElementName.RemoveFromStart(TEXT("ESpellElement::"));

        if (Character->InnateElement == ESpellElement::Generic)
        {
            Output += TEXT("❌ CANNOT CAST: Generic element characters cannot cast spells\n");
        }
        else
        {
            Output += FString::Printf(TEXT("❌ CANNOT CAST: Character is %s, spell requires %s\n"),
                                      *CharElementName, *ElementName);
        }
        Output += TEXT("===================================\n");
        return Output;
    }

    // Show if universal
    if (Spell->bIsUniversalSpell)
    {
        Output += TEXT("  🌟 UNIVERSAL SPELL (All elements can cast)\n\n");
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

    // Mode Display
    if (Spell->bIsRawMode)
    {
        Output += TEXT("MODE: Raw (+10% damage, no status)\n\n");
    }
    else
    {
        Output += TEXT("MODE: Elemental (applies status)\n\n");
    }

    Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character));
    Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character));

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
    if (Spell->PrimaryEffect != EAbilityEffectType::None)
    {
        FString EffectTypeName = UEnum::GetValueAsString(Spell->PrimaryEffect);
        EffectTypeName.RemoveFromStart(TEXT("EAbilityEffectType::"));

        Output += TEXT("PRIMARY EFFECT:\n");
        Output += FString::Printf(TEXT("  Type: %s\n"), *EffectTypeName);

        if (Spell->PrimaryEffectMagnitude > 0.0f)
        {
            Output += FString::Printf(TEXT("  Magnitude: %.0f%%\n"), Spell->PrimaryEffectMagnitude * 100.0f);
        }

        if (Spell->PrimaryEffectValue != 0)
        {
            Output += FString::Printf(TEXT("  Value: %d\n"), Spell->PrimaryEffectValue);
        }

        if (Spell->PrimaryEffectDuration > 0)
        {
            Output += FString::Printf(TEXT("  Duration: %d turn%s\n"),
                                      Spell->PrimaryEffectDuration,
                                      Spell->PrimaryEffectDuration == 1 ? TEXT("") : TEXT("s"));
        }
        Output += TEXT("\n");
    }

    if (Spell->SecondaryEffect != EAbilityEffectType::None)
    {
        FString EffectTypeName = UEnum::GetValueAsString(Spell->SecondaryEffect);
        EffectTypeName.RemoveFromStart(TEXT("EAbilityEffectType::"));

        Output += TEXT("SECONDARY EFFECT (Cross-School!):\n");
        Output += FString::Printf(TEXT("  Type: %s\n"), *EffectTypeName);

        if (Spell->SecondaryEffectMagnitude > 0.0f)
        {
            Output += FString::Printf(TEXT("  Magnitude: %.0f%%\n"), Spell->SecondaryEffectMagnitude * 100.0f);
        }

        if (Spell->SecondaryEffectValue != 0)
        {
            Output += FString::Printf(TEXT("  Value: %d\n"), Spell->SecondaryEffectValue);
        }

        if (Spell->SecondaryEffectDuration > 0)
        {
            Output += FString::Printf(TEXT("  Duration: %d turn%s\n"),
                                      Spell->SecondaryEffectDuration,
                                      Spell->SecondaryEffectDuration == 1 ? TEXT("") : TEXT("s"));
        }
        Output += TEXT("\n");
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
    Output += FString::Printf(TEXT("SPELL COMPARISON: %s\n"), *Spell->SpellName);
    Output += FString::Printf(TEXT("MODE: %s\n"), Spell->bIsRawMode ? TEXT("Raw") : TEXT("Elemental"));
    Output += TEXT("===================================\n\n");

    // Character 1
    Output += FString::Printf(TEXT("%s:\n"), *Character1->CharacterName);
    if (Spell->CanCharacterCast(Character1))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character1));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character1));
    }
    else
    {
        Output += TEXT("  Cannot cast (wrong element)\n");
    }
    Output += TEXT("\n");

    // Character 2
    Output += FString::Printf(TEXT("%s:\n"), *Character2->CharacterName);
    if (Spell->CanCharacterCast(Character2))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character2));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character2));
    }
    else
    {
        Output += TEXT("  Cannot cast (wrong element)\n");
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