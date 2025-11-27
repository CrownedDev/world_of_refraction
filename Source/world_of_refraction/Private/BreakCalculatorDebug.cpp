// BreakCalculatorDebug.cpp

#include "BreakCalculatorDebug.h"
#include "RingData.h"
#include "SpellData.h"

void UBreakCalculatorDebug::LogBreakChance(EItemTier EquipmentTier, EItemTier ActionTier, bool bInfused)
{
    float Chance = UBreakCalculator::CalculateBreakChance(EquipmentTier, ActionTier, bInfused);

    UE_LOG(LogTemp, Display, TEXT("========== BREAK CHANCE =========="));
    UE_LOG(LogTemp, Display, TEXT("Equipment: %s | Action: %s | Infused: %s"),
           *TierHelpers::GetTierName(EquipmentTier),
           *TierHelpers::GetTierName(ActionTier),
           bInfused ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT("Break Chance: %.1f%%"), Chance * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("=================================="));
}

void UBreakCalculatorDebug::LogBreakChanceDetailed(EItemTier EquipmentTier, EItemTier ActionTier, bool bInfused, bool bIsCustomSpell, float DurabilityPercent)
{
    FBreakCalculationResult Result = UBreakCalculator::CalculateBreakChanceDetailed(
        EquipmentTier, ActionTier, bInfused, bIsCustomSpell, DurabilityPercent);

    UE_LOG(LogTemp, Display, TEXT("========== DETAILED BREAK CALCULATION =========="));
    UE_LOG(LogTemp, Display, TEXT("Equipment Tier: %s"), *TierHelpers::GetTierName(EquipmentTier));
    UE_LOG(LogTemp, Display, TEXT("Action Tier: %s"), *TierHelpers::GetTierName(ActionTier));
    UE_LOG(LogTemp, Display, TEXT("Infused: %s | Custom: %s | Durability: %.0f%%"),
           bInfused ? TEXT("Yes") : TEXT("No"),
           bIsCustomSpell ? TEXT("Yes") : TEXT("No"),
           DurabilityPercent * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("Tier Gap: %d"), Result.TierGap);
    UE_LOG(LogTemp, Display, TEXT("Tier Mismatch: +%.1f%%"), Result.TierMismatchChance * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("Infusion Bonus: +%.1f%%"), Result.InfusionBonus * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("Custom Spell: +%.1f%%"), Result.CustomSpellBonus * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("Durability: +%.1f%%"), Result.DurabilityBonus * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("TOTAL: %.1f%% [%s]"),
           Result.TotalBreakChance * 100.0f,
           *Result.RiskLevel);
    if (Result.bGuaranteedBreak)
    {
        UE_LOG(LogTemp, Warning, TEXT("*** GUARANTEED BREAK ***"));
    }
    UE_LOG(LogTemp, Display, TEXT("================================================"));
}

void UBreakCalculatorDebug::LogRingSpellBreak(URingData *Ring, USpellData *Spell, bool bInfused)
{
    if (!Ring || !Spell)
    {
        UE_LOG(LogTemp, Error, TEXT("LogRingSpellBreak: Ring or Spell is NULL"));
        return;
    }

    float Chance = UBreakCalculator::GetRingSpellBreakChance(Ring, Spell, bInfused);

    UE_LOG(LogTemp, Display, TEXT("========== RING + SPELL BREAK =========="));
    UE_LOG(LogTemp, Display, TEXT("Ring: %s (%s)"), *Ring->RingName, *TierHelpers::GetTierName(Ring->Tier));
    UE_LOG(LogTemp, Display, TEXT("Spell: %s (%s)"), *Spell->SpellName, *TierHelpers::GetTierName(Spell->Tier));
    UE_LOG(LogTemp, Display, TEXT("Infused: %s | Custom: %s"),
           bInfused ? TEXT("Yes") : TEXT("No"),
           Ring->IsCustomSpell(Spell) ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT("Durability: %.0f%%"), Ring->GetDurabilityPercent() * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("----------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("Break Chance: %.1f%%"), Chance * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("========================================"));
}
}

void UBreakCalculatorDebug::PrintBreakTable(EItemTier EquipmentTier)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== BREAK TABLE: %s Equipment =========="), *TierHelpers::GetTierName(EquipmentTier));
    UE_LOG(LogTemp, Display, TEXT("Action Tier | No Infuse | Infused"));
    UE_LOG(LogTemp, Display, TEXT("-----------+-----------+--------"));

    for (int32 i = 0; i < static_cast<int32>(EItemTier::S_Tier) + 1; ++i)
    {
        EItemTier ActionTier = static_cast<EItemTier>(i);
        float NoInfuse = UBreakCalculator::CalculateBreakChance(EquipmentTier, ActionTier, false);
        float Infused = UBreakCalculator::CalculateBreakChance(EquipmentTier, ActionTier, true);

        UE_LOG(LogTemp, Display, TEXT("    %s      |   %5.1f%%  |  %5.1f%%"),
               *TierHelpers::GetTierName(ActionTier),
               NoInfuse * 100.0f,
               Infused * 100.0f);
    }

    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
}

FString UBreakCalculatorDebug::GetBreakResultString(const FBreakCalculationResult &Result)
{
    FString Output;
    Output += FString::Printf(TEXT("Break: %.1f%% [%s]\n"), Result.TotalBreakChance * 100.0f, *Result.RiskLevel);
    Output += FString::Printf(TEXT("  Tier Gap: %d (+%.1f%%)\n"), Result.TierGap, Result.TierMismatchChance * 100.0f);
    if (Result.InfusionBonus > 0.0f)
    {
        Output += FString::Printf(TEXT("  Infusion: +%.1f%%\n"), Result.InfusionBonus * 100.0f);
    }
    if (Result.CustomSpellBonus > 0.0f)
    {
        Output += FString::Printf(TEXT("  Custom: +%.1f%%\n"), Result.CustomSpellBonus * 100.0f);
    }
    if (Result.DurabilityBonus > 0.0f)
    {
        Output += FString::Printf(TEXT("  Durability: +%.1f%%\n"), Result.DurabilityBonus * 100.0f);
    }
    if (Result.bGuaranteedBreak)
    {
        Output += TEXT("  *** GUARANTEED BREAK ***\n");
    }
    return Output;
}