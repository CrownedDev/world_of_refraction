// FEquipmentStatBonus.cpp
// Shared equipment stat-bonus implementation.

#include "FEquipmentStatBonus.h"

int32 FEquipmentStatBonus::GetBudget(EItemTier Tier)
{
    // Inline table — separate EquipmentBonusConstants module not yet present.
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return 6;
    case EItemTier::E_Tier:
        return 10;
    case EItemTier::D_Tier:
        return 15;
    case EItemTier::C_Tier:
        return 21;
    case EItemTier::B_Tier:
        return 28;
    case EItemTier::A_Tier:
        return 36;
    case EItemTier::S_Tier:
        return 45;
    default:
        return 0;
    }
}

int32 FEquipmentStatBonus::GetTotalSpent() const
{
    // BonusCritChance is float — rounded for the capacity-point sum.
    return BonusRawDamage +
           BonusSpellDamage +
           BonusEfficiency +
           BonusStatusMultiplier +
           FMath::RoundToInt(BonusCritChance) +
           BonusSpellSpeed +
           BonusDefense +
           BonusActionSpeed +
           BonusMaxHP +
           BonusMaxEnergy +
           BonusResistance +
           BonusTurnSpeed +
           BonusLuck;
}

int32 FEquipmentStatBonus::GetRemainingCapacity(EItemTier Tier) const
{
    return GetBudget(Tier) - GetTotalSpent();
}

bool FEquipmentStatBonus::IsAtCap(EItemTier Tier) const
{
    return GetTotalSpent() >= GetBudget(Tier);
}

void FEquipmentStatBonus::AddPendingPoints(int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    PendingPoints += Amount;
}

void FEquipmentStatBonus::SpendPendingPoints(EItemTier Tier)
{
    // TODO: implement the capped broken-stick distribution.
    //
    // Open design questions (do NOT implement without sign-off):
    //  - RNG source: FMath::Rand vs FRandomStream-with-seed (determinism for
    //    save/load? Replay safety?).
    //  - Per-stat cap rounding: floor(budget * 0.4), ceil, or round?
    //    F-tier budget 6 * 0.4 = 2.4 — different rounding flips between 2 and 3.
    //  - Reroll trigger: "at cap" means GetTotalSpent() >= GetBudget(Tier)?
    //    Or all 13 fields at per-stat cap? Behaviour differs sharply.
    //  - Reroll behaviour: wipe all 13 and redistribute fresh, or keep
    //    spent and re-attempt the remainder?
    //  - Locked-field handling: are individual fields ever locked, or only
    //    the whole struct via bLocked?
    //  - Pending-overflow: when PendingPoints exceeds GetRemainingCapacity
    //    after caps, do we discard, refund, or queue for next call?
    //
    // Until resolved, this is a no-op; PendingPoints is left untouched so
    // accidental early calls don't silently lose data.
    (void)Tier;
}
