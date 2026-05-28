// CrystalEffectTable.cpp
// Out-of-line bodies for CrystalEffectTable helpers whose switch bodies are
// too large to inline in the header. Currently: GetBuffPercentage (nested
// type+tier switches across Emerald/Amber/Opal).

#include "Equipment/Crystals/CrystalEffectTable.h"

namespace CrystalEffectTable
{
    float GetBuffPercentage(const FCrystalId &Id)
    {
        switch (Id.Type)
        {
        case ECrystalType::Emerald: // Attack Speed
            switch (Id.Tier)
            {
            case EItemTier::F_Tier:
                return 10.0f;
            case EItemTier::E_Tier:
                return 15.0f;
            case EItemTier::D_Tier:
                return 20.0f;
            case EItemTier::C_Tier:
                return 25.0f;
            case EItemTier::B_Tier:
                return 30.0f;
            case EItemTier::A_Tier:
                return 35.0f;
            case EItemTier::S_Tier:
                return 40.0f;
            default:
                return 0.0f;
            }

        case ECrystalType::Amber: // Defense
            switch (Id.Tier)
            {
            case EItemTier::F_Tier:
                return 15.0f;
            case EItemTier::E_Tier:
                return 20.0f;
            case EItemTier::D_Tier:
                return 25.0f;
            case EItemTier::C_Tier:
                return 30.0f;
            case EItemTier::B_Tier:
                return 35.0f;
            case EItemTier::A_Tier:
                return 40.0f;
            case EItemTier::S_Tier:
                return 50.0f;
            default:
                return 0.0f;
            }

        case ECrystalType::Opal: // Crit Chance
            switch (Id.Tier)
            {
            case EItemTier::F_Tier:
                return 5.0f;
            case EItemTier::E_Tier:
                return 8.0f;
            case EItemTier::D_Tier:
                return 10.0f;
            case EItemTier::C_Tier:
                return 12.0f;
            case EItemTier::B_Tier:
                return 15.0f;
            case EItemTier::A_Tier:
                return 18.0f;
            case EItemTier::S_Tier:
                return 20.0f;
            default:
                return 0.0f;
            }

        default:
            return 0.0f;
        }
    }
}
