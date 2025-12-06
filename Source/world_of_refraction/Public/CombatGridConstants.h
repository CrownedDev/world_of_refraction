// CombatGridConstants.h
// Constants for combat grid system

#pragma once

namespace CombatGridConstants
{
    // ==================== GRID DIMENSIONS ====================
    
    /** Number of rows per team (Back, Middle, Front) */
    constexpr int32 GRID_ROWS = 3;
    
    /** Number of columns per team */
    constexpr int32 GRID_COLUMNS = 3;
    
    /** Total positions per team */
    constexpr int32 POSITIONS_PER_TEAM = GRID_ROWS * GRID_COLUMNS; // 9
    
    /** Maximum team size (3v3) */
    constexpr int32 MAX_TEAM_SIZE = 3;
    
    // ==================== ROW MODIFIERS ====================
    
    /** Front row damage bonus (multiplicative) */
    constexpr float FRONT_ROW_DAMAGE_MODIFIER = 1.05f;  // +5%
    
    /** Front row defense penalty (multiplicative) */
    constexpr float FRONT_ROW_DEFENSE_MODIFIER = 0.95f; // -5%
    
    /** Middle row damage modifier (no change) */
    constexpr float MIDDLE_ROW_DAMAGE_MODIFIER = 1.00f; // 0%
    
    /** Middle row defense modifier (no change) */
    constexpr float MIDDLE_ROW_DEFENSE_MODIFIER = 1.00f; // 0%
    
    /** Back row damage penalty (multiplicative) */
    constexpr float BACK_ROW_DAMAGE_MODIFIER = 0.95f;   // -5%
    
    /** Back row defense bonus (multiplicative) */
    constexpr float BACK_ROW_DEFENSE_MODIFIER = 1.05f;  // +5%
    
    // ==================== WORLD POSITIONING ====================
    
    /** Distance between grid cell centers (world units) */
    constexpr float CELL_SPACING = 200.0f;
    
    /** Distance between teams (world units) */
    constexpr float TEAM_SEPARATION = 800.0f;
    
    /** Height offset for characters */
    constexpr float CHARACTER_HEIGHT_OFFSET = 0.0f;
}
