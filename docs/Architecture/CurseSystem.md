# Curse System

## Overview
Curses (MemoryCatalogue §3.8; Shangri-La Frontier's curses, D-100) are rewards that are also penalties and keys. A memory gives one; it lasts across runs until its own quest or memory lifts it; while held it opens other secrets (the CurseHeld condition).

## Architecture
- `UCurseSubsystem` (`Memory/CurseSubsystem.h`, game instance subsystem: the player's save state): `HeldCurses` (ids). `AddCurse`, `RemoveCurse`, `HasCurse`, `ApplyRewards`, `Capture` / `Restore`, `GetCursesString`.
- Data: `EMemoryRewardKind::Curse` / `RemoveCurse` with `FMemoryReward::RewardId`; condition `EMemoryConditionKind::CurseHeld` / `EWoRConditionSubject::CurseHeld` with `CurseId`.
- Save: `FSavedCurses` in `FWoRSaveData` (v26, D-059).

## Integration Points
- `UMemoryJournalSubsystem::OnMemoryRecorded` → `ApplyRewards` (on whichever machine's journal records it: the player's own).
- `WoRConditions::MakeContext` fills `FWoRConditionContext::HeldCurses` from the journal's game instance.
- `USaveSubsystem` captures and restores it.

## Known Limitations / TODOs
- What a curse weakens in a fight, and its name and text (`UCurseData`): B-378b.
- On the host, a co-op friend's part never reads the host's curses (`AreMet(..., bReadsThisPlayersState = false)`): their CurseHeld fails there until their own curses reach the host (B-377b).
- A lifted curse is never given again (`LiftedCurses`, saved): the journal is not saved yet, so its memory could be recorded again after a restart.
- CurseHeld reads memory contexts only; fight contexts (bosses, skills, reactions) do not set `HeldCurses` yet (B-378b).
- Which curses exist is Crown's.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-378): held curses, Curse / RemoveCurse rewards, the CurseHeld condition, save v26, `wor.Curse.Print`, CurseSpec. | `feature/B-378-curses-cloud` (in dev) |
| 2026-11-08 | Review: a friend's part never reads the host's curses; a lifted curse stays lifted (`FSavedCurses::Lifted`). | `feature/B-378-curses-cloud` (in dev) |
