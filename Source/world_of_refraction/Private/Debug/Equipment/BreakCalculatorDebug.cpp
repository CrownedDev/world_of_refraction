// BreakCalculatorDebug.cpp

#include "Debug/Equipment/BreakCalculatorDebug.h"

// ============================================================
// DURABILITY WEAR LOGGING
// ============================================================

void UBreakCalculatorDebug::LogDurabilityWear(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    const float Pct = UBreakCalculator::CalculateWearPercentOfMax(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);
    const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);
    const int32 MaxDur = DurabilityConstants::GetMaxDurabilityForTier(CrystalTier);

    UE_LOG(LogTemp, Display, TEXT("========== DURABILITY WEAR (percent-of-max) =========="));
    UE_LOG(LogTemp, Display, TEXT("Crystal: %s (Max %d) | Action: %s | Infusion: L%d | %s"),
           *TierHelpers::GetTierName(CrystalTier), MaxDur,
           *TierHelpers::GetTierName(ActionTier),
           InfusionLevel,
           bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("Wear: %.1f%% of max = %d durability (base, pre-substat)"), Pct * 100.0f, Wear);
    UE_LOG(LogTemp, Display, TEXT("====================================="));
}

void UBreakCalculatorDebug::LogDurabilityWearDetailed(
    EItemTier CrystalTier,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    const FDurabilityWearResult Result = UBreakCalculator::CalculateDurabilityWearDetailed(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);
    const float Pct = UBreakCalculator::CalculateWearPercentOfMax(
        CrystalTier, ActionTier, InfusionLevel, bIsSpell);
    const int32 MaxDur = DurabilityConstants::GetMaxDurabilityForTier(CrystalTier);

    UE_LOG(LogTemp, Display, TEXT("========== DETAILED WEAR (percent-of-max) =========="));
    UE_LOG(LogTemp, Display, TEXT("Crystal Tier:   %s (Max %d)"), *TierHelpers::GetTierName(CrystalTier), MaxDur);
    UE_LOG(LogTemp, Display, TEXT("Action Tier:    %s"), *TierHelpers::GetTierName(ActionTier));
    UE_LOG(LogTemp, Display, TEXT("Infusion Level: L%d"), InfusionLevel);
    UE_LOG(LogTemp, Display, TEXT("Action Type:    %s"), bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("Tier Gap:           %d"), Result.TierGap);
    UE_LOG(LogTemp, Display, TEXT("Wear %% of max:      %.1f%%"), Pct * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("Tier Mismatch Wear: %d dur"), Result.TierMismatchWear);
    UE_LOG(LogTemp, Display, TEXT("Infusion Wear:      %d dur"), Result.InfusionWear);
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("TOTAL WEAR:         %d dur (of %d max)"), Result.TotalWear, MaxDur);
    UE_LOG(LogTemp, Display, TEXT("Note: base/pre-substat; live path applies power/control + min-1 mismatch floor."));
    UE_LOG(LogTemp, Display, TEXT("==============================================="));
}

void UBreakCalculatorDebug::PrintWearTable(EItemTier CrystalTier)
{
    const int32 MaxDurability = DurabilityConstants::GetMaxDurabilityForTier(CrystalTier);

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== WEAR TABLE: %s Crystal (Max %d) — percent-of-max =========="),
           *TierHelpers::GetTierName(CrystalTier), MaxDurability);
    UE_LOG(LogTemp, Display, TEXT("Percent is tier-independent; durability wear = round(pct x Max). Ability infusion adds 0%% (only spells gain infusion wear)."));
    UE_LOG(LogTemp, Display, TEXT("Base/pre-substat (no power/control, no min-1 floor). cb = casts-to-break from full = ceil(Max/wear)."));
    UE_LOG(LogTemp, Display, TEXT("Action |    Ability     |   Spell L0     |   Spell L1     |   Spell L2"));
    UE_LOG(LogTemp, Display, TEXT(" tier  | pct  dur  cb   | pct  dur  cb   | pct  dur  cb   | pct  dur  cb"));
    UE_LOG(LogTemp, Display, TEXT("-------+----------------+----------------+----------------+---------------"));

    const EItemTier Tiers[] = {
        EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier,
        EItemTier::C_Tier, EItemTier::B_Tier, EItemTier::A_Tier,
        EItemTier::S_Tier};

    // Four meaningful variants: ability (infusion adds nothing now), spell L0/L1/L2.
    const int32 VarInf[4] = {0, 0, 1, 2};
    const bool VarSpell[4] = {false, true, true, true};

    for (EItemTier ActionTier : Tiers)
    {
        FString Cells;
        for (int32 i = 0; i < 4; ++i)
        {
            const float P = UBreakCalculator::CalculateWearPercentOfMax(CrystalTier, ActionTier, VarInf[i], VarSpell[i]);
            const int32 W = UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, VarInf[i], VarSpell[i]);
            const FString Cb = (W <= 0) ? TEXT("inf") : FString::Printf(TEXT("%d"), FMath::DivideAndRoundUp(MaxDurability, W));
            Cells += FString::Printf(TEXT(" %3.0f%% %3d %4s |"), P * 100.0f, W, *Cb);
        }

        UE_LOG(LogTemp, Display, TEXT("  %s   |%s"),
               *TierHelpers::GetTierName(ActionTier), *Cells);
    }

    UE_LOG(LogTemp, Display, TEXT("==================================================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
}

FString UBreakCalculatorDebug::GetWearResultString(const FDurabilityWearResult &Result)
{
    // Percent-of-max model: the amounts below are durability points (already pct x Max,
    // rounded). The wear percent itself needs the tier inputs — call
    // UBreakCalculator::CalculateWearPercentOfMax for it (not stored on this struct).
    return FString::Printf(
        TEXT("Wear: %d dur (mismatch %d + infusion %d, gap %d)"),
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
    UE_LOG(LogTemp, Display, TEXT("========== SUBSTAT-MODIFIED WEAR TABLE (percent-of-max) =========="));
    UE_LOG(LogTemp, Display, TEXT("Action: %s tier, Infusion L%d, %s"),
           *TierHelpers::GetTierName(ActionTier),
           InfusionLevel,
           bIsSpell ? TEXT("Spell") : TEXT("Ability/Attack"));
    UE_LOG(LogTemp, Display, TEXT("Caster: SpellDmg=%+.2f StatusMult=%+.2f Efficiency=%+.2f Resistance=%+.2f"),
           SpellDamageFrac, StatusMultiplierFrac, EfficiencyFrac, ResistanceFrac);
    UE_LOG(LogTemp, Display, TEXT("PowerF clamp [%.1f..%.1f], CtrlF clamp [%.1f..%.1f]; no ceiling; min-1 floor on mismatch (gap>0)."),
           DurabilityConstants::SUBSTAT_POWER_FACTOR_MIN, DurabilityConstants::SUBSTAT_POWER_FACTOR_MAX,
           DurabilityConstants::SUBSTAT_CONTROL_FACTOR_MIN, DurabilityConstants::SUBSTAT_CONTROL_FACTOR_MAX);
    UE_LOG(LogTemp, Display, TEXT("Crystal | %%ofMax | Base | PowerF | CtrlF | Final | MaxDur |  cb  | Flag"));
    UE_LOG(LogTemp, Display, TEXT("--------+--------+------+--------+-------+-------+--------+------+--------"));

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

        // Casts-to-break: how many casts before durability reaches 0 (from full).
        // Final<=0 means zero wear — infinite casts. Final>=MaxDur shatters in one cast.
        // FLOORED = min-1 mismatch floor raised this to 1 (would otherwise round to 0).
        FString CastsStr;
        FString FlagStr;
        if (R.FinalWear <= 0)
        {
            CastsStr = TEXT("inf");
            FlagStr = TEXT("NoWear");
        }
        else
        {
            CastsStr = FString::Printf(TEXT("%d"), FMath::DivideAndRoundUp(MaxDur, R.FinalWear));
            if (R.FinalWear >= MaxDur)
            {
                FlagStr = TEXT("SHATTER");
            }
            else if (R.bMinFloored)
            {
                FlagStr = TEXT("FLOORED");
            }
            else
            {
                FlagStr = TEXT("");
            }
        }

        UE_LOG(LogTemp, Display,
               TEXT("   %s   | %5.1f%% |  %3d | %5.2f  | %5.2f |  %3d  |  %3d   | %4s | %s"),
               *TierHelpers::GetTierName(CrystalTier),
               R.WearPercentOfMax * 100.0f,
               R.BaseWear,
               R.PowerFactor,
               R.ControlFactor,
               R.FinalWear,
               MaxDur,
               *CastsStr,
               *FlagStr);
    }

    UE_LOG(LogTemp, Display, TEXT("================================================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
}