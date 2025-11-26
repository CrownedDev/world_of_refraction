// BreakCalculator.cpp
// Implementation for break chance calculations

#include "BreakCalculator.h"
#include "RingData.h"
#include "SpellData.h"
#include "UltimateData.h"

float UBreakCalculator::CalculateBreakChance(
	ETier EquipmentTier,
	ETier ActionTier,
	bool bWasInfused)
{
	using namespace BreakConstants;

	float BreakChance = 0.0f;

	// Calculate tier gap
	int32 TierGap = TierHelpers::GetTierGap(EquipmentTier, ActionTier);

	// Only add break chance if action tier exceeds equipment tier
	if (TierGap > 0)
	{
		BreakChance += TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion always adds break risk
	if (bWasInfused)
	{
		BreakChance += INFUSION_BREAK_BONUS;

		// Even S-tier equipment can break when infused
		if (EquipmentTier == ETier::S && BreakChance == INFUSION_BREAK_BONUS)
		{
			// Only the infusion bonus applies, but ensure minimum
			BreakChance = FMath::Max(BreakChance, S_TIER_INFUSION_BREAK);
		}
	}

	return FMath::Clamp(BreakChance, 0.0f, 1.0f);
}

FBreakCalculationResult UBreakCalculator::CalculateBreakChanceDetailed(
	ETier EquipmentTier,
	ETier ActionTier,
	bool bWasInfused,
	bool bIsCustomSpell,
	float DurabilityPercent)
{
	using namespace BreakConstants;

	FBreakCalculationResult Result;

	// Calculate tier gap
	Result.TierGap = TierHelpers::GetTierGap(EquipmentTier, ActionTier);

	// Tier mismatch component
	if (Result.TierGap > 0)
	{
		Result.TierMismatchChance = Result.TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion component
	if (bWasInfused)
	{
		Result.InfusionBonus = INFUSION_BREAK_BONUS;

		// S-tier minimum when infused
		if (EquipmentTier == ETier::S && Result.TierMismatchChance == 0.0f)
		{
			Result.InfusionBonus = FMath::Max(Result.InfusionBonus, S_TIER_INFUSION_BREAK);
		}
	}

	// Custom spell component
	if (bIsCustomSpell)
	{
		Result.CustomSpellBonus = CUSTOM_SPELL_BREAK_BONUS;
	}

	// Durability component
	if (DurabilityPercent < 0.25f)
	{
		Result.DurabilityBonus = LOW_DURABILITY_BREAK_BONUS;
	}
	else if (DurabilityPercent < 0.50f)
	{
		Result.DurabilityBonus = MED_DURABILITY_BREAK_BONUS;
	}

	// Total
	Result.TotalBreakChance = Result.TierMismatchChance +
		Result.InfusionBonus +
		Result.CustomSpellBonus +
		Result.DurabilityBonus;

	Result.TotalBreakChance = FMath::Clamp(Result.TotalBreakChance, 0.0f, 1.0f);

	// Check for guaranteed break (very high tier gap)
	Result.bGuaranteedBreak = (Result.TierGap >= 5); // E-tier ring vs S-tier spell

	// Risk level
	Result.RiskLevel = GetRiskLevelString(Result.TotalBreakChance);

	return Result;
}

float UBreakCalculator::GetRingSpellBreakChance(
	URingData* Ring,
	USpellData* Spell,
	bool bInfused)
{
	if (!Ring || !Spell)
	{
		return 0.0f;
	}

	// Get spell tier (TODO: Add Tier to SpellData)
	// For now, use a placeholder based on energy cost
	ETier SpellTier = ETier::E;
	
	// When SpellData has Tier field, replace with:
	// ETier SpellTier = Spell->Tier;

	bool bIsCustom = Ring->IsCustomSpell(Spell);
	float Durability = Ring->GetDurabilityPercent();

	FBreakCalculationResult Result = CalculateBreakChanceDetailed(
		Ring->Tier,
		SpellTier,
		bInfused,
		bIsCustom,
		Durability);

	return Result.TotalBreakChance;
}

float UBreakCalculator::GetRingUltimateBreakChance(
	URingData* Ring,
	UUltimateData* Ultimate,
	bool bInfused)
{
	if (!Ring || !Ultimate)
	{
		return 0.0f;
	}

	// Get ultimate tier (TODO: Add Tier to UltimateData)
	ETier UltimateTier = ETier::E;
	
	// When UltimateData has Tier field, replace with:
	// ETier UltimateTier = Ultimate->Tier;

	float Durability = Ring->GetDurabilityPercent();

	// Ultimates are never "custom" but they're high power
	FBreakCalculationResult Result = CalculateBreakChanceDetailed(
		Ring->Tier,
		UltimateTier,
		bInfused,
		false,
		Durability);

	return Result.TotalBreakChance;
}

bool UBreakCalculator::RollForBreak(float BreakChance)
{
	if (BreakChance <= 0.0f)
	{
		return false;
	}
	if (BreakChance >= 1.0f)
	{
		return true;
	}

	return FMath::FRand() < BreakChance;
}

bool UBreakCalculator::RollForBreakSeeded(float BreakChance, int32 Seed)
{
	if (BreakChance <= 0.0f)
	{
		return false;
	}
	if (BreakChance >= 1.0f)
	{
		return true;
	}

	FRandomStream Stream(Seed);
	return Stream.FRand() < BreakChance;
}

FString UBreakCalculator::GetRiskLevelString(float BreakChance)
{
	if (BreakChance <= 0.0f)
	{
		return TEXT("Safe");
	}
	else if (BreakChance < 0.10f)
	{
		return TEXT("Minimal Risk");
	}
	else if (BreakChance < 0.25f)
	{
		return TEXT("Low Risk");
	}
	else if (BreakChance < 0.40f)
	{
		return TEXT("Moderate Risk");
	}
	else if (BreakChance < 0.60f)
	{
		return TEXT("High Risk");
	}
	else if (BreakChance < 0.80f)
	{
		return TEXT("Very High Risk");
	}
	else if (BreakChance < 1.0f)
	{
		return TEXT("Extreme Risk");
	}
	else
	{
		return TEXT("Guaranteed Break");
	}
}

FLinearColor UBreakCalculator::GetRiskLevelColor(float BreakChance)
{
	if (BreakChance <= 0.0f)
	{
		// Safe - Green
		return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	}
	else if (BreakChance < 0.25f)
	{
		// Low - Yellow-Green
		return FLinearColor(0.5f, 1.0f, 0.0f, 1.0f);
	}
	else if (BreakChance < 0.50f)
	{
		// Moderate - Yellow
		return FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
	}
	else if (BreakChance < 0.75f)
	{
		// High - Orange
		return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
	}
	else
	{
		// Very High / Guaranteed - Red
		return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	}
}

bool UBreakCalculator::IsDangerousBreakChance(float BreakChance)
{
	// Consider anything above 25% as "dangerous"
	return BreakChance >= 0.25f;
}
