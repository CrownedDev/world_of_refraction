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
            // as ×(1 + pct/100) by GetDefenderFlatDefense.
            switch (Id.Tier)
            {
            case EItemTier::F_Tier:
                return 6.0f;
            case EItemTier::E_Tier:
                return 10.0f;
            case EItemTier::D_Tier:
                return 14.0f;
            case EItemTier::C_Tier:
                return 18.0f;
            case EItemTier::B_Tier:
                return 22.0f;
            case EItemTier::A_Tier:
                return 26.0f;
            case EItemTier::S_Tier:
                return 30.0f;
            default:
                return 0.0f;
            }

        // Opal (crit) intentionally omitted — the live, non-duplicated Opal source
        // is GetCritBuffPercent (header). An Opal Id falls to the default below.

        default:
            return 0.0f;
        }
    }
}
