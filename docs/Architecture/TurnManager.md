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
- **Pinned bonus turns preempt all of the above**: a ready `PendingTurns` entry fires
  at its slot regardless of debt (see *Pinned bonus turns*).

Debt accumulates per ROUND, not per turn — the file's top comment
("CORRECTED VERSION") and the inline notes call out that per-turn accumulation was
a prior bug.

The single scheduler step is `AdvanceSimState`: the live path (`AdvanceToNextTurn`,
real state) and the materialized upcoming-turn **belt** (`RebuildBelt`, scratch state)
both run through it — turn selection exists in exactly one place.

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
- `TArray<FScheduledTurn> PendingTurns` — pinned bonus turns (Emerald); delay burns
  down on normal turns only, ready entries fire from `AdvanceSimState`'s pinned-fire
  step; see *Pinned bonus turns*.
- `TArray<FPreviewTurnEntry> TurnBelt` — materialized upcoming turns (excludes the
  current turn), up to `TURN_BELT_HORIZON` (16). Rebuilt by `RebuildBelt` every
  `AdvanceToNextTurn` *before* `OnTurnStarted` fires; `PreviewTurnOrder` slices it.
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

### `FScheduledTurn` (USTRUCT)

A pinned bonus turn (Emerald) — fires at its slot regardless of debt:
- `AActor* Actor` — who receives the bonus turn(s).
- `int32 TurnsRemaining` — delay in NORMAL turns until fire; decremented only when a
  normal (debt-picked) turn is taken, never by bonus turns. Ready at `<= 0`.
- `int32 Count` — bonus turns granted when it fires (S-rank = 2, back-to-back).

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
2. Calls `AdvanceSimState(Combatants, PendingTurns, Picked)` — the single scheduler
   step, on REAL state. Returns false when no living combatant remains → error log +
   `EndCombat()`.
3. Sets `CurrentActor = Picked.Actor`, `bCurrentTurnIsBonus = Picked.bIsBonusTurn`
   (set *before* the broadcast so the UI's refresh reads the result), increments
   `GlobalTurnCount`.
4. `RebuildBelt()` — the belt reflects post-advance state. Deliberately *before* the
   broadcast: the turn-order strip refreshes synchronously inside `OnTurnStarted` and
   reads `PreviewTurnOrder` (a belt slice).
5. Broadcasts `OnTurnStarted(CurrentActor, GlobalTurnCount)`.

### The scheduler step (`AdvanceSimState`)

Advances ONE turn on the passed state (live arrays or belt scratch copies):

1. **Pinned-fire** — scans `Pending` (FIFO) for a ready entry (`TurnsRemaining <= 0`).
   If the grantee is alive: emit it as a bonus turn (`bIsBonusTurn=true`), decrement
   `Count` (entry removed at 0), `TurnsTaken++` on the grantee, return — no debt
   machinery runs and no delay countdown happens (bonus turns don't burn other
   bonuses' delay). A dead/invalid grantee's entry is dropped without emitting a turn.
2. **Round-check** — if no living combatant has net debt above `KINDA_SMALL_NUMBER`,
   adds each combatant's `SpeedRatio` to its `TurnsOwed` (all entries, dead included).
3. **Select** — living combatant with highest net debt; ties via
   `ShouldBreakTieInFavor()`. `TurnsTaken++` on the pick (a normal, unflagged turn).
4. **Delay countdown** — every `Pending` entry's `TurnsRemaining` drops by 1 (normal
   turns only — this never runs on the pinned-fire path).

Deliberately log-free: `RebuildBelt` replays it 16× per rebuild.

### The belt (`RebuildBelt`)

Copies `Combatants` + `PendingTurns` into scratch arrays and runs `AdvanceSimState`
forward up to `TURN_BELT_HORIZON` (16) steps, filling `TurnBelt`. Pinned bonuses
therefore appear in the belt at their exact future slots.

### Tie-breaking (`ShouldBreakTieInFavor`)

Ordered cascade:
1. Higher `CachedSpeed` wins.
2. Higher `CachedActionSpeed` wins.
3. Underdog rule — *lower* total of Mind+Body+Spirit wins (rewards glass cannons).
4. Higher `CachedBody`, then 5. higher `CachedMind`, then 6. higher `CachedSpirit`.
7. Deterministic fallback — lower `TeamIndex`, then lower `PositionInTeam`.

### Turn-order preview (`PreviewTurnOrder`)

A slice of `TurnBelt` — no forward simulation. Callers ask ≤ 10; the belt horizon is
16, so the slice always covers them (asking beyond the belt returns what's available).
Pinned Emerald bonuses appear **inline at their exact slot** flagged
`bIsBonusTurn=true`. Used by UI, AI Emerald-exposure lookahead, and
`DebugPrintTurnOrder`. Note: a snapshot from the last advance — mid-turn state changes
don't appear until the next turn (no runtime caller queries in that window).

### Speed/life-state changes

- `OnActorSpeedChanged(Actor)` — re-caches that combatant's stats, recalculates all
  ratios, broadcasts `OnSpeedChanged`.
- `OnActorDied(Actor)` / `OnActorResurrected(Actor)` — recalculate ratios (the
  slowest combatant may have changed); they do NOT re-cache stats.
- `RequestExtraTurn(Actor, bIsBonusTurn=false)` — credits `+1.0` to that combatant's
  `TurnsOwed`, so the debt scheduler picks it again soon (the ExtraAction skill effect
  path). The bonus path is RETIRED: `bIsBonusTurn=true` is a warn-and-ignore no-op —
  bonuses go through `ScheduleBonusTurn`.

### Pinned bonus turns (`ScheduleBonusTurn`, Emerald)

Bonus turns are **positional**, not debt credits: they land at guaranteed slots.
`ScheduleBonusTurn(Actor, DelayTurns, Count=1)` (DelayTurns ≥ 0) appends an
`FScheduledTurn`. Delay N means "N normal turns pass, then fire" — only normal turns
burn delay. All Emerald tiers route through it (F=6 … A=1, count 1); S-rank is
delay 0 count 2 — two back-to-back bonus turns starting on the very next scheduler
step. Bonus = **borrow-from-future**: the fire does `TurnsTaken++` with no `TurnsOwed`
credit, so the grantee's next *normal* turn slips one slot per bonus taken. A dead
grantee's pinned turn is skipped without emitting a turn. `bCurrentTurnIsBonus` is set
from the picked entry each advance and surfaced via `GetCurrentTurnIsBonus()` for the
current-actor slot tint; upcoming bonuses show flagged in the belt/preview.
`PendingTurns` is cleared on `InitializeCombat`/`EndCombat`.

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
- `UItemExecutor` (Emerald) — calls `ScheduleBonusTurn(Target, Delay, Count)` for ALL
  tiers from `ExecuteBonusTurnEffect` (S = delay 0 count 2, identified by delay==0).

## Known Limitations / TODOs

No `// TODO`, `// FIXME`, or `// HACK` markers were found in either file.

Observations from the code:
- `OnTurnEnded` is declared as a delegate and bound by `ACombatOrchestrator`, but
  `TurnManager.cpp` never calls `OnTurnEnded.Broadcast`. `ACombatOrchestrator`'s
  `HandleTurnEnded` even references an `EndCurrentTurn()` method that does not
  exist on `UTurnManager` — so `OnTurnEnded` appears to be effectively dead. End-of-
  turn logic is handled in the orchestrator's `OnActionCompleted` instead.
- The `// CORRECTED VERSION` file header and the `// CORRECTED` / `// NEW` section
  banners are historical refactor markers, not active issues.
- `AccumulateDebtRound` is dead since the 2c cleanup deleted `GetNextCombatant` (its
  only caller) — `AdvanceSimState` inlines the same loop because it operates on passed
  state, not members. Candidate for a follow-up deletion.
- `OnActorDied` / `OnActorResurrected` do not re-cache stats; this is intentional
  (commented in `CalculateSpeedRatios`) but means a resurrected actor keeps its
  pre-death cached stats until an explicit `OnActorSpeedChanged`.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-06-09 | Delayed bonus-turn scheduler (Emerald): `FScheduledTurn`/`PendingTurns`, `ScheduleBonusTurn`, decrement-and-fire loop in `AdvanceToNextTurn` (reuses `RequestExtraTurn`), dead-actor skip, combat-scoped clear. Bonus-flag surfacing: `UntakenBonusTurns` + `bCurrentTurnIsBonus` + the `FPreviewTurnEntry::bIsBonusTurn` preview flag (inline scheduled bonuses + current-actor slot tint). | feature/weapon-stones |
| 2026-06-11 | Turn-order migrated to materialized belt. `AdvanceSimState` is the single scheduler step driving both live turns and the belt; `PreviewTurnOrder` now slices `TurnBelt`. Bonus turns are positional (pinned `PendingTurns` entries with `Count`), not debt credits: delay N = N normal turns then fire, S-rank = delay0 count2 (two back-to-back). Bonus = borrow-from-future (`TurnsTaken++` no `TurnsOwed` credit, so grantee's next normal turn slips one slot per bonus). Dead grantee's pinned turn is skipped. Retired: `GetNextCombatant`, `UntakenBonusTurns`, debt-soft `RequestExtraTurn` bonus path (now warn-no-op). | feature/weapon-stones |
