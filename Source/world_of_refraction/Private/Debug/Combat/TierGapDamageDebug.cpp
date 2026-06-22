// TierGapDamageDebug.cpp

#include "Debug/Combat/TierGapDamageDebug.h"
#include "Combat/Damage/TierGapConstants.h"

void UTierGapDamageDebug::PrintTable()
{
    UE_LOG(LogTemp, Display, TEXT("=== Tier-Gap Damage Multipliers (row: action tier, col: channel tier) ==="));

    FString Header = TEXT("Action\\Channel |");
    for (int32 Col = 0; Col <= 6; ++Col)
    {
        Header += FString::Printf(TEXT("   %s  |"),
                                  *TierHelpers::GetTierName(TierHelpers::GetTierFromValue(Col)));
    }
    UE_LOG(LogTemp, Display, TEXT("%s"), *Header);

    for (int32 Row = 0; Row <= 6; ++Row)
    {
        const EItemTier ActionTier = TierHelpers::GetTierFromValue(Row);
        FString Line = FString::Printf(TEXT("       %s       |"), *TierHelpers::GetTierName(ActionTier));
        for (int32 Col = 0; Col <= 6; ++Col)
        {
            const EItemTier ChannelTier = TierHelpers::GetTierFromValue(Col);
            Line += FString::Printf(TEXT(" %.2f |"),
                                    TierGapDamage::GetTierGapDamageMultiplier(ActionTier, ChannelTier));
        }
        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
    }
}

FString UTierGapDamageDebug::GetMultiplierString(EItemTier ActionTier, EItemTier ChannelTier)
{
    const int32 Gap = TierHelpers::GetTierGap(ChannelTier, ActionTier);
    const float Multiplier = TierGapDamage::GetTierGapDamageMultiplier(ActionTier, ChannelTier);
    return FString::Printf(TEXT("Action %s through Channel %s: gap %+d -> x%.2f damage"),
                           *TierHelpers::GetTierDisplayString(ActionTier),
                           *TierHelpers::GetTierDisplayString(ChannelTier),
                           Gap,
                           Multiplier);
}
