// CrystalTypeHelpers.h
// Free-function helpers keyed on ECrystalType. Decouples crystal-type-derived
// data (element, future identity/effects/durability) from UEvolutionItemData so callers
// can resolve these without holding an asset pointer — required for refined
// attachments after the crystal/evolution refactor, where refined crystals
// become enum pairs rather than asset references.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Combat/Elements/ElementTable.h"
#include "Skills/Definitions/ESpellElement.h"

namespace CrystalTypeHelpers
{
    /** Returns the spell element associated with the given crystal type.
     *  Pure function — no side effects, no asset access. Mirrors the byte-
     *  for-byte body lifted from UEvolutionItemData::GetAssociatedElement in commit 0
     *  of the crystal/evolution refactor sequence. */
    inline ESpellElement GetElement(ECrystalType Type)
    {
        if (Type == ECrystalType::None)
        {
            return ESpellElement::None;
        }
        for (const FElementRow &Row : ElementTable::Rows())
        {
            if (Row.Gem == Type)
            {
                return Row.Element; // Quartz carries None: the non-elemental gem
            }
        }
        return ESpellElement::None; // a stone, or an unknown crystal type
    }

    /** True for augment-stone sub-types (DamageStone, DefenseStone,
     *  CritStone, TurnSpeedStone, StatusStone, EfficiencyStone) — the attach-only
     *  stones that grant raw damage / a stat. Single source of truth
     *  for the gem/stone split; the editor dropdown filter, IsGemType, and the
     *  IsDataValid backstop all key off this. */
    inline bool IsAugmentStoneType(ECrystalType Type)
    {
        return Type == ECrystalType::DamageStone ||
               Type == ECrystalType::DefenseStone || Type == ECrystalType::CritStone ||
               Type == ECrystalType::TurnSpeedStone || Type == ECrystalType::StatusStone ||
               Type == ECrystalType::EfficiencyStone || Type == ECrystalType::MaxHPStone ||
               Type == ECrystalType::MaxEPStone || Type == ECrystalType::SpellDamageStone ||
               Type == ECrystalType::ResistanceStone || Type == ECrystalType::SpellSpeedStone ||
               Type == ECrystalType::ActionSpeedStone || Type == ECrystalType::DurabilityStone ||
               Type == ECrystalType::LuckStone || Type == ECrystalType::ReflexStone ||
               Type == ECrystalType::HealingStone || Type == ECrystalType::StatStone; // StatStone: B-353 (D-142)
    }

    /** True for gem sub-types (Garnet..Quartz). None is neither gem nor stone,
     *  so it is excluded. Complement of IsAugmentStoneType over the non-None
     *  range. */
    inline bool IsGemType(ECrystalType Type)
    {
        return Type != ECrystalType::None && !IsAugmentStoneType(Type);
    }

    /** Every augment stone, since the Ability Stone retired (B-437, D-175). */
    inline bool IsStatStone(ECrystalType Type)
    {
        return IsAugmentStoneType(Type);
    }

    // OBSOLETE (B-352, D-141; delete in the D-124 sweep): the fusion-pair predicates below — keyed on the two half
    // TYPES, kept struct-free so this header stays a pure ECrystalType helper (callers pass F.HalfA.Type / F.HalfB.Type).

    /** Elemental fusion = exactly one crystal (gem) half — its element drives the fusion.
     *  Allowed on weapon OR ring. */
    inline bool IsElementalFusion(ECrystalType HalfAType, ECrystalType HalfBType)
    {
        return IsGemType(HalfAType) != IsGemType(HalfBType);
    }

    /** Augmented fusion = two stones, no crystal half — weapon-only at the validation
     *  layer. */
    inline bool IsAugmentedFusion(ECrystalType HalfAType, ECrystalType HalfBType)
    {
        return !IsGemType(HalfAType) && !IsGemType(HalfBType);
    }

    /** OBSOLETE (B-352, D-141; delete in the D-124 sweep): fusions retire, nothing calls this in play.
     *  The old rule: a fusion pair was valid iff each half was a stone or gem (never
     *  None — and evolution is unrepresentable as a half) AND at least one half was a stone, which
     *  rejected crystal+crystal. */
    inline bool IsValidFusionPair(ECrystalType HalfAType, ECrystalType HalfBType)
    {
        auto IsAllowedHalf = [](ECrystalType T)
        { return IsStatStone(T) || IsGemType(T); };
        return IsAllowedHalf(HalfAType) && IsAllowedHalf(HalfBType) &&
               (IsStatStone(HalfAType) || IsStatStone(HalfBType));
    }
}
