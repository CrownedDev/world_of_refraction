# Cloud branches: merge plan (2026-11-08)

For the desktop builder. It covers every cloud branch marked `UNBUILT` in `docs/Backlog.md`, as of `origin/dev` at `dba4a0f5`.

## This is a trial merge

`integration/cloud-2026-11-08` is a **trial**. It was cut from `origin/dev` and merges every branch below in this order (`--no-ff`), with each conflict resolved. It shows that the whole set goes together and what each conflict looks like.

- **Never merge `integration/cloud-2026-11-08` into `dev`.**
- The real merges still go **branch by branch**: build, test, `wor-reviewer` and Crown's PIE pass for each job.
- Use the trial branch to look up a resolution: `git show <merge commit> -- <file>`. Each merge commit is titled `Integration (trial): merge <branch>`.
- Nothing was built or tested in the cloud. The first desktop build of the trial branch is the first compile of the combined code.

## How to use the table

Go top to bottom. Every branch's base comes before it. For each row:

1. Merge the branch into `dev`: `git merge --no-ff -m "<real message>" origin/<branch>`.
2. Resolve the conflicts listed for that row.
3. Build, run the tests, then run the PIE sheet `docs/PIE/B-xxx.md`.

**Conflict kinds used in the table:**

| Kind | What it is | How to resolve |
|---|---|---|
| **docs** | `Backlog.md`, `PIE/README.md`, `DesignDecisions.md`, `Rules/*.md`, `Architecture/*.md` | Keep both sides. Then fix the leftovers below. |
| **save** | The `WoRSaveVersion::LATEST` line in `Public/Save/WoRSaveGame.h` | Take the higher number and write the comment once (see "Save versions"). |
| **ini** | `Config/DefaultGame.ini` `+PrimaryAssetTypesToScan` | Keep every asset type from both sides, once each. |
| **both** | Code hunks where each side adds something different | Keep both, in the order the row says. |

**Leftovers after keeping both sides of the docs:**

- **`Backlog.md`:** the job's old `TODO` row and its new `REVIEW` row both survive. Keep the `REVIEW` row.
- **Rules rows:** some rows are doubled. Keep the newer wording, as the notes say.

| # | Branch | Base (merge first) | Save | Expected conflicts → resolution |
|---|---|---|---|---|
| 1 | `feature/B-189-element-map-look-cloud` | dev | – | none |
| 2 | `feature/B-278-asset-manager-rules-cloud` | dev | – | docs |
| 3 | `feature/B-337e-condition-migration-cloud` | dev | – | none |
| 4 | `chore/B-349-console-names-cloud` | dev | – | none (Backlog: keep the REVIEW row) |
| 5 | `feature/B-355b-boss-moment-rules-cloud` | dev | – | docs |
| 6 | `feature/B-367b-perfect-guard-only-cloud` | dev | – | docs. Rules ED-37 / 37b / 37c: keep B-367b's "Perfect Guard only" wording; drop the older copy. |
| 7 | `feature/B-374-six-schools-cloud` | dev | – | docs |
| 8 | `feature/B-368-notify-validator-cloud` | dev | – | docs (Rules 02) |
| 9 | `feature/B-369-impact-vfx-outcome-cloud` | dev | – | **ini**: keep B-278's AlwaysCook rules and add B-369's `ImpactVisualDefaults` type. Docs. |
| 10 | `feature/B-370-cinematic-window-cloud` | dev | – | **both** `Debug/Combat/CombatCameraDebug.cpp`: B-349's command name `wor.Camera.Print` with B-370's help text. Docs (CombatCamera changelog). **Then apply the read-time constant note below.** |
| 11 | `feature/B-371-shot-patterns-cloud` | dev | – | none |
| 12 | `feature/B-372-camera-framing-checks-cloud` | B-371 | – | none |
| 13 | `feature/B-230-replicated-status-bars-cloud` | dev | – | docs |
| 14 | `feature/B-231-actions-server-multicast-cloud` | B-230 | – | none |
| 15 | `feature/B-230b-grid-slot-replicated-cloud` | B-231 | – | none |
| 16 | `feature/B-231b-cosmetic-projectiles-cloud` | B-231 | – | **both** `Combat/Actions/ActionExecutor.cpp` projectile launch: keep B-371's `OnProjectileLaunched.Broadcast` and B-231b's `Presentation->SendProjectile`. Docs (TurnState changelog). |
| 17 | `feature/B-231c-client-telegraphs-cloud` | B-231b | – | none |
| 18 | `feature/B-231c2-impact-visuals-clients-cloud` | B-231c + B-369 | – | none |
| 19 | `feature/B-228b-client-combat-hud-cloud` | dev | – | docs (TurnState changelog, Rules 06 PA rows) |
| 20 | `feature/B-232b-menu-on-commander-cloud` | B-228b | – | none |
| 21 | `feature/B-232c-run-readers-cloud` | B-232b | – | **both** `UI/Combat/CombatCommandMenuSubsystem.h`: B-374's `BuildSchoolButtons` and B-232c's `HandleFreeCommandApplied` |
| 22 | `feature/B-232d-run-readers-cloud` | B-232c | – | **both** `Trial/TrialRunSubsystem.cpp` includes (`TrialMapLook.h`, `RunStateComponent.h`) |
| 23 | `feature/B-232e-recruit-ai-seat-cloud` | B-232d | – | none |
| 24 | `feature/B-233-sessions-null-cloud` | dev | – | docs (Rules 06 PA-37 area, Backlog, DesignDecisions) |
| 25 | `feature/B-234-checkpoint-copies-cloud` | B-233 | – | **both** `Player/WoRPlayerState.cpp`: B-232e's `ServerRecruit_Implementation`, then B-234's `SendCheckpoint` / `Client_ReceiveCheckpointChunk_Implementation` / `GetCheckpointString` |
| 26 | `feature/B-234b-host-hand-over-cloud` | B-234 | – | none |
| 27 | `feature/B-348b-save-head-starts-cloud` | dev | **21** | docs |
| 28 | `feature/B-348c-broken-darkness-persists-cloud` | B-348b | 21 | docs |
| 29 | `feature/B-351b-stone-socket-carry-cloud` | dev | **22** | **save** (21 → 22). Docs. |
| 30 | `feature/B-351c-one-list-readers-cloud` | B-351b | 22 | docs |
| 31 | `feature/B-351d-crystal-spell-lists-cloud` | B-351c | 22 | docs |
| 32 | `feature/B-357-evolution-one-cost-cloud` | dev | – | docs (LoadoutSystem changelog) |
| 33 | `feature/B-357b-evolution-resolve-path-cloud` | B-357 | – | docs. **Rules 01: B-351b and B-357b both used `CL-28b`.** Renumber the evolution-placement row (B-357b/c) to **CL-28c**. |
| 34 | `feature/B-357c-gear-evolutions-passives-only-cloud` | B-357b | – | docs. Rules 01: keep B-357c's wording of the evolution row (now CL-28c). |
| 35 | `feature/B-073-run-rewards-cloud` | dev | **23** | **save** (22 → 23). Docs. |
| 36 | `feature/B-187-rest-spot-cloud` | dev | 23 | docs |
| 37 | `feature/B-187b-mini-boss-rest-spots-cloud` | B-187 | 23 | docs |
| 38 | `feature/B-363-levelling-at-rest-spots-cloud` | B-187 | 23 | **both** `Trial/TrialRestSpot.cpp`: B-187b's `CanReviveHere` / `ReviveMiniBoss`, then B-363's `StepAway`. **Both** `TrialRunSubsystem.cpp` `RestoreRun`: B-187b's pending-tier resets, then B-363's `bAtRestSpot = false`. Docs. |
| 39 | `feature/B-364-over-the-pillar-cap-cloud` | B-363 | 23 | **both** `TrialRunSubsystem.cpp` includes (add `Character/OverCapRules.h`) |
| 40 | `feature/B-187c-revive-return-and-save-cloud` | B-187b | **24** | **save** (23 → 24). Docs. |
| 41 | `feature/B-234c-planned-host-hand-over-cloud` | B-234b + B-187 | 24 | **both** `UI/Hub/RestSpotWidget.h`: Revive members, then HandOver. **`RestSpotWidget.cpp`:** the button list gets Revive, then HandOver. In `Refresh()`, put **B-234c's hand-over block first, then B-187b's revive block**: the revive side of the hunk also closes `Refresh()` and opens `HandleReviveClicked`, so the other order puts the hand-over code in the wrong function. |
| 42 | `feature/B-235-personal-prism-drops-cloud` | dev | 24 | **both** `TrialRunSubsystem.cpp` `ResetRun`: B-073/B-363/B-187's resets plus B-235's `OtherPlayerDrops.Reset()`. Docs. |
| 43 | `feature/B-235b-personal-rewards-cloud` | B-235 | 24 | **both** `WoRPlayerState.h/.cpp`: B-232e `ServerRecruit` and B-234 checkpoint RPCs, plus B-235b's `Client_ShowFeatureMessage`. Includes: keep both. |
| 44 | `feature/B-235c-instanced-chests-cloud` | B-235b | 24 | `Trial/TrialMapFeature.cpp` chest open: one block, i.e. B-073's `RecordChestReward()` plus B-235c's `RecordFound(PlayerFeatureId…)` / `ShowMessageTo(Pawn…)`. **Both** `.h` (B-232d's `RefreshFromRun`, B-235c's `IsWithinReach` / `GetFeatureIdFor`) and includes. **Then apply the chest-hint fix below.** |
| 45 | `feature/B-187d-co-op-rest-cloud` | B-187c + B-073 + B-235c | 24 | `TrialMapFeature.cpp`: take B-187d's once-per-chest guard around `RecordChestReward()`. `RestSpotWidget.cpp`: HandOver button, then B-187d's `LeaveButton = …`. **Both** `.h` (HandOver and LeaveButton members). |
| 46 | `feature/B-187e-client-rest-menu-cloud` | B-187d | 24 | **both** `RestSpotWidget.cpp` includes (`LevelDropWidget.h`, `WoRLog.h`) |
| 47 | `feature/B-235d-private-memories-cloud` | B-235c | 24 | **both** `WoRPlayerState.h/.cpp` includes (`CoopCheckpoint.h` and `Memory/…`). Docs. |
| 48 | `feature/B-235e-own-memory-finds-cloud` | B-235d | 24 | **both** `WoRPlayerState.cpp` includes; drop the duplicate `#include "Trial/TrialRunSubsystem.h"`. Docs. |
| 49 | `feature/B-375-memory-debug-cloud` | B-235e | 24 | none |
| 50 | `feature/B-376-party-has-cloud` | B-375 | 24 | none |
| 51 | `feature/B-377-hidden-quests-cloud` | B-376 | **25** | **save** (24 → 25). **ini** (keep the hidden-quest type). **Both** `TrialRunSubsystem.cpp` includes (add `Memory/HiddenQuestSubsystem.h`). |
| 52 | `feature/B-378-curses-cloud` | B-377 | **26** | **save** |
| 53 | `feature/B-378b-curse-penalty-cloud` | B-378 | 26 | **ini** (keep the curse AlwaysCook scan). Rules 05 ER-12i: keep B-378b's flat-penalty wording. |
| 54 | `feature/B-379-hidden-counters-cloud` | B-378 | **27** | **save**. Docs (Rules 05). |
| 55 | `feature/B-381-gear-quests-cloud` | B-379 | 27 | none |
| 56 | `feature/B-377b-coop-quests-journal-save-cloud` | B-381 | **28** | **save**. Rules 05 ER-12h: keep B-377b's co-op line. |
| 57 | `feature/B-380-apparel-cloud` | dev | 28 | docs (LoadoutSystem, Backlog, DesignDecisions) |
| 58 | `feature/B-380b-apparel-store-save-cloud` | B-380 | 28 | none |
| 59 | `feature/B-380c-apparel-slots-saved-cloud` | B-380b | **29** | **save** |
| 60 | `docs/Q-164b-amp-cloud` | dev | – | docs (DesignDecisions) |

**Skip:** `feature/B-361b-mentor-tokens-cloud`. It is already in `dev` (the "Merge B-361b" commit; save v17).

**Code that met without a textual conflict.** The reviewer of the trial branch checks these files; read them again after the real merges:

- `Loadout/LoadoutComponent.cpp`: B-357c, B-378b and B-380/B-380c all change stats, resistances and effects.
- `Save/WoRSaveTypes.h` and `SaveSubsystem.cpp`: nine branches add save fields.
- `Inventory/InventoryComponent.cpp`

## Save versions: no renumbering in this order

Each cloud branch took the next free number after the branches it knew of. In the order above the numbers already rise. So **no branch changes its number**. Every `LATEST` conflict resolves to the higher number.

| Version | Branch | What it saves |
|---|---|---|
| 21 | B-348b | Head-start pillars per party member (`FSavedPartyMember`) |
| 22 | B-351b | The carried stone socket (`FSavedCarriedAttachment`) |
| 23 | B-073 | The run reward tally (`FSavedRun`) |
| 24 | B-187c | Mini-boss marks and revivals (`FSavedRun`) |
| 25 | B-377 | Hidden quest progress (`FSavedHiddenQuests`) |
| 26 | B-378 | Curses held (`FSavedCurses`) |
| 27 | B-379 | Hidden counters (`FSavedCounters`) |
| 28 | B-377b | The memory journal (`FSavedJournal`) |
| 29 | B-380c | Apparel in loadouts (`FSavedLoadout`, `FSavedCustomLoadout::Apparel`) |

**Branches that don't bump the version:** B-348c, B-351c/d, B-187, B-187b, B-363/364, B-187d/e, B-381, B-380b. They carry their base's number.

**The `LATEST` comment.** A text merge copies the whole 2–20 history once per branch. After each save merge, write the comment once: the 2–20 history, then 21 up to the version just merged, using the "What it saves" column. The trial branch's final line, in commit `c17141b7`, is the target. Remove the "(renumber at merge)" / "(another branch; renumber if it lands later)" notes as you go.

`MIN_SUPPORTED = LATEST` stays as it is (D-060: older saves are wiped with a warning until the first public build). So after these merges a save from **any** earlier build is wiped once.

**If the order changes:** the numbers must still rise in merge order. A branch merged earlier than shown takes the next free number, and every later branch moves up by one. Bump the number and its comment entry only. The save code reads fields by struct, not by version, apart from the `MIN_SUPPORTED` wipe.

## B-368 / B-370: one source for the read-time constants

Once both B-368 (row 8) and B-370 (row 10) are in, three constants exist twice:

| B-368 (`NotifyValidation`) | B-370 (`FCombatCameraRules`) | Value |
|---|---|---|
| `NotifyValidation::MIN_DEFENSE_READ_SECONDS` (`.h`) | `FCombatCameraRules::MinDefenseReadSeconds` | 0.75 s |
| `MIN_PLAY_RATE` (`NotifyValidation.cpp`, local) | `CombatCameraWindow::MinPlayRate` (`CombatCameraTypes.cpp`, local) | 0.1 |
| `RETURN_LEG_RATE` (`NotifyValidation.cpp`, local) | `CombatCameraWindow::ReturnLegRate` (`CombatCameraTypes.cpp`, local) | 1.0 |

Make B-370's `FCombatCameraRules` the one source. Do it in two commits (trial commits `c17141b7` and `f0076b06`):

1. **`CombatCameraTypes.h` / `.cpp`:** add `static constexpr float MinPlayRate = 0.1f;` and `ReturnLegRate = 1.0f;` to `FCombatCameraRules`. Make the `.cpp`'s `CombatCameraWindow` names read them.
2. **`NotifyValidation.h` / `.cpp`:** include `Combat/Camera/CombatCameraTypes.h`. Set `MIN_DEFENSE_READ_SECONDS = FCombatCameraRules::MinDefenseReadSeconds`, and make the two local rates read `FCombatCameraRules::MinPlayRate` / `ReturnLegRate`.

The values don't change, so `NotifyValidationSpec` and the camera specs expect the same numbers.

## B-232d + B-235c: a client's chest hint

**The problem:**

- B-232d's chest hint (`ATrialChest::GetHintText`) reads the host-copied found list by the chest's plain id.
- B-235c gives every other player their own chest copy, recorded on the host as `"<id>@<player key>"`.
- Merged as-is, a client sees "Opened chest" as soon as the host opens their copy, while the client's own copy is still closed.

**The fix (trial commit `6d2b7b57`, apply after row 44):** on a client, the hint looks up `"<id>@<WoRDefeat::PlayerDropKey(local player state)>"`. That is the same key `GetFeatureIdFor` records on the host: the unique net id, else the player id, and both replicate. It also needs `#include "GameFramework/PlayerState.h"`.

**PIE check (two players):** the host opens a chest. On the client, the same chest still says "Press E to open the chest". After the client opens it, it says "Opened chest".

## After the last merge

- `Backlog.md` has one row per job (the REVIEW row).
- Rules 01 has one CL-28b (B-351b) and one CL-28c (B-357b/c). Rules 02 has one ED-37 / 37b / 37c. Rules 05 has one ER-12h and one ER-12i.
- `DesignDecisions.md` already has two headings each for Q-155, Q-321 and Q-042i in `dev`. They are not from these merges; leave them.
- The first full build of the combined code is where any compile errors from the merges show up (see the reviewer note in the trial branch's hand-over).
