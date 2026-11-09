# Action pips — the turn system rework (B-170 plan)

**Status:** plan for Crown's approval. **No code until Crown approves it** (D-088, 2026-11-09).
**Ruling:** D-088 as confirmed by Crown on 2026-11-09:
- turns go round in a **fixed rotation**; Turn Speed is retired;
- **actions are banked**: you gain 1 per turn, up to your **Max Actions**, which **Reflex** sets; unused actions carry over; a fight starts with 1;
- **effects can give or take actions** (+1 now, −1 next turn, …), alongside Wind's action loss.

**What this supersedes:** the "keep the speed timeline" part of D-133 (its belt rules stay: bonus turn lands next, skip, death purge, revive); D-088's own 2026-10-26 paragraphs "Turn Speed stays" and "one battle action + one utility action"; D-049 (the 2:1 cap); ET-15 (Extra Action as a bonus turn, reversed by Q-170f).

Open details are the **Q-170** entries in `docs/DesignDecisions.md` (Q-170 to Q-170h). Each carries a reference-backed default; the plan below is written with those defaults.

---

## 1. The new turn, in plain words

1. The rotation reaches you. Your **turn starts**: effects that end, end; payouts land (ET-17, unchanged); you gain **+1 action** (up to Max Actions) and your Energy refill (B-173).
2. You act. **Attack, Ability, Refraction and Item cost 1 action each** (Q-170b). After each one, if you still have actions, you choose again.
3. Your turn **ends** when you reach 0 actions, choose **End Turn** (the rest stays banked), **Defend** (costs nothing, ends the turn), or **arm a ritual** (Q-170c).
4. The turn-end step runs once (ET-17's end half). The rotation moves to the next combatant.

Durations still count the holder's own **turns**, not actions (D-051): three actions in one turn tick a "for 1 turn" effect once.

## 2. The numbers (Q-170)

| What | Default | Constant (all `// TODO: tune`) |
|---|---|---|
| Actions at fight start | 1 | `ACTIONS_AT_FIGHT_START` |
| Gained per turn | 1 | `ACTIONS_GAINED_PER_TURN` |
| Base Max Actions | 2 | `MAX_ACTIONS_BASE` |
| Extra Max Actions from Reflex | +1 at each third of the Reflex range: 2, 3, 4, 5 | `MAX_ACTIONS_FROM_REFLEX` (3) |
| Hard cap | 5 | `MAX_ACTIONS_CAP` |

Reflex's share is read the way the defense window already reads it (`UCharacterData::CalculateReflexWindowBonus` ÷ `WINDOW_CAP_SECONDS`, 0..1), so Reflex gear, stones and buffs raise Max Actions with no new stat maths.

## 3. What changes, system by system

### 3.1 Actions on the character (new state)
- `UCharacterDataComponent` gets `CurrentActions` (replicated, beside `CurrentEP`), `GetMaxActions()`, `GainActions(N)`, `SpendActions(N)`, `OnActionsChanged`. Per-fight state on the battle actor (D-064); co-op clients see it through the same replication as EP.
- Reset to `ACTIONS_AT_FIGHT_START` when a fight starts; meaningless outside a fight.
- Later, with Energy's GAS move (B-043 line), actions can become a GAS attribute. Not part of this plan.

### 3.2 `UTurnManager` — fixed rotation
- **Rotation (Q-170d, the deckbuilder's turn belt, §7c):** the sides alternate, party first: P1 → E1 → P2 → E2 → …. Within a side, party-screen order (the party) and spawn slot (enemies). An uneven side fills the larger side's empty seats in turn, and that rotation carries on across rounds (deckbuilder item 53): 4 heroes vs 1 boss gives P1 B P2 B P3 B P4 B.
- **Kept as built (D-133, B-341):** a bonus turn lands right after the current turn; a skip removes the holder's next turn; death purges a holder's turns; a revive re-enters from the next round; Execution (ritual) turns stay pinned, and their delay counts turns as now; `OnRoundAdded`; the turn belt and `PreviewTurnOrder` (the strip keeps working unchanged).
- **First strike (Q-170f, as built, ET-03):** the striking side takes its whole opening round; then the alternation starts with that side.
- **Retired** (marked OBSOLETE, deleted only after Crown's PIE, D-067):
  - `CalculateSpeedRatios`, `TurnsOwed` / `SpeedRatio` (the debt pick);
  - the 2:1 cap (`MAX_TURN_RATIO`, `TurnsSinceOpponent`, D-049): Max Actions is the cap now;
  - the 3-step tie-break (no ties in a fixed order);
  - `DelayNextTurn` / `PlacesToYield` (Wind now takes an action);
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
- `ProceedWithTurnStart`: after the turn-start step, gain the turn's actions and apply any waiting next-turn gains or losses (Wind), then ask for an action.
- `ValidateAction`: refuse an action that costs more than `CurrentActions` ("Needs 1 action."). The menu greys it with that reason. The check and the spend run on the server (co-op), like every action; clients only read the replicated count.
- `OnActionCompleted`: spend the cost, check the win condition, then either **ask again** (actions left and the action doesn't end the turn) or run the turn-end step and `AdvanceToNextTurn`. The turn-start step never re-runs inside a turn.
- **End Turn** is new: an `EActionType` appended (`EndTurn`), so it uses the existing submit path and the co-op server call.
- **Kept free, once per turn:** the weapon / ring switch (ET-19; Crown's note there: "the first switch is free, further switches cost a pip") and the row move (D-096). A second switch then costs 1 action instead of being refused. A second row move stays refused (D-096 gives one; no ruling for more).
- The camera's turn-start shot (`CombatCameraDirector::HandleActorTurnStarted`) stays once per turn; each action keeps its own action shot.

### 3.4 Effects
- **New effect types** (appended to `ESkillEffectType`, Q-170g; the deckbuilder's "an Action effect arrives in the pool at once; a next-turn version is a row with a one-turn delay"):
  - `GainActions` — +N actions. Timing *now*, or *at the holder's next turn start*.
  - `LoseActions` — −N actions (never below 0). Same two timings.
  - `MaxActionsBuff` / `MaxActionsDebuff` — ±N Max Actions for N holder turns (D-051).
  - Crown's example "+1 now, −1 next turn" is one skill with two rows (`GainActions` now + `LoseActions` next turn), as Divinity: Original Sin 2's Adrenaline.
- **Wind's break** (`BreakTable` row `SkipTurn`): `DelayTurnPlaces` 1 → 0; applies `LoseActions` 1 at the holder's next turn start (D-090's placeholder −1). The `SkipTurn` effect type keeps its slot; only the row changes.
- **Ice / Water breaks** (Q-170e): unchanged. Both slow Action Speed and Spell Speed, Ice adds its damage (D-090, B-064a). With Turn Speed gone, the slow means the holder's attacks and spells are easier to defend against (wider windows) and play slower.
- **Extra Action** (`ESkillEffectType::ExtraAction`, Q-170f): becomes +1 action now (its name), no longer a bonus turn.
- **Emerald** (Q-170f): keeps its bonus turn (D-052, D-133, ET-12; built). A bonus turn is a real turn: it gains +1 action and runs the turn steps.
- **Retired** (enum entries kept, append-only; data validation refuses them, the D-056 pattern): `ModifyTurnSpeed`, `TurnSpeedBuff`, `TurnSpeedDebuff`; `EItemEffectType::BuffTurnSpeed`; the Turn Speed sub-stat (`ESubStat::TurnSpeed`, `UCharacterData::TurnSpeed`, Q-170h); the GAS `TurnSpeed` attribute, its sync, GEs and debug lines (§3.2 callers). `TurnSpeedStone` is already retired (B-353).
- Effect text (`EffectText.cpp`) gets lines for the four new types: "Gain 1 action.", "Lose 1 action at the start of their next turn.", "+1 Max Actions for 2 turns.".

### 3.5 Turn strip and menus (placeholder C++ until the visual phase)
- **Turn strip** (`UTurnOrderStripWidget`): no code change; it reads `PreviewTurnOrder`, which now shows the rotation.
- **Character panel:** action pips beside the EP bar (filled = current, empty up to Max), bound to `OnActionsChanged`.
- **Command menu** (`UCombatCommandMenuSubsystem`): an **End Turn** button; actions you can't afford greyed with the reason; Defend's tooltip says it ends the turn.
- **Input:** End Turn gets a key and its row in `docs/Design/InputMap.md` in the same change (rule of 2026-10-27).
- **Stat screen:** a Max Actions line; the Turn Speed line goes.
- The real HUD comes in the visual phase (B-391 onward); these are placeholders it replaces.

### 3.6 AI (`UAIDecisionManager`)
- Asked once per action (the orchestrator asks again while actions remain), so the existing pick runs per action.
- **Spend or bank:** act while the best pick scores above `AI_END_TURN_SCORE_FLOOR` (`// TODO: tune`); otherwise End Turn and bank. Defend still ends the turn.
- Scoring: drop the Turn Speed debuff value; value `LoseActions` on a foe and `GainActions` on an ally as one action of that unit's threat; value Wind's break the same way.
- The Emerald look-ahead (`PreviewTurnOrder`, `EMERALD_EXPOSURE_LOOKAHEAD`) keeps working.

### 3.7 Debug tools
- `wor.Actions.Print`: each combatant's actions / Max / waiting next-turn gains and losses (`GetActionsString()`).
- `wor.Turns.PrintBelt` stays; it prints the rotation and its round.

### 3.8 Docs to update as each part lands
- `docs/Rules/04-effects-and-turns.md`: ET-03 to ET-14 (turn order), ET-15 (Extra Action: +1 action, no longer a bonus turn), ET-18 (one action per turn → banked actions), ET-19 (second switch costs an action).
- `docs/Rules/03-status-bars.md` SB-23 (Wind).
- `docs/Architecture/TurnManager.md`, `CombatOrchestrator.md`, `TurnState.md`; `docs/Design/InputMap.md`.

## 4. Tests

| Spec | Change |
|---|---|
| `TurnManagerSpec`, `TurnBeltSpec`, `TurnStartOrderSpec`, `FirstStrikeSpec` | Add rotation cases with the switch on; old cases stay with it off until the deletion job |
| `TurnCapSpec`, `TurnSpeedSourcesSpec`, `TurnSpeedTieBreakSpec` | Kept with the switch off; deleted with the old path |
| `BonusTurnSpec`, `EmeraldExtraTurnSpec`, `CombatRitualFireSpec` | Re-checked with the switch on (Extra Action now +1 action) |
| `StatusBreakSpec` | Wind takes 1 action at the next turn start |
| `EffectTextSpec` | Extra Action's line ("take an extra turn" → "Gain 1 action."); the four new types' lines |
| `StoneRowSpec`, `SignAwareModifierSpec`, `GASStatGettersSpec`, `StatModifierSpec` | Their Turn Speed cases retire with the family (B-170g / B-170h) |
| **New** `ActionPipsSpec` | Fight start 1; +1 per turn; bank up to Max; Reflex → Max 2..5; cap 5; spend; End Turn banks; 0 actions ends the turn |
| **New** `ActionEffectsSpec` | `GainActions` / `LoseActions` now and next turn; Max Actions buff and debuff; the "+1 now, −1 next" pair; never below 0 |
| **New** `MultiActionTurnSpec` | Turn-start and turn-end steps run once per turn; durations tick once; win check after each action; Defend and ritual end the turn |
| `AIOneSkillPathSpec` + new AI case | AI acts until its best pick is under the floor, then banks |

## 5. Build order (each ≤ 3 files plus its spec)

| Job | What | Size |
|---|---|---|
| B-170a | Actions on the character: state, constants, Max from Reflex, `OnActionsChanged`, `wor.Actions.Print`, `ActionPipsSpec`. No play change | S |
| B-170b | `UTurnManager` rotation behind `wor.Turns.ActionPips`; rotation specs | M |
| B-170c | Orchestrator: several actions per turn, `EndTurn`, cost check; `MultiActionTurnSpec` | M |
| B-170d | Effects: the four new types, Wind's row, Extra Action, effect text; `ActionEffectsSpec` | M |
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
