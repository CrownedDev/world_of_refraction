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
