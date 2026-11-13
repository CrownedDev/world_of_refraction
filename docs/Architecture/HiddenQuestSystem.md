# Hidden Quest System

## Overview
Hidden quests (MemoryCatalogue §3.6; Shangri-La Frontier's Unique Scenarios, D-100) are multi-step quests found like hidden memories. A start memory begins one; its steps are condition sets done in order; the last step unlocks an end memory, which carries the quest's rewards. How a quest starts is never shown. **D-171 (Crown, 2026-11-09; B-397):** once found, the player is told privately and the journal shows the current step's goal only; later steps and the end memory stay hidden until reached or earned. **Q-397b (Crown, 2026-11-12):** masked by default for screen-sharing: notices never name the quest ("Hidden quest found. Open the journal to view it."; its end memory "New memory found. …"), and the journal shows "Hidden Quest <n>" (found order) until the player presses Reveal; it masks again when the journal closes.

## Architecture
- `UHiddenQuestData` (`Memory/HiddenQuestData.h`, primary data asset, AlwaysCook): `QuestId`, `Starts` (`FHiddenQuestStart`: a memory and the depth it must have for the player; None = any), `Steps` (`FHiddenQuestStep`: an `FWoRConditionSet`, `TimesInOneRun`, a designer note), `EndMemory`. Debug: `GetQuestString`, `PrintQuest` button. B-397: `GetDisplayTitle` / `GetGoalAfter` give placeholder text for a hidden quest's missing Title / Goal; `IsDataValid` warns for each.
- `UHiddenQuestSubsystem` (`Memory/HiddenQuestSubsystem.h`, game instance subsystem: cross-run save state, not per-fight): `Progress` per quest id (`StepsDone`, `bFinished`, `FoundOrder` (Q-397), the transient `RunCount` of the step in progress). `StartQuest`, `CheckEvent`, `EndRun`, `Capture` / `Restore`, `GetQuestsString`, `GetDepthForPlayer`. B-397: every notice goes through `ShowNotice` (this machine's screen only; kept as `GetLastNotice`); `GetVisibleQuests` lists started quests main, side, then hidden (unfinished first, newest found first: Q-397), each with the current goal only.
- Save: `FSavedHiddenQuests` / `FSavedQuestProgress` in `FWoRSaveData` (v25, D-059).

## Integration Points
- B-377b: a client's `ReportToHost` → `AWoRPlayerState::ServerReportQuests`; the host's `CheckFriendsQuests` → `Client_QuestStepMet` → `ApplyStepMetFromHost`; a friend's worn gear → `Client_StartQuest`. The host's own steps read its own part of the event.
- B-381: `FBoundQuest` on `UEquipmentDataBase::BoundQuests` / `UEvolutionItemData::BoundQuests`; `StartGearQuests` runs first in `CheckEvent` (the event's worn gear and primary evolution, this player's own characters).
- `UMemoryJournalSubsystem::OnMemoryRecorded` → `HandleMemoryRecorded` starts quests (any machine's journal).
- `MemoryConditions::CheckEvent` → `UHiddenQuestSubsystem::CheckEvent` after the event's memories.
- `UTrialRunSubsystem::ExitTrial` → `EndRun`.
- `USaveSubsystem` `CaptureInto` / `LoadForPlayer`.
- End memory through `MemoryUnlock::Unlock` (`EMemorySource::HiddenQuest`).

## Known Limitations / TODOs
- Co-op (B-377b): a friend's curses and counters are not read for their steps on the host; a client's quest end memory is not added to the host's known set.
- Notices, and the placeholder title / goal for unwritten hidden quests, are placeholder text (Crown's words).
- A load replaces any progress started before it; a mid-run reload loses the step in progress's count (by design: it resets at run end anyway).
- The quest list is gathered once per game instance: an asset added during a session is seen next session.
- Quest content is Crown's.
- "Quest complete" on a hidden quest's finish, the placeholder title and goal text, and the journal order (main, side, hidden) are reference-backed defaults (Q-397); Crown confirmed them and asked for the newest found quest first within each group (built, save v35). The finish notice is replaced at once by the "New memory" notice (one on-screen slot).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created (B-377): data asset, subsystem, save v25, `wor.Quest.Print`, HiddenQuestSpec. | `feature/B-377-hidden-quests-cloud` (in dev) |
| 2026-11-08 | Review: the end memory is its player's alone and always lands (`Finish`); a quest started this event waits for the next; empty steps finish at once; quests gathered once. | `feature/B-377-hidden-quests-cloud` (in dev) |
| 2026-11-08 | B-388 (MemoryCatalogue §3.12 / §3.13): main and side quests are this data with `EQuestKind` Main / Side, a `Title` and a step `Goal`; `GetVisibleQuests`, `StartQuestById` (`wor.Quest.Give`), visible notices; the journal's Quests page. | `feature/B-388-main-side-quests-cloud` (in dev) |
| 2026-11-08 | B-381 (D-165): gear and evolutions carry hidden quests (`FBoundQuest`), started while worn (`StartGearQuests`). | `feature/B-381-gear-quests-cloud` (in dev) |
| 2026-11-08 | B-377b: co-op friends' quests (report, host check on their part, step met / start RPCs); `CountStepMet`; the host's own quests read its own part. | `feature/B-377b-coop-quests-journal-save-cloud` (in dev) |
| 2026-11-08 | B-381 activations (Crown, MemoryCatalogue §3.11): `FWornItemState` per worn item (`MemoryConditions::GetWornStates`: weapons, adapter, apparel; socket element, evolution, stone, tier, upgrade level), the `WornItemState` condition reading `FWoRConditionContext::BoundItem`, set by `StartGearQuests` per item. | `feature/B-381-gear-quests-cloud` (in dev) |
| 2026-11-09 | D-171 recorded: a found hidden quest shows its current step's goal (job B-397); docs only. | `docs/D-171-hidden-quest-steps` |
| 2026-11-09 | B-397 (D-171): "Hidden quest discovered: <title>" private notice (`ShowNotice`, `GetLastNotice`); found hidden quests in `GetVisibleQuests` after main and side with the current goal only; step done notices name the next goal; placeholder title / goal and `IsDataValid` warnings; `wor.Quest.Print` shows every quest's goal and the last notice. | `feature/B-397-hidden-quests-shown` (in dev, built 2026-11-12) |
| 2026-11-12 | Q-397 (Crown): the journal lists the newest found quest first within each group (`FHiddenQuestProgress::FoundOrder`, `NextFoundOrder`; saved as `FSavedQuestProgress::FoundOrder`, save v35). | `fix/Q-397-newest-first` |
| 2026-11-12 | Q-397b (Crown): hidden quests masked by default. Notices `HiddenFound` / `HiddenStepDone` / `HiddenFinished` never name the quest; the journal masks each as "Hidden Quest <n>" (found order) with a Reveal button, masked again on close (`UMemoryJournalWidget::RevealQuest` / `Close`). | `feature/Q-397b-hidden-quest-mask` |
| 2026-11-12 | Q-397b flag (Crown): a hidden quest's end memory notice is masked too (`HiddenNewMemory`: "New memory found. Open the journal to view it."); main and side quests still name theirs. | `fix/Q-397b-mask-new-memory` |
| 2026-11-14 | B-457 (D-196, D-198): the player reads "Secret quest". `WoRGameNames::GetQuestKindName(Hidden)` is "Secret"; the notices are "Secret quest found. / updated. / complete."; the placeholder title is "Unnamed secret quest"; the journal masks as "Secret Quest <n>". The journal's Reveal is one toggle: `UMemoryJournalWidget::ToggleQuestShown` (was `RevealQuest`), its button reads "Show …" or "Hide …". Code names, the asset validation warnings and `EMemorySource`'s editor label stay "Hidden". | `fix/B-457-secret-quest` |
