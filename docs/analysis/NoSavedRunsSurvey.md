# Survey — mid-run save and resume vs "no saved runs" (B-408, D-172)

**Ruling:** D-172 (Crown, 2026-11-12): there are no saved runs; to leave a run the party extracts. D-176 (2026-11-13): quitting a run loses everything from it.
**Scope:** survey only, nothing deleted (D-172's notes). Read on `dev` at 2026-11-14 (cloud writer 6). File:line refs are from that read.

## The short answer

Almost nothing can go, because **the same saved run serves two things that stay**:

1. **Co-op host hand-over** (B-234, B-234b, B-234d, B-428). The host sends a checkpoint copy of the save, the run included, to every guest. When the host drops, the next player hosts from that copy. `AdoptCheckpointFrom` then sets the pending run, and the next hub resumes it through the same `TryResumeRun` → `RestoreRun` → `ResumeRun` path a reload uses.
2. **Settling a game closed mid-run.** The pool is saved mid-run *with the run's changes in it*: run-only gear sits in the store, and home pieces are in their run state. Only the saved run layer (`FSavedRun::RunMarks`, `RunStartItems`) knows how to put them back. Delete it, and closing the game during a run would turn everything found in it into home gear for free. That would make closing the game better than extracting.

So the change D-172 needs is **one behaviour**, not a deletion. A run found in the player's own save at start is no longer resumed. It is **settled as a quit**: it is restored just long enough to apply D-176's end (home pieces back, found items taken back, nothing paid, run recruits leave), then the player is in the hub. The hand-over path keeps resuming, because there the run is still being played by the others.

## Each piece

| # | Piece | Where | What reads it | Remove / keep | Why |
|---|---|---|---|---|---|
| 1 | `FSavedRun` (trial, seed, variation, difficulty, beaten fights, found features, memories, Gold, wipes, losses, drop, tally, mini-boss marks, revivals, run layer, run-start items) | `Public/Save/WoRSaveTypes.h:242`, in `FWoRSaveData::Run` `:890` | `CaptureRun` / `RestoreRun`, the checkpoint copy (`CoopCheckpoint.cpp`), host succession (`HostSuccession.cpp`), the title screen (`TitleScreenModel.cpp:51`), `wor.Save.Print` (`SaveDebug.cpp:85`) | **Keep** | Hand-over hosts from it; settling a closed game needs its run layer and found counts |
| 1a | `FSavedRun::DropEncounters` | `WoRSaveTypes.h:~291` | nothing (OBSOLETE since B-362c) | **Remove** at the next save bump | Already marked OBSOLETE; unrelated to D-172 but in the same struct |
| 2 | `USaveSubsystem::CaptureRun` | `Private/Save/SaveSubsystem.cpp:1214` | `CaptureInto` `:811` (every save: fight exit, rest spot, level end, shutdown) | **Keep** | The checkpoint copies are built from it |
| 3 | `USaveSubsystem::RestoreRun` (and its calls `RestoreRunLosses`, `RestoreDrop`, `RestoreRunTally`, `MarkRunBossBeaten`, `RestoreMiniBossRevivals`, `RestoreRunLayer`, run-start items) | `SaveSubsystem.cpp:1269` | `TryResumeRun` `:738` | **Keep** | Hand-over resumes with it; settling as a quit needs the layer and tally put back first, so D-176's end takes back exactly what the run added |
| 4 | Load-time pending run (`PendingRun`, the `OnWorldInitializedActors` wait) | `SaveSubsystem.cpp:446-458`, `:715`, `:723` | `Initialize` (own slot), `AdoptCheckpointFrom` `:562` (hand-over) | **Keep, change for the own slot** | From the own slot: settle as a quit (proposed B-408a). From a hand-over copy: resume as now |
| 5 | `USaveSubsystem::TryResumeRun` | `SaveSubsystem.cpp:723-756` | both load paths above | **Keep, change** | Branch on `bPlayingHostCopy`: hand-over → `ResumeRun` (as now); own slot → `AbandonRun` after the restore (B-408a) |
| 6 | `wor.Save.ResumeRun` console variable | `SaveSubsystem.cpp:357-361` | `TryResumeRun` `:731` | **Remove** (with B-408a) | A switch to *not* resume; once the own slot never resumes it has nothing to switch. Hand-over must not be switchable off |
| 7 | `DropRunVitals` | `SaveSubsystem.cpp:758` | `TryResumeRun` when a run can't be restored | **Keep** | A run that can't be restored still has its HP, wear and recruits on the party; this clears them. The quit settle also needs it |
| 8 | Party captured "in run" (vitals, run recruits kept) | `CaptureInto` `:778-781` → `CaptureParty(…, bInRun)` | the save | **Keep** | Hand-over carries the run's HP and recruits. For a settled quit, `DropRunVitals` clears them |
| 9 | `UTrialRunSubsystem::RestoreRun` | `Private/Trial/TrialRunSubsystem.cpp:796` | `USaveSubsystem::RestoreRun` | **Keep** | As 3 |
| 10 | `UTrialRunSubsystem::ResumeRun` | `TrialRunSubsystem.cpp:840` | `TryResumeRun` `:754` | **Keep** | Only the hand-over calls it after B-408a |
| 11 | `RestoreRunTally`, `RestoreRunLosses`, `RestoreDrop`, `RestoreMiniBossRevivals` (B-073, B-362, B-362b/c, B-187c) | `TrialRunSubsystem.h:312`, `.cpp:664`, `:656`, `:929` | `USaveSubsystem::RestoreRun` | **Keep** | Hand-over: the run goes on with its wipes, tally and marks. Settled quit: the tally must be there so the quit visibly pays nothing of it (the log line says what was lost) |
| 12 | `UPoolSubsystem::RestoreRunLayer` and the saved run-start items (B-314b, B-439b) | `Private/Pool/PoolRunLayer.cpp:74`; `SaveSubsystem.cpp:1318-1340` | `USaveSubsystem::RestoreRun` | **Keep** | The heart of item 2 above: without it a closed game keeps the run's gear |
| 13 | `RekeyRunForNewHost` (B-428) | `SaveSubsystem.cpp:474` | `AdoptCheckpointFrom` | **Keep** | Hand-over only |
| 14 | `AdoptCheckpointFrom`, `EndHandedOverRun`, `ReturnToOwnSave`, `MergeRunUpgradeMemory` (B-234b, B-234d) | `SaveSubsystem.cpp:517`, `:630`, `:680`, `:565` | `HostSuccession.cpp` | **Keep** | Hand-over only |
| 15 | Checkpoint copies to guests (`WoRCheckpoint::SendToClients`) | `SaveSubsystem.cpp:821`, `Private/Save/CoopCheckpoint.cpp` | `SaveNow` | **Keep** | Hand-over only |
| 16 | Rest-spot return stand and trial reload (B-187c: `NoteRestStand`, `SetPendingReturnStand`, `ReloadTrialLevel`) | `TrialRestSpot.cpp:100-105`, `:148`; `TrialRunSubsystem.cpp:953` | the rest spot's revive | **Keep** | In-session: a revive reloads the trial level and puts the party at its rest spot. It never reads the save |
| 17 | Title screen's Continue line naming the run's trial | `Private/UI/Menus/TitleScreenModel.cpp:51` | the title screen | **Change** (B-408a) | After B-408a a run in the own save never resumes, so the place should read "The hub". It could also warn: "Your last run was lost when the game closed." |
| 18 | `URunStateComponent` (replicated run state) | `Private/Player/RunStateComponent.cpp` | co-op guests' HUD and rules | **Keep** | Replication, not the save |
| 19 | `wor.Save.Print`'s run line | `Private/Debug/Save/SaveDebug.cpp:85` | debug | **Keep** | Shows the saved run, which still exists for hand-over and settling |

## Proposed jobs (for Crown)

- **B-408a — A game closed mid-run is settled as a quit** (D-172, D-176). In `USaveSubsystem::TryResumeRun`, a run from the player's own slot (`!bPlayingHostCopy`) is restored, then ended by `UTrialRunSubsystem::AbandonRun`: home pieces back, found items taken back, nothing paid, run recruits leave, vitals cleared. A handed-over run resumes as now. Delete `wor.Save.ResumeRun`. The title screen's Continue line says "The hub", plus a one-line note that the last run was lost. Spec: a save with a run loads into the hub with the run's found gear gone and its home pieces back; a hand-over copy still resumes. M, CC.
- **B-408b — `FSavedRun::DropEncounters` goes** with the next save bump (OBSOLETE since B-362c). S, CC. Fold it into whichever job bumps the save next.

Nothing else is proposed for removal. The resume path is the hand-over's path.

## The question this raises (Q-408)

**A game that closes in the middle of a run (quit to desktop, crash, power cut): what happens to the run?** Recorded as Q-408 in `DesignDecisions.md`. The reference-backed default is **A**.
- **A (default): it counts as a quit.** The run is lost on the next start, with D-176's rules. Escape from Tarkov counts leaving a raid as "missing in action": the gear is lost. Dark and Darker does the same for a disconnect. Both games' players accept the rule because it keeps extracting meaningful. It also closes the "close the game instead of quitting" loophole.
- **B: crash recovery.** The run resumes once, from the last checkpoint. Tarkov does let a player reconnect to a raid still running after a crash, and Returnal's lack of any mid-run save was its most criticised flaw until the Suspend Cycle patch. This runs against D-172's "no saved runs", so it would need Crown's word.
