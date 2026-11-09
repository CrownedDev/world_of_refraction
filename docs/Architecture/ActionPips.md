# Action Pips (banked actions)

## Overview

D-088 as confirmed by Crown (2026-11-09): turns go round in a fixed rotation, and on each turn a character spends **banked actions**. Reflex sets how many come back each turn (1, 2 or 3), **Max Actions** (2 + one per 100 action points from gear, evolutions and the like) caps the regain, and unused actions carry over. The plan is `docs/analysis/ActionPips.md` (B-170a..h); rulings Q-170..Q-170h, build choices Q-170a.

**Built so far (B-170a, B-170b, B-170c, UNBUILT):** the state and the numbers on the character (B-170a); the fixed turn rotation in `UTurnManager` behind `wor.Turns.ActionPips` (B-170b, default 0); several actions per turn in the orchestrator, action costs on skills, spells and items, End Turn, free switching (B-170c). Default play is unchanged until Crown's PIE flips the switch.

## Architecture

- **`Public/Combat/Actions/ActionPipsRules.h`** (header only, pure):
  - `ActionPipsConstants`: `MAX_ACTIONS_BASE` 2, `ACTION_POINTS_PER_ACTION` 100, `MAX_ACTIONS_CAP` 5 (safety ceiling), `ACTIONS_REGAIN_BASE` 1, `ACTIONS_REGAIN_AT_THRESHOLD_2` 2 / `_3` 3, `ACTIONS_REGAIN_REFLEX_THRESHOLD_2` 0.5 / `_3` 0.8. All `// TODO: tune`.
  - `FWoRActionPointLayers`: gear + evolution + transient + debug, summed, never below 0 (action points add up; they are not a Pattern P sub-stat, Q-170a).
  - `ActionPipsRules`: `MaxActionsForPoints`, `ActionPointProgress`, `MaxActionsString` ("Max 3 (+75/100)"), `RegainForReflexShare`, `BankAfterRegain` (never past Max, nothing at or above it), `BankAfterGain` (past Max only when allowed; never trims), `ActionsAtFightStart`.
- **`UCharacterDataComponent`** (per fight, D-064):
  - `CurrentActions`: replicated (`OnRep_CurrentActions`), not saved.
  - Reads: `GetActionPointLayers`, `GetActionPoints`, `GetMaxActions`, `GetActionPointProgress`, `GetReflexRegainShare` ((`CalculateReflexWindowBonus` × `ReflexWindowGearFactor`) ÷ `WINDOW_CAP_SECONDS`, 0..1), `GetActionRegain`.
  - Writes (authority only, like the EP functions): `ResetActionsForFight`, `RegainActions`, `GainActions(N, bMayPassMax)`, `SpendActions(N)` (false when above the bank), `LoseActions(N)` (floors at 0). All go through `SetCurrentActions`, which broadcasts only on a change.
  - `DebugActionPoints` (transient): the debug layer.

- **Action cost** (B-170c, Q-170b, Q-170c2): `USkillDataBase::ActionCost` (int, default `ACTION_COST_DEFAULT` 1, `ClampMin 0`; `IsDataValid` warns above `MAX_ACTIONS_CAP`); items `CrystalEffectTable::GetActionCost` (all 1 for now); `FAction::GetActionCost()` reads them (Defend, End Turn, switch, Flee and a ritual's fire cost 0). `ActionPipsRules::IsCostAboveMax` / `CanAfford` are the check.
- **The turn** (B-170c): `ACombatOrchestrator` sets the fight-start bank, regains at each turn start (not on a first turn), skips a turn at 0, pays each action's cost and asks again until 0, End Turn (`EActionType::EndTurn`) or Defend. Details: `docs/Architecture/CombatOrchestrator.md`, "Action pips".
- **`wor.Turns.ActionPips`** (B-170b, `Private/Combat/TurnRotation.cpp`, `TurnRotation::IsActionPipsOn()`): the one switch for every part. `UTurnManager` reads it per fight; with 1 it runs `FTurnRotation` (sides alternate, a smaller side's seats rotate on). Details: `docs/Architecture/TurnManager.md`, "Fixed rotation".

## Integration Points

- `OnActionsChanged(CurrentActions, MaxActions)`: dynamic multicast on the component, fired on the server on each change and on clients by the OnRep. The placeholder pips (B-170e) bind to it.
- The orchestrator calls `ResetActionsForFight` at fight start, `RegainActions` in the turn-start step and `SpendActions` per action (B-170c). `ULoadoutComponent::GetTurnSwitchRefusedReason` is empty while the switch is 1 (switching free).
- Coming: the effects call `GainActions` / `LoseActions` (B-170d, in `BeginTurnActions` step 2 for the next-turn ones); the AI reads the bank (B-170f).

## Debug

`UActionPipsDebug` (`Debug/Combat/ActionPipsDebug.h`): `GetActionsString`, `PrintActions`.
- `wor.Actions.Print`: each combatant's `actions N / Max M (+P/100) | points (layers) | regain R (Reflex share S)`, then the current turn's spends (B-170c, `ACombatOrchestrator::GetTurnSpendsString`).
- `wor.Actions.AddPoints <N>`: debug action points on every combatant.
- `wor.Actions.Reset`: every combatant to its fight-start bank.

Specs: `WoR.Combat.ActionPips` (`Private/Tests/ActionPipsSpec.cpp`), `WoR.Turns.MultiAction` (`Private/Tests/MultiActionTurnSpec.cpp`, B-170c).

## Known Limitations / TODOs

- The gear and evolution layers read 0: no data field carries action points yet (Q-170a). A later job adds `BonusActionPoints` with the first authored item.
- Max Actions is computed where read, not replicated; replicate it (as `MaxEP`) once a source exists that a client may not hold.
- The `wor.Actions.Print` line has no "waiting next-turn gains and losses" yet; they come with the effects (B-170d).
- The AI takes one action per turn and banks the rest until B-170f; a stunned unit gets one action (Q-170c2).
- The turn-start skip at 0 can't happen in play before B-170d's waiting losses; "No actions" on the HUD is B-170e.
- Not listed in CLAUDE.md's "Existing documents" (that file is edited only by Crown's sessions); add it there.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | B-170a: created. Constants, action-point layers, Max Actions with progress, Reflex regain, the bank on `UCharacterDataComponent` (`CurrentActions`, replicated, not saved) with `OnActionsChanged`; `wor.Actions.Print` / `AddPoints` / `Reset`; `ActionPipsSpec`. No play change. | `feature/B-170a-action-pips-cloud` (UNBUILT) |
| 2026-11-09 | B-170a built and merged into dev (cloud branch + dev; full suite green, wor-reviewer: no must-fix). | `feature/B-170a-action-pips` |

| 2026-11-09 | B-170b: `wor.Turns.ActionPips` defined (`TurnRotation::IsActionPipsOn`); the fixed rotation in `UTurnManager` (see TurnManager.md). The bank is still not read by turns (B-170c). | `feature/B-170b-turn-rotation-cloud` (UNBUILT) |
| 2026-11-09 | B-170c: the bank plays. Action costs (`USkillDataBase::ActionCost`, items in `CrystalEffectTable`, `FAction::GetActionCost`, `ACTION_COST_DEFAULT`, `IsCostAboveMax` / `CanAfford`); the orchestrator's multi-action turn, `EActionType::EndTurn`, free switching; `wor.Actions.Print` shows the turn's spends; `MultiActionTurnSpec`. Build choices Q-170c2. | `feature/B-170c-multi-action-turns-cloud` (UNBUILT) |
