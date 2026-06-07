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

    /** True for weapon-stone sub-types (DamageStone, AbilityStone, DefenseStone) —
     *  the attach-only stones that grant raw damage / ability slots / defense.
     *  Single source of truth for the gem/stone split; the editor dropdown filter,
     *  IsGemType, and the IsDataValid backstop all key off this. */
    inline bool IsWeaponStoneType(ECrystalType Type)
    {
        return Type == ECrystalType::DamageStone || Type == ECrystalType::AbilityStone ||
               Type == ECrystalType::DefenseStone;
    }

    /** True for gem sub-types (Garnet..Quartz). None is neither gem nor stone,
     *  so it is excluded. Complement of IsWeaponStoneType over the non-None
     *  range. */
    inline bool IsGemType(ECrystalType Type)
    {
        return Type != ECrystalType::None && !IsWeaponStoneType(Type);
    }
}
