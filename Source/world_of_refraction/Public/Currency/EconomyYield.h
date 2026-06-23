// EconomyYield.h
// Stateless yield + type-mapping helpers for the dismantle economy (Resources_Design.md
// §3 / §4.2). TierHelpers / CrystalTypeHelpers style — a namespace of pure inline functions,
// no state and no reflection. The CALLER decides what to do with the result: the leveling
// curve is shared by Gear (weapons/rings) and Skill (abilities/spells) essence, and the
// caller picks which scalar to grant + performs the actual currency Add and item removal.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
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
