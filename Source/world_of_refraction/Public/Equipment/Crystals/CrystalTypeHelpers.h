// CrystalTypeHelpers.h
// Free-function helpers keyed on ECrystalType. Decouples crystal-type-derived
// data (element, future identity/effects/durability) from UEvolutionItemData so callers
// can resolve these without holding an asset pointer — required for refined
// attachments after the crystal/evolution refactor, where refined crystals
// become enum pairs rather than asset references.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Skills/Definitions/ESpellElement.h"

namespace CrystalTypeHelpers
{
    /** Returns the spell element associated with the given crystal type.
     *  Pure function — no side effects, no asset access. Mirrors the byte-
     *  for-byte body lifted from UEvolutionItemData::GetAssociatedElement in commit 0
     *  of the crystal/evolution refactor sequence. */
    inline ESpellElement GetElement(ECrystalType Type)
    {
        switch (Type)
        {
        case ECrystalType::Garnet:
            return ESpellElement::Fire;
        case ECrystalType::Sapphire:
            return ESpellElement::Water;
        case ECrystalType::Citrine:
            return ESpellElement::Lightning;
        case ECrystalType::Emerald:
            return ESpellElement::Wind;
        case ECrystalType::Amber:
            return ESpellElement::Earth;
        case ECrystalType::Opal:
            return ESpellElement::Light;
        case ECrystalType::Onyx:
            return ESpellElement::Darkness;
        case ECrystalType::Amethyst:
            return ESpellElement::Void;
        case ECrystalType::Iolite:
            return ESpellElement::Reality;
        case ECrystalType::Quartz:
            return ESpellElement::Generic;
        default:
            return ESpellElement::Generic;
        }
    }

    /** True for augment-stone sub-types (DamageStone, AbilityStone, DefenseStone,
     *  CritStone, TurnSpeedStone, StatusStone, EfficiencyStone) — the attach-only
     *  stones that grant raw damage / ability slots / a stat. Single source of truth
     *  for the gem/stone split; the editor dropdown filter, IsGemType, and the
     *  IsDataValid backstop all key off this. */
    inline bool IsAugmentStoneType(ECrystalType Type)
    {
        return Type == ECrystalType::DamageStone || Type == ECrystalType::AbilityStone ||
               Type == ECrystalType::DefenseStone || Type == ECrystalType::CritStone ||
               Type == ECrystalType::TurnSpeedStone || Type == ECrystalType::StatusStone ||
               Type == ECrystalType::EfficiencyStone || Type == ECrystalType::MaxHPStone ||
               Type == ECrystalType::MaxEPStone || Type == ECrystalType::SpellDamageStone ||
               Type == ECrystalType::ResistanceStone || Type == ECrystalType::SpellSpeedStone ||
               Type == ECrystalType::ActionSpeedStone;
    }

    /** True for gem sub-types (Garnet..Quartz). None is neither gem nor stone,
     *  so it is excluded. Complement of IsAugmentStoneType over the non-None
     *  range. */
    inline bool IsGemType(ECrystalType Type)
    {
        return Type != ECrystalType::None && !IsAugmentStoneType(Type);
    }

    /** True for stat-granting augment stones — every augment stone EXCEPT AbilityStone
     *  (which grants ability slots, not a stat). A valid fusion needs at least one. */
    inline bool IsStatStone(ECrystalType Type)
    {
        return IsAugmentStoneType(Type) && Type != ECrystalType::AbilityStone;
    }

    // Fusion-pair predicates — keyed on the two half TYPES, kept struct-free so this
    // header stays a pure ECrystalType helper (callers pass F.HalfA.Type / F.HalfB.Type).

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

    /** A fusion pair is valid iff each half is a stat-stone, AbilityStone, or gem (never
     *  None — and evolution is unrepresentable as a half) AND at least one half is a
     *  stat-stone. That single rule rejects every banned pair: crystal+crystal,
     *  AbilityStone+AbilityStone, and AbilityStone+crystal all have zero stat-stones. */
    inline bool IsValidFusionPair(ECrystalType HalfAType, ECrystalType HalfBType)
    {
        auto IsAllowedHalf = [](ECrystalType T)
        { return IsStatStone(T) || T == ECrystalType::AbilityStone || IsGemType(T); };
        return IsAllowedHalf(HalfAType) && IsAllowedHalf(HalfBType) &&
               (IsStatStone(HalfAType) || IsStatStone(HalfBType));
    }
}
