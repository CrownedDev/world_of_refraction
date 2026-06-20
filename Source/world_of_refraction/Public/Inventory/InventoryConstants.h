// InventoryConstants.h
// Constants for inventory capacities and slot costs
//
// ARCHITECTURE NOTE:
// - LoadoutConstants.h: Equipment slot counts (abilities per weapon, spell slots, etc.)
// - InventoryConstants.h: Inventory capacities and slot COSTS (weighted storage)
//
// Last working commit before inventory system: d41163ca28f551914091753c49a3561377894ade

#pragma once

#include "CoreMinimal.h"
#include "Loadout/LoadoutConstants.h" // Reference existing constants

namespace InventoryConstants
{
    // ==================== SPELL/ABILITY COLLECTIONS ====================

    /** Maximum spells a character can learn */
    constexpr int32 MAX_LEARNED_SPELLS = 50;

    /** Maximum abilities a character can learn */
    constexpr int32 MAX_LEARNED_ABILITIES = 50;

    // ==================== CASTER INNATE SPELLS ====================
    // Innate spell caps moved to SpellPoolConstants.h — MAX_EQUIPPED_SLOT_POOL (per-school
    // count) + INNATE_SPELL_BUDGET (weight). The former flat MAX_INNATE_SPELLS_PER_SCHOOL /
    // MAX_INNATE_SPELLS_TOTAL are retired (feature/innate-bd-spell-budget).

    // ==================== EQUIPMENT INVENTORIES ====================

    /** Maximum weighted slots for weapons */
    constexpr int32 MAX_WEAPON_INVENTORY_SLOTS = 20;

    /** Maximum weighted slots for rings */
    constexpr int32 MAX_RING_INVENTORY_SLOTS = 20;

    /** Maximum evolution crystals */
    constexpr int32 MAX_EVOLUTION_CRYSTALS = 10;

    /** Hard cap on owned (unslotted) evolution items in
     *  UEvolutionInventoryComponent. */
    constexpr int32 MAX_EVOLUTION_ITEMS = 5;

    /** Per-tier cap for each pool in UCrystalInventoryComponent. Item-crystal
     *  and refined-crystal pools each respect this cap independently. */
    constexpr int32 CRYSTAL_PER_TIER_CAP = 20;

    // ==================== EQUIPMENT SLOT COSTS ====================
    // These determine how much inventory space each item uses

    /** Slot cost for base weapon (no crystal) */
    constexpr int32 WEAPON_BASE_SLOT_COST = 1;

    /** Slot cost for weapon with crystal attached */
    constexpr int32 WEAPON_CRYSTAL_SLOT_COST = 2;

    /** Slot cost for weapon with evolution crystal */
    constexpr int32 WEAPON_EVOLUTION_SLOT_COST = 3;

    /** Slot cost for base ring (crystal attached) */
    constexpr int32 RING_BASE_SLOT_COST = 1;

    /** Slot cost for evolved ring */
    constexpr int32 RING_EVOLUTION_SLOT_COST = 2;

    // ==================== EQUIPMENT SPELL/ABILITY SLOTS ====================
    // Reference LoadoutConstants for these - single source of truth
    // LoadoutConstants::MAX_WEAPON_ABILITIES = 6
    // LoadoutConstants::MAX_SPELL_SLOTS = 6

    // ==================== ITEM CRYSTAL CAPACITIES (BY TIER) ====================

    /** S-tier crystal capacity (precious) */
    constexpr int32 ITEM_CAPACITY_S = 3;

    /** A-tier crystal capacity */
    constexpr int32 ITEM_CAPACITY_A = 5;

    /** B-tier crystal capacity */
    constexpr int32 ITEM_CAPACITY_B = 8;

    /** C-tier crystal capacity */
    constexpr int32 ITEM_CAPACITY_C = 12;

    /** D-tier crystal capacity */
    constexpr int32 ITEM_CAPACITY_D = 15;

    /** E-tier crystal capacity */
    constexpr int32 ITEM_CAPACITY_E = 20;

    /** F-tier crystal capacity (common) */
    constexpr int32 ITEM_CAPACITY_F = 25;

    /** Total maximum item crystals across all tiers */
    constexpr int32 ITEM_CAPACITY_TOTAL = ITEM_CAPACITY_S + ITEM_CAPACITY_A + ITEM_CAPACITY_B +
                                          ITEM_CAPACITY_C + ITEM_CAPACITY_D + ITEM_CAPACITY_E +
                                          ITEM_CAPACITY_F; // 88

    // ==================== COMBAT LOADOUT ====================

    /** Maximum item slots in combat loadout */
    constexpr int32 MAX_ITEM_LOADOUT_SLOTS = 6;

    /** Maximum starting/refilled quantity per equipped item slot (new model). */
    constexpr int32 MAX_QUANTITY_PER_ITEM_SLOT = 3;

    /** Uses per item slot in combat (legacy — removed in 9.5b reader migration). */
    constexpr int32 ITEM_USES_PER_SLOT = 3;

    /** Maximum total item uses per battle */
    constexpr int32 MAX_ITEM_USES_PER_BATTLE = MAX_ITEM_LOADOUT_SLOTS * ITEM_USES_PER_SLOT; // 18

    /** Maximum rings for Resonator class */
    constexpr int32 MAX_RESONATOR_RINGS = 5;

    /** Maximum evolved rings for Resonator class */
    constexpr int32 MAX_EVOLVED_RESONATOR_RINGS = 2;

    // ==================== HELPER FUNCTIONS ====================

    /** Get item capacity for a specific tier */
    inline int32 GetItemCapacityForTier(int32 TierIndex)
    {
        switch (TierIndex)
        {
        case 0:
            return ITEM_CAPACITY_F; // F_Tier
        case 1:
            return ITEM_CAPACITY_E; // E_Tier
        case 2:
            return ITEM_CAPACITY_D; // D_Tier
        case 3:
            return ITEM_CAPACITY_C; // C_Tier
        case 4:
            return ITEM_CAPACITY_B; // B_Tier
        case 5:
            return ITEM_CAPACITY_A; // A_Tier
        case 6:
            return ITEM_CAPACITY_S; // S_Tier
        default:
            return 0;
        }
    }

    /** Get weapon slot cost based on state */
    inline int32 GetWeaponSlotCost(bool bHasCrystal, bool bIsEvolved)
    {
        if (bIsEvolved)
            return WEAPON_EVOLUTION_SLOT_COST;
        if (bHasCrystal)
            return WEAPON_CRYSTAL_SLOT_COST;
        return WEAPON_BASE_SLOT_COST;
    }

    /** Get ring slot cost based on state */
    inline int32 GetRingSlotCost(bool bIsEvolved)
    {
        return bIsEvolved ? RING_EVOLUTION_SLOT_COST : RING_BASE_SLOT_COST;
    }
}
