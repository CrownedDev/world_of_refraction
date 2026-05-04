// BreakCalculator.cpp

#include "BreakCalculator.h"
#include "SpellData.h"
#include "DurabilityConstants.h"

// ============================================================
// DURABILITY WEAR
// ============================================================

int32 UBreakCalculator::CalculateDurabilityWear(
	EItemTier CrystalTier,
	EItemTier ActionTier,
	int32 InfusionLevel,
	bool bIsSpell)
{
	using namespace DurabilityConstants;

	int32 TotalWear = 0;

	// Tier mismatch wear (only when action tier exceeds crystal tier)
	const int32 TierGap = TierHelpers::GetTierGap(CrystalTier, ActionTier);
	if (TierGap > 0)
	{
		TotalWear += TierGap * WEAR_PER_TIER_MISMATCH;
	}

	// Infusion wear (different scale for spells vs ability/attack)
	if (InfusionLevel == 1)
	{
		TotalWear += bIsSpell ? SPELL_L1_WEAR : ABILITY_L1_WEAR;
	}
	else if (InfusionLevel >= 2)
	{
		TotalWear += bIsSpell ? SPELL_L2_WEAR : ABILITY_L2_WEAR;
	}

	return TotalWear;
}

FDurabilityWearResult UBreakCalculator::CalculateDurabilityWearDetailed(
	EItemTier CrystalTier,
	EItemTier ActionTier,
	int32 InfusionLevel,
	bool bIsSpell)
{
	using namespace DurabilityConstants;

	FDurabilityWearResult Result;
	Result.TierGap = TierHelpers::GetTierGap(CrystalTier, ActionTier);

	// Tier mismatch wear
	if (Result.TierGap > 0)
	{
		Result.TierMismatchWear = Result.TierGap * WEAR_PER_TIER_MISMATCH;
	}

	// Infusion wear
	if (InfusionLevel == 1)
	{
		Result.InfusionWear = bIsSpell ? SPELL_L1_WEAR : ABILITY_L1_WEAR;
	}
	else if (InfusionLevel >= 2)
	{
		Result.InfusionWear = bIsSpell ? SPELL_L2_WEAR : ABILITY_L2_WEAR;
	}

	Result.TotalWear = Result.TierMismatchWear + Result.InfusionWear;

	return Result;
}

bool UBreakCalculator::WouldBreakCrystal(int32 CurrentDurability, int32 ProposedWear)
{
	return CurrentDurability > 0 && (CurrentDurability - ProposedWear) <= 0;
}
