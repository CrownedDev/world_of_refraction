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