// BreakCalculator.cpp

#include "BreakCalculator.h"
#include "RingData.h"
#include "SpellData.h"
#include "DurabilityConstants.h"

float UBreakCalculator::CalculateBreakChance(EItemTier EquipmentTier, EItemTier ActionTier, bool bWasInfused)
{
	using namespace BreakConstants;

	float BreakChance = 0.0f;

	// Calculate tier gap
	int32 TierGap = TierHelpers::GetTierGap(EquipmentTier, ActionTier);

	// Base break chance from tier mismatch
	if (TierGap > 0)
	{
		BreakChance = TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion always adds break risk
	if (bWasInfused)
	{
		if (EquipmentTier == EItemTier::S_Tier)
		{
			BreakChance += S_TIER_INFUSION_BREAK;
		}
		else
		{
			BreakChance += INFUSION_BREAK_BONUS;
		}
	}

	return FMath::Clamp(BreakChance, 0.0f, 1.0f);
}

FBreakCalculationResult UBreakCalculator::CalculateBreakChanceDetailed(
	EItemTier EquipmentTier,
	EItemTier ActionTier,
	bool bWasInfused,
	float DurabilityPercent)
{
	using namespace BreakConstants;

	FBreakCalculationResult Result;

	// Calculate tier gap
	Result.TierGap = TierHelpers::GetTierGap(EquipmentTier, ActionTier);

	// Base break chance from tier mismatch
	if (Result.TierGap > 0)
	{
		Result.TierMismatchChance = Result.TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion bonus
	if (bWasInfused)
	{
		if (EquipmentTier == EItemTier::S_Tier)
		{
			Result.InfusionBonus = S_TIER_INFUSION_BREAK;
		}
		else
		{
			Result.InfusionBonus = INFUSION_BREAK_BONUS;
		}
	}

	// Durability bonus
	if (DurabilityPercent < LOW_DURABILITY_THRESHOLD)
	{
		Result.DurabilityBonus = LOW_DURABILITY_BREAK_BONUS;
	}
	else if (DurabilityPercent < MED_DURABILITY_THRESHOLD)
	{
		Result.DurabilityBonus = MED_DURABILITY_BREAK_BONUS;
	}

	// Total
	Result.TotalBreakChance = FMath::Clamp(
		Result.TierMismatchChance + Result.InfusionBonus + Result.DurabilityBonus,
		0.0f, 1.0f);

	Result.bGuaranteedBreak = Result.TotalBreakChance >= 1.0f;
	Result.RiskLevel = GetRiskLevelString(Result.TotalBreakChance);

	return Result;
}

float UBreakCalculator::GetRingSpellBreakChance(URingData *Ring, USpellData *Spell, bool bInfused)
{
	if (!Ring || !Spell)
	{
		return 0.0f;
	}

	EItemTier SpellTier = Spell->Tier;

	float Durability = Ring->GetDurabilityPercent();

	FBreakCalculationResult Result = CalculateBreakChanceDetailed(
		Ring->Tier,
		SpellTier,
		bInfused,
		Durability);

	return Result.TotalBreakChance;
}

bool UBreakCalculator::RollForBreak(float BreakChance)
{
	if (BreakChance <= 0.0f)
		return false;
	if (BreakChance >= 1.0f)
		return true;

	return FMath::FRand() < BreakChance;
}

bool UBreakCalculator::RollForBreakSeeded(float BreakChance, int32 Seed)
{
	if (BreakChance <= 0.0f)
		return false;
	if (BreakChance >= 1.0f)
		return true;

	FRandomStream Stream(Seed);
	return Stream.FRand() < BreakChance;
}

FString UBreakCalculator::GetRiskLevelString(float BreakChance)
{
	if (BreakChance <= 0.0f)
		return TEXT("Safe");
	if (BreakChance < 0.15f)
		return TEXT("Low Risk");
	if (BreakChance < 0.30f)
		return TEXT("Moderate Risk");
	if (BreakChance < 0.50f)
		return TEXT("High Risk");
	if (BreakChance < 0.75f)
		return TEXT("Very High Risk");
	if (BreakChance < 1.0f)
		return TEXT("Extreme Risk");
	return TEXT("Guaranteed Break");
}

FLinearColor UBreakCalculator::GetRiskLevelColor(float BreakChance)
{
	if (BreakChance <= 0.0f)
		return FLinearColor::Green;
	if (BreakChance < 0.15f)
		return FLinearColor(0.5f, 1.0f, 0.0f); // Yellow-green
	if (BreakChance < 0.30f)
		return FLinearColor::Yellow;
	if (BreakChance < 0.50f)
		return FLinearColor(1.0f, 0.5f, 0.0f); // Orange
	if (BreakChance < 0.75f)
		return FLinearColor(1.0f, 0.25f, 0.0f); // Red-orange
	return FLinearColor::Red;
}

// ============================================================
// DURABILITY WEAR (new model — coexists with % chance for now)
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
