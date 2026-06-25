// EconomyYield.h
// Stateless yield + type-mapping helpers for the dismantle economy (Resources_Design.md
// §3 / §4.2). TierHelpers / CrystalTypeHelpers style — a namespace of pure inline functions,
// no state and no reflection. The CALLER decides what to do with the result: the leveling
// curve is shared by Gear (weapons/rings) and Skill (abilities/spells) essence, and the
// caller picks which scalar to grant + performs the actual currency Add and item removal.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Currency/CurrencyTypes.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/EScalingTier.h"

namespace EconomyYield
{
    // ── Yield curves (named, PIE-tunable) ──────────────────────────────────────────────
    namespace Constants
    {
        // Dismantling a crystal/stone → typed acquisition essence (§4.2).
        constexpr int32 TYPED_ESSENCE_YIELD_F = 5;
        constexpr int32 TYPED_ESSENCE_YIELD_E = 7;
        constexpr int32 TYPED_ESSENCE_YIELD_D = 9;
        constexpr int32 TYPED_ESSENCE_YIELD_C = 11;
        constexpr int32 TYPED_ESSENCE_YIELD_B = 15;
        constexpr int32 TYPED_ESSENCE_YIELD_A = 19;
        constexpr int32 TYPED_ESSENCE_YIELD_S = 24;

        // Dismantling equipment OR a skill → leveling essence (§3, ½-cumulative). One curve
        // serves BOTH Gear and Skill essence — the caller routes by category.
        constexpr int32 LEVELING_ESSENCE_YIELD_F = 5;
        constexpr int32 LEVELING_ESSENCE_YIELD_E = 15;
        constexpr int32 LEVELING_ESSENCE_YIELD_D = 30;
        constexpr int32 LEVELING_ESSENCE_YIELD_C = 50;
        constexpr int32 LEVELING_ESSENCE_YIELD_B = 75;
        constexpr int32 LEVELING_ESSENCE_YIELD_A = 110;
        constexpr int32 LEVELING_ESSENCE_YIELD_S = 145;

        // World-stat vendor buy (§7 C4) — Gold cost, ESCALATING per buy this run:
        //   cost = WORLDSTAT_BUY_BASE_COST + (purchaseCount * WORLDSTAT_BUY_COST_STEP).
        // The count resets each run (UCharacterDataComponent::ResetRunWorldStats), so the ramp is
        // per-run. ⚠️ TUNING KNOBS — placeholders (mirror the 75+25 augment-shop shape).
        constexpr int32 WORLDSTAT_BUY_BASE_COST = 50; // first buy this run
        constexpr int32 WORLDSTAT_BUY_COST_STEP = 25; // added per prior buy this run

        // Prisms (hub buy-currency) base price by tier — doubling ladder (§5 pricing).
        constexpr int32 PRISMS_BASE_F = 25;
        constexpr int32 PRISMS_BASE_E = 50;
        constexpr int32 PRISMS_BASE_D = 100;
        constexpr int32 PRISMS_BASE_C = 200;
        constexpr int32 PRISMS_BASE_B = 400;
        constexpr int32 PRISMS_BASE_A = 800;
        constexpr int32 PRISMS_BASE_S = 1600;

        // Typed-essence PURCHASE cost by tier — the §4.2 "buy" row (2:1 spread vs the yield row).
        // Used for BOTH a spell's element essence (at spell tier) and each scaling grade's pillar
        // essence (§4.3 — same numbers, keyed by grade letter).
        constexpr int32 TYPED_ESSENCE_BUY_F = 10;
        constexpr int32 TYPED_ESSENCE_BUY_E = 13;
        constexpr int32 TYPED_ESSENCE_BUY_D = 17;
        constexpr int32 TYPED_ESSENCE_BUY_C = 22;
        constexpr int32 TYPED_ESSENCE_BUY_B = 29;
        constexpr int32 TYPED_ESSENCE_BUY_A = 37;
        constexpr int32 TYPED_ESSENCE_BUY_S = 48;

        // Prisms surcharge per spell scaling grade: 50 x grade-number (F=1 .. S=7).
        constexpr int32 PRISMS_SCALING_SURCHARGE_PER_GRADE = 50;

        // Gear-essence cost to tier-up ONE step FROM the keyed tier to the NEXT (§5.3 ladder).
        // Rides TIER_POWER so the step widens with tier (C→B > F→E). The Reality-essence
        // co-cost is half this (caller computes cost/2). S has no next tier (max) → 0.
        constexpr int32 TIER_UP_COST_F_TO_E = 10;
        constexpr int32 TIER_UP_COST_E_TO_D = 20;
        constexpr int32 TIER_UP_COST_D_TO_C = 30;
        constexpr int32 TIER_UP_COST_C_TO_B = 40;
        constexpr int32 TIER_UP_COST_B_TO_A = 50;
        constexpr int32 TIER_UP_COST_A_TO_S = 70;

        // §11 Quality drop curve — base weights (sum 100), rolled per fresh pickup. PIE-tunable.
        constexpr float QUALITY_WEIGHT_F = 26.0f;
        constexpr float QUALITY_WEIGHT_E = 22.0f;
        constexpr float QUALITY_WEIGHT_D = 18.0f;
        constexpr float QUALITY_WEIGHT_C = 14.0f;
        constexpr float QUALITY_WEIGHT_B = 10.0f;
        constexpr float QUALITY_WEIGHT_A = 6.0f;
        constexpr float QUALITY_WEIGHT_S = 4.0f;

        // Max RELATIVE weight tilt at normalized Luck 1.0 (maxed Luck stat). Modest by design:
        // at 1.0 it roughly doubles S's share (4% -> ~7%), never trivializing it.
        constexpr float QUALITY_LUCK_MAX_TILT = 0.5f;

        // Fraction of a gem component's crystal-curve yield returned when a FUSION breaks (§4.5).
        // Half + lossy (floored) per gem component — discourages fuse-then-break for value vs
        // breaking gems individually.
        constexpr float FUSION_BREAK_ESSENCE_FRACTION = 0.5f;

        // §4.5 crystal MERGE values (F-unit ladder). Same-Type crystals sum lowest-first to a
        // target tier's value to produce 1 of it. Doubling F..A, then S = 3xA (the 3:1 final step
        // echoed in value space).
        constexpr int32 CRYSTAL_VALUE_F = 1;
        constexpr int32 CRYSTAL_VALUE_E = 2;
        constexpr int32 CRYSTAL_VALUE_D = 4;
        constexpr int32 CRYSTAL_VALUE_C = 8;
        constexpr int32 CRYSTAL_VALUE_B = 16;
        constexpr int32 CRYSTAL_VALUE_A = 32;
        constexpr int32 CRYSTAL_VALUE_S = 96;
    }

    /** Typed acquisition essence yielded by dismantling a crystal/stone of this tier (§4.2). */
    inline int32 GetTypedEssenceYieldForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return Constants::TYPED_ESSENCE_YIELD_F;
        case EItemTier::E_Tier: return Constants::TYPED_ESSENCE_YIELD_E;
        case EItemTier::D_Tier: return Constants::TYPED_ESSENCE_YIELD_D;
        case EItemTier::C_Tier: return Constants::TYPED_ESSENCE_YIELD_C;
        case EItemTier::B_Tier: return Constants::TYPED_ESSENCE_YIELD_B;
        case EItemTier::A_Tier: return Constants::TYPED_ESSENCE_YIELD_A;
        case EItemTier::S_Tier: return Constants::TYPED_ESSENCE_YIELD_S;
        default:                return 0;
        }
    }

    /** Leveling essence yielded by dismantling equipment OR a skill of this tier (§3). The
     *  caller routes the result to Gear (weapons/rings) or Skill (abilities/spells) essence. */
    inline int32 GetLevelingEssenceYieldForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return Constants::LEVELING_ESSENCE_YIELD_F;
        case EItemTier::E_Tier: return Constants::LEVELING_ESSENCE_YIELD_E;
        case EItemTier::D_Tier: return Constants::LEVELING_ESSENCE_YIELD_D;
        case EItemTier::C_Tier: return Constants::LEVELING_ESSENCE_YIELD_C;
        case EItemTier::B_Tier: return Constants::LEVELING_ESSENCE_YIELD_B;
        case EItemTier::A_Tier: return Constants::LEVELING_ESSENCE_YIELD_A;
        case EItemTier::S_Tier: return Constants::LEVELING_ESSENCE_YIELD_S;
        default:                return 0;
        }
    }

    /** Gear-essence cost to tier-up ONE step FROM CurrentTier to the next (§5.3 ladder). The
     *  Reality-essence co-cost is half this (caller computes cost/2). S_Tier is max — there is
     *  no next tier, so this returns 0; callers gate on "already S" before charging anything. */
    inline int32 GetTierUpCostForTier(EItemTier CurrentTier)
    {
        switch (CurrentTier)
        {
        case EItemTier::F_Tier: return Constants::TIER_UP_COST_F_TO_E;
        case EItemTier::E_Tier: return Constants::TIER_UP_COST_E_TO_D;
        case EItemTier::D_Tier: return Constants::TIER_UP_COST_D_TO_C;
        case EItemTier::C_Tier: return Constants::TIER_UP_COST_C_TO_B;
        case EItemTier::B_Tier: return Constants::TIER_UP_COST_B_TO_A;
        case EItemTier::A_Tier: return Constants::TIER_UP_COST_A_TO_S;
        case EItemTier::S_Tier:
        default:                return 0; // S is max — no tier-up
        }
    }

    /** Roll a per-instance drop Quality on the §11 weighted curve, Luck-biased (§11). Called ONLY
     *  at the fresh-pickup mint point (bRandomGenerateOnPickup); non-rolled items keep C_Quality.
     *  NormalizedLuck = GetEquipmentModifiedLuck() (0 none · 1 maxed stat · >1 via gear · <0 cursed).
     *  Luck linearly tilts weight from low grades toward high grades around C (the curve's center);
     *  unbiased when Luck = 0. PERK SEAM: the drop-grade-shift perk (not built yet) will add a flat
     *  grade-index shift here. */
    inline EItemQuality RollQuality(float NormalizedLuck = 0.0f)
    {
        constexpr int32 NumGrades = 7;
        const float BaseWeights[NumGrades] = {
            Constants::QUALITY_WEIGHT_F, Constants::QUALITY_WEIGHT_E, Constants::QUALITY_WEIGHT_D,
            Constants::QUALITY_WEIGHT_C, Constants::QUALITY_WEIGHT_B, Constants::QUALITY_WEIGHT_A,
            Constants::QUALITY_WEIGHT_S};

        constexpr float CenterIndex = 3.0f; // C — the tilt pivot
        float Weights[NumGrades];
        float Total = 0.0f;
        for (int32 i = 0; i < NumGrades; ++i)
        {
            const float Tilt = 1.0f + Constants::QUALITY_LUCK_MAX_TILT * NormalizedLuck * ((i - CenterIndex) / CenterIndex);
            Weights[i] = FMath::Max(0.0f, BaseWeights[i] * Tilt); // floor at 0 — no negative weight
            Total += Weights[i];
        }
        if (Total <= 0.0f)
        {
            return EItemQuality::C_Quality; // degenerate guard (shouldn't happen)
        }

        float Roll = FMath::FRandRange(0.0f, Total);
        for (int32 i = 0; i < NumGrades; ++i)
        {
            Roll -= Weights[i];
            if (Roll <= 0.0f)
            {
                return static_cast<EItemQuality>(i); // EItemQuality is forward-ordered F=0..S=6
            }
        }
        return EItemQuality::S_Quality; // FP-safety fallthrough
    }

    // ── Purchase pricing (§4.2 buy row / §5 Prisms) ────────────────────────────────────

    /** Prisms base price for an item/spell of this tier (§5 doubling ladder). */
    inline int32 GetPrismsBaseForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return Constants::PRISMS_BASE_F;
        case EItemTier::E_Tier: return Constants::PRISMS_BASE_E;
        case EItemTier::D_Tier: return Constants::PRISMS_BASE_D;
        case EItemTier::C_Tier: return Constants::PRISMS_BASE_C;
        case EItemTier::B_Tier: return Constants::PRISMS_BASE_B;
        case EItemTier::A_Tier: return Constants::PRISMS_BASE_A;
        case EItemTier::S_Tier: return Constants::PRISMS_BASE_S;
        default:                return 0;
        }
    }

    /** Prisms cost to MERGE up to ProducedTier (§4.5 crystal/stone merge) — HALF the §5 buy price
     *  of the produced tier (E25/D50/C100/B200/A400/S800). Derived from GetPrismsBaseForTier so it
     *  tracks the one Prisms curve. F is never produced by a merge (it is the floor). */
    inline int32 GetMergeCostForTier(EItemTier ProducedTier)
    {
        return GetPrismsBaseForTier(ProducedTier) / 2;
    }

    /** Crystal MERGE value in F-units (§4.5). Same-Type crystals sum (lowest-first) to a target
     *  tier's value to produce 1 of it. Doubling F..A, then S = 3xA (the 3:1 final step). */
    inline int32 GetCrystalValue(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return Constants::CRYSTAL_VALUE_F;
        case EItemTier::E_Tier: return Constants::CRYSTAL_VALUE_E;
        case EItemTier::D_Tier: return Constants::CRYSTAL_VALUE_D;
        case EItemTier::C_Tier: return Constants::CRYSTAL_VALUE_C;
        case EItemTier::B_Tier: return Constants::CRYSTAL_VALUE_B;
        case EItemTier::A_Tier: return Constants::CRYSTAL_VALUE_A;
        case EItemTier::S_Tier: return Constants::CRYSTAL_VALUE_S;
        default:                return 0;
        }
    }

    /** Typed-essence PURCHASE cost for a tier — §4.2 buy row. Used for a spell's element essence
     *  (at spell tier) and, via ScalingGradeToItemTier, each scaling grade's pillar essence. */
    inline int32 GetTypedEssencePurchaseCostForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::F_Tier: return Constants::TYPED_ESSENCE_BUY_F;
        case EItemTier::E_Tier: return Constants::TYPED_ESSENCE_BUY_E;
        case EItemTier::D_Tier: return Constants::TYPED_ESSENCE_BUY_D;
        case EItemTier::C_Tier: return Constants::TYPED_ESSENCE_BUY_C;
        case EItemTier::B_Tier: return Constants::TYPED_ESSENCE_BUY_B;
        case EItemTier::A_Tier: return Constants::TYPED_ESSENCE_BUY_A;
        case EItemTier::S_Tier: return Constants::TYPED_ESSENCE_BUY_S;
        default:                return 0;
        }
    }

    /** A scaling grade as a 1..7 number (F=1 shallowest .. S=7 steepest), for the Prisms scaling
     *  surcharge. EScalingTier is REVERSE-ordered (S=0 .. F=6), so this is a switch, not a cast. */
    inline int32 GetScalingGradeNumber(EScalingTier Grade)
    {
        switch (Grade)
        {
        case EScalingTier::F: return 1;
        case EScalingTier::E: return 2;
        case EScalingTier::D: return 3;
        case EScalingTier::C: return 4;
        case EScalingTier::B: return 5;
        case EScalingTier::A: return 6;
        case EScalingTier::S: return 7;
        default:              return 0;
        }
    }

    /** Map a scaling-grade letter to the matching EItemTier letter (F->F_Tier .. S->S_Tier) so the
     *  scaling-grade pillar essence reuses the tier-keyed purchase-cost row (§4.3 — same numbers). */
    inline EItemTier ScalingGradeToItemTier(EScalingTier Grade)
    {
        switch (Grade)
        {
        case EScalingTier::F: return EItemTier::F_Tier;
        case EScalingTier::E: return EItemTier::E_Tier;
        case EScalingTier::D: return EItemTier::D_Tier;
        case EScalingTier::C: return EItemTier::C_Tier;
        case EScalingTier::B: return EItemTier::B_Tier;
        case EScalingTier::A: return EItemTier::A_Tier;
        case EScalingTier::S: return EItemTier::S_Tier;
        default:              return EItemTier::F_Tier;
        }
    }

    /** Fold a sub-stat onto its pillar typed-essence (Mind / Body / Spirit). Reusable — spell
     *  pricing reuses this. NOTE: StatusMultiplier is a SPIRIT sub-stat (see ActionStatModifiers.h
     *  — kept in its Mind-block enum position for serialization safety, the pillar lives here).
     *  None (no sub-stat) → Generic + a warning: a stat-stone always resolves to a real sub-stat,
     *  so None reaching here is unexpected. */
    inline EEssenceType SubStatToPillarEssence(ESubStat SubStat)
    {
        switch (SubStat)
        {
        // Mind
        case ESubStat::Efficiency:
        case ESubStat::SpellDamage:
        case ESubStat::CritDamage:
        case ESubStat::SpellSpeed:
            return EEssenceType::Mind;
        // Body
        case ESubStat::Defense:
        case ESubStat::ActionSpeed:
        case ESubStat::RawDamage:
        case ESubStat::Reflex:
            return EEssenceType::Body;
        // Spirit (incl. StatusMultiplier)
        case ESubStat::Resistance:
        case ESubStat::TurnSpeed:
        case ESubStat::Luck:
        case ESubStat::StatusMultiplier:
            return EEssenceType::Spirit;
        case ESubStat::None:
        default:
            UE_LOG(LogTemp, Warning,
                   TEXT("[EconomyYield] SubStatToPillarEssence: unmapped sub-stat (None) — defaulting to Generic essence"));
            return EEssenceType::Generic;
        }
    }

    /** Map a resolved spell element to its typed essence — 1:1 by NAME for the 10 elements
     *  (the two enums differ in order, so this is a switch, not a cast). Generic / None →
     *  Generic (Quartz's element is None; EEssenceType::Generic == "Quartz's element"). */
    inline EEssenceType ElementToEssenceType(ESpellElement Element)
    {
        switch (Element)
        {
        case ESpellElement::Fire:      return EEssenceType::Fire;
        case ESpellElement::Water:     return EEssenceType::Water;
        case ESpellElement::Lightning: return EEssenceType::Lightning;
        case ESpellElement::Wind:      return EEssenceType::Wind;
        case ESpellElement::Earth:     return EEssenceType::Earth;
        case ESpellElement::Light:     return EEssenceType::Light;
        case ESpellElement::Darkness:  return EEssenceType::Darkness;
        case ESpellElement::Void:      return EEssenceType::Void;
        case ESpellElement::Reality:   return EEssenceType::Reality;
        case ESpellElement::Generic:
        case ESpellElement::None:
        default:                       return EEssenceType::Generic;
        }
    }

    /** Which typed essence a crystal/stone yields when dismantled. Gems → element essence;
     *  AbilityStone → Ability; other (stat) stones → pillar essence via the sub-stat fold.
     *  Variant does NOT affect type or amount. */
    inline EEssenceType ResolveEssenceType(const FCrystalId &Id)
    {
        if (CrystalTypeHelpers::IsGemType(Id.Type))
        {
            return ElementToEssenceType(ItemIdentity::GetElement(Id));
        }
        if (Id.Type == ECrystalType::AbilityStone)
        {
            return EEssenceType::Ability;
        }
        if (CrystalTypeHelpers::IsStatStone(Id.Type))
        {
            return SubStatToPillarEssence(CrystalEffectTable::StoneTargetStat(Id.Type));
        }

        UE_LOG(LogTemp, Warning,
               TEXT("[EconomyYield] ResolveEssenceType: unclassified crystal type — defaulting to Generic essence"));
        return EEssenceType::Generic;
    }
}
