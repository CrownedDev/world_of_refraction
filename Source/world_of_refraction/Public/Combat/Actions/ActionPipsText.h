// ActionPipsText.h
// B-170e (ActionPips.md §3.5, Q-170e2): what the player sees of banked actions, as FText from plain numbers, so specs
// check it headless (WoR.Combat.ActionPipsText). The placeholder battle HUD's party cards (UBattleHUDWidget) show the
// pips and Max Actions line; the command menu (UCombatCommandMenuSubsystem) shows each skill, spell and item's cost
// and greys one the character can't pay; the orchestrator refuses with the same reason (GetActionCostRefusedReason).
// The numbers themselves are ActionPipsRules (B-170a / B-170c).

#pragma once

#include "CoreMinimal.h"

/** Whether a character can pay an action cost now (B-170c's two refusals, Q-170b). */
enum class EActionCostState : uint8
{
	Affordable,
	/** Above the bank now; banking more turns pays it. */
	NeedsMore,
	/** Above Max Actions: never payable, so the skill is unusable. */
	AboveMax
};

namespace ActionPipsText
{
	/** Pips drawn on one line at most; a bank past it (gains past Max) shows the number only. A display bound,
	 *  not a rule: Max Actions stops at ActionPipsConstants::MAX_ACTIONS_CAP. */
	constexpr int32 MAX_PIPS_SHOWN = 8;

	/** AboveMax first (it can never be paid), then NeedsMore, else Affordable. A cost of 0 is always Affordable. */
	WORLD_OF_REFRACTION_API EActionCostState GetCostState(int32 Cost, int32 CurrentActions, int32 MaxActions);

	/** The greyed button's reason, empty when Affordable: "Costs more actions than you can hold." or
	 *  "Needs 2 actions.". */
	WORLD_OF_REFRACTION_API FText GetCostRefusedReason(int32 Cost, int32 CurrentActions, int32 MaxActions);

	/** "Fireball (2 actions)": a skill, spell or item button's label with its cost (Expedition 33 shows every
	 *  skill's AP cost). Label alone for a cost of 0. */
	WORLD_OF_REFRACTION_API FText WithCost(const FText &Label, int32 Cost);

	/** Filled pips for the bank, empty ones up to Max: 2 of 3 -> "●●○". Actions past Max (a gain, or a lowered Max)
	 *  are bonus pips, drawn apart because they are lost at the end of the holder's turn (Q-170f2, Q-170f3, Crown):
	 *  4 with Max 3 -> "●●●◆". Empty when either side passes MAX_PIPS_SHOWN. */
	WORLD_OF_REFRACTION_API FText MakePips(int32 CurrentActions, int32 MaxActions);

	/** How many banked actions are bonus actions (above Max, lost at the holder's turn end); 0 at or below Max. */
	WORLD_OF_REFRACTION_API int32 GetBonusActions(int32 CurrentActions, int32 MaxActions);

	/** "Max 3 (+75/100)": Max Actions with the action points toward the next one (UCharacterDataComponent::
	 *  GetActionPointProgress, Q-170); "Max 5" at ActionPipsConstants::MAX_ACTIONS_CAP, where more points add nothing. */
	WORLD_OF_REFRACTION_API FText MakeMaxActionsText(int32 MaxActions, int32 ActionPointProgress);

	/** The party card's line: "Actions ●●○ 2 · Max 3 (+75/100)" (the pips, then the number, as the deckbuilder's
	 *  "ACT 2"). With bonus actions it says they expire: "Actions ●●●◆ 4 · Max 3 (+75/100) · 1 bonus, lost at turn end". */
	WORLD_OF_REFRACTION_API FText MakeActionsLine(int32 CurrentActions, int32 MaxActions, int32 ActionPointProgress);

	/** Defend's tooltip in an action-pips fight: "Ends your turn." (§3.5). */
	WORLD_OF_REFRACTION_API FText GetDefendEndsTurnText();

	/** End Turn's tooltip: "Ends your turn; the actions left stay banked." */
	WORLD_OF_REFRACTION_API FText GetEndTurnDescription();

	/** True when this fight banks actions: the server's TurnManager mode (latched at fight start). On a co-op client,
	 *  which runs no TurnManager, the wor.Turns.ActionPips switch as it is now.
	 *  TODO: replicate the fight's mode on the turn state so a client never reads a changed switch mid-fight. */
	WORLD_OF_REFRACTION_API bool IsActionPipsFight(const UObject *Context);

	/** B-170e (Crown, 2026-11-12): the main menu shows End Turn in an action-pips fight unless this machine's player
	 *  hid it (USaveSubsystem::IsEndTurnButtonHidden, default shown). Hidden, the turn still ends at 0 actions and on
	 *  Defend. */
	WORLD_OF_REFRACTION_API bool ShouldShowEndTurnButton(const UObject *Context);
}
