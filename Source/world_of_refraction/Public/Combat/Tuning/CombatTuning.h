// CombatTuning.h
// B-218 (StatMathsRedesign.md §3, Q-146): the one accessor for combat tuning numbers that live in
// data. Light on purpose (no UObject include), so the ladder headers can call it.
// Ladders are curve-table rows read through FScalableFloat on UCombatTuningData; single values are
// that asset's fields (UCombatTuningData.h, WoRTuning::Get()). With no asset, or no curve on a
// ladder, every call returns the C++ value passed in, so a missing asset can't change the game.
// Debug: UCombatTuningDebug (wor.PrintCombatTuning, wor.CompareCombatTuning).

#pragma once

#include "CoreMinimal.h"

/** The curve-table ladders. Each row's key is the integer the C++ ladder is keyed by. */
enum class ECombatLadder : uint8
{
	TierPower,      // key: item tier index, F = 0 .. S = 6
	TierGapDamage,  // key: tier gap (action - channel), -4 .. +4 (beyond holds the end value)
	TierGapCost,    // key: tier gap, -4 .. +4
	RequirementGap, // key: requirement gap (required - character level), -5 .. +5
	ScalingGrade,   // key: EScalingTier value, S = 0 .. F = 6
};

namespace WoRTuning
{
	/** The ladder's value at Key from the active tuning asset, else CodeValue. */
	WORLD_OF_REFRACTION_API float GetLadderValue(ECombatLadder Ladder, int32 Key, float CodeValue);
}
