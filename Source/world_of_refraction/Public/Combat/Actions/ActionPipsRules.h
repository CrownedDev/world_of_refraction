// ActionPipsRules.h
// B-170a (ActionPips.md §2, Q-170): the numbers of banked actions. A character's Max Actions is a base
// plus one per whole block of action points (a composed, additive stat: gear, evolutions, buffs add up),
// with the leftover shown as progress ("Max 3 (+75/100)"). Reflex sets the per-turn regain (1 / 2 / 3), which
// never fills the bank past Max. Pure maths, no actor: the character component
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

	/** Actions regained per turn below the first Reflex threshold (Crown, Q-170). */
	constexpr int32 ACTIONS_REGAIN_BASE = 1; // TODO: tune
	/** The regain at or above each Reflex threshold. */
	constexpr int32 ACTIONS_REGAIN_AT_THRESHOLD_2 = 2; // TODO: tune
	constexpr int32 ACTIONS_REGAIN_AT_THRESHOLD_3 = 3; // TODO: tune
	/** Reflex's share of its window range (0..1) for a regain of 2, and of 3 (Crown, Q-170; 7bc136f4). */
	constexpr float ACTIONS_REGAIN_REFLEX_THRESHOLD_2 = 0.5f; // TODO: tune
	constexpr float ACTIONS_REGAIN_REFLEX_THRESHOLD_3 = 0.8f; // TODO: tune
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

	/** Reflex sets the regain (Q-170, Q-170h): 1, 2 at threshold 2, 3 at threshold 3 (ReflexShare 0..1). */
	inline int32 RegainForReflexShare(float ReflexShare)
	{
		if (ReflexShare >= ActionPipsConstants::ACTIONS_REGAIN_REFLEX_THRESHOLD_3)
		{
			return ActionPipsConstants::ACTIONS_REGAIN_AT_THRESHOLD_3;
		}
		if (ReflexShare >= ActionPipsConstants::ACTIONS_REGAIN_REFLEX_THRESHOLD_2)
		{
			return ActionPipsConstants::ACTIONS_REGAIN_AT_THRESHOLD_2;
		}
		return ActionPipsConstants::ACTIONS_REGAIN_BASE;
	}

	/** The bank after a turn's regain: it never fills past Max, and adds nothing at or above it, so actions
	 *  over Max from a gain stay banked (Q-170g as answered). */
	inline int32 BankAfterRegain(int32 Current, int32 Regain, int32 MaxActions)
	{
		return Current >= MaxActions ? Current : FMath::Min(Current + FMath::Max(0, Regain), MaxActions);
	}

	/** The bank after a gain of Amount: past Max only when the gain may pass it (Emerald, a Gain Action
	 *  effect, Q-170f / Q-170g); otherwise it stops at Max and never trims actions already over it. */
	inline int32 BankAfterGain(int32 Current, int32 Amount, int32 MaxActions, bool bMayPassMax)
	{
		const int32 Raised = Current + FMath::Max(0, Amount);
		return bMayPassMax ? Raised : FMath::Max(Current, FMath::Min(Raised, MaxActions));
	}

	/** A fight starts with the regain amount, capped at Max (Q-170). */
	inline int32 ActionsAtFightStart(int32 Regain, int32 MaxActions)
	{
		return FMath::Clamp(Regain, 0, MaxActions);
	}
}
