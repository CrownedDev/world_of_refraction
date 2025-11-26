// UltimateDataDebug.cpp
// Implementation of UltimateData debug utilities

#include "UltimateDataDebug.h"
#include "Engine/Engine.h"

void UUltimateDataDebug::PrintUltimateStats(UUltimateData *Ultimate, UCharacterData *Character, float Duration)
{
    if (!Ultimate)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: UltimateData is NULL"));
        }
        return;
    }

    FString StatsString = GetUltimateStatsString(Ultimate, Character);

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

void UUltimateDataDebug::LogUltimateStats(UUltimateData *Ultimate, UCharacterData *Character)
{
    if (!Ultimate)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: UltimateData is NULL"));
        return;
    }

    FString StatsString = GetUltimateStatsString(Ultimate, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString UUltimateDataDebug::GetUltimateStatsString(UUltimateData *Ultimate, UCharacterData *Character)
{
    if (!Ultimate)
    {
        return TEXT("ERROR: Invalid Ultimate Data");
    }

    FString Output = TEXT("");
    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("ULTIMATE: %s\n"), *Ultimate->UltimateName);
    Output += TEXT("==========================================\n\n");

    // Identity
    Output += TEXT("IDENTITY:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *Ultimate->GetElementName());
    Output += FString::Printf(TEXT("  Type: %s\n"), *Ultimate->GetUltimateTypeName());
    Output += FString::Printf(TEXT("  Cooldown: %s\n"), *Ultimate->GetCooldownDescription());
    if (!Ultimate->Theme.IsEmpty())
    {
        Output += FString::Printf(TEXT("  Theme: \"%s\"\n"), *Ultimate->Theme);
    }
    Output += TEXT("\n");

    // Requirements
    Output += TEXT("REQUIREMENTS:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *Ultimate->GetElementName());
    if (Ultimate->RequiredWorldMind > 0 || Ultimate->RequiredWorldBody > 0 || Ultimate->RequiredWorldSpirit > 0)
    {
        if (Ultimate->RequiredWorldMind > 0)
        {
            Output += FString::Printf(TEXT("  World Mind: %d\n"), Ultimate->RequiredWorldMind);
        }
        if (Ultimate->RequiredWorldBody > 0)
        {
            Output += FString::Printf(TEXT("  World Body: %d\n"), Ultimate->RequiredWorldBody);
        }
        if (Ultimate->RequiredWorldSpirit > 0)
        {
            Output += FString::Printf(TEXT("  World Spirit: %d\n"), Ultimate->RequiredWorldSpirit);
        }
    }
    else
    {
        Output += TEXT("  World Stats: None\n");
    }

    // Character compatibility check
    if (Character)
    {
        bool bCanUse = Ultimate->CanCharacterUse(Character);
        bool bMeetsElement = Ultimate->MeetsElementRequirement(Character);
        bool bMeetsWorld = Ultimate->MeetsWorldStatRequirements(Character);

        Output += FString::Printf(TEXT("  %s: %s\n"),
                                  *Character->CharacterName,
                                  bCanUse ? TEXT("CAN USE") : TEXT("CANNOT USE"));

        if (!bCanUse)
        {
            if (!bMeetsElement)
            {
                Output += TEXT("    - Element mismatch\n");
            }
            if (!bMeetsWorld)
            {
                Output += TEXT("    - World stat requirements not met\n");
            }
        }
    }
    Output += TEXT("\n");

    // Cost
    Output += TEXT("COST:\n");
    Output += FString::Printf(TEXT("  Energy: %d\n"), Ultimate->EnergyCost);
    Output += TEXT("\n");

    // Primary Effect
    Output += TEXT("PRIMARY EFFECT:\n");
    if (Ultimate->BaseDamage > 0)
    {
        if (Character)
        {
            float CalcDamage = Ultimate->CalculateDamage(Character);
            Output += FString::Printf(TEXT("  Damage: %d (base) -> %.0f (with %s)\n"),
                                      Ultimate->BaseDamage, CalcDamage, *Character->CharacterName);
        }
        else
        {
            Output += FString::Printf(TEXT("  Damage: %d\n"), Ultimate->BaseDamage);
        }
    }
    if (Ultimate->BaseHealing > 0)
    {
        if (Character)
        {
            float CalcHealing = Ultimate->CalculateHealing(Character);
            Output += FString::Printf(TEXT("  Healing: %d (base) -> %.0f (with %s)\n"),
                                      Ultimate->BaseHealing, CalcHealing, *Character->CharacterName);
        }
        else
        {
            Output += FString::Printf(TEXT("  Healing: %d\n"), Ultimate->BaseHealing);
        }
    }
    if (Ultimate->EffectPercentage > 0.0f)
    {
        Output += FString::Printf(TEXT("  Effect: %.0f%% for %d turns\n"),
                                  Ultimate->EffectPercentage, Ultimate->EffectDuration);
    }
    Output += TEXT("\n");

    // Secondary Effect
    if (Ultimate->bHasSecondaryEffect)
    {
        Output += TEXT("SECONDARY EFFECT:\n");
        FString EffectTypeName = UEnum::GetValueAsString(Ultimate->SecondaryEffectType);
        EffectTypeName.RemoveFromStart(TEXT("EAbilityEffectType::"));
        Output += FString::Printf(TEXT("  Type: %s\n"), *EffectTypeName);
        Output += FString::Printf(TEXT("  Value: %d for %d turns\n"),
                                  Ultimate->SecondaryValue, Ultimate->SecondaryDuration);
        Output += TEXT("\n");
    }

    // Status
    if (Ultimate->bAppliesStatus)
    {
        Output += TEXT("STATUS:\n");
        Output += FString::Printf(TEXT("  Buildup: %d (triggers at 100)\n"), Ultimate->StatusBuildup);
        Output += TEXT("\n");
    }

    // Scaling
    Output += TEXT("SCALING:\n");
    Output += FString::Printf(TEXT("  Type: %s\n"), *Ultimate->GetScalingTypeName());

    if (Character && Ultimate->ScalingType == EStatScalingType::Mind)
    {
        float BonusCrit = Ultimate->CalculateBonusCritChance(Character);
        float CritMult = Ultimate->CalculateCritMultiplier(Character);
        float BaseCrit = Character->CalculateCritChance();

        Output += FString::Printf(TEXT("  Base Crit: %.1f%%\n"), BaseCrit);
        Output += FString::Printf(TEXT("  Bonus Crit: +%.1f%%\n"), BonusCrit);
        Output += FString::Printf(TEXT("  Total Crit: %.1f%%\n"), BaseCrit + BonusCrit);
        Output += FString::Printf(TEXT("  Crit Multiplier: %.2fx\n"), CritMult);
    }
    Output += TEXT("\n");

    // Special Flags
    Output += TEXT("SPECIAL:\n");
    Output += FString::Printf(TEXT("  Ignores Defense: %s\n"), Ultimate->bIgnoresDefense ? TEXT("Yes") : TEXT("No"));
    Output += FString::Printf(TEXT("  Can Crit: %s\n"), Ultimate->bCanCrit ? TEXT("Yes") : TEXT("No"));
    Output += FString::Printf(TEXT("  Grants Invuln: %s\n"), Ultimate->bGrantsInvulnerability ? TEXT("Yes") : TEXT("No"));

    Output += TEXT("==========================================\n");

    return Output;
}

void UUltimateDataDebug::LogAllUltimatesByElement(ERefractionElement Element)
{
    FString ElementName = UEnum::GetValueAsString(Element);
    ElementName.RemoveFromStart(TEXT("ERefractionElement::"));

    UE_LOG(LogTemp, Display, TEXT("=========================================="));
    UE_LOG(LogTemp, Display, TEXT("Logging all %s ultimates..."), *ElementName);
    UE_LOG(LogTemp, Display, TEXT("(Requires asset loading - implement with AssetRegistry)"));
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}

void UUltimateDataDebug::CompareUltimates(UUltimateData *Ultimate1, UUltimateData *Ultimate2, UCharacterData *Character)
{
    if (!Ultimate1 || !Ultimate2)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: One or both ultimates are NULL"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== ULTIMATE COMPARISON =========="));
    UE_LOG(LogTemp, Display, TEXT(""));

    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Property"), *Ultimate1->UltimateName, *Ultimate2->UltimateName);
    UE_LOG(LogTemp, Display, TEXT("----------------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Element"), *Ultimate1->GetElementName(), *Ultimate2->GetElementName());
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Type"), *Ultimate1->GetUltimateTypeName(), *Ultimate2->GetUltimateTypeName());
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Scaling"), *Ultimate1->GetScalingTypeName(), *Ultimate2->GetScalingTypeName());
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20d | %-20d"), TEXT("Energy Cost"), Ultimate1->EnergyCost, Ultimate2->EnergyCost);
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20d | %-20d"), TEXT("Base Damage"), Ultimate1->BaseDamage, Ultimate2->BaseDamage);
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20d | %-20d"), TEXT("Base Healing"), Ultimate1->BaseHealing, Ultimate2->BaseHealing);
    UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Cooldown"), *Ultimate1->GetCooldownDescription(), *Ultimate2->GetCooldownDescription());

    if (Character)
    {
        float Dmg1 = Ultimate1->CalculateDamage(Character);
        float Dmg2 = Ultimate2->CalculateDamage(Character);
        bool bCan1 = Ultimate1->CanCharacterUse(Character);
        bool bCan2 = Ultimate2->CanCharacterUse(Character);

        UE_LOG(LogTemp, Display, TEXT("%-25s | %-20.0f | %-20.0f"), TEXT("Calc Damage"), Dmg1, Dmg2);
        UE_LOG(LogTemp, Display, TEXT("%-25s | %-20s | %-20s"), TEXT("Can Use"), bCan1 ? TEXT("YES") : TEXT("NO"), bCan2 ? TEXT("YES") : TEXT("NO"));
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}