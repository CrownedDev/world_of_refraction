// AIActionPips.h
// B-170f (ActionPips.md §3.6, Q-170f2): how the AI spends or banks its actions in an action-pips fight
// (wor.Turns.ActionPips 1). The AI is asked once per action; after each pick it either spends (submits the
// pick) or banks (End Turn: the actions left stay for its next turn). Pure rules, no actor: UAIDecisionManager
// gathers the inputs and asks DecideSpendOrBank, so a spec checks every branch headless.
// Debug: FAISpendOrBankDecision::ToString(), printed in the AI's per-decision Verbose line and kept in
// UAIDecisionManager::GetLastSpendOrBankString().

#pragma once

#include "CoreMinimal.h"

namespace AIActionPipsConstants
{
	/** The AI spends while its best pick scores above this (HP-damage units); a pick worth nothing banks.
	 *  The deckbuilder's Medium AI leaves out moves worth nothing, then ends the turn (MatchAi.MediumPlan). */
	constexpr float AI_END_TURN_SCORE_FLOOR = 0.0f; // TODO: tune

	/** The AI banks for a held pick (it costs more actions than the bank holds, but no more than Max) when the held
	 *  pick's score times this beats the best pick it can pay now: one turn's wait, discounted. */
	constexpr float AI_BANK_FOR_HELD_PICK_FACTOR = 0.5f; // TODO: tune

	/** Easy ends its turn early this often after its first action of a turn (the deckbuilder's Easy AI: EasyStopPercent 30). */
	constexpr float AI_EASY_END_TURN_EARLY_CHANCE = 0.3f; // TODO: tune

	/** One action gained or lost is worth this many of the unit's best single actions (ActionPips.md §3.6: "one action
	 *  of that unit's threat"). */
	constexpr float AI_ACTION_WORTH_IN_ACTIONS = 1.0f; // TODO: tune

	/** One Max Action up or down for its span is worth this share of an action (it only matters once the bank is full). */
	constexpr float AI_MAX_ACTION_WORTH_IN_ACTIONS = 0.5f; // TODO: tune
}

/** What the AI does with the pick it just built. */
enum class EAISpendOrBank : uint8
{
	Spend,
	Bank
};

/** What DecideSpendOrBank reads. Scores are in HP-damage units, as the AI's other scores. */
struct FAISpendOrBankInput
{
	int32 CurrentActions = 0;
	int32 MaxActions = 0;
	/** The pick's action cost (0 for Defend). */
	int32 PickActionCost = 0;
	/** The pick's score. Read only when bPickIsScored. */
	float PickScore = 0.0f;
	/** False for picks the AI makes by need, not by score (heal, cleanse, Defend, the fallback): always spent. */
	bool bPickIsScored = false;
	/** Defend: it ends the turn itself and keeps the rest banked, so there is nothing to decide. */
	bool bPickEndsTurn = false;
	/** The best pick that costs more actions than the bank holds but no more than Max (0 cost = none). */
	int32 HeldActionCost = 0;
	float HeldScore = 0.0f;
	/** Easy: random picks, no planning; may stop early after its first action. */
	bool bEasy = false;
	/** The AI already acted this turn. */
	bool bActedThisTurn = false;
	/** A battle-random roll in [0, 1), for Easy's early stop. */
	float EasyRoll = 1.0f;
};

struct FAISpendOrBankDecision
{
	EAISpendOrBank Choice = EAISpendOrBank::Spend;
	FString Reason;

	FString ToString() const
	{
		return FString::Printf(TEXT("%s (%s)"), Choice == EAISpendOrBank::Bank ? TEXT("bank") : TEXT("spend"), *Reason);
	}
};

namespace AIActionPips
{
	/** A held pick exists: it costs more than the bank, no more than Max, so banking can reach it. */
	inline bool HasHeldPick(const FAISpendOrBankInput &In)
	{
		return In.HeldActionCost > In.CurrentActions && In.HeldActionCost <= In.MaxActions;
	}

	/** B-170f (ActionPips.md §3.6, Q-170f2). In order:
	 *  1. Defend spends (it ends the turn and banks the rest itself).
	 *  2. Easy spends, but after its first action of a turn stops early AI_EASY_END_TURN_EARLY_CHANCE of the time.
	 *  3. A pick made by need (heal, cleanse, fallback) spends.
	 *  4. A held pick whose score x AI_BANK_FOR_HELD_PICK_FACTOR beats the pick now: bank for it.
	 *  5. A pick at or under AI_END_TURN_SCORE_FLOOR banks, unless the bank is already at Max (banking there
	 *     wastes the regain, and an AI that never acts would stall the fight).
	 *  6. Otherwise spend. */
	inline FAISpendOrBankDecision DecideSpendOrBank(const FAISpendOrBankInput &In)
	{
		using namespace AIActionPipsConstants;
		FAISpendOrBankDecision Out;
		if (In.bPickEndsTurn)
		{
			Out.Reason = FString::Printf(TEXT("Defend ends the turn, %d banked"), In.CurrentActions);
			return Out;
		}
		if (In.bEasy)
		{
			if (In.bActedThisTurn && In.EasyRoll < AI_EASY_END_TURN_EARLY_CHANCE)
			{
				Out.Choice = EAISpendOrBank::Bank;
				Out.Reason = FString::Printf(TEXT("Easy stops early, %d banked"), In.CurrentActions);
				return Out;
			}
			Out.Reason = TEXT("Easy spends");
			return Out;
		}
		if (!In.bPickIsScored)
		{
			Out.Reason = TEXT("a pick by need (heal, cleanse or fallback)");
			return Out;
		}
		if (HasHeldPick(In) && In.HeldScore * AI_BANK_FOR_HELD_PICK_FACTOR > In.PickScore)
		{
			Out.Choice = EAISpendOrBank::Bank;
			Out.Reason = FString::Printf(TEXT("banks %d for a cost-%d pick (%.0f vs %.0f now)"), In.CurrentActions,
										 In.HeldActionCost, In.HeldScore, In.PickScore);
			return Out;
		}
		if (In.PickScore <= AI_END_TURN_SCORE_FLOOR && In.CurrentActions < In.MaxActions)
		{
			Out.Choice = EAISpendOrBank::Bank;
			Out.Reason = FString::Printf(TEXT("best pick worth %.0f, %d banked"), In.PickScore, In.CurrentActions);
			return Out;
		}
		Out.Reason = FString::Printf(TEXT("pick worth %.0f, cost %d of %d"), In.PickScore, In.PickActionCost, In.CurrentActions);
		return Out;
	}

	/** The worth of an action change on a unit, in HP-damage units: Actions x one action of the unit's threat
	 *  (ActionPerUnitThreat, its best single action), x the share for a Max Actions change. Positive when it helps
	 *  the AI's side (a gain on its own side, a loss on a foe), negative otherwise. */
	inline float ActionChangeWorth(int32 Actions, float ActionPerUnitThreat, bool bOnOwnSide, bool bGain, bool bMaxChange)
	{
		using namespace AIActionPipsConstants;
		const float Share = bMaxChange ? AI_MAX_ACTION_WORTH_IN_ACTIONS : AI_ACTION_WORTH_IN_ACTIONS;
		const float Worth = FMath::Max(0, Actions) * FMath::Max(0.0f, ActionPerUnitThreat) * Share;
		return bOnOwnSide == bGain ? Worth : -Worth;
	}
}
