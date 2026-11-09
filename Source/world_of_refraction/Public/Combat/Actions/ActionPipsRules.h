// ActionPipsRules.h
// B-170a (ActionPips.md §2, Q-170): the numbers of banked actions. A character's Max Actions is a base
// plus one per whole block of action points (a composed, additive stat: gear, evolutions, buffs add up),
// with the leftover shown as progress ("Max 3 (+75/100)"). Pure maths, no actor: the character component
// reads its sources and asks these functions. Debug: UActionPipsDebug (wor.Actions.Print).

#pragma once

#include "CoreMinimal.h"

namespace ActionPipsConstants
{
	/** Max Actions with no action points (Crown, Q-170). */
	constexpr int32 MAX_ACTIONS_BASE = 2; // TODO: tune
	/** Action points per extra Max Action; only whole blocks count (Crown, Q-170). */
	constexpr int32 ACTION_POINTS_PER_ACTION = 100; // TODO: tune
	/** A safety ceiling on Max Actions, not a design limit; raise it if gear needs more (Q-170). */
	constexpr int32 MAX_ACTIONS_CAP = 5; // TODO: tune
}

/** B-170a (Q-170a): action points are additive (+25 and +50 make +75), unlike the multiplied Pattern P
 *  sub-stats in FWoRStatLayers, so they compose as a sum, never below 0. */
struct FWoRActionPointLayers
{
	/** The equipped gear's action points. */
	int32 Gear = 0;
	/** The slotted evolutions' action points. */
	int32 Evolution = 0;
	/** Timed buffs and debuffs on action points. */
	int32 Transient = 0;
	/** Added by wor.Actions.AddPoints, for PIE checks before any asset authors action points. */
	int32 Debug = 0;

	int32 Compose() const
	{
		return FMath::Max(0, Gear + Evolution + Transient + Debug);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("gear %d + evolution %d + transient %d + debug %d = %d"), Gear, Evolution, Transient, Debug,
							   Compose());
	}
};

namespace ActionPipsRules
{
	/** MAX_ACTIONS_BASE + one per whole ACTION_POINTS_PER_ACTION, at most MAX_ACTIONS_CAP. */
	inline int32 MaxActionsForPoints(int32 ActionPoints)
	{
		const int32 WholeActions = FMath::Max(0, ActionPoints) / ActionPipsConstants::ACTION_POINTS_PER_ACTION;
		return FMath::Min(ActionPipsConstants::MAX_ACTIONS_BASE + WholeActions, ActionPipsConstants::MAX_ACTIONS_CAP);
	}

	/** The points toward the next Max Action (175 -> 75). */
	inline int32 ActionPointProgress(int32 ActionPoints)
	{
		return FMath::Max(0, ActionPoints) % ActionPipsConstants::ACTION_POINTS_PER_ACTION;
	}

	/** "Max 3 (+75/100)", the stat screen's line (Q-170); a debug string, the player-facing FText comes with B-170e. */
	inline FString MaxActionsString(int32 ActionPoints)
	{
		return FString::Printf(TEXT("Max %d (+%d/%d)"), MaxActionsForPoints(ActionPoints), ActionPointProgress(ActionPoints),
							   ActionPipsConstants::ACTION_POINTS_PER_ACTION);
	}
}
