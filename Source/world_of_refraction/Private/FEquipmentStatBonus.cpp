// FEquipmentStatBonus.cpp
// Shared equipment stat-bonus implementation. Zero-sum broken-stick
// distribution lets a single Spend call land both positive AND negative
// values on individual substats, so long as the net signed sum equals the
// points spent. See header comments for the capped-pool / reroll model.

#include "FEquipmentStatBonus.h"
#include "FPillarWeights.h"
#include "CombatConstants.h"
#include "EquipmentBonusGenerator.h" // EquipmentBonusGen::GetSubstatBudget / GetPillarBudget

namespace
{
    // ==================== PILLAR SUBSTAT TABLES ====================
    // 4 Mind / 4 Body / 5 Spirit fields. Pointer-to-member-style accessors
    // would be nicer but they're awkward across int32 + float fields, so we
    // use small dispatch lambdas keyed by integer indices.

    enum class EPillar : uint8 { Mind, Body, Spirit };

    constexpr int32 MIND_SLOT_COUNT   = 4;
    constexpr int32 BODY_SLOT_COUNT   = 4;
    constexpr int32 SPIRIT_SLOT_COUNT = 5;
    constexpr int32 MAX_PILLAR_SLOTS  = 5;

    int32 SlotsInPillar(EPillar Pillar)
    {
        switch (Pillar)
        {
        case EPillar::Mind:   return MIND_SLOT_COUNT;
        case EPillar::Body:   return BODY_SLOT_COUNT;
        case EPillar::Spirit: return SPIRIT_SLOT_COUNT;
        }
        return 0;
    }

    int32 ReadSubstatRoundedInt(const FEquipmentStatBonus &Bonus, EPillar Pillar, int32 SlotIndex)
    {
        switch (Pillar)
        {
        case EPillar::Mind:
            switch (SlotIndex)
            {
            case 0: return Bonus.BonusEfficiency;
            case 1: return Bonus.BonusSpellDamage;
            case 2: return FMath::RoundToInt(Bonus.BonusCritChance);
            case 3: return Bonus.BonusSpellSpeed;
            }
            break;
        case EPillar::Body:
            switch (SlotIndex)
            {
            case 0: return Bonus.BonusRawDamage;
            case 1: return Bonus.BonusDefense;
            case 2: return Bonus.BonusActionSpeed;
            case 3: return Bonus.BonusMaxHP;
            }
            break;
        case EPillar::Spirit:
            switch (SlotIndex)
            {
            case 0: return Bonus.BonusMaxEnergy;
            case 1: return Bonus.BonusResistance;
            case 2: return Bonus.BonusTurnSpeed;
            case 3: return Bonus.BonusLuck;
            case 4: return Bonus.BonusStatusMultiplier;
            }
            break;
        }
        return 0;
    }

    /** Apply a signed delta to the substat at (Pillar, SlotIndex). The
     *  resulting field value is clamped to ±CRYSTAL_BONUS_MAX. */
    void ApplySubstatDelta(FEquipmentStatBonus &Bonus, EPillar Pillar, int32 SlotIndex, int32 Delta)
    {
        if (Delta == 0)
        {
            return;
        }
        const int32 Lo = CombatConstants::CRYSTAL_BONUS_MIN;
        const int32 Hi = CombatConstants::CRYSTAL_BONUS_MAX;

        switch (Pillar)
        {
        case EPillar::Mind:
            switch (SlotIndex)
            {
            case 0: Bonus.BonusEfficiency  = FMath::Clamp(Bonus.BonusEfficiency  + Delta, Lo, Hi); return;
            case 1: Bonus.BonusSpellDamage = FMath::Clamp(Bonus.BonusSpellDamage + Delta, Lo, Hi); return;
            case 2: Bonus.BonusCritChance  = FMath::Clamp(Bonus.BonusCritChance  + static_cast<float>(Delta),
                                                          static_cast<float>(Lo), static_cast<float>(Hi)); return;
            case 3: Bonus.BonusSpellSpeed  = FMath::Clamp(Bonus.BonusSpellSpeed  + Delta, Lo, Hi); return;
            }
            break;
        case EPillar::Body:
            switch (SlotIndex)
            {
            case 0: Bonus.BonusRawDamage   = FMath::Clamp(Bonus.BonusRawDamage   + Delta, Lo, Hi); return;
            case 1: Bonus.BonusDefense     = FMath::Clamp(Bonus.BonusDefense     + Delta, Lo, Hi); return;
            case 2: Bonus.BonusActionSpeed = FMath::Clamp(Bonus.BonusActionSpeed + Delta, Lo, Hi); return;
            case 3: Bonus.BonusMaxHP       = FMath::Clamp(Bonus.BonusMaxHP       + Delta, Lo, Hi); return;
            }
            break;
        case EPillar::Spirit:
            switch (SlotIndex)
            {
            case 0: Bonus.BonusMaxEnergy        = FMath::Clamp(Bonus.BonusMaxEnergy        + Delta, Lo, Hi); return;
            case 1: Bonus.BonusResistance       = FMath::Clamp(Bonus.BonusResistance       + Delta, Lo, Hi); return;
            case 2: Bonus.BonusTurnSpeed        = FMath::Clamp(Bonus.BonusTurnSpeed        + Delta, Lo, Hi); return;
            case 3: Bonus.BonusLuck             = FMath::Clamp(Bonus.BonusLuck             + Delta, Lo, Hi); return;
            case 4: Bonus.BonusStatusMultiplier = FMath::Clamp(Bonus.BonusStatusMultiplier + Delta, Lo, Hi); return;
            }
            break;
        }
    }

    /** Zero all 13 substat bonus fields — used by reroll. */
    void WipeSubstats(FEquipmentStatBonus &Bonus)
    {
        Bonus.BonusRawDamage        = 0;
        Bonus.BonusSpellDamage      = 0;
        Bonus.BonusEfficiency       = 0;
        Bonus.BonusStatusMultiplier = 0;
        Bonus.BonusCritChance       = 0.0f;
        Bonus.BonusSpellSpeed       = 0;
        Bonus.BonusDefense          = 0;
        Bonus.BonusActionSpeed      = 0;
        Bonus.BonusMaxHP            = 0;
        Bonus.BonusMaxEnergy        = 0;
        Bonus.BonusResistance       = 0;
        Bonus.BonusTurnSpeed        = 0;
        Bonus.BonusLuck             = 0;
    }

    /** Zero-sum distribute Share signed-net points across one pillar's
     *  substat slots. Internally generates a random "negative offset" pool
     *  on top of Share, then distributes (Share + Neg) positives and Neg
     *  negatives via broken-stick. Each per-slot |delta| capped at
     *  max(1, Share × SUBSTAT_CAP_FRACTION). Final field value clamped to
     *  CRYSTAL_BONUS_MIN/MAX by ApplySubstatDelta. Returns the net signed
     *  change actually applied (will equal Share unless cap pressure
     *  caused points to drop). */
    int32 DistributePillarSubstatsZeroSum(FEquipmentStatBonus &Bonus, EPillar Pillar, int32 Share, FRandomStream &RNG)
    {
        if (Share <= 0)
        {
            return 0;
        }
        const int32 SlotCount  = SlotsInPillar(Pillar);
        const int32 PerSlotCap = FMath::Max(1, FMath::FloorToInt(Share * CombatConstants::SUBSTAT_CAP_FRACTION));

        // Random negative offset: 0..Share/2. With weights (1,1,1) on F-tier this
        // resolves to 0 most of the time (Share=2 → max_neg=1), so low-tier rolls
        // stay mostly positive while higher tiers see meaningful negative spread.
        const int32 MaxNeg = Share / 2;
        const int32 NegPool = (MaxNeg > 0) ? RNG.RandRange(0, MaxNeg) : 0;
        int32 RemainingPos = Share + NegPool;
        int32 RemainingNeg = NegPool;

        int32 SlotDeltas[MAX_PILLAR_SLOTS] = {0};

        auto PickSlot = [&](int32 SignWanted) -> int32
        {
            // 20 random retries, then linear-scan fallback. SignWanted is +1
            // (want a slot whose delta can still grow) or -1 (whose delta can
            // still shrink).
            for (int32 Retry = 0; Retry < 20; ++Retry)
            {
                const int32 Cand = RNG.RandRange(0, SlotCount - 1);
                const bool bFits = (SignWanted > 0)
                    ? (SlotDeltas[Cand] < PerSlotCap)
                    : (SlotDeltas[Cand] > -PerSlotCap);
                if (bFits)
                {
                    return Cand;
                }
            }
            for (int32 i = 0; i < SlotCount; ++i)
            {
                const bool bFits = (SignWanted > 0)
                    ? (SlotDeltas[i] < PerSlotCap)
                    : (SlotDeltas[i] > -PerSlotCap);
                if (bFits)
                {
                    return i;
                }
            }
            return -1;
        };

        while (RemainingPos > 0)
        {
            const int32 Slot = PickSlot(+1);
            if (Slot < 0) { break; }
            SlotDeltas[Slot] += 1;
            --RemainingPos;
        }
        while (RemainingNeg > 0)
        {
            const int32 Slot = PickSlot(-1);
            if (Slot < 0) { break; }
            SlotDeltas[Slot] -= 1;
            --RemainingNeg;
        }

        int32 NetApplied = 0;
        for (int32 i = 0; i < SlotCount; ++i)
        {
            ApplySubstatDelta(Bonus, Pillar, i, SlotDeltas[i]);
            NetApplied += SlotDeltas[i];
        }
        return NetApplied;
    }

    /** Internal substat distribution body — shared by Spend (pending → 0)
     *  and Reroll (wipe + redistribute full tier budget). Returns net
     *  points distributed. */
    int32 SpendOrRerollSubstatsCore(FEquipmentStatBonus &Bonus, EItemTier Tier, const FPillarWeights &Weights, int32 PointsToDistribute, FRandomStream &RNG)
    {
        if (PointsToDistribute <= 0)
        {
            return 0;
        }
        const float TotalW = Weights.Total();
        const float WMind   = TotalW > 0.0f ? (Weights.Mind   / TotalW) : (1.0f / 3.0f);
        const float WBody   = TotalW > 0.0f ? (Weights.Body   / TotalW) : (1.0f / 3.0f);

        const int32 MindShare   = FMath::FloorToInt(PointsToDistribute * WMind);
        const int32 BodyShare   = FMath::FloorToInt(PointsToDistribute * WBody);
        const int32 SpiritShare = PointsToDistribute - MindShare - BodyShare; // remainder absorbs rounding

        int32 Net = 0;
        Net += DistributePillarSubstatsZeroSum(Bonus, EPillar::Mind,   MindShare,   RNG);
        Net += DistributePillarSubstatsZeroSum(Bonus, EPillar::Body,   BodyShare,   RNG);
        Net += DistributePillarSubstatsZeroSum(Bonus, EPillar::Spirit, SpiritShare, RNG);
        return Net;
    }

    /** Zero-sum distribution across the 3 pillar percent fields. Mirrors
     *  the substat algorithm but operates in floats and applies the
     *  PILLAR_CAP_FRACTION / PILLAR_MODIFIER_MIN/MAX clamps. */
    float DistributePillarPercentsZeroSum(FEquipmentStatBonus &Bonus, float Share, FRandomStream &RNG)
    {
        if (Share <= 0.0f)
        {
            return 0.0f;
        }
        const float PerPillarCap = FMath::Max(SMALL_NUMBER, Share * CombatConstants::PILLAR_CAP_FRACTION);

        // Random negative offset on top of Share so net signed sum = Share but
        // individual pillars can flip negative.
        const float NegPool = RNG.FRandRange(0.0f, Share * 0.5f);
        float RemainingPos = Share + NegPool;
        float RemainingNeg = NegPool;

        // Track per-pillar delta to enforce per-pillar |delta| cap.
        float Delta[3] = {0.0f, 0.0f, 0.0f};

        auto PickPillar = [&](int32 SignWanted) -> int32
        {
            for (int32 Retry = 0; Retry < 20; ++Retry)
            {
                const int32 Cand = RNG.RandRange(0, 2);
                const bool bFits = (SignWanted > 0)
                    ? (Delta[Cand] < PerPillarCap)
                    : (Delta[Cand] > -PerPillarCap);
                if (bFits)
                {
                    return Cand;
                }
            }
            for (int32 i = 0; i < 3; ++i)
            {
                const bool bFits = (SignWanted > 0)
                    ? (Delta[i] < PerPillarCap)
                    : (Delta[i] > -PerPillarCap);
                if (bFits)
                {
                    return i;
                }
            }
            return -1;
        };

        // Step size keeps things from running away in tiny float increments —
        // 1/10th of cap or 0.5%, whichever is larger.
        const float Step = FMath::Max(0.5f, PerPillarCap * 0.10f);

        while (RemainingPos > Step * 0.5f)
        {
            const int32 P = PickPillar(+1);
            if (P < 0) { break; }
            const float Add = FMath::Min(Step, RemainingPos);
            const float Headroom = PerPillarCap - Delta[P];
            const float Applied = FMath::Min(Add, Headroom);
            if (Applied <= 0.0f) { break; }
            Delta[P] += Applied;
            RemainingPos -= Applied;
        }
        while (RemainingNeg > Step * 0.5f)
        {
            const int32 P = PickPillar(-1);
            if (P < 0) { break; }
            const float Sub = FMath::Min(Step, RemainingNeg);
            const float Headroom = PerPillarCap + Delta[P]; // distance from -cap
            const float Applied = FMath::Min(Sub, Headroom);
            if (Applied <= 0.0f) { break; }
            Delta[P] -= Applied;
            RemainingNeg -= Applied;
        }

        const float Lo = CombatConstants::PILLAR_MODIFIER_MIN;
        const float Hi = CombatConstants::PILLAR_MODIFIER_MAX;
        Delta[0] = FMath::Clamp(Delta[0], Lo, Hi);
        Delta[1] = FMath::Clamp(Delta[1], Lo, Hi);
        Delta[2] = FMath::Clamp(Delta[2], Lo, Hi);
        Delta[0] = FMath::RoundToFloat(Delta[0] * 100.0f) / 100.0f;
        Delta[1] = FMath::RoundToFloat(Delta[1] * 100.0f) / 100.0f;
        Delta[2] = FMath::RoundToFloat(Delta[2] * 100.0f) / 100.0f;
        Bonus.BonusMindModifierPercent   = FMath::Clamp(Bonus.BonusMindModifierPercent   + Delta[0], Lo, Hi);
        Bonus.BonusBodyModifierPercent   = FMath::Clamp(Bonus.BonusBodyModifierPercent   + Delta[1], Lo, Hi);
        Bonus.BonusSpiritModifierPercent = FMath::Clamp(Bonus.BonusSpiritModifierPercent + Delta[2], Lo, Hi);

        return Delta[0] + Delta[1] + Delta[2];
    }
}

// ==================== BUDGET QUERY ====================

int32 FEquipmentStatBonus::GetSubstatBudget(EItemTier Tier)
{
    return EquipmentBonusGen::GetSubstatBudget(Tier);
}

// ==================== REROLLS ====================

int32 FEquipmentStatBonus::RerollSubstats(EItemTier Tier, const FPillarWeights &Weights)
{
    const int32 Budget = GetSubstatBudget(Tier);
    if (Budget <= 0)
    {
        return 0;
    }

    WipeSubstats(*this);

    FRandomStream RNG(FMath::Rand());
    SpendOrRerollSubstatsCore(*this, Tier, Weights, Budget, RNG);

    return Budget;
}

float FEquipmentStatBonus::RerollPillars(EItemTier Tier)
{
    const float Budget = EquipmentBonusGen::GetPillarBudget(Tier);
    if (Budget <= 0.0f)
    {
        return 0.0f;
    }

    BonusMindModifierPercent   = 0.0f;
    BonusBodyModifierPercent   = 0.0f;
    BonusSpiritModifierPercent = 0.0f;

    FRandomStream RNG(FMath::Rand());
    DistributePillarPercentsZeroSum(*this, Budget, RNG);

    return Budget;
}
