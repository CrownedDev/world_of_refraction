// CombatGridConstants.h
// Constants for combat grid system

#pragma once

namespace CombatGridConstants
{
    // ==================== GRID DIMENSIONS ====================
    
    /** Number of rows per team (Back, Middle, Front) */
    constexpr int32 GRID_ROWS = 3;
    
    /** Slots per row. B-342 (D-134): three rows, no columns. A slot only places a character in the world;
     *  it is not a rule: a row holds the whole side (MAX_TEAM_SIZE), and nothing reads which slot.
     *  OBSOLETE as a column (B-342, D-134). */
    constexpr int32 GRID_COLUMNS = 3;

    /** OBSOLETE (B-342, D-134; delete in the D-124 sweep): positions per team (3x3 cells). */
    constexpr int32 POSITIONS_PER_TEAM = GRID_ROWS * GRID_COLUMNS; // 9
    
    /** Maximum team size (3v3) */
    constexpr int32 MAX_TEAM_SIZE = 3;
    
    // ==================== ROW EFFECTS (B-342, D-134) ====================
    // Placeholder: the front row deals AND takes +20%, the back row deals and takes -20%, the middle row
    // neither (Q-342: Final Fantasy's front / back row). Damage taken is applied as a defense modifier that
    // DamageMath divides by, so each DEFENSE_MODIFIER below is 1 / the row's damage-taken multiplier.

    /** Front row: damage dealt x1.20. */
    constexpr float FRONT_ROW_DAMAGE_DEALT_MULTIPLIER = 1.20f; // TODO: tune

    /** Front row: damage taken x1.20. */
    constexpr float FRONT_ROW_DAMAGE_TAKEN_MULTIPLIER = 1.20f; // TODO: tune

    /** Back row: damage dealt x0.80. */
    constexpr float BACK_ROW_DAMAGE_DEALT_MULTIPLIER = 0.80f; // TODO: tune

    /** Back row: damage taken x0.80. */
    constexpr float BACK_ROW_DAMAGE_TAKEN_MULTIPLIER = 0.80f; // TODO: tune

    /** Middle row: no effect. */
    constexpr float MIDDLE_ROW_MULTIPLIER = 1.00f;

    /** Front row damage modifier (multiplicative on damage dealt) */
    constexpr float FRONT_ROW_DAMAGE_MODIFIER = FRONT_ROW_DAMAGE_DEALT_MULTIPLIER;

    /** Front row defense modifier (damage taken is divided by it) */
    constexpr float FRONT_ROW_DEFENSE_MODIFIER = MIDDLE_ROW_MULTIPLIER / FRONT_ROW_DAMAGE_TAKEN_MULTIPLIER;

    /** Middle row damage modifier (no change) */
    constexpr float MIDDLE_ROW_DAMAGE_MODIFIER = MIDDLE_ROW_MULTIPLIER;

    /** Middle row defense modifier (no change) */
    constexpr float MIDDLE_ROW_DEFENSE_MODIFIER = MIDDLE_ROW_MULTIPLIER;

    /** Back row damage modifier (multiplicative on damage dealt) */
    constexpr float BACK_ROW_DAMAGE_MODIFIER = BACK_ROW_DAMAGE_DEALT_MULTIPLIER;

    /** Back row defense modifier (damage taken is divided by it) */
    constexpr float BACK_ROW_DEFENSE_MODIFIER = MIDDLE_ROW_MULTIPLIER / BACK_ROW_DAMAGE_TAKEN_MULTIPLIER;
    
    // ==================== WORLD POSITIONING ====================
    
    /** Distance between grid cell centers (world units) */
    constexpr float CELL_SPACING = 200.0f;
    
    /** Distance between teams (world units) */
    constexpr float TEAM_SEPARATION = 800.0f;
    
    /** Height offset for characters */
    constexpr float CHARACTER_HEIGHT_OFFSET = 0.0f;
}
