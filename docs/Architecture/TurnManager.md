# Turn Manager

## Overview

`UTurnManager` is a `UGameInstanceSubsystem` that owns turn order for a combat
encounter. It uses a debt-based scheduling algorithm driven by speed ratios: faster
combatants accumulate "turn debt" more quickly and therefore act more often. It does
not run combat itself — `ACombatOrchestrator` drives it and listens to its events.

The scheduling rule (per the header comment):
- `SpeedRatio = ActorSpeed / SlowestSpeed` (the slowest combatant is 1.0).
- Each *round* adds `SpeedRatio` to every combatant's `TurnsOwed`.
- The combatant with the highest net debt (`TurnsOwed - TurnsTaken`) goes next.
- A new round begins only when no living combatant has positive net debt.

Debt accumulates per ROUND, not per turn — the file's top comment
("CORRECTED VERSION") and the inline notes call out that per-turn accumulation was
a prior bug.

## Architecture

### `UTurnManager` (UGameInstanceSubsystem)

Internal state:
- `TArray<FCombatantTurnDebt> Combatants` — one entry per combatant in the encounter.
- `AActor* CurrentActor` — the actor whose turn is currently active.
- `AActor* PreviousActor` — the prior `CurrentActor` (set in `AdvanceToNextTurn`).
- `int32 GlobalTurnCount` — total turns taken since combat started.
- `bool bCombatActive` — whether an encounter is in progress.
- `mutable USkillEffectManager* SkillEffectManagerRef` — lazily acquired via
  `GetSkillEffectManager()`; used to fold turn-speed modifiers into effective speed.
- `TArray<FScheduledTurn> PendingTurns` — delayed bonus turns (Emerald) counting down
  to their fire boundary; see *Delayed bonus turns*.
- `bool bCurrentTurnIsBonus` — true when the current turn is an Emerald bonus turn being
  taken. Recomputed every `AdvanceToNextTurn` at the consume point; surfaced to the
  turn-order UI via `GetCurrentTurnIsBonus()`. Reset on `InitializeCombat`/`EndCombat`.

### `FCombatantTurnDebt` (USTRUCT)

Per-combatant scheduling record:
- `AActor* Actor`, `int32 TeamIndex`, `int32 PositionInTeam` — identity.
- `float TurnsOwed` — accumulated debt.
- `int32 TurnsTaken` — turns this combatant has actually taken.
- `float SpeedRatio` — recomputed by `CalculateSpeedRatios()`.
- Cached tie-break stats: `CachedSpeed`, `CachedActionSpeed`, `CachedMind`,
  `CachedBody`, `CachedSpirit`.
- `int32 UntakenBonusTurns` — granted-but-not-yet-taken Emerald bonus turns. The next
  pick for this combatant consumes one and flags the turn as a bonus; the count is held
  on the combatant so the flag survives the `PendingTurns` entry being removed at fire
  time (the consume-before-observe fix).

### `FScheduledTurn` (USTRUCT)

A delayed bonus turn awaiting its fire boundary (Emerald):
- `AActor* Actor` — who receives the bonus turn.
- `int32 TurnsRemaining` — global turn boundaries until it fires; decremented once per
  `AdvanceToNextTurn`, fires at 0.

## How It Works

### Initialization (`InitializeCombat`)

1. If combat is already active, calls `EndCombat()` first.
2. Clears `Combatants`, resets `GlobalTurnCount`, `CurrentActor`, `PreviousActor`,
   `bCurrentTurnIsBonus` (and `PendingTurns` is not re-seeded — bonus turns are
   combat-scoped).
3. Builds an `FCombatantTurnDebt` for each non-null actor — Team1 actors get
   `TeamIndex = 0`, Team2 actors get `TeamIndex = 1` — and calls
   `CacheActorStats()` on each.
4. Sets `bCombatActive = true`, calls `CalculateSpeedRatios()`, then
   `AdvanceToNextTurn()` to start the first turn.

### Stat caching (`CacheActorStats`)

Reads the actor's `UCharacterDataComponent` / `UCharacterData`:
- `CachedSpeed` = `CharacterData->CalculateTurnSpeed()` (pillar-scaled, rounded),
  plus the actor's active equipment `FEquipmentStatBonus::BonusTurnSpeed` from
  `ULoadoutComponent`.
- `CachedActionSpeed` = `GetTotalActionSpeed()`.
- `CachedMind/Body/Spirit` = `WorldMindLevel/WorldBodyLevel/WorldSpiritLevel`.
- Fallback defaults (Speed 5, ActionSpeed 5, Mind/Body/Spirit 3) when no character
  data is present (testing).

`CachedSpeed` is always the *pristine base* — turn-speed status effects are not
baked in here.

### Speed ratios (`CalculateSpeedRatios`)

Computes effective speed on the fly each call so that `OnActorDied` /
`OnActorResurrected` (which recalc without re-caching) do not compound:

`EffectiveSpeed = CachedSpeed * (1 + (ModifyTurnSpeed + TurnSpeedBuff - TurnSpeedDebuff)/100)`

Those three modifier values come from `SkillEffectManager::GetTotalStatModifier`.
The slowest effective speed among *living* combatants becomes the baseline; each
combatant's `SpeedRatio = EffectiveSpeed / SlowestSpeed`.

### Advancing turns (`AdvanceToNextTurn`)

1. Saves `PreviousActor = CurrentActor`.
2. `GetNextCombatant()` picks the next combatant.
3. If none found, logs an error and calls `EndCombat()`.
4. Otherwise sets `CurrentActor`, increments that combatant's `TurnsTaken` and
   `GlobalTurnCount`. Sets `bCurrentTurnIsBonus = (UntakenBonusTurns > 0)` and, when
   true, decrements `UntakenBonusTurns` — this pick consumes a granted bonus turn. Then
   broadcasts `OnTurnStarted(CurrentActor, GlobalTurnCount)`. (The consume happens
   *before* the broadcast so the UI's refresh reads the result, not the pre-consume
   count.)
5. **Bonus-turn fire loop** (after the broadcast) — decrements every
   `PendingTurns[i].TurnsRemaining` once; any entry reaching 0 is removed and, if its
   actor is still alive, granted its bonus turn via
   `RequestExtraTurn(Actor, bIsBonusTurn=true)`. Dead/invalid actors are dropped
   silently. Runs exactly once per global turn boundary.

### Picking the next combatant (`GetNextCombatant`)

1. Scans living combatants for the maximum net debt (`TurnsOwed - TurnsTaken`).
2. If the max net debt is `<= KINDA_SMALL_NUMBER` (no one has positive debt), calls
   `AccumulateDebtRound()` — adds each combatant's `SpeedRatio` to its `TurnsOwed`.
3. Selects the living combatant with the highest net debt; ties are resolved by
   `ShouldBreakTieInFavor()`.

### Tie-breaking (`ShouldBreakTieInFavor`)

Ordered cascade:
1. Higher `CachedSpeed` wins.
2. Higher `CachedActionSpeed` wins.
3. Underdog rule — *lower* total of Mind+Body+Spirit wins (rewards glass cannons).
4. Higher `CachedBody`, then 5. higher `CachedMind`, then 6. higher `CachedSpirit`.
7. Deterministic fallback — lower `TeamIndex`, then lower `PositionInTeam`.

### Turn-order preview (`PreviewTurnOrder`)

Copies `Combatants` into a temp array and simulates `NumTurns` iterations of the
same round-accumulation + highest-debt selection logic (without mutating real
state). Returns the projected sequence as `FPreviewTurnEntry` rows (`Actor` +
`bIsBonusTurn`). The simulation also walks a copy of `PendingTurns` and each
combatant's `UntakenBonusTurns`, so scheduled/granted Emerald bonus turns appear
**inline at their projected slot** flagged `bIsBonusTurn=true` (false for every slot
in the common no-bonus case). Used by UI and `DebugPrintTurnOrder`.

### Speed/life-state changes

- `OnActorSpeedChanged(Actor)` — re-caches that combatant's stats, recalculates all
  ratios, broadcasts `OnSpeedChanged`.
- `OnActorDied(Actor)` / `OnActorResurrected(Actor)` — recalculate ratios (the
  slowest combatant may have changed); they do NOT re-cache stats.
- `RequestExtraTurn(Actor, bIsBonusTurn=false)` — credits `+1.0` to that combatant's
  `TurnsOwed`, so the debt scheduler picks it again soon. `bIsBonusTurn=true` (Emerald)
  additionally flags the granted turn via `UntakenBonusTurns` — see *Delayed bonus turns*.

### Delayed bonus turns (`ScheduleBonusTurn`, Emerald)

Emerald grants a combatant a **bonus turn** after a tier-scaled delay (F=6 … S=0 global
turns). `ScheduleBonusTurn(Actor, DelayTurns)` (DelayTurns ≥ 1) appends an
`FScheduledTurn`; the bonus-turn fire loop in `AdvanceToNextTurn` counts it down and, at
0, credits the bonus via `RequestExtraTurn(Actor, bIsBonusTurn=true)`. S-tier (delay 0)
bypasses the scheduler — the Emerald handler calls `RequestExtraTurn` immediately.

`bIsBonusTurn=true` increments the combatant's `UntakenBonusTurns`, marking the granted
turn so it is flagged as a bonus *when taken* — this survives the originating
`PendingTurns` entry being removed at fire time. `AdvanceToNextTurn`'s consume step then
clears one `UntakenBonusTurns` and sets `bCurrentTurnIsBonus`, which the turn-order UI
reads via `GetCurrentTurnIsBonus()` (an immediate/self-target bonus *is* the current
turn, so it never shows as an upcoming slot — only the current-actor slot tint reflects
it). `PendingTurns` is cleared on `InitializeCombat`/`EndCombat`.

### Ending combat (`EndCombat`)

Broadcasts `OnCombatEnded(GlobalTurnCount)`, then clears `bCombatActive`,
`Combatants`, `CurrentActor`, `PreviousActor`, `PendingTurns`, resets
`GlobalTurnCount`, and clears `bCurrentTurnIsBonus`.

## Integration Points

### Delegates broadcast

- `FOnTurnStarted OnTurnStarted` — `(AActor* Actor, int32 TurnNumber)`, fired in
  `AdvanceToNextTurn`.
- `FOnTurnEnded OnTurnEnded` — `(AActor* Actor, int32 TurnNumber)`. Declared and
  bound by consumers, but no `OnTurnEnded.Broadcast(...)` call exists in
  `TurnManager.cpp` — see Known Limitations.
- `FOnCombatEnded OnCombatEnded` — `(int32 FinalTurnCount)`, fired in `EndCombat`.
- `FOnSpeedChanged OnSpeedChanged` — `(AActor* Actor)`, fired in
  `OnActorSpeedChanged`.

### Subsystems / components it depends on

- `USkillEffectManager` (GameInstanceSubsystem) — lazily acquired; supplies
  `ModifyTurnSpeed` / `TurnSpeedBuff` / `TurnSpeedDebuff` modifiers.
- `UCharacterDataComponent` / `UCharacterData` — `bIsAlive`, `CalculateTurnSpeed`,
  `GetTotalActionSpeed`, world stat levels.
- `ULoadoutComponent` — `GetActiveStatBonus` for equipment turn-speed bonus.

### What depends on it

- `ACombatOrchestrator` — calls `InitializeCombat`, `AdvanceToNextTurn`,
  `EndCombat`; binds `OnTurnStarted` / `OnTurnEnded` / `OnCombatEnded`.
- `ULoadoutComponent` — calls `OnActorSpeedChanged` on weapon/ring hot-swap (per
  the comment in `CacheActorStats`).
- UI — consumes `PreviewTurnOrder` for turn-order display (including the `bIsBonusTurn`
  slot flag) and `GetCurrentTurnIsBonus()` for the current-actor slot tint.
- `UItemExecutor` (Emerald) — calls `ScheduleBonusTurn` (delayed tiers) or
  `RequestExtraTurn(..., bIsBonusTurn=true)` (S-tier immediate) from
  `ExecuteBonusTurnEffect`.

## Known Limitations / TODOs

No `// TODO`, `// FIXME`, or `// HACK` markers were found in either file.

Observations from the code:
- `OnTurnEnded` is declared as a delegate and bound by `ACombatOrchestrator`, but
  `TurnManager.cpp` never calls `OnTurnEnded.Broadcast`. `ACombatOrchestrator`'s
  `HandleTurnEnded` even references an `EndCurrentTurn()` method that does not
  exist on `UTurnManager` — so `OnTurnEnded` appears to be effectively dead. End-of-
  turn logic is handled in the orchestrator's `OnActionCompleted` instead.
- `GetNextCombatant` contains a stale comment, "No longer calling
  CalculateTurnDebts() here - that was the bug!", referring to a removed method.
- The `// CORRECTED VERSION` file header and the `// CORRECTED` / `// NEW` section
  banners are historical refactor markers, not active issues.
- `PreviewTurnOrder` duplicates the round-accumulation and selection logic of
  `GetNextCombatant` rather than sharing it.
- `OnActorDied` / `OnActorResurrected` do not re-cache stats; this is intentional
  (commented in `CalculateSpeedRatios`) but means a resurrected actor keeps its
  pre-death cached stats until an explicit `OnActorSpeedChanged`.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-06-09 | Delayed bonus-turn scheduler (Emerald): `FScheduledTurn`/`PendingTurns`, `ScheduleBonusTurn`, decrement-and-fire loop in `AdvanceToNextTurn` (reuses `RequestExtraTurn`), dead-actor skip, combat-scoped clear. Bonus-flag surfacing: `UntakenBonusTurns` + `bCurrentTurnIsBonus` + the `FPreviewTurnEntry::bIsBonusTurn` preview flag (inline scheduled bonuses + current-actor slot tint). | feature/weapon-stones |
