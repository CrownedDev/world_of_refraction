# Hidden Quest System

## Overview
Hidden quests (MemoryCatalogue §3.6; Shangri-La Frontier's Unique Scenarios, D-100) are multi-step quests found like hidden memories. A start memory begins one; its steps are condition sets done in order; the last step unlocks an end memory, which carries the quest's rewards. No text ever says what the next step is.

## Architecture
- `UHiddenQuestData` (`Memory/HiddenQuestData.h`, primary data asset, AlwaysCook): `QuestId`, `Starts` (`FHiddenQuestStart`: a memory and the depth it must have for the player; None = any), `Steps` (`FHiddenQuestStep`: an `FWoRConditionSet`, `TimesInOneRun`, a designer note), `EndMemory`. Debug: `GetQuestString`, `PrintQuest` button.
- `UHiddenQuestSubsystem` (`Memory/HiddenQuestSubsystem.h`, game instance subsystem: cross-run save state, not per-fight): `Progress` per quest id (`StepsDone`, `bFinished`, the transient `RunCount` of the step in progress). `StartQuest`, `CheckEvent`, `EndRun`, `Capture` / `Restore`, `GetQuestsString`, `GetDepthForPlayer`.
- Save: `FSavedHiddenQuests` / `FSavedQuestProgress` in `FWoRSaveData` (v25, D-059).

## Integration Points
- B-381: `FBoundQuest` on `UEquipmentDataBase::BoundQuests` / `UEvolutionItemData::BoundQuests`; `StartGearQuests` runs first in `CheckEvent` (the event's worn gear and primary evolution, this player's own characters).
- `UMemoryJournalSubsystem::OnMemoryRecorded` → `HandleMemoryRecorded` starts quests (any machine's journal).
- `MemoryConditions::CheckEvent` → `UHiddenQuestSubsystem::CheckEvent` after the event's memories.
- `UTrialRunSubsystem::ExitTrial` → `EndRun`.
- `USaveSubsystem` `CaptureInto` / `LoadForPlayer`.
- End memory through `MemoryUnlock::Unlock` (`EMemorySource::HiddenQuest`).

## Known Limitations / TODOs
- Co-op: a friend's quests do not move on events the host runs (B-377b).
- The journal is not saved yet, so a start memory is gone after a reload while the quest's progress stays (Q-377 note).
- Notices are placeholder text (Crown's words).
- A load replaces any progress started before it; a mid-run reload loses the step in progress's count (by design: it resets at run end anyway).
- The quest list is gathered once per game instance: an asset added during a session is seen next session.
- Quest content is Crown's.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-377): data asset, subsystem, save v25, `wor.Quest.Print`, HiddenQuestSpec. | `feature/B-377-hidden-quests-cloud` (in dev) |
| 2026-11-08 | Review: the end memory is its player's alone and always lands (`Finish`); a quest started this event waits for the next; empty steps finish at once; quests gathered once. | `feature/B-377-hidden-quests-cloud` (in dev) |
| 2026-11-08 | B-381 (D-165): gear and evolutions carry hidden quests (`FBoundQuest`), started while worn (`StartGearQuests`). | `feature/B-381-gear-quests-cloud` (in dev) |
