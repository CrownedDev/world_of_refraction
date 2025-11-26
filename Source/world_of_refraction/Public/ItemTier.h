// ItemTier.h
// Unified tier system for items AND equipment (F through S)
// Used by: Items, Rings, Spells, Ultimates, Crystals, Weapons

#pragma once

#include "CoreMinimal.h"
#include "ItemTier.generated.h"

/**
 * Universal tier system from weakest (F) to strongest (S)
 * 
 * For Items: Determines power level and rarity
 * For Equipment: Used in break chance calculations
 * 
 * Break Mechanics (Equipment):
 * - Same tier = safe (0% break chance)
 * - 1 tier gap = low break chance (15%)
 * - 2 tier gap = moderate (30%)
 * - 3+ tier gap = high to guaranteed
 * - Infusion ALWAYS adds break risk (+10%)
 * - Even S-tier can break when infused (5%)
 */
UENUM(BlueprintType)
enum class EItemTier : uint8
{
	F_Tier UMETA(DisplayName = "F-Tier (Weakest)"),
	E_Tier UMETA(DisplayName = "E-Tier"),
	D_Tier UMETA(DisplayName = "D-Tier"),
	C_Tier UMETA(DisplayName = "C-Tier"),
	B_Tier UMETA(DisplayName = "B-Tier"),
	A_Tier UMETA(DisplayName = "A-Tier"),
	S_Tier UMETA(DisplayName = "S-Tier (Strongest)")
};

/**
 * Helper functions for tier calculations
 * Works for both items and equipment break mechanics
 */
namespace TierHelpers
{
	/** Get numeric value for tier comparisons (F=0, S=6) */
	inline int32 GetTierValue(EItemTier Tier)
	{
		return static_cast<int32>(Tier);
	}

	/** Calculate tier gap (positive = action/item is higher tier than equipment) */
	inline int32 GetTierGap(EItemTier EquipmentTier, EItemTier ActionTier)
	{
		return GetTierValue(ActionTier) - GetTierValue(EquipmentTier);
	}

	/** Get short display name for tier (e.g., "B") */
	inline FString GetTierName(EItemTier Tier)
	{
		switch (Tier)
		{
		case EItemTier::F_Tier: return TEXT("F");
		case EItemTier::E_Tier: return TEXT("E");
		case EItemTier::D_Tier: return TEXT("D");
		case EItemTier::C_Tier: return TEXT("C");
		case EItemTier::B_Tier: return TEXT("B");
		case EItemTier::A_Tier: return TEXT("A");
		case EItemTier::S_Tier: return TEXT("S");
		default: return TEXT("?");
		}
	}

	/** Get full display string (e.g., "B-Tier") */
	inline FString GetTierDisplayString(EItemTier Tier)
	{
		return FString::Printf(TEXT("%s-Tier"), *GetTierName(Tier));
	}

	/** Get tier from numeric value */
	inline EItemTier GetTierFromValue(int32 Value)
	{
		Value = FMath::Clamp(Value, 0, 6);
		return static_cast<EItemTier>(Value);
	}

	/** Is this a "high" tier? (A or S) */
	inline bool IsHighTier(EItemTier Tier)
	{
		return Tier >= EItemTier::A_Tier;
	}

	/** Is this a "low" tier? (F or E) */
	inline bool IsLowTier(EItemTier Tier)
	{
		return Tier <= EItemTier::E_Tier;
	}
}
