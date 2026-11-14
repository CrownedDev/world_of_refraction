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

    // B-357 (D-147, Q-357, Crown 2026-11-08): the frame, plus what sits in it. Since D-206 (B-472) these feed only the
    // pouch capacity (InventoryConstants: WEAPON_BASE_SLOT_COST and the rest, read by UInventoryComponent and
    // UEconomyService); the loadout reads the D-206 costs below.
    constexpr int32 WEAPON_FRAME_COST = 1;
    constexpr int32 ADAPTER_FRAME_COST = 0;
    constexpr int32 CRYSTAL_ATTACHMENT_COST = 1;
    /** An evolution attached to a weapon or an adapter, in the pouch capacity. */
    constexpr int32 EVOLUTION_SLOT_COST = 2;

    // B-472 (D-206, Crown 2026-11-14): the loadout costs. The gear itself is free; only what is in it costs: a
    // crystal (or fusion) 1, an augment stone 1, an evolution 2 (it sits in the crystal socket). An evolution on its
    // own (the character's own slot) costs 2. So a weapon with a crystal costs 1, the same as an adapter with one.
    // FLoadoutRules::SlotCost is the one reader.
    constexpr int32 LOADOUT_GEAR_COST = 0;
    constexpr int32 LOADOUT_CRYSTAL_COST = 1;
    constexpr int32 LOADOUT_STONE_COST = 1;
    constexpr int32 LOADOUT_EVOLUTION_COST = 2;
    constexpr int32 LOADOUT_EVOLUTION_ALONE_COST = 2;

    /** An evolution on its own in the loadout slot (the character's own slot), D-206. */
    constexpr int32 EVOLUTION_ALONE_SLOT_COST = LOADOUT_EVOLUTION_ALONE_COST;

    // The evolution on the character's own slot (the name the readers use): EVOLUTION_ALONE_SLOT_COST.
    constexpr int32 EVOLUTION_PRIMARY_SLOT_COST = EVOLUTION_ALONE_SLOT_COST;

    // B-472 (D-206): the class budgets and counts that are not the shared 5.
    /** Refractor: one item, budget 2 (was 3): an evolved weapon, an evolved adapter, an evolution alone, or a weapon
     *  with a crystal and a stone all fit; evolved gear plus a stone does not. */
    constexpr int32 REFRACTOR_LOADOUT_BUDGET = 2;
    /** Generic: at most three items, two weapons and one adapter. */
    constexpr int32 MAX_GENERIC_LOADOUT_ITEMS = 3;
    /** Resonator: at most 5 adapters (reference-backed default: with bare adapters free, the budget alone no longer
     *  limits them, CL-11). */
    constexpr int32 MAX_RESONATOR_ADAPTERS = 5;

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