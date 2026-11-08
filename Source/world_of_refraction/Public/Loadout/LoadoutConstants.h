// LoadoutConstants.h
// Constants for character loadout configuration

#pragma once

namespace LoadoutConstants
{
    // Abilities
    constexpr int32 MAX_WEAPON_ABILITIES = 6;
    constexpr int32 MAX_AUGMENTSTONE_ABILITIES = 6; // Absolute augment-stone ability-slot ceiling (AbilityStone S-tier = 6)
    constexpr int32 MAX_GRANTED_ABILITIES = 5; // +1 Cancel = 6 total

    // Spells
    constexpr int32 MAX_SPELL_SLOTS = 6;
    constexpr int32 MAX_RING_SPELLS = 6;

    // Broken Darkness spell pools. Per-pool spell COUNT moved to
    // SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL (+ the weighted budget); the
    // element-pool COUNT cap below stays here.
    constexpr int32 MAX_BD_ELEMENT_POOLS = 7; // Fire/Water/Earth/Wind/Light/Lightning/Void

    // Max payloads per skill effect (Cluster D1). Bounds the cast EffectID payload-index
    // packing (SourceID*100 + EffectIndex + PayloadIndex*10) — must stay <= 9.
    constexpr int32 MAX_PAYLOADS = 9;

    // Weapons
    constexpr int32 MAX_WEAPONS_GENERIC = 2;
    constexpr int32 MAX_WEAPONS_ELEMENTAL = 1;

    // Unified loadout budget — primary slot cost + ring slot costs must not exceed this.
    constexpr int32 LOADOUT_TOTAL_BUDGET = 5;

    // B-357 (D-147, Q-357): the one cost of an evolution, wherever it attaches: the character's own slot, a weapon or
    // an adapter (the gear's own base cost comes on top). The separate cost rows retire.
    constexpr int32 EVOLUTION_SLOT_COST = 2;

    // OBSOLETE (B-357): the evolution on the character's own slot; = EVOLUTION_SLOT_COST. Remove with the sweep.
    constexpr int32 EVOLUTION_PRIMARY_SLOT_COST = EVOLUTION_SLOT_COST;

    // Evolution
    constexpr int32 MAX_EVOLUTION_SPELLS = 6;

    // B-354 (D-143, Q-354): the one crystal tier cap left: at most this many S-tier crystals equipped in a battle
    // loadout (attached to its gear, or a gem in its item slots). Owned, carried and stored crystals have no cap.
    constexpr int32 MAX_EQUIPPED_S_TIER_CRYSTALS = 1;

    // OBSOLETE (B-354, D-143 retires D-072's bands): utility crystal tier caps per character, crystals carried in
    // item slots, counted per crystal, bands combined. Nothing enforces them; delete after Crown's PIE.
    constexpr int32 MAX_UTILITY_CRYSTALS_S_TIER = 1;
    constexpr int32 MAX_UTILITY_CRYSTALS_A_B_TIER = 2;
    constexpr int32 MAX_UTILITY_CRYSTALS_C_D_TIER = 3;
    constexpr int32 MAX_UTILITY_CRYSTALS_E_F_TIER = 4;
}