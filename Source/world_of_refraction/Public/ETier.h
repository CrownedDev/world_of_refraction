// ETier.h
// Combat equipment tier system (E → S)
// Separate from EItemTier (consumables use F → S)

#pragma once

#include "CoreMinimal.h"
#include "ETier.generated.h"

/**
 * Equipment tier for combat systems
 * Applies to: Rings, Spells, Ultimates, Crystals, Weapons
 * 
 * Break Mechanics:
 * - Same tier = safe (0% break chance)
 * - 1 tier gap = low break chance (15%)
 * - 2 tier gap = moderate (30%)
 * - 3+ tier gap = high to guaranteed
 * - Infusion ALWAYS adds break risk (+10%)
 * - Even S-tier can break when infused (5%)
 */
UENUM(BlueprintType)
enum class ETier : uint8
{
	E UMETA(DisplayName = "E-Tier"),
	D UMETA(DisplayName = "D-Tier"),
	C UMETA(DisplayName = "C-Tier"),
	B UMETA(DisplayName = "B-Tier"),
	A UMETA(DisplayName = "A-Tier"),
	S UMETA(DisplayName = "S-Tier")
};

/**
 * Helper functions for tier calculations
 */
namespace TierHelpers
{
	/** Get numeric value for tier comparisons (E=0, S=5) */
	inline int32 GetTierValue(ETier Tier)
	{
		return static_cast<int32>(Tier);
	}

	/** Calculate tier gap (positive = action is higher tier than equipment) */
	inline int32 GetTierGap(ETier EquipmentTier, ETier ActionTier)
	{
		return GetTierValue(ActionTier) - GetTierValue(EquipmentTier);
	}

	/** Get display name for tier */
	inline FString GetTierName(ETier Tier)
	{
		switch (Tier)
		{
		case ETier::E: return TEXT("E");
		case ETier::D: return TEXT("D");
		case ETier::C: return TEXT("C");
		case ETier::B: return TEXT("B");
		case ETier::A: return TEXT("A");
		case ETier::S: return TEXT("S");
		default: return TEXT("?");
		}
	}

	/** Get full display string (e.g., "B-Tier") */
	inline FString GetTierDisplayString(ETier Tier)
	{
		return FString::Printf(TEXT("%s-Tier"), *GetTierName(Tier));
	}
}
