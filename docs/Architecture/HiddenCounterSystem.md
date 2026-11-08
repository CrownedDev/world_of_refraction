# Hidden Counter System

## Overview
Hidden counters (MemoryCatalogue §3.9; Shangri-La Frontier's hidden stats) are running totals on the player's save that keep counting across runs. Reaching a milestone opens a memory (the CounterReached condition).

## Architecture
- `UHiddenCounterSubsystem` (`Memory/HiddenCounterSubsystem.h`, game instance subsystem: the player's save state): `Counts` (counter → character asset name, None = all → count). `Add`, `Get`, `Capture` / `Restore`, `GetCountersString`, `KeyFor`.
- `HiddenCounters::PARRIES`, `KILLS` (TCHAR names).
- Condition: `EMemoryConditionKind::CounterReached` (`CounterId`, `CounterTarget`, optional `Character`) → `EWoRConditionSubject::CounterReached` (`CounterId`, `Operator`, `Value`).
- Save: `FSavedCounters` in `FWoRSaveData` (v27, D-059).

## Integration Points
- `ABattleGameMode`: `HandleDefenseResolved` (the defense system's `OnDefenseResolved`: a Perfect Guard) and `HandleTargetKilled` → `AddHiddenCount`; a co-op friend's character → `AWoRPlayerState::Client_AddCounter`.
- `WoRConditions::MakeContext` fills `FWoRConditionContext::Counters`; a friend's part on the host reads none (`AreMet(..., bReadsThisPlayersState = false)`).
- `USaveSubsystem` captures and restores it.

## Known Limitations / TODOs
- Only Parries and Kills are counted so far; each new counter is one constant and one `AddHiddenCount` call.
- On the host, a friend's CounterReached fails until their counts reach the host (B-377b).
- Milestones are Crown's.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-379): counters per character and all, Parries and Kills, the CounterReached condition, save v27, `wor.Counter.Print` / `wor.Counter.Add`, HiddenCounterSpec. | `feature/B-379-hidden-counters-cloud` (UNBUILT) |
