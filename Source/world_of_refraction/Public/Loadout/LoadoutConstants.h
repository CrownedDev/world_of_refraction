// LoadoutConstants.h
// Constants for character loadout configuration

#pragma once

namespace LoadoutConstants
{
    // Abilities
    constexpr int32 MAX_WEAPON_ABILITIES = 6;
    constexpr int32 MAX_WHETSTONE_ABILITIES = 6; // Whetstone Resonate-weapon ability slots
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

    // Cost of an Evolution primary (the evolution crystal itself; no weapon platform).
    constexpr int32 EVOLUTION_PRIMARY_SLOT_COST = 2;

    // Evolution
    constexpr int32 MAX_EVOLUTION_SPELLS = 6;
}