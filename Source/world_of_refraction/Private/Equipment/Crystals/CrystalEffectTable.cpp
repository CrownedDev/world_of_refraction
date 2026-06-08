// CrystalEffectTable.cpp
// Out-of-line bodies for CrystalEffectTable helpers whose switch bodies are
// too large to inline in the header. Currently: GetBuffPercentage (Amber
// defense tier switch; Emerald/Opal duplicate arms removed — see header).

#include "Equipment/Crystals/CrystalEffectTable.h"

namespace CrystalEffectTable
{
    float GetBuffPercentage(const FCrystalId &Id)
    {
        switch (Id.Type)
        {
        // Emerald (speed) intentionally omitted — the live, non-duplicated Emerald
        // source is GetSpeedBuffPercent (header). An Emerald Id falls to the default.

        case ECrystalType::Amber: // Defense
            // Stone×2 uniform curve (Balance framework). Applied multiplicatively
            // as ×(1 + pct/100) by GetDefenderFlatDefense. Shares the consolidated
            // AugmentStoneConstants curve with Emerald/Opal.
            {
                const int32 i = TierHelpers::GetTierValue(Id.Tier);
                if (i < 0 || i >= 7)
                {
                    return 0.0f;
                }
                return AugmentStoneConstants::STAT_CRYSTAL_BUFF_PERCENT[i];
            }

        // Opal (crit) intentionally omitted — the live, non-duplicated Opal source
        // is GetCritBuffPercent (header). An Opal Id falls to the default below.

        default:
            return 0.0f;
        }
    }
}
