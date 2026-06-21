// ElementHelpers.h
// Small inline helpers for ESpellElement queries.

#pragma once

#include "Skills/Definitions/ESpellElement.h"

namespace ElementHelpers
{
    /**
     * True if the element acts as an "any-element" spell source.
     * Reality slots/innates accept spells of any element; all other elements are
     * element-locked. (A Broken Darkness character is Darkness + the BD toggle — no
     * host ever carries the BrokenDarkness enum value, so it is not checked here.)
     *
     * Used by loadout validation (FWeaponLoadoutEntry, FRingLoadoutEntry,
     * LoadoutComponent innate spell check) and by ActionExecutor::ValidateAction
     * to determine whether the element-match gate should fire.
     */
    constexpr bool IsAnySpellSource(ESpellElement Element)
    {
        return Element == ESpellElement::Reality;
    }

    /**
     * True if a spell of SpellElement may be slotted into / cast from a host of
     * HostElement. The element-match rule, in ONE place:
     *   - Generic spells are wildcards — they resolve to the host's element at cast,
     *     so they fit any host.
     *   - A Reality / BrokenDarkness host (IsAnySpellSource) accepts any element.
     *   - Otherwise the spell's element must match the host's exactly.
     * Concrete-element spells in a wrong-element host are still rejected.
     * NOTE: the design's "Reality cannot cast Generic" prohibition is enforced at
     * cast-resolve time, NOT here — this slot-validation predicate treats Generic
     * as fitting any host (including Reality).
     */
    constexpr bool SpellElementMatchesHost(ESpellElement SpellElement, ESpellElement HostElement)
    {
        return SpellElement == ESpellElement::Generic
            || IsAnySpellSource(HostElement)
            || SpellElement == HostElement;
    }
}
