// FResistanceBonus.cpp
// Gear resistance roll — a flat 12-slot zero-sum broken-stick over the shared
// ZeroSumRoll core (the same algorithm the substat roll uses, minus the pillar
// split). Own per-tier budget; results written as percent-point floats.

#include "Equipment/FResistanceBonus.h"
#include "Combat/CombatConstants.h"
#include "Equipment/ZeroSumBrokenStick.h"
#include "Math/RandomStream.h"

namespace
{
	// 12 categories: Fire Water Earth Wind Light Darkness Lightning Void Reality
	// | Slash Pierce Impact (mirrors FResistanceRow / FResistanceBonus field order).
	constexpr int32 RESISTANCE_SLOT_COUNT = 12;

	void WipeResistance(FResistanceBonus &B)
	{
		B.Fire = 0.0f;  B.Water = 0.0f;     B.Earth = 0.0f;   B.Wind = 0.0f;
		B.Light = 0.0f; B.Darkness = 0.0f;  B.Lightning = 0.0f;
		B.Void = 0.0f;  B.Reality = 0.0f;   B.Slash = 0.0f;   B.Pierce = 0.0f;  B.Impact = 0.0f;
	}

	/** Apply an integer percent-point delta to category Slot (0..11), written as a
	 *  float and clamped to RESISTANCE_BONUS_MIN/MAX. Slot order mirrors the struct. */
	void ApplyResistanceDelta(FResistanceBonus &B, int32 Slot, int32 Delta)
	{
		if (Delta == 0)
		{
			return;
		}
		const float Lo = CombatConstants::RESISTANCE_BONUS_MIN;
		const float Hi = CombatConstants::RESISTANCE_BONUS_MAX;
		const float D  = static_cast<float>(Delta);
		switch (Slot)
		{
		case 0:  B.Fire      = FMath::Clamp(B.Fire      + D, Lo, Hi); return;
		case 1:  B.Water     = FMath::Clamp(B.Water     + D, Lo, Hi); return;
		case 2:  B.Earth     = FMath::Clamp(B.Earth     + D, Lo, Hi); return;
		case 3:  B.Wind      = FMath::Clamp(B.Wind      + D, Lo, Hi); return;
		case 4:  B.Light     = FMath::Clamp(B.Light     + D, Lo, Hi); return;
		case 5:  B.Darkness  = FMath::Clamp(B.Darkness  + D, Lo, Hi); return;
		case 6:  B.Lightning = FMath::Clamp(B.Lightning + D, Lo, Hi); return;
		case 7:  B.Void      = FMath::Clamp(B.Void      + D, Lo, Hi); return;
		case 8:  B.Reality   = FMath::Clamp(B.Reality   + D, Lo, Hi); return;
		case 9:  B.Slash     = FMath::Clamp(B.Slash     + D, Lo, Hi); return;
		case 10: B.Pierce    = FMath::Clamp(B.Pierce    + D, Lo, Hi); return;
		case 11: B.Impact    = FMath::Clamp(B.Impact    + D, Lo, Hi); return;
		}
	}
}

int32 FResistanceBonus::GetResistanceBudget(EItemTier Tier)
{
	switch (Tier)
	{
	case EItemTier::F_Tier: return CombatConstants::RESISTANCE_BUDGET_F;
	case EItemTier::E_Tier: return CombatConstants::RESISTANCE_BUDGET_E;
	case EItemTier::D_Tier: return CombatConstants::RESISTANCE_BUDGET_D;
	case EItemTier::C_Tier: return CombatConstants::RESISTANCE_BUDGET_C;
	case EItemTier::B_Tier: return CombatConstants::RESISTANCE_BUDGET_B;
	case EItemTier::A_Tier: return CombatConstants::RESISTANCE_BUDGET_A;
	case EItemTier::S_Tier: return CombatConstants::RESISTANCE_BUDGET_S;
	default:                return 0;
	}
}

int32 FResistanceBonus::RerollResistance(EItemTier Tier)
{
	const int32 Budget = GetResistanceBudget(Tier);
	if (Budget <= 0)
	{
		return 0;
	}

	WipeResistance(*this);

	FRandomStream RNG(FMath::Rand());
	int32 Deltas[RESISTANCE_SLOT_COUNT] = {0};
	ZeroSumRoll::Distribute(RESISTANCE_SLOT_COUNT, CombatConstants::RESISTANCE_CATEGORY_CAP, Budget, RNG, Deltas);

	for (int32 i = 0; i < RESISTANCE_SLOT_COUNT; ++i)
	{
		ApplyResistanceDelta(*this, i, Deltas[i]);
	}

	return Budget;
}
