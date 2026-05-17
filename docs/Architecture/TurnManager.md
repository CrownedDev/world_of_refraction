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

### `FCombatantTurnDebt` (USTRUCT)

Per-combatant scheduling record:
- `AActor* Actor`, `int32 TeamIndex`, `int32 PositionInTeam` — identity.
- `float TurnsOwed` — accumulated debt.
- `int32 TurnsTaken` — turns this combatant has actually taken.
- `float SpeedRatio` — recomputed by `CalculateSpeedRatios()`.
- Cached tie-break stats: `CachedSpeed`, `CachedActionSpeed`, `CachedMind`,
  `CachedBody`, `CachedSpirit`.

## How It Works

### Initialization (`InitializeCombat`)

1. If combat is already active, calls `EndCombat()` first.
2. Clears `Combatants`, resets `GlobalTurnCount`, `CurrentActor`, `PreviousActor`.
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
   `GlobalTurnCount`, and broadcasts `OnTurnStarted(CurrentActor, GlobalTurnCount)`.

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
state). Returns the projected actor sequence. Used by UI and `DebugPrintTurnOrder`.

### Speed/life-state changes

- `OnActorSpeedChanged(Actor)` — re-caches that combatant's stats, recalculates all
  ratios, broadcasts `OnSpeedChanged`.
- `OnActorDied(Actor)` / `OnActorResurrected(Actor)` — recalculate ratios (the
  slowest combatant may have changed); they do NOT re-cache stats.
- `RequestExtraTurn(Actor)` — credits `+1.0` to that combatant's `TurnsOwed`, so the
  debt scheduler picks it again soon.

### Ending combat (`EndCombat`)

Broadcasts `OnCombatEnded(GlobalTurnCount)`, then clears `bCombatActive`,
`Combatants`, `CurrentActor`, `PreviousActor`, and resets `GlobalTurnCount`.

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
- UI — consumes `PreviewTurnOrder` for turn-order display.

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
