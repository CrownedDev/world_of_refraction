// EvolutionDataDebug.cpp
// Implementation of EvolutionData debug utilities

#include "EvolutionDataDebug.h"
#include "UltimateData.h"
#include "SpellData.h"
#include "Engine/Engine.h"

void UEvolutionDataDebug::PrintEvolutionStats(UEvolutionData *Evolution, UCharacterData *Character, float Duration)
{
    if (!Evolution)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: EvolutionData is NULL"));
        }
        return;
    }

    FString StatsString = GetEvolutionStatsString(Evolution, Character);

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

void UEvolutionDataDebug::LogEvolutionStats(UEvolutionData *Evolution, UCharacterData *Character)
{
    if (!Evolution)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: EvolutionData is NULL"));
        return;
    }

    FString StatsString = GetEvolutionStatsString(Evolution, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString UEvolutionDataDebug::GetEvolutionStatsString(UEvolutionData *Evolution, UCharacterData *Character)
{
    if (!Evolution)
    {
        return TEXT("ERROR: Invalid Evolution Data");
    }

    FString Output = TEXT("");
    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("EVOLUTION: %s\n"), *Evolution->EvolutionName);
    Output += TEXT("==========================================\n\n");

    // Identity
    Output += TEXT("IDENTITY:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *Evolution->GetElementName());
    Output += FString::Printf(TEXT("  Type: %s\n"), *Evolution->GetEvolutionTypeName());
    if (!Evolution->Lore.IsEmpty())
    {
        Output += FString::Printf(TEXT("  Lore: \"%s\"\n"), *Evolution->Lore);
    }
    Output += TEXT("\n");

    // Requirements
    Output += TEXT("REQUIREMENTS:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *Evolution->GetElementName());

    if (Character)
    {
        bool bCanUse = Evolution->CanCharacterUse(Character);

        Output += FString::Printf(TEXT("  %s: %s\n"),
                                  *Character->CharacterName,
                                  bCanUse ? TEXT("CAN EVOLVE") : TEXT("CANNOT EVOLVE"));

        if (!bCanUse)
        {
            if (Character->InnateElement == ESpellElement::Generic)
            {
                Output += TEXT("    - Generic characters cannot evolve\n");
            }
            else
            {
                Output += TEXT("    - Element mismatch\n");
            }
        }
    }
    Output += TEXT("\n");

    // Stat Modifications
    Output += TEXT("STAT MODIFIERS:\n");
    Output += FString::Printf(TEXT("  %s\n"), *Evolution->GetStatModifierSummary());

    if (Character)
    {
        float BaseMind = Character->GetEffectiveMind();
        float BaseBody = Character->GetEffectiveBody();
        float BaseSpirit = Character->GetEffectiveSpirit();

        float ModMind = Evolution->CalculateModifiedMind(BaseMind);
        float ModBody = Evolution->CalculateModifiedBody(BaseBody);
        float ModSpirit = Evolution->CalculateModifiedSpirit(BaseSpirit);

        Output += FString::Printf(TEXT("  With %s:\n"), *Character->CharacterName);
        Output += FString::Printf(TEXT("    Mind: %.1f -> %.1f\n"), BaseMind, ModMind);
        Output += FString::Printf(TEXT("    Body: %.1f -> %.1f\n"), BaseBody, ModBody);
        Output += FString::Printf(TEXT("    Spirit: %.1f -> %.1f\n"), BaseSpirit, ModSpirit);
    }
    Output += TEXT("\n");

    // Ultimate Override
    if (Evolution->bOverridesUltimate)
    {
        Output += TEXT("ULTIMATE OVERRIDE:\n");
        if (Evolution->EvolutionUltimate)
        {
            Output += FString::Printf(TEXT("  Replaces with: %s\n"), *Evolution->EvolutionUltimate->UltimateName);
        }
        else
        {
            Output += TEXT("  ERROR: No ultimate assigned\n");
        }
        Output += TEXT("\n");
    }

    // Exclusive Spells
    if (Evolution->HasExclusiveSpells())
    {
        Output += FString::Printf(TEXT("EXCLUSIVE SPELLS (%d):\n"), Evolution->GetExclusiveSpellCount());
        for (int32 i = 0; i < Evolution->ExclusiveSpells.Num(); ++i)
        {
            if (Evolution->ExclusiveSpells[i])
            {
                Output += FString::Printf(TEXT("  %d. %s\n"), i + 1, *Evolution->ExclusiveSpells[i]->SpellName);
            }
            else
            {
                Output += FString::Printf(TEXT("  %d. (empty slot)\n"), i + 1);
            }
        }
        Output += TEXT("\n");
    }

    // Passive Effects
    if (Evolution->GetPassiveCount() > 0)
    {
        Output += FString::Printf(TEXT("PASSIVE EFFECTS (%d):\n"), Evolution->GetPassiveCount());
        for (int32 i = 0; i < Evolution->PassiveEffects.Num(); ++i)
        {
            const FPassiveEffect &Passive = Evolution->PassiveEffects[i];
            Output += FString::Printf(TEXT("  %d. %s\n"), i + 1, *Passive.PassiveName);
            Output += FString::Printf(TEXT("     %s\n"), *Passive.GetDescription());
        }
        Output += TEXT("\n");
    }

    Output += TEXT("==========================================\n");

    return Output;
}

void UEvolutionDataDebug::CompareEvolutions(UEvolutionData *Evolution1, UEvolutionData *Evolution2, UCharacterData *Character)
{
    if (!Evolution1 || !Evolution2)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: One or both evolutions are NULL"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== EVOLUTION COMPARISON =========="));
    UE_LOG(LogTemp, Display, TEXT(""));

    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Property"), *Evolution1->EvolutionName, *Evolution2->EvolutionName);
    UE_LOG(LogTemp, Display, TEXT("------------------------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Element"), *Evolution1->GetElementName(), *Evolution2->GetElementName());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Type"), *Evolution1->GetEvolutionTypeName(), *Evolution2->GetEvolutionTypeName());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25.0f%% | %-25.0f%%"), TEXT("Mind Mod"), Evolution1->MindModifierPercent, Evolution2->MindModifierPercent);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25.0f%% | %-25.0f%%"), TEXT("Body Mod"), Evolution1->BodyModifierPercent, Evolution2->BodyModifierPercent);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25.0f%% | %-25.0f%%"), TEXT("Spirit Mod"), Evolution1->SpiritModifierPercent, Evolution2->SpiritModifierPercent);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25d | %-25d"), TEXT("Passives"), Evolution1->GetPassiveCount(), Evolution2->GetPassiveCount());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25d | %-25d"), TEXT("Exclusive Spells"), Evolution1->GetExclusiveSpellCount(), Evolution2->GetExclusiveSpellCount());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Overrides Ult"), Evolution1->HasUltimateOverride() ? TEXT("Yes") : TEXT("No"), Evolution2->HasUltimateOverride() ? TEXT("Yes") : TEXT("No"));

    if (Character)
    {
        bool bCan1 = Evolution1->CanCharacterUse(Character);
        bool bCan2 = Evolution2->CanCharacterUse(Character);
        UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Can Use"), bCan1 ? TEXT("YES") : TEXT("NO"), bCan2 ? TEXT("YES") : TEXT("NO"));
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}