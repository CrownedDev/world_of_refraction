// LoadoutConstants.h
// Constants for character loadout configuration

#pragma once

namespace LoadoutConstants
{
    // Abilities
    constexpr int32 MAX_WEAPON_ABILITIES = 6;
    constexpr int32 MAX_GRANTED_ABILITIES = 5; // +1 Cancel = 6 total

    // Spells
    constexpr int32 MAX_SPELL_SLOTS = 6;

    // Weapons
    constexpr int32 MAX_WEAPONS_GENERIC = 2;
    constexpr int32 MAX_WEAPONS_ELEMENTAL = 1;

    constexpr int32 RESONATOR_RING_SLOTS_NORMAL = 5;
    constexpr int32 RESONATOR_RING_SLOTS_EVOLVED = 3;
    constexpr int32 RESONATOR_MAX_EVOLVED_RINGS_NORMAL = 2;
    constexpr int32 RESONATOR_MAX_EVOLVED_RINGS_EVOLVED = 1;

    // Evolution
    constexpr int32 MAX_EVOLUTION_SPELLS = 6;
}