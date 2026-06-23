// CrystalEffectTable.cpp
// Out-of-line bodies for CrystalEffectTable helpers whose switch bodies are
// too large to inline in the header. Currently: GetAmberDefensePercent (Amber
// defense tier switch; Emerald/Opal duplicate arms removed — see header).

#include "Equipment/Crystals/CrystalEffectTable.h"

namespace AmberConstants
{
    // Amber defense buff/debuff % (F..S). Directional: ally enhances defense,
    // enemy weakens it (anti-tank). Bespoke curve — Amber is no longer a
    // "stat crystal" sharing the stone x2 table; it's the Earth defense crystal.
    constexpr float DEFENSE_PERCENT[7] = {20, 25, 30, 35, 40, 45, 50};
}

namespace CrystalEffectTable
{
    float GetAmberDefensePercent(const FCrystalId &Id)
    {
        switch (Id.Type)
        {
        // Emerald (speed) intentionally omitted — the live, non-duplicated Emerald
        // source is GetSpeedBuffPercent (header). An Emerald Id falls to the default.

        case ECrystalType::Amber: // Defense
            // Bespoke directional anti-tank curve (20..50, F..S) — Amber's OWN, no
            // longer the shared stone×2 stat-crystal table. Still applied
            // multiplicatively as ×(1 + pct/100) by GetDefenderFlatDefense (ally
            // DefenseBuff enhances reduction, enemy DefenseDebuff cracks it).
            {
                const int32 i = TierHelpers::GetTierValue(Id.Tier);
                if (i < 0 || i >= 7)
                {
                    return 0.0f;
                }
                return AmberConstants::DEFENSE_PERCENT[i];
            }

        // Opal (crit) intentionally omitted — the live, non-duplicated Opal source
        // is GetCritBuffPercent (header). An Opal Id falls to the default below.

        default:
            return 0.0f;
        }
    }
}
