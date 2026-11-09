# Curse System

## Overview
Curses (MemoryCatalogue §3.8; Shangri-La Frontier's curses, D-100) are rewards that are also penalties and keys. A memory gives one; it lasts across runs until its own quest or memory lifts it; while held it opens other secrets (the CurseHeld condition).

## Architecture
- `UCurseSubsystem` (`Memory/CurseSubsystem.h`, game instance subsystem: the player's save state): `HeldCurses` (ids). `AddCurse`, `RemoveCurse`, `HasCurse`, `ApplyRewards`, `Capture` / `Restore`, `GetCursesString`.
- Data: `EMemoryRewardKind::Curse` / `RemoveCurse` with `FMemoryReward::RewardId`; condition `EMemoryConditionKind::CurseHeld` / `EWoRConditionSubject::CurseHeld` with `CurseId`.
- Save: `FSavedCurses` in `FWoRSaveData` (v26, D-059).

## Integration Points
- B-378b: `UCurseData` (per CurseId: Title, Description, StatPenalty, ResistancePenalty); `GetStatPenalty` / `GetResistancePenalty` / `IsCursedHere` read by `ULoadoutComponent::GetActiveStatBonus` / `GetActiveResistanceBonus` for the holder's own party characters.
- `UMemoryJournalSubsystem::OnMemoryRecorded` → `ApplyRewards` (on whichever machine's journal records it: the player's own).
- `WoRConditions::MakeContext` fills `FWoRConditionContext::HeldCurses` from the journal's game instance.
- `USaveSubsystem` captures and restores it.

## Known Limitations / TODOs
- Journal display of curses; max pools re-synced the moment a curse changes mid-run (they re-read at the next loadout change).
- On the host, a co-op friend's part never reads the host's curses (`AreMet(..., bReadsThisPlayersState = false)`): their CurseHeld fails there until their own curses reach the host (B-377b).
- A lifted curse is never given again (`LiftedCurses`, saved): the journal is not saved yet, so its memory could be recorded again after a restart.
- CurseHeld reads memory contexts only; fight contexts (bosses, skills, reactions) do not set `HeldCurses` yet (B-378b).
- Which curses exist is Crown's.
- The party check matches by character asset: an enemy built from a party member's asset would also be weakened.
- No spec drives a party member's stats through `ULoadoutComponent` with a curse held (the subsystem sums are tested; PIE B-378b covers the rest).
- **Co-op regain gaps (merge review, 2026-11-09).** On the host, `CheckRegain` reads the whole event, so a friend's find can regain the host's cleared curse (`HiddenQuestSubsystem::CheckEvent` uses `FilterParty` for this; do the same). A client's cleared quest-step curse is never regained: `RegainCurses` runs only in the host's own `CheckEvent`. TODO(co-op).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-378): held curses, Curse / RemoveCurse rewards, the CurseHeld condition, save v26, `wor.Curse.Print`, CurseSpec. | `feature/B-378-curses-cloud` (in dev) |
| 2026-11-08 | Review: a friend's part never reads the host's curses; a lifted curse stays lifted (`FSavedCurses::Lifted`). | `feature/B-378-curses-cloud` (in dev) |
| 2026-11-08 | B-378b: a curse's penalty (`UCurseData`) weakens the holder's own party characters through the gear stat and resistance sums. | `feature/B-378b-curse-penalty-cloud` (in dev) |
| 2026-11-08 | Q-378 changed by Crown: a hidden quest gives curses (`FHiddenQuestStep::Rewards`); the player clears one (`UCurseSubsystem::ClearCurse`, `wor.Curse.Clear`; hub button B-378b); a lifted or cleared curse comes back when its giver is met again (`CheckRegain` for memories, `UHiddenQuestSubsystem::RegainCurses` for steps); `AddCurse` no longer refuses a lifted one. | `feature/B-378-curses-cloud` (UNBUILT) |
| 2026-11-08 | Q-378 changed by Crown: placeholder hub service `UCurseClearWidget` (`wor.UI.ClearCurse`; refused during a run) clears a held curse by the player's choice; its penalty stops until it is regained. | `feature/B-378b-curse-penalty-cloud` (UNBUILT) |
