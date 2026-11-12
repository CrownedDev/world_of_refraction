# Action Pips (banked actions)

## Overview

D-088 as confirmed by Crown (2026-11-09): turns go round in a fixed rotation, and on each turn a character spends **banked actions**. Reflex sets how many come back each turn (1 + one per whole step of Reflex value, B-170i / Q-170i), **Max Actions** (2 + one per 100 action points from gear, evolutions and the like) caps the regain, and unused actions carry over. The plan is `docs/analysis/ActionPips.md` (B-170a..h); rulings Q-170..Q-170h, build choices Q-170a.

**Built so far (B-170a, B-170b, B-170c; B-170e written, unbuilt):** the state and the numbers on the character (B-170a); the fixed turn rotation in `UTurnManager` behind `wor.Turns.ActionPips` (B-170b, default 0); several actions per turn in the orchestrator, action costs on skills, spells and items, End Turn, free switching (B-170c); the placeholder UI on B-391's new HUD: pips and Max Actions on party cards, End Turn button and key, costs shown and greyed (B-170e). Default play is unchanged until Crown's PIE flips the switch.
**Built so far (B-170a, B-170b, B-170c; B-170d and B-170f written):** the state and the numbers on the character (B-170a); the fixed turn rotation in `UTurnManager` behind `wor.Turns.ActionPips` (B-170b, default 0); several actions per turn in the orchestrator, action costs on skills, spells and items, End Turn, free switching (B-170c); effects that give or take actions (B-170d, unbuilt); the AI spending or banking (B-170f, unbuilt). Default play is unchanged until Crown's PIE flips the switch.

## Architecture

- **`Public/Combat/Actions/ActionPipsRules.h`** (header only, pure):
  - `ActionPipsConstants`: `MAX_ACTIONS_BASE` 2, `ACTION_POINTS_PER_ACTION` 100, `MAX_ACTIONS_CAP` 5 (safety ceiling), `ACTIONS_REGAIN_BASE` 1, `ACTIONS_REGAIN_AT_THRESHOLD_2` 2 / `_3` 3, `ACTIONS_REGAIN_REFLEX_THRESHOLD_2` 0.5 / `_3` 0.8. All `// TODO: tune`.
  - `FWoRActionPointLayers`: gear + evolution + transient + debug, summed, never below 0 (action points add up; they are not a Pattern P sub-stat, Q-170a).
  - `ActionPipsRules`: `MaxActionsForPoints`, `ActionPointProgress`, `MaxActionsString` ("Max 3 (+75/100)"), `RegainForReflexValue` (1 + ⌊value ÷ step⌋, at most `MAX_ACTIONS_CAP`), `NextRegainReflexValue`, `RegainString` ("Regain 2 (Reflex 18.0 / 30.0)"), `RegainForReflexShare` (OBSOLETE since B-170i, kept until PIE), `BankAfterRegain` (never past Max, nothing at or above it), `BankAfterGain` (past Max only when allowed; never trims), `ActionsAtFightStart`.
- **`UCharacterDataComponent`** (per fight, D-064):
  - `CurrentActions`: replicated (`OnRep_CurrentActions`), not saved.
  - Reads: `GetActionPointLayers`, `GetActionPoints`, `GetMaxActions`, `GetActionPointProgress`, `GetRegainReflexValue` ((own `Reflex` + gear `BonusReflex`, tier worth applied in `GetActiveStatBonus`) × `GetEvolutionModifiedBody`), `GetRegainReflexStep` (`REGAIN_STEP` × `GetEffectiveBodyForLevel(GetWorldBody())`), `GetActionRegain`, `GetActionRegainString`; `GetReflexRegainShare` is OBSOLETE since B-170i (kept until PIE).
  - Writes (authority only, like the EP functions): `ResetActionsForFight`, `RegainActions`, `GainActions(N, bMayPassMax)`, `SpendActions(N)` (false when above the bank), `LoseActions(N)` (floors at 0). All go through `SetCurrentActions`, which broadcasts only on a change.
  - `DebugActionPoints` (transient): the debug layer.

- **Action cost** (B-170c, Q-170b, Q-170c2): `USkillDataBase::ActionCost` (int, default `ACTION_COST_DEFAULT` 1, `ClampMin 1`; `IsDataValid` warns above `MAX_ACTIONS_CAP`); items `CrystalEffectTable::GetActionCost` (all 1 for now); `FAction::GetActionCost()` reads them (Defend, End Turn, switch, Flee and a ritual's fire cost 0). `ActionPipsRules::IsCostAboveMax` / `CanAfford` are the check.
- **The turn** (B-170c): `ACombatOrchestrator` sets the fight-start bank, regains at each turn start (not on a first turn), skips a turn at 0, pays each action's cost and asks again until 0, End Turn (`EActionType::EndTurn`) or Defend. Details: `docs/Architecture/CombatOrchestrator.md`, "Action pips".
- **Player-facing text** (B-170e, Q-170e2): `Public/Combat/Actions/ActionPipsText.h`: `GetCostState` / `GetCostRefusedReason` (the orchestrator's refusal and the menu's greyed reason, one copy), `WithCost` ("Fireball (2 actions)"), `MakePips` ("●●○"), `MakeMaxActionsText` ("Max 3 (+75/100)", "Max 5" at the cap), `MakeActionsLine`, the Defend / End Turn tooltips, and `IsActionPipsFight(Context)` (the TurnManager's latched mode; a co-op client reads the switch). Shared words "Actions" / "End Turn" in `WoRGameNames`.
- **Placeholder UI** (B-170e): `UBattleHUDWidget` party cards show `MakeActionsLine` under EP, bound to `OnActionsChanged`; `UCombatCommandMenuSubsystem` adds the End Turn button (`EPieMenuCategory::EndTurn`, submits `EActionType::EndTurn` through `SubmitConfirmedAction`) and `ShowActionCost` on Attack / ability / spell / item buttons; key `IA_Combat_MenuEndTurn` (Backspace / View). Details: `UISystem.md`.
- **`wor.Turns.ActionPips`** (B-170b, `Private/Combat/TurnRotation.cpp`, `TurnRotation::IsActionPipsOn()`): the one switch for every part. `UTurnManager` reads it per fight; with 1 it runs `FTurnRotation` (sides alternate, a smaller side's seats rotate on). Details: `docs/Architecture/TurnManager.md`, "Fixed rotation".

## Integration Points

- `OnActionsChanged(CurrentActions, MaxActions)`: dynamic multicast on the component, fired on the server on each change and on clients by the OnRep. The placeholder pips (B-170e, `UBattleHUDWidget`) bind to it.
- The orchestrator calls `ResetActionsForFight` at fight start, `RegainActions` in the turn-start step and `SpendActions` per action (B-170c). `ULoadoutComponent::GetTurnSwitchRefusedReason` is empty while the switch is 1 (switching free).
- B-170d: Gain / Lose Action "now" call `GainActions` / `LoseActions` (`USkillEffectManager::ApplyEffectLogic`); timed ones, Wind's break and a delayed Emerald gain call `AddWaitingActionChange`, and `BeginTurnActions` step 2 lands them (`ApplyWaitingActionChanges`, gains before losses). The orchestrator clears them at every fight start and end (`ClearWaitingActionChanges`, D-064). `GetMaxActions` adds the net `MaxActionsBuff` − `MaxActionsDebuff` (floor `MAX_ACTIONS_FLOOR` 1). The action executor passes an action effect's authored Value through unscaled (`SkillEffectClassification::CountsWholeActions`). Coming: the AI reads the bank and scores the action effects and Emerald (B-170f).

## Debug

`UActionPipsDebug` (`Debug/Combat/ActionPipsDebug.h`): `GetActionsString`, `PrintActions`.
- `wor.Actions.Print`: each combatant's `actions N / Max M (from points: Max P (+x/100), effects ±E) | points (layers) | Regain R (Reflex value / next) | waiting (+1 next turn, …)`, then the current turn's spends (B-170c, `ACombatOrchestrator::GetTurnSpendsString`).
- `wor.Actions.AddPoints <N>`: debug action points on every combatant.
- `wor.Actions.Reset`: every combatant to its fight-start bank.
- `wor.Data.ListTurnSpeed` (B-170g, `Debug/Character/TurnSpeedRetireDebug.h`): every asset that still authors the retired Turn Speed family, and what (the list B-170h works from).

Specs: `WoR.Combat.ActionPips` (`Private/Tests/ActionPipsSpec.cpp`), `WoR.Turns.MultiAction` (`Private/Tests/MultiActionTurnSpec.cpp`, B-170c), `WoR.Combat.ActionEffects` (`Private/Tests/ActionEffectsSpec.cpp`, B-170d).
- `wor.UI.Print` (B-170e): the battle HUD's `actionPips=on/off` and each party card's actions line.
Specs: `WoR.Combat.ActionPips` (`Private/Tests/ActionPipsSpec.cpp`), `WoR.Turns.MultiAction` (`Private/Tests/MultiActionTurnSpec.cpp`, B-170c), `WoR.Combat.ActionPipsText` (`Private/Tests/ActionPipsTextSpec.cpp`, B-170e); the UI cases in `WoR.UI.BattleHUD` and `WoR.UI.CommandMenu.Keys` (B-170e).
Specs: `WoR.Combat.ActionPips` (`Private/Tests/ActionPipsSpec.cpp`), `WoR.Turns.MultiAction` (`Private/Tests/MultiActionTurnSpec.cpp`, B-170c), `WoR.Combat.ActionEffects` (`Private/Tests/ActionEffectsSpec.cpp`, B-170d), `WoR.Data.TurnSpeedRetire` (`Private/Tests/TurnSpeedRetireSpec.cpp`, B-170g).

## Known Limitations / TODOs

- The gear and evolution layers read 0: no data field carries action points yet (Q-170a). A later job adds `BonusActionPoints` with the first authored item.
- Max Actions is computed where read, not replicated; replicate it (as `MaxEP`) once a source exists that a client may not hold.
- A turn removed by `SkipNextTurn` (Emerald on an enemy) is not a turn start, so waiting changes don't count down on it: a waiting Wind loss lands at the next turn the holder really starts (Q-170d2). Waiting changes can't be cleansed.
- The AI spends or banks per `AI/AIActionPips.h` (B-170f, Q-170f2; AISystem.md "Action pips"); a stunned or Void-broken unit gets one action (Q-170c2). The AI never gives an Emerald to an ally other than itself.
- "No actions" on the HUD is B-170e (a waiting loss or Wind's break can now empty a bank at turn start, so the skip happens in play).
- The turn-start skip at 0 can't happen in play before B-170d's waiting losses; it is logged, not shown (B-170e shows the empty pips, not a "No actions" banner).
- A co-op client reads `wor.Turns.ActionPips` for the fight's mode (no TurnManager there); replicate the mode on the turn state (TODO in `ActionPipsText::IsActionPipsFight`).
- The old WBP HUD (`wor.UI.NewHUD 0`) gets the End Turn button and cost labels (same subsystem buttons) but no pips line.
- Turn Speed is retired for new authoring only (B-170g, Q-170g2): validation **warns** on its effect types, skill scaling, character points and gear / evolution bonuses while the speed timeline still reads them at 0; gear rolls still roll it. B-170h deletes the reads (`OBSOLETE (B-170h)` flags: `ESkillEffectType` Turn Speed entries, `ESubStat::TurnSpeed`, `EItemEffectType::BuffTurnSpeed`, `UCharacterData::TurnSpeed` and its getters, `FEquipmentStatBonus::BonusTurnSpeed`, the GAS `TurnSpeed` attribute) and makes the warnings errors.
- Not listed in CLAUDE.md's "Existing documents" (that file is edited only by Crown's sessions); add it there.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | B-170a: created. Constants, action-point layers, Max Actions with progress, Reflex regain, the bank on `UCharacterDataComponent` (`CurrentActions`, replicated, not saved) with `OnActionsChanged`; `wor.Actions.Print` / `AddPoints` / `Reset`; `ActionPipsSpec`. No play change. | `feature/B-170a-action-pips-cloud` (UNBUILT) |
| 2026-11-09 | B-170a built and merged into dev (cloud branch + dev; full suite green, wor-reviewer: no must-fix). | `feature/B-170a-action-pips` |
| 2026-11-09 | B-170b: `wor.Turns.ActionPips` defined (`TurnRotation::IsActionPipsOn`); the fixed rotation in `UTurnManager` (see TurnManager.md). The bank is still not read by turns (B-170c). | `feature/B-170b-turn-rotation-cloud` (UNBUILT) |
| 2026-11-09 | B-170b built and merged into dev (cloud branch + dev; full suite green, wor-reviewer: no must-fix). | `feature/B-170b-turn-rotation` |
| 2026-11-09 | B-170c: the bank plays. Action costs (`USkillDataBase::ActionCost`, items in `CrystalEffectTable`, `FAction::GetActionCost`, `ACTION_COST_DEFAULT`, `IsCostAboveMax` / `CanAfford`); the orchestrator's multi-action turn, `EActionType::EndTurn`, free switching; `wor.Actions.Print` shows the turn's spends; `MultiActionTurnSpec`. Build choices Q-170c2. | `feature/B-170c-multi-action-turns-cloud` (UNBUILT) |
| 2026-11-09 | B-170c built and merged into dev (cloud branch + dev; full suite green, wor-reviewer: no code must-fix). Build fixes: an armed ritual shows on the strip at once (`ScheduleExecutionTurn` rebuilds the belt); a refused Flee spends nothing; Q-170c2: a bonus turn regains like any turn. | `feature/B-170c-multi-action-turns` |
| 2026-11-09 | B-170d: effects on the bank. The four action effect types (Gain / Lose Action now or at the Nth turn start, Max Actions Up / Down), waiting changes on `UCharacterDataComponent` (`AddWaitingActionChange`, `ApplyWaitingActionChanges`, `ClearWaitingActionChanges`, per fight), `GetMaxActions` with the Max effects and `MAX_ACTIONS_FLOOR`; the action-pips break rows (Ice −1 now, Wind −1 next turn); Emerald's ally ladder; authored Value passed through the action executor unscaled; `EffectDefinition` validation warnings; `wor.Actions.Print` shows the live Max and waiting changes; `ActionEffectsSpec`. Build choices Q-170d2. | `feature/B-170d-action-effects-cloud` (built 2026-11-12, in dev) |
| 2026-11-12 | B-170i: Crown's Reflex regain rework (Q-170i). Regain = 1 + floor(Reflex value / step), at most `MAX_ACTIONS_CAP`; value = (own + gear Reflex points) x `GetEvolutionModifiedBody`; step = `REGAIN_STEP` (15, TODO: tune) x the natural Body multiplier, so levelling Body cancels. `wor.Actions.Print` shows the progress. The old share thresholds are OBSOLETE, kept until PIE; the defence window is unchanged. `ActionPipsSpec` cases. | `feature/B-170i-reflex-regain` |
| 2026-11-09 | B-170e: placeholder UI on B-391's HUD: `ActionPipsText` (pips, Max line, cost label and reasons; the orchestrator's refusal uses it), party-card pips line from `OnActionsChanged`, End Turn button and key (Backspace / View), costs on every skill / spell / item and greyed with the reason, Defend's tooltip; build choices Q-170e2. | `feature/B-170e-action-pips-ui-cloud` (UNBUILT, on B-391) |
| 2026-11-09 | B-170d: effects on the bank. The four action effect types (Gain / Lose Action now or at the Nth turn start, Max Actions Up / Down), waiting changes on `UCharacterDataComponent` (`AddWaitingActionChange`, `ApplyWaitingActionChanges`, `ClearWaitingActionChanges`, per fight), `GetMaxActions` with the Max effects and `MAX_ACTIONS_FLOOR`; the action-pips break rows (Ice −1 now, Wind −1 next turn); Emerald's ally ladder; authored Value passed through the action executor unscaled; `EffectDefinition` validation warnings; `wor.Actions.Print` shows the live Max and waiting changes; `ActionEffectsSpec`. Build choices Q-170d2. | `feature/B-170d-action-effects-cloud` (UNBUILT) |
| 2026-11-09 | B-170f: the AI plays the bank. Asked again while it has actions (`ACombatOrchestrator::CanContinueTurn`), it spends or banks (`AI/AIActionPips.h` `DecideSpendOrBank`, End Turn), skips picks the bank can't pay, values the action effects and Emerald's new rule; `AIActionPipsSpec`. Build choices Q-170f2. | `feature/B-170f-ai-actions-cloud` (UNBUILT) |
| 2026-11-09 | B-170g: Turn Speed retired for new authoring, nothing removed. `SkillEffectClassification::IsTurnSpeedFamily`; `IsDataValid` warnings on Turn Speed effects (`UEffectDefinition`), skill `StatScaling` (`USkillDataBase`), character points (`UCharacterData`), gear and evolution `BonusTurnSpeed`; "(retired)" Details names; OBSOLETE (B-170h) flags; `wor.Data.ListTurnSpeed`; `TurnSpeedRetireSpec`; rules pages fold in the action-pips rows. Build choices Q-170g2. No play change. | `feature/B-170g-turn-speed-retire-cloud` (UNBUILT) |
