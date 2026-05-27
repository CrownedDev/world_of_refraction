// BreakCalculatorDebug.cpp

#include "BreakCalculatorDebug.h"

// ============================================================
// DURABILITY WEAR LOGGING
// ============================================================

void UBreakCalculatorDebug::LogDurabilityWear(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    int32 Wear = UBreakCalculator::CalculateDurabilityWear(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);

    UE_LOG(LogTemp, Display, TEXT("========== DURABILITY WEAR =========="));
    UE_LOG(LogTemp, Display, TEXT("Crystal: %s | Action: %s | Infusion: L%d | %s"),
           *TierHelpers::GetTierName(CrystalTier),
           *TierHelpers::GetTierName(ActionTier),
           InfusionLevel,
           bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("Total Wear: %d"), Wear);
    UE_LOG(LogTemp, Display, TEXT("====================================="));
}

void UBreakCalculatorDebug::LogDurabilityWearDetailed(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    FDurabilityWearResult Result = UBreakCalculator::CalculateDurabilityWearDetailed(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);

    UE_LOG(LogTemp, Display, TEXT("========== DETAILED WEAR CALCULATION =========="));
    UE_LOG(LogTemp, Display, TEXT("Crystal Tier:   %s"), *TierHelpers::GetTierName(CrystalTier));
    UE_LOG(LogTemp, Display, TEXT("Action Tier:    %s"), *TierHelpers::GetTierName(ActionTier));
    UE_LOG(LogTemp, Display, TEXT("Infusion Level: L%d"), InfusionLevel);
    UE_LOG(LogTemp, Display, TEXT("Action Type:    %s"), bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("Tier Gap:           %d"), Result.TierGap);
    UE_LOG(LogTemp, Display, TEXT("Tier Mismatch Wear: %d"), Result.TierMismatchWear);
    UE_LOG(LogTemp, Display, TEXT("Infusion Wear:      %d"), Result.InfusionWear);
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("TOTAL WEAR:         %d"), Result.TotalWear);
    UE_LOG(LogTemp, Display, TEXT("Max Crystal HP:     %d"), DurabilityConstants::GetMaxDurabilityForTier(CrystalTier));
    UE_LOG(LogTemp, Display, TEXT("==============================================="));
}

void UBreakCalculatorDebug::PrintWearTable(EItemTier CrystalTier)
{
    const int32 MaxDurability = DurabilityConstants::GetMaxDurabilityForTier(CrystalTier);

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== WEAR TABLE: %s Crystal (Max %d) =========="),
           *TierHelpers::GetTierName(CrystalTier), MaxDurability);
    UE_LOG(LogTemp, Display, TEXT("Action Tier  | None | L1 Abi | L2 Abi | L1 Spell | L2 Spell"));
    UE_LOG(LogTemp, Display, TEXT("-------------+------+--------+--------+----------+----------"));

    const EItemTier Tiers[] = {
        EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier,
        EItemTier::C_Tier, EItemTier::B_Tier, EItemTier::A_Tier,
        EItemTier::S_Tier};

    for (EItemTier ActionTier : Tiers)
    {
        const int32 None = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, 0, false);
        const int32 L1Abi = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, 1, false);
        const int32 L2Abi = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, 2, false);
        const int32 L1Spell = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, 1, true);
        const int32 L2Spell = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, 2, true);

        UE_LOG(LogTemp, Display, TEXT("    %s        |  %2d  |   %2d   |   %2d   |    %2d    |    %2d"),
               *TierHelpers::GetTierName(ActionTier),
               None, L1Abi, L2Abi, L1Spell, L2Spell);
    }

    UE_LOG(LogTemp, Display, TEXT("==================================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
}

FString UBreakCalculatorDebug::GetWearResultString(const FDurabilityWearResult &Result)
{
    return FString::Printf(
        TEXT("Wear: %d (mismatch: %d, infusion: %d, gap: %d)"),
        Result.TotalWear,
        Result.TierMismatchWear,
        Result.InfusionWear,
        Result.TierGap);
}

// ============================================================
// SUBSTAT-MODIFIED WEAR
// ============================================================

void UBreakCalculatorDebug::PrintWearTableWithSubstats(
    float SpellDamageFrac,
    float StatusMultiplierFrac,
    float EfficiencyFrac,
    float ResistanceFrac,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== SUBSTAT-MODIFIED WEAR TABLE =========="));
    UE_LOG(LogTemp, Display, TEXT("Action: %s tier, Infusion L%d, %s"),
           *TierHelpers::GetTierName(ActionTier),
           InfusionLevel,
           bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("Caster: SpellDmg=%+.2f StatusMult=%+.2f Efficiency=%+.2f Resistance=%+.2f"),
           SpellDamageFrac, StatusMultiplierFrac, EfficiencyFrac, ResistanceFrac);
    UE_LOG(LogTemp, Display, TEXT("Crystal | Base | PowerF | CtrlF | Final | MaxDur | CastsToBreak | Flag"));
    UE_LOG(LogTemp, Display, TEXT("--------+------+--------+-------+-------+--------+--------------+------"));

    const EItemTier Tiers[] = {
        EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier,
        EItemTier::C_Tier, EItemTier::B_Tier, EItemTier::A_Tier,
        EItemTier::S_Tier};

    for (EItemTier CrystalTier : Tiers)
    {
        const FDurabilityWearWithSubstatsResult R =
            UBreakCalculator::CalculateDurabilityWearWithSubstatsDetailed(
                CrystalTier, ActionTier, InfusionLevel, bIsSpell,
                SpellDamageFrac, StatusMultiplierFrac,
                EfficiencyFrac, ResistanceFrac);

        const int32 MaxDur = DurabilityConstants::GetMaxDurabilityForTier(CrystalTier);

        // Casts-to-break: how many casts before durability reaches 0.
        // Final<=0 means zero wear — infinite casts (no overdrive, no infusion).
        // Final>=MaxDur means a single cast shatters the crystal.
        FString CastsStr;
        FString FlagStr;
        if (R.FinalWear <= 0)
        {
            CastsStr = TEXT("inf");
            FlagStr = TEXT("NoWear");
        }
        else
        {
            const int32 Casts = FMath::DivideAndRoundUp(MaxDur, R.FinalWear);
            CastsStr = FString::Printf(TEXT("%d"), Casts);
            FlagStr = (R.FinalWear >= MaxDur) ? TEXT("SHATTER") : TEXT("");
        }

        UE_LOG(LogTemp, Display,
               TEXT("   %s   |  %2d  |  %4.2f  | %4.2f  |  %3d  |  %3d   |     %5s    | %s"),
               *TierHelpers::GetTierName(CrystalTier),
               R.BaseWear,
               R.PowerFactor,
               R.ControlFactor,
               R.FinalWear,
               MaxDur,
               *CastsStr,
               *FlagStr);
    }

    UE_LOG(LogTemp, Display, TEXT("================================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
}