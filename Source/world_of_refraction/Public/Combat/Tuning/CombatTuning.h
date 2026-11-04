// CombatTuning.h
// B-218 (StatMathsRedesign.md §3, Q-146): the one accessor for combat tuning numbers that live in
// data. Light on purpose (no UObject include), so the ladder headers can call it.
// Ladders are curve-table rows read through FScalableFloat on UCombatTuningData; single values are
// that asset's fields (UCombatTuningData.h, WoRTuning::Get()). With no asset, or no curve on a
// ladder, every call returns the C++ value passed in, so a missing asset can't change the game.
// Debug: UCombatTuningDebug (wor.PrintCombatTuning, wor.CompareCombatTuning).

#pragma once

#include "CoreMinimal.h"

/** The curve-table ladders. Each row's key is the integer the C++ ladder is keyed by. Append only:
 *  the enum value is not serialized, but the row names (CombatTuningData.cpp) are. */
enum class ECombatLadder : uint8
{
	TierPower,      // key: item tier index, F = 0 .. S = 6
	TierGapDamage,  // key: tier gap (action - channel), -4 .. +4 (beyond holds the end value)
	TierGapCost,    // key: tier gap, -4 .. +4
	RequirementGap, // key: requirement gap (required - character level), -5 .. +5
	ScalingGrade,   // key: EScalingTier value, S = 0 .. F = 6
	// B-224 (BaselineNumbers.md "Decided"; C++ tables in CombatBaselineConstants.h):
	PillarCurve,            // key: live pillar level 0 .. 7 (the soft cap, 0 at 0, 1 at 7)
	WeaponReferencePower,   // key: item tier index, F = 0 .. S = 6 (F 40 .. S 192)
	WeaponRequirementByTier, // key: item tier index, F = 0 .. S = 6 (main-pillar level: F0 E1 D2 C3 B5 A6 S7)
	EnemyRankHealth,        // key: EEncounterRank value (Normal 1, Mini-boss 4, Boss 8, Elite 1.5)
	// B-223 (D-102): pillar level drops.
	LevelDropChance,        // key: enemy tier index, F = 0 .. S = 6 (a normal enemy's chance to drop a level)
	MiniBossLevels,         // key: enemy tier index, F = 0 .. S = 6 (levels a mini-boss or boss always gives)
};

namespace WoRTuning
{
	/** The ladder's value at Key from the active tuning asset, else CodeValue. */
	WORLD_OF_REFRACTION_API float GetLadderValue(ECombatLadder Ladder, int32 Key, float CodeValue);
}
