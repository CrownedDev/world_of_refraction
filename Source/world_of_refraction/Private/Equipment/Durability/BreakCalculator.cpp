// BreakCalculator.cpp

#include "Equipment/Durability/BreakCalculator.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Durability/DurabilityConstants.h"

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

	// Wear is now a PERCENT of the crystal's max durability (DurabilityWearPercentRework
	// Cluster 2). Accumulate the percent across all terms, then multiply by max-durability
	// ONCE so rounding happens a single time, not per-term.
	float TotalPct = 0.0f;

	// Tier mismatch wear (only when action tier exceeds crystal tier)
	const int32 TierGap = TierHelpers::GetTierGap(CrystalTier, ActionTier);
	if (TierGap > 0)
	{
		TotalPct += TierGap * (bIsSpell ? SPELL_WEAR_PCT_PER_GAP : ABILITY_WEAR_PCT_PER_GAP);
	}

	// Infusion wear (different scale for spells vs ability/attack)
	if (InfusionLevel == 1)
	{
		TotalPct += bIsSpell ? SPELL_L1_INFUSION_PCT : ABILITY_L1_INFUSION_PCT;
	}
	else if (InfusionLevel >= 2)
	{
		TotalPct += bIsSpell ? SPELL_L2_INFUSION_PCT : ABILITY_L2_INFUSION_PCT;
	}

	const int32 MaxDur = GetMaxDurabilityForTier(CrystalTier);
	return FMath::RoundToInt(TotalPct * static_cast<float>(MaxDur));
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

	const int32 MaxDur = GetMaxDurabilityForTier(CrystalTier);
	const float MaxDurF = static_cast<float>(MaxDur);

	// Percent-of-max per component (DurabilityWearPercentRework Cluster 2).
	float MismatchPct = 0.0f;
	if (Result.TierGap > 0)
	{
		MismatchPct = Result.TierGap * (bIsSpell ? SPELL_WEAR_PCT_PER_GAP : ABILITY_WEAR_PCT_PER_GAP);
	}

	float InfusionPct = 0.0f;
	if (InfusionLevel == 1)
	{
		InfusionPct = bIsSpell ? SPELL_L1_INFUSION_PCT : ABILITY_L1_INFUSION_PCT;
	}
	else if (InfusionLevel >= 2)
	{
		InfusionPct = bIsSpell ? SPELL_L2_INFUSION_PCT : ABILITY_L2_INFUSION_PCT;
	}

	// TotalWear single-rounds the combined percent (identical to CalculateDurabilityWear).
	// TierMismatchWear rounds its own component; InfusionWear takes the residual so the
	// broken-out fields ALWAYS sum to TotalWear (no silent divergence in debug/UI readouts).
	Result.TotalWear = FMath::RoundToInt((MismatchPct + InfusionPct) * MaxDurF);
	Result.TierMismatchWear = FMath::RoundToInt(MismatchPct * MaxDurF);
	Result.InfusionWear = Result.TotalWear - Result.TierMismatchWear;

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

	Result.PowerFactor = FMath::Clamp(
		1.0f + (SpellDamageFrac + StatusMultiplierFrac) * SUBSTAT_AMP,
		SUBSTAT_POWER_FACTOR_MIN,
		SUBSTAT_POWER_FACTOR_MAX);

	Result.ControlFactor = FMath::Clamp(
		1.0f + (EfficiencyFrac + ResistanceFrac) * SUBSTAT_AMP,
		SUBSTAT_CONTROL_FACTOR_MIN,
		SUBSTAT_CONTROL_FACTOR_MAX);

	const float BaseAsFloat = static_cast<float>(Result.BaseWear);
	const float Raw = BaseAsFloat * Result.PowerFactor / Result.ControlFactor;
	const float Floor = SUBSTAT_FLOOR_FRAC * BaseAsFloat;

	// Ceiling removed (DurabilityWearPercentRework Cluster 3): with percent-of-max
	// base wear the 45%-of-max single-cast cap is redundant and capped high-tier
	// shatter incorrectly. Shatter is always allowed now, so the one-shot-gap branch
	// (its only job was lifting that ceiling) collapses into this single path.
	const float Bounded = FMath::Max(Floor, Raw);

	Result.FinalWear = FMath::RoundToInt(Bounded);

	// Min-1 mismatch floor: a real tier mismatch always costs >= 1 durability, so a
	// control-stacked caster can't grind mismatch wear down to 0. Gated STRICTLY on
	// TierGap > 0 — matched/over-spec crystals (gap <= 0) keep whatever they computed
	// (a plain matched cast already returned 0 via the BaseWear==0 guard above;
	// infusion-only wear on a matched crystal is NOT floored here, since this is the
	// MISMATCH guarantee, not an infusion guarantee).
	if (Result.TierGap > 0 && Result.FinalWear < 1)
	{
		Result.FinalWear = 1;
	}

	return Result;
}
