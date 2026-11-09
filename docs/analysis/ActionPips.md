# Action pips — the turn system rework (B-170 plan)

**Status:** **approved by Crown (2026-11-09)**, with Crown's answers to Q-170..Q-170h folded in below. B-170a..g are buildable; B-170h waits on Crown's PIE.
**Ruling:** D-088 as confirmed by Crown on 2026-11-09, and Crown's answers of the same day:
- turns go round in a **fixed rotation**; Turn Speed is retired;
- **actions are banked**: each turn you regain actions (**Reflex** sets how many: 1, 2 or 3), up to your **Max Actions** (base 2, raised by **action points** from gear, evolutions and the like); unused actions carry over; a fight starts with your regain amount;
- every skill, spell and item has its own **action cost** (default 1); Energy still applies too;
- **effects can give or take actions** (+1 now, −1 next turn, ±Max for N turns), alongside the status bars' action losses.

**What this supersedes:** the "keep the speed timeline" part of D-133 (its belt rules stay: bonus turn lands next, skip, death purge, revive); D-088's own 2026-10-26 paragraphs "Turn Speed stays" and "one battle action + one utility action"; D-049 (the 2:1 cap); ET-16's "arming ends the turn"; ET-19's "one free switch per turn"; SB-22 (Water slows Action Speed too) and SB-23 (Wind's one-place delay). **ET-15 stays:** the Extra Action effect keeps its bonus turn and is renamed **Extra Turn** (Crown).

The details are the **Q-170** entries in `docs/DesignDecisions.md` (Q-170 to Q-170h), all answered by Crown on 2026-11-09. Every number below is a named constant marked `// TODO: tune`.

---

## 1. The new turn, in plain words

1. The rotation reaches you. Your **turn starts**: effects that end, end; payouts land (ET-17, unchanged); your Energy refill (B-173). Then the actions, **always in this order (Crown, 2026-11-09):**
   1. **Gain** the turn's actions (your regain amount from Reflex, never past Max Actions).
   2. **Apply every start-of-turn change** to them: waiting next-turn gains and losses (`GainActions` / `LoseActions` timed "next turn"), Wind's loss, a delayed Emerald gain landing this turn (it may pass Max), and any other effect that adds or removes actions at turn start.
   3. **Only then check:** 0 actions means **the turn is skipped**. The turn-end step still runs, then the rotation moves on. No action prompt opens.

   **Rule: no unit is ever asked for an action at 0 actions.** The same check runs before every prompt (§3.3), so a unit dropped to 0 mid-turn has its turn end too.
2. You act. **Every Attack, Ability, Refraction and Item costs its own action cost** (default 1, set per asset; a big skill can cost 2 or more; Energy is paid too) (Q-170b). A skill that costs more than your Max Actions can't be used (greyed); one that costs more than your current actions waits until you have them. After each act, if you still have actions, you choose again. **Arming a ritual** costs its action cost and does **not** end the turn. **Switching weapon** is always free and never ends the turn (Q-170c).
3. Your turn **ends** only when you reach 0 actions, choose **End Turn**, or choose **Defend** (costs nothing). Unused actions stay banked (Q-170c).
4. The turn-end step runs once (ET-17's end half). The rotation moves to the next combatant.

Durations still count the holder's own **turns**, not actions (D-051): three actions in one turn tick a "for 1 turn" effect once.

## 2. The numbers (Q-170)

Crown, 2026-11-09 (Q-170, Q-170h):

| What | Value | Constant (all `// TODO: tune`) |
|---|---|---|
| Actions at fight start | your regain amount (capped at Max) | — (reads the regain) |
| Regain per turn | 1; 2 at the first Reflex threshold; 3 at the second | `ACTIONS_REGAIN_BASE` (1), `ACTIONS_REGAIN_REFLEX_THRESHOLD_2` (0.5), `ACTIONS_REGAIN_REFLEX_THRESHOLD_3` (0.8) |
| Regain never fills past | Max Actions | — |
| Base Max Actions | 2 | `MAX_ACTIONS_BASE` |
| Action points per Max Action | 100 (+25, +50 … add up; only whole actions count) | `ACTION_POINTS_PER_ACTION` |
| Safety ceiling on Max | 5 | `MAX_ACTIONS_CAP` (not a design limit; raise it if gear needs more) |

- **Reflex sets the regain, not the Max.** Reflex's share is read the way the defense window already reads it (`UCharacterData::CalculateReflexWindowBonus` ÷ `WINDOW_CAP_SECONDS`, 0..1); at or above threshold 2 the regain is 2, at or above threshold 3 it is 3. **Confirmed by Crown (2026-11-09):** "room for 3 at a second threshold" is a real second step at 0.8 of the range (reachable with heavy Reflex investment, not only at the cap). Reflex keeps its defense-window job too (Q-170h).
- **Max Actions = `MAX_ACTIONS_BASE` + ⌊action points ÷ 100⌋.** Action points are a new composed stat: gear, evolutions, stones and buffs add to it through the stat composition (`docs/Architecture/StatComposition.md`), so +25 and +50 from two items make +75. The leftover shows as progress: **"Max 3 (+75/100)"**. No asset has action points yet: authoring them is content work (until then everyone has Max 2).
- Example: Max 3 (175 points), regain 2: start the fight with 2; bank 3 at most from regains.

## 3. What changes, system by system

### 3.1 Actions on the character (new state)
- `UCharacterDataComponent` gets `CurrentActions` (replicated, beside `CurrentEP`), `GetMaxActions()`, `GetActionPoints()` (the composed total; the progress line reads its remainder), `GetActionRegain()` (from Reflex), `GainActions(N, bMayPassMax)`, `SpendActions(N)`, `OnActionsChanged`. Per-fight state on the battle actor (D-064); co-op clients see it through the same replication as EP.
- **Action points** are a new stat in the stat composition (one new stat entry, summed from gear, evolutions, stones and buffs like any other composed stat). No asset authors any yet. *(As built in B-170a: an additive layer set instead of a stat entry; stones fold into gear. See Q-170a; Crown may veto.)*
- Set to the regain amount (capped at Max) when a fight starts; meaningless outside a fight.
- **Action cost on the assets (Q-170b):** `USkillDataBase::ActionCost` (skills and spells; weapon attacks are abilities) and the item asset's `ActionCost`, `int32`, default 1, `ClampMin=1`. Default `ACTION_COST_DEFAULT` (1, `// TODO: tune`). Data validation warns when a cost is above `MAX_ACTIONS_CAP`. A `MaxActionsBuff` / `MaxActionsDebuff` row's size and turns are authored per effect (data, like every buff's), not constants.
- Later, with Energy's GAS move (B-043 line), actions can become a GAS attribute. Not part of this plan.

### 3.2 `UTurnManager` — fixed rotation
- **Rotation (Q-170d, the deckbuilder's turn belt, §7c):** the sides alternate, party first: P1 → E1 → P2 → E2 → …. Within a side, party-screen order (the party) and spawn slot (enemies). An uneven side fills the larger side's empty seats in turn, and that rotation carries on across rounds (deckbuilder item 53): 4 heroes vs 1 boss gives P1 B P2 B P3 B P4 B.
- **Kept as built (D-133, B-341):** a bonus turn lands right after the current turn; a skip removes the holder's next turn; death purges a holder's turns; a revive re-enters from the next round; Execution (ritual) turns stay pinned, and their delay counts turns as now; `OnRoundAdded`; the turn belt and `PreviewTurnOrder` (the strip keeps working unchanged).
- **First strike (Q-170f, as built, ET-03):** the striking side takes its whole opening round; then the alternation starts with that side.
- **Retired** (marked OBSOLETE, deleted only after Crown's PIE, D-067):
  - `CalculateSpeedRatios`, `TurnsOwed` / `SpeedRatio` (the debt pick);
  - the 2:1 cap (`MAX_TURN_RATIO`, `TurnsSinceOpponent`, D-049): Max Actions is the cap now;
  - the 3-step tie-break (no ties in a fixed order);
  - `DelayNextTurn` / `PlacesToYield` (Wind now slows and takes an action);
  - `GetEffectiveTurnSpeed`, `GetTurnSpeedLayers`, `ComputeCachedTurnSpeed`, the cached speed stats;
  - `OnActorSpeedChanged` becomes a no-op (callers stay until the deletion job);
  - `RequestExtraTurn`, `ScheduleBonusTurn` (already OBSOLETE).
- **Callers of the retired functions outside `TurnManager.cpp`** (they keep working with the switch off; B-170g / B-170h retire them):
  - `OnActorSpeedChanged`: `SkillEffectManager.cpp` (`NotifySpeedChanged`), `InventoryComponent.cpp`, `LoadoutComponent.cpp` (3 places), `CrystalManager.cpp`.
  - `DelayNextTurn`: `SkillEffectManager.cpp` (`ApplyTriggeredSkillEffect`, Wind).
  - `GetEffectiveTurnSpeed` / `GetTurnSpeedLayers`: the GAS `TurnSpeed` attribute's sync (`WoRAttributeSyncComponent.cpp`, `WoRAttributeSets.cpp`), its GEs (`WoRGameplayEffects.h/.cpp`, `WoREffectSpecBuilder.cpp`), `WoRAttributeDebug.cpp`, `StatModifierDebug.cpp`.
  - `ScheduleBonusTurn`, `BuffTurnSpeed`: `ItemExecutor.cpp`. `TurnManagerTestActor.cpp` (legacy test actor, D-067).
- Built **behind one switch, `wor.Turns.ActionPips`** (default 0 = today's speed timeline), so the old path keeps working until Crown's PIE; then the default flips and a later job deletes the old path.

### 3.3 `ACombatOrchestrator` — several actions per turn
- `ProceedWithTurnStart`, fixed order (§1, Crown): (1) gain the turn's actions; (2) apply every start-of-turn action change (waiting gains and losses, Wind, a delayed Emerald gain, any turn-start effect that removes actions); (3) only then check. **At 0 actions the turn is skipped:** go straight to `OnActionCompleted`'s turn-end path, as a stunned-out or dead actor's turn does today; log it and show "No actions" on the HUD (an `FText` in the shared turn words, D-062). Otherwise ask for an action. Steps (1) to (3) are one function, so no path can check before the changes or prompt before the check.
- **One gate before every prompt:** a single check (`CanStillAct`: alive, not stunned, `CurrentActions` > 0) runs before the orchestrator opens the menu or asks the AI, at turn start and after each action. An effect that lands mid-turn and drops the unit to 0 (a counter, a reflect) ends the turn once the current action has resolved; if a prompt is open when the count reaches 0 (a co-op client's menu), it closes and the turn ends.
- `ValidateAction`: refuse an action whose `ActionCost` is above `CurrentActions` ("Needs 2 actions."), or above Max Actions ("Costs more actions than you can hold."). The menu greys it with that reason. The check and the spend run on the server (co-op), like every action; clients only read the replicated count.
- `OnActionCompleted`: spend the action's `ActionCost`, check the win condition, then either **ask again** (actions left and the action doesn't end the turn) or run the turn-end step and `AdvanceToNextTurn`. The turn-start step never re-runs inside a turn. Only reaching 0, End Turn and Defend end the turn (Q-170c); **arming a ritual no longer ends it** (ET-16 changes): the ritual's `ActionCost` is spent at arming, with its EP, HP and wear as today.
- **End Turn** is new: an `EActionType` appended (`EndTurn`), so it uses the existing submit path and the co-op server call.
- **Always free:** the weapon / ring switch, as many times as wanted (Crown, Q-170c: no free-first rule, never costs; ET-19 changes). **The row move** stays one free per turn (D-096); a second stays refused.
- The camera's turn-start shot (`CombatCameraDirector::HandleActorTurnStarted`) stays once per turn; each action keeps its own action shot.

### 3.4 Effects
- **New effect types** (appended to `ESkillEffectType`, Q-170g, approved by Crown; the deckbuilder's "an Action effect arrives in the pool at once; a next-turn version is a row with a one-turn delay"):
  - `GainActions` — +N actions. Timing *now* (within the current turn), or *at the holder's next turn start*. Shown as **Gain Action**.
  - `LoseActions` — −N actions (never below 0). Same two timings.
  - `MaxActionsBuff` / `MaxActionsDebuff` — ±N Max Actions (whole actions) for N holder turns (D-051).
  - Crown's example "+1 now, −1 next turn" is one skill with two rows (`GainActions` now + `LoseActions` next turn), as Divinity: Original Sin 2's Adrenaline.
- **Status-bar breaks (Crown, Q-170e)**, all sizes named constants, `// TODO: tune`:
  - **Ice** (SB-29): slows Action Speed and Spell Speed (as built), deals its 10% max HP (as built), **and removes 1 action at once** from the holder's bank (`ICE_BREAK_ACTIONS_LOST` 1, never below 0). **Confirmed by Crown (2026-11-09):** at once (Wind's loss is the one timed at the target's next turn).
  - **Water** (SB-22): slows **Spell Speed only** (Action Speed no longer). The `BreakTable` Water row drops its Action Speed part.
  - **Wind** (SB-23, `BreakTable` row `SkipTurn`): slows **Action Speed** (`WIND_BREAK_ACTION_SPEED_SLOW_PERCENT`, new; starts at Water's 30%) **and** takes 1 action at the holder's next turn start (`WIND_BREAK_ACTIONS_LOST` 1, a `LoseActions` next-turn row; D-090's placeholder −1). `DelayTurnPlaces` 1 → 0. The `SkipTurn` effect type keeps its slot; only the row changes.
  - With Turn Speed gone, a slow means the holder's attacks (Action Speed) or spells (Spell Speed) are easier to defend against (wider windows) and play slower.
- **Extra Turn (Crown, Q-170f):** both effects are kept. The existing **Extra Action** effect (`ESkillEffectType::ExtraAction`) **stays a whole bonus turn, as built** (ET-12, ET-15), and its player-facing name becomes **Extra Turn** (display name and effect text only; the enum entry keeps its name, it is serialized). **+1 action now** is the new `GainActions` (Gain Action).
- **Wind, two rules (Crown, D-088, 2026-11-09):** the Wind **status bar** (its break) slows and takes an action (above); the **Wind crystal effect** (Emerald, Wind's gem) makes an **enemy target lose their next turn** in the rotation. A unit left with 0 actions after its turn's gain loses that turn too (§1).
- **Emerald** (Q-170f, Crown, 2026-11-09): on an **enemy**, it still removes their next turn (`SkipNextTurn`, already built: ET-13; S: the next two, Q-341b; replaces today's "force their turn" gamble). On **yourself or an ally** it **grants actions instead of a bonus turn**, by tier (Crown's ladder). A delayed gain is a `GainActions` row timed at that turn's start, so it lands in step 2 of the turn-start order (§1). **Emerald's actions may go past Max Actions** (Crown). Named constants, `// TODO: tune` (`AugmentStoneConstants.h`, beside the retired bonus-turn table):

  | Tier | F | E | D | C | B | A | S |
  |---|---|---|---|---|---|---|---|
  | Ally / self | +1 in 2 turns | +1 next turn | +1 now | +2 in 2 turns | +2 next turn | +2 now | +3 now |
  | Enemy | loses next turn | same | same | same | same | same | loses next 2 turns (Q-341b) |

  Constants: `EMERALD_ALLY_ACTIONS_GAINED[7] = {1, 1, 1, 2, 2, 2, 3}` and `EMERALD_ALLY_GAIN_DELAY_TURNS[7] = {2, 1, 0, 2, 1, 0, 0}` (F..S; 0 = now, 1 = the holder's next turn start, 2 = the one after). No per-instance roll (S is a fixed +3). Code: `UItemExecutor::ExecuteBonusTurnEffect` branches on the target's side (enemy: skip; ally: `GainActions`, may pass Max); the AI's Emerald scoring (`AIFreeActionFactor`, the exposure look-ahead) flips to "the foe loses a turn" on enemies and values the ally gain as actions.
- **Over Max:** a gain (an effect or Emerald) may pass Max, and the extra actions **stay banked until spent** (Crown: Emerald's actions "may go over max"; trimming them at turn end would undo that). The per-turn regain adds nothing while the bank is at or above Max. **Confirmed by Crown (2026-11-09):** the extra stays banked until spent (the Q-170g default's trim at turn end is dropped).
- **Retired** (enum entries kept, append-only; data validation refuses them, the D-056 pattern): `ModifyTurnSpeed`, `TurnSpeedBuff`, `TurnSpeedDebuff`; `EItemEffectType::BuffTurnSpeed`; the Turn Speed sub-stat (`ESubStat::TurnSpeed`, `UCharacterData::TurnSpeed`, Q-170h); the GAS `TurnSpeed` attribute, its sync, GEs and debug lines (§3.2 callers). `TurnSpeedStone` is already retired (B-353).
- Effect text (`EffectText.cpp`) gets lines for the four new types: "Gain 1 action.", "Lose 1 action at the start of their next turn.", "+1 Max Actions for 2 turns."; Extra Action's line reads "Take an extra turn." under the name Extra Turn; the Ice, Water and Wind break lines follow their new rows.

### 3.5 Turn strip and menus (placeholder C++ until the visual phase)
- **Turn strip** (`UTurnOrderStripWidget`): no code change; it reads `PreviewTurnOrder`, which now shows the rotation.
- **Character panel:** action pips beside the EP bar (filled = current, empty up to Max), bound to `OnActionsChanged`.
- **Command menu** (`UCombatCommandMenuSubsystem`): an **End Turn** button; each skill, spell and item shows its action cost; ones you can't afford now greyed with the reason, ones above your Max greyed as unusable; Defend's tooltip says it ends the turn.
- **Input:** End Turn gets a key and its row in `docs/Design/InputMap.md` in the same change (rule of 2026-10-27).
- **Stat screen:** a Max Actions line with the action-point progress ("Max 3 (+75/100)") and the regain per turn; the Turn Speed line goes.
- The real HUD comes in the visual phase (B-391 onward); these are placeholders it replaces.

### 3.6 AI (`UAIDecisionManager`)
- Asked once per action (the orchestrator asks again while actions remain), so the existing pick runs per action.
- **Spend or bank:** act while the best pick scores above `AI_END_TURN_SCORE_FLOOR` (`// TODO: tune`); otherwise End Turn and bank. Defend still ends the turn. A pick whose action cost is above the unit's current actions is skipped (it may bank for it).
- Scoring: drop the Turn Speed debuff value; value `LoseActions` on a foe and `GainActions` on an ally as one action of that unit's threat; value the Wind and Ice breaks' action loss the same way.
- The Emerald look-ahead (`PreviewTurnOrder`, `EMERALD_EXPOSURE_LOOKAHEAD`) keeps working.

### 3.7 Debug tools
- `wor.Actions.Print`: each combatant's actions / Max (with action points and progress) / regain / waiting next-turn gains and losses (`GetActionsString()`).
- `wor.Turns.PrintBelt` stays; it prints the rotation and its round.

### 3.8 Docs to update as each part lands
- `docs/Rules/04-effects-and-turns.md`: ET-03 to ET-14 (turn order), ET-15 (Extra Action keeps its bonus turn, shown as Extra Turn), ET-16 (arming a ritual no longer ends the turn), ET-18 (one action per turn → banked actions with costs), ET-19 (switching always free). The page's "Coming with B-170" section lists them until each part lands.
- `docs/Rules/03-status-bars.md` SB-22 (Water: Spell Speed only), SB-23 (Wind: Action Speed slow + 1 action), SB-29 (Ice: + 1 action at once); same "Coming with B-170" section.
- The status-bar row changes ship behind `wor.Turns.ActionPips` like the rest: at 0 the rows play as built today.
- `docs/Architecture/TurnManager.md`, `CombatOrchestrator.md`, `TurnState.md`; `docs/Design/InputMap.md`.

## 4. Tests

| Spec | Change |
|---|---|
| `TurnManagerSpec`, `TurnBeltSpec`, `TurnStartOrderSpec`, `FirstStrikeSpec` | Add rotation cases with the switch on; old cases stay with it off until the deletion job |
| `TurnCapSpec`, `TurnSpeedSourcesSpec`, `TurnSpeedTieBreakSpec` | Kept with the switch off; deleted with the old path |
| `BonusTurnSpec`, `EmeraldExtraTurnSpec`, `CombatRitualFireSpec` | Re-checked with the switch on (Extra Action still a bonus turn; Emerald on an enemy removes their next turn; on an ally, each tier's gain lands on its turn (now, next turn, or the one after) and may pass Max; arming a ritual spends its action cost and the turn goes on) |
| `StatusBreakSpec` | With the switch on: Ice takes 1 action at once; Water slows Spell Speed only; Wind slows Action Speed and takes 1 action at the next turn start. With it off: the rows as built |
| `EffectTextSpec` | Extra Action shown as Extra Turn (its line unchanged); the four new types' lines; the new break lines |
| `StoneRowSpec`, `SignAwareModifierSpec`, `GASStatGettersSpec`, `StatModifierSpec` | Their Turn Speed cases retire with the family (B-170g / B-170h) |
| **New** `ActionPipsSpec` | Fight start = regain; regain 1 / 2 / 3 at the Reflex thresholds; regain never fills past Max; Max 2 + ⌊action points ÷ 100⌋ (+25 +50 → 75 → Max 2, "+75/100"; 175 → Max 3); spend by cost; End Turn banks; 0 actions ends the turn |
| **New** `ActionEffectsSpec` | `GainActions` / `LoseActions` now and next turn; Max Actions buff and debuff; the "+1 now, −1 next" pair; never below 0; a gain passes Max and stays banked; the regain adds nothing at or above Max |
| **New** `MultiActionTurnSpec` | Turn-start and turn-end steps run once per turn; durations tick once; win check after each action; Defend and End Turn end the turn, arming a ritual does not; a switch is free; a cost-2 skill needs 2 actions and is refused above Max. **Turn-start order (Crown):** (a) the turn's gain, then a waiting loss to 0: the turn is skipped, turn-end still runs, no prompt opens; (b) a waiting loss that leaves 1: the turn plays with 1 action; (c) an effect landing mid-turn that drops the unit to 0: the turn ends after that action, no further prompt; (d) a delayed gain landing the same turn as a loss counts before the check (gain, changes, then check) |
| `AIOneSkillPathSpec` + new AI case | AI acts until its best pick is under the floor, then banks |

## 5. Build order (each ≤ 3 files plus its spec)

| Job | What | Size |
|---|---|---|
| B-170a | Actions on the character: state, constants, action points (new composed stat) → Max, Reflex → regain, `OnActionsChanged`, `wor.Actions.Print`, `ActionPipsSpec`. No play change | S |
| B-170b | `UTurnManager` rotation behind `wor.Turns.ActionPips`; rotation specs | M |
| B-170c | Orchestrator: several actions per turn, `EndTurn`, `ActionCost` on skills / spells / items and the cost check, ritual arming no longer ends the turn, free switching; `MultiActionTurnSpec` | M |
| B-170d | Effects: the four new types, the Ice / Water / Wind break rows, Extra Turn naming, Emerald's ally ladder, effect text; `ActionEffectsSpec`, `StatusBreakSpec` | M |
| B-170e | Placeholder UI: pips, End Turn button, greyed costs, InputMap row | S |
| B-170f | AI spend or bank, scoring | S |
| B-170g | Retire the Turn Speed family (data validation, stat screen), rules pages | S |
| — | **Crown's PIE** with `wor.Turns.ActionPips 1`; then flip the default | — |
| B-170h | Delete the speed timeline (after PIE, D-067) | M |

B-170a to B-170g can each merge behind the switch without changing play.

## 6. Risks
- **Balance:** a lone boss acting between every hero (Q-170d) acts 4 times a round against 4 heroes. Boss damage per hit needs a tuning pass once it plays (the balance simulator, B-291, counts rounds already).
- **Multiplayer:** a co-op client's End Turn and the action count must go through the server like any action (B-228 path); covered by appending `EndTurn` to `EActionType`.
- **Saves:** none. Actions live only inside a fight.
