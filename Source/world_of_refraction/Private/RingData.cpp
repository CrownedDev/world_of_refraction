// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"

ESpellElement URingData::GetRingElement() const
{
    if (SlottedCrystal)
    {
        return SlottedCrystal->GetAssociatedElement();
    }
    return Element;
}

bool URingData::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal;
}

float URingData::CalculateBreakChance(USpellData *Spell, bool bIsInfused) const
{
    using namespace RingBreakConstants;

    if (!Spell || bIsBroken)
    {
        return 0.0f;
    }

    float BreakChance = 0.0f;

    EItemTier SpellTier = Spell->Tier;
    int32 TierGap = TierHelpers::GetTierGap(Tier, SpellTier);

    if (TierGap > 0)
    {
        BreakChance = TierGap * BASE_BREAK_CHANCE_PER_TIER;
    }

    if (bIsInfused)
    {
        if (Tier == EItemTier::S_Tier)
        {
            BreakChance += S_TIER_INFUSION_BREAK;
        }
        else
        {
            BreakChance += INFUSION_BREAK_BONUS;
        }
    }

    if (IsCustomSpell(Spell))
    {
        BreakChance += CUSTOM_SPELL_BREAK_BONUS;
    }

    float DurabilityPercent = GetDurabilityPercent();
    if (DurabilityPercent < LOW_DURABILITY_THRESHOLD)
    {
        BreakChance += LOW_DURABILITY_BREAK_BONUS;
    }
    else if (DurabilityPercent < MED_DURABILITY_THRESHOLD)
    {
        BreakChance += MED_DURABILITY_BREAK_BONUS;
    }

    return FMath::Clamp(BreakChance, 0.0f, 1.0f);
};
