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
- On the host, a co-op friend's curses are not known for conditions read on their part (with B-377b).
- Which curses exist is Crown's.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-378): held curses, Curse / RemoveCurse rewards, the CurseHeld condition, save v26, `wor.Curse.Print`, CurseSpec. | `feature/B-378-curses-cloud` (UNBUILT) |
