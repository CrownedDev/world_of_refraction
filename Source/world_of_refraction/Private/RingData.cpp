// RingData.cpp

#include "RingData.h"
#include "SpellData.h"

float URingData::CalculateBreakChance(USpellData *Spell, bool bIsInfused) const
{
	using namespace RingBreakConstants;

	if (!Spell || bIsBroken)
	{
		return 0.0f;
	}

	float BreakChance = 0.0f;

	// Get spell tier (assuming SpellData will have Tier field)
	// For now, use E_Tier as default if not implemented
	EItemTier SpellTier = Spell->Tier;
	// TODO: SpellTier = Spell->Tier; // Uncomment when SpellData has Tier

	// Calculate tier gap
	int32 TierGap = TierHelpers::GetTierGap(Tier, SpellTier);

	// Base break chance from tier mismatch (only if spell is higher tier)
	if (TierGap > 0)
	{
		BreakChance = TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion always adds break risk
	if (bIsInfused)
	{
		// S-tier equipment has reduced infusion risk
		if (Tier == EItemTier::S_Tier)
		{
			BreakChance += S_TIER_INFUSION_BREAK;
		}
		else
		{
			BreakChance += INFUSION_BREAK_BONUS;
		}
	}

	// Custom spells have additional break risk
	if (IsCustomSpell(Spell))
	{
		BreakChance += CUSTOM_SPELL_BREAK_BONUS;
	}

	// Low durability increases break chance
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
}
