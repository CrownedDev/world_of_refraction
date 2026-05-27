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

// ============================================================
// SUBSTAT-MODIFIED WEAR
// ============================================================

int32 UBreakCalculator::CalculateDurabilityWearWithSubstats(
	EItemTier CrystalTier,
	EItemTier ActionTier,
	int32 InfusionLevel,
	bool bIsSpell,
	float SpellDamageFrac,
	float StatusMultiplierFrac,
	float EfficiencyFrac,
	float ResistanceFrac)
{
	return CalculateDurabilityWearWithSubstatsDetailed(
		CrystalTier, ActionTier, InfusionLevel, bIsSpell,
		SpellDamageFrac, StatusMultiplierFrac,
		EfficiencyFrac, ResistanceFrac).FinalWear;
}

FDurabilityWearWithSubstatsResult UBreakCalculator::CalculateDurabilityWearWithSubstatsDetailed(
	EItemTier CrystalTier,
	EItemTier ActionTier,
	int32 InfusionLevel,
	bool bIsSpell,
	float SpellDamageFrac,
	float StatusMultiplierFrac,
	float EfficiencyFrac,
	float ResistanceFrac)
{
	using namespace DurabilityConstants;

	FDurabilityWearWithSubstatsResult Result;
	Result.BaseWear = CalculateDurabilityWear(CrystalTier, ActionTier, InfusionLevel, bIsSpell);
	Result.TierGap = TierHelpers::GetTierGap(CrystalTier, ActionTier);

	// No overdrive and no infusion — substats cannot manufacture wear out of nothing.
	if (Result.BaseWear == 0)
	{
		Result.PowerFactor = 1.0f;
		Result.ControlFactor = 1.0f;
		Result.FinalWear = 0;
		return Result;
	}

	Result.PowerFactor = FMath::Max(
		SUBSTAT_POWER_FACTOR_MIN,
		1.0f + (SpellDamageFrac + StatusMultiplierFrac) * SUBSTAT_AMP);

	Result.ControlFactor = FMath::Clamp(
		1.0f + (EfficiencyFrac + ResistanceFrac) * SUBSTAT_AMP,
		SUBSTAT_CONTROL_FACTOR_MIN,
		SUBSTAT_CONTROL_FACTOR_MAX);

	const float BaseAsFloat = static_cast<float>(Result.BaseWear);
	const float Raw = BaseAsFloat * Result.PowerFactor / Result.ControlFactor;
	const float Floor = SUBSTAT_FLOOR_FRAC * BaseAsFloat;

	float Bounded;
	if (Result.TierGap >= SUBSTAT_ONE_SHOT_GAP)
	{
		// Extreme tier mismatch — ceiling lifted, shatter is allowed.
		Bounded = FMath::Max(Floor, Raw);
	}
	else
	{
		const float Ceiling = SUBSTAT_CEIL_FRAC *
			static_cast<float>(GetMaxDurabilityForTier(CrystalTier));
		Bounded = FMath::Max(Floor, FMath::Min(Raw, Ceiling));
	}

	Result.FinalWear = FMath::RoundToInt(Bounded);
	return Result;
}
