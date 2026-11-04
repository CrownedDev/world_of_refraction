// FResistanceBonus.cpp
// Gear resistance roll — a flat 10-slot (B-358: nine elements + one Physical) zero-sum broken-stick over the shared
// ZeroSumRoll core (the same algorithm the substat roll uses, minus the pillar
// split). Own per-tier budget; results written as percent-point floats.

#include "Equipment/FResistanceBonus.h"
#include "Combat/CombatConstants.h"
#include "Combat/Resistance/ResistanceColumns.h"
#include "Equipment/ZeroSumBrokenStick.h"
#include "Math/RandomStream.h"

namespace
{
	// 10 categories: Fire Water Earth Wind Light Darkness Lightning Void Reality | Physical (B-358, D-148: the
	// Slash / Pierce / Impact fields hold one Physical value, ResistanceColumns::Set).
	constexpr int32 RESISTANCE_SLOT_COUNT = 10;

	void WipeResistance(FResistanceBonus &B)
	{
		for (const EWoRDamageType Type : ResistanceColumns::Types())
		{
			ResistanceColumns::Set(B, Type, 0.0f);
		}
	}

	/** Apply an integer percent-point delta to category Slot (0..RESISTANCE_SLOT_COUNT-1), written as a
	 *  float and clamped to RESISTANCE_BONUS_MIN/MAX. Slot order mirrors the struct. */
	void ApplyResistanceDelta(FResistanceBonus &B, int32 Slot, int32 Delta)
	{
		if (Delta == 0)
		{
			return;
		}
		const float Lo = CombatConstants::RESISTANCE_BONUS_MIN;
		const float Hi = CombatConstants::RESISTANCE_BONUS_MAX;
		if (!ResistanceColumns::Types().IsValidIndex(Slot))
		{
			return;
		}
		const EWoRDamageType Type = ResistanceColumns::Types()[Slot];
		ResistanceColumns::Set(B, Type, FMath::Clamp(ResistanceColumns::Get(B, Type) + static_cast<float>(Delta), Lo, Hi));
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
	return RerollResistance(GetResistanceBudget(Tier));
}

int32 FResistanceBonus::RerollResistance(int32 Budget)
{
	if (Budget <= 0)
	{
		return 0;
	}

	WipeResistance(*this);
	ensureMsgf(ResistanceColumns::Types().Num() == RESISTANCE_SLOT_COUNT,
			   TEXT("RESISTANCE_SLOT_COUNT is out of step with ResistanceColumns::Types(): a new column isn't rolled"));

	FRandomStream RNG(FMath::Rand());
	int32 Deltas[RESISTANCE_SLOT_COUNT] = {0};
	ZeroSumRoll::Distribute(RESISTANCE_SLOT_COUNT, CombatConstants::RESISTANCE_CATEGORY_CAP, Budget, RNG, Deltas);

	for (int32 i = 0; i < RESISTANCE_SLOT_COUNT; ++i)
	{
		ApplyResistanceDelta(*this, i, Deltas[i]);
	}

	return Budget;
}

void FResistanceBonus::Accumulate(const FResistanceBonus &Other)
{
	ResistanceColumns::Add(*this, Other);
}
