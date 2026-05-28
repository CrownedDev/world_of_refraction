// ElementHelpers.h
// Small inline helpers for ESpellElement queries.

#pragma once

#include "Skills/Definitions/ESpellElement.h"

namespace ElementHelpers
{
    /**
     * True if the element acts as an "any-element" spell source.
     * Reality and BrokenDarkness slots/innates accept spells of any element;
     * all other elements are element-locked.
     *
     * Used by loadout validation (FWeaponLoadoutEntry, FRingLoadoutEntry,
     * LoadoutComponent innate spell check) and by ActionExecutor::ValidateAction
     * to determine whether the element-match gate should fire.
     */
    constexpr bool IsAnySpellSource(ESpellElement Element)
    {
        return Element == ESpellElement::Reality
            || Element == ESpellElement::BrokenDarkness;
    }
}
