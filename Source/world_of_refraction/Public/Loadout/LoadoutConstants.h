// LoadoutConstants.h
// Constants for character loadout configuration

#pragma once

#include "Equipment/Weapons/EWeaponSlotType.h"

namespace LoadoutConstants
{
    // Abilities
    constexpr int32 MAX_WEAPON_ABILITIES = 6;
    constexpr int32 MAX_GRANTED_ABILITIES = 5; // +1 Cancel = 6 total

    // Spells
    constexpr int32 MAX_SPELL_SLOTS = 6;
    constexpr int32 MAX_RING_SPELLS = 6;

    // Broken Darkness spell pools
    constexpr int32 MAX_BD_POOL_SPELLS = 6;   // per element pool, and the Darkness pool
    constexpr int32 MAX_BD_ELEMENT_POOLS = 7; // Fire/Water/Earth/Wind/Light/Lightning/Void

    // Skills (abilities/spells/attacks all share this cap on Effects array)
    constexpr int32 MAX_SKILL_EFFECTS = 5;

    // Weapons
    constexpr int32 MAX_WEAPONS_GENERIC = 2;
    constexpr int32 MAX_WEAPONS_ELEMENTAL = 1;

    // Unified loadout budget — primary slot cost + ring slot costs must not exceed this.
    constexpr int32 LOADOUT_TOTAL_BUDGET = 5;

    // Evolution
    constexpr int32 MAX_EVOLUTION_SPELLS = 6;

    /** Budget cost of occupying the primary slot.
     *  None = 0 (empty primary), Weapon = 1, Evolution = 2.
     *  Ring = 0: a ring is never a Resonator primary, and Generic/Caster
     *  ring-primaries carry no ring-budget cost — treated as no adjustment. */
    inline int32 GetPrimarySlotCost(EPrimarySlotType SlotType)
    {
        switch (SlotType)
        {
        case EPrimarySlotType::Weapon:
            return 1;
        case EPrimarySlotType::Evolution:
            return 2;
        case EPrimarySlotType::Ring:
        case EPrimarySlotType::None:
            return 0;
        }
        return 0;
    }
}