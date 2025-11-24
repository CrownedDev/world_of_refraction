// ItemConstants.h
// Constants for the item system

#pragma once

#include "CoreMinimal.h"

namespace ItemConstants
{
    // ==================== INVENTORY LIMITS ====================
    constexpr int32 MAX_ITEM_SLOTS = 6;
    constexpr int32 MAX_STACKS_PER_SLOT = 3;
    constexpr int32 MAX_TOTAL_ITEMS = MAX_ITEM_SLOTS * MAX_STACKS_PER_SLOT; // 18

    // ==================== GENERIC CHARACTER BONUSES ====================
    // Generic element gains resistance to item's element when using any item
    // Tier-based resistance (flat scaling)
    constexpr float GENERIC_RESISTANCE_F = 10.0f; // 2 turns
    constexpr int32 GENERIC_DURATION_F = 2;

    constexpr float GENERIC_RESISTANCE_E = 15.0f; // 2 turns
    constexpr int32 GENERIC_DURATION_E = 2;

    constexpr float GENERIC_RESISTANCE_D = 20.0f; // 2 turns
    constexpr int32 GENERIC_DURATION_D = 2;

    constexpr float GENERIC_RESISTANCE_C = 25.0f; // 3 turns
    constexpr int32 GENERIC_DURATION_C = 3;

    constexpr float GENERIC_RESISTANCE_B = 30.0f; // 3 turns
    constexpr int32 GENERIC_DURATION_B = 3;

    constexpr float GENERIC_RESISTANCE_A = 35.0f; // 3 turns
    constexpr int32 GENERIC_DURATION_A = 3;

    constexpr float GENERIC_RESISTANCE_S = 40.0f; // 4 turns
    constexpr int32 GENERIC_DURATION_S = 4;

    // ==================== BROKEN DARKNESS ENERGY BONUSES ====================
    // Broken Darkness gains extra energy based on item tier
    constexpr int32 BD_ENERGY_F = 15;
    constexpr int32 BD_ENERGY_E = 20;
    constexpr int32 BD_ENERGY_D = 25;
    constexpr int32 BD_ENERGY_C = 30;
    constexpr int32 BD_ENERGY_B = 35;
    constexpr int32 BD_ENERGY_A = 40;
    constexpr int32 BD_ENERGY_S = 50;
}