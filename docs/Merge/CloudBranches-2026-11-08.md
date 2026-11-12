# Cloud branches: merge plan (2026-11-08)

For the desktop builder. It covers every cloud branch marked `UNBUILT` in `docs/Backlog.md`, as of `origin/dev` at `dba4a0f5`.

## This is a trial merge

`integration/cloud-2026-11-08` is a **trial**. It was cut from `origin/dev` and merges every branch below in this order (`--no-ff`), with each conflict resolved. It shows that the whole set goes together and what each conflict looks like.

- **Never merge `integration/cloud-2026-11-08` into `dev`.**
- The real merges still go **branch by branch**: build, test, `wor-reviewer` and Crown's PIE pass for each job.
- Use the trial branch to look up a resolution: `git show <merge commit> -- <file>`. Each merge commit is titled `Integration (trial): merge <branch>`.
- Nothing was built or tested in the cloud. The first desktop build of the trial branch is the first compile of the combined code.

## Update, 21:50 UTC: B-187d and B-187e have new commits after reaching dev

Crown changed Q-187d (1) on 2026-11-08: **only the presser's characters rest**. In solo that is still the whole party.
- The new commits are on `feature/B-187d-co-op-rest-cloud`, merged forward into `feature/B-187e-client-rest-menu-cloud`.
- `dev` already holds the older B-187d and B-187e, so **merge both branches into dev again**.
- One conflict to expect, in `WoRRestSpot::Rest`: keep the `Presser` signature. The host's at-rest flag is set when `bPresserIsHost` is true; that is what `bLocalPlayerRests` meant.
- The trial branch has the four commits cherry-picked.

Crown also changed Q-187e (1) on 2026-11-08: **reviving the mini-boss is a vote**.
- Every player outside battle votes, and it passes only if they all say yes. In solo it just revives.
- The new commits are on `feature/B-187e-client-rest-menu-cloud`, after the Q-187d merge.
- New files: `Trial/ReviveVote.h/.cpp` and `Tests/ReviveVoteSpec.cpp`.
- Changed files: `AWoRGameState` (replicated `ReviveVoteState`), `AWoRPlayerState::ServerCastReviveVote`, and the rest menu's vote buttons.
- Merge the branch into dev again after B-187d.
- The trial branch has these commits cherry-picked too.
- After review the branch got four more commits:
  - the vote acts once;
  - a passed vote waits for any running fight to end (Q-187e2);
  - `ReviveVote::Cast` is renamed `Answer`;
  - a refused vote says why.
  All are cherry-picked onto the trial branch.

## Update, 23:00 UTC: Q-377 confirmed, Q-378 changed, and a new row B-388

These branches are all in `dev` already, so **merge them into dev again**, in this order:
1. **B-377** (Q-377 confirmed by Crown): one commit. The two notices now come from `HiddenQuestNotice` at the top of `HiddenQuestSubsystem.cpp`.
   - Expect a conflict where `dev`'s B-377b code calls `ReportToHost()` in `StartQuest`: keep that call and use `HiddenQuestNotice::Started()`.
   - The same goes for `CountStepMet`'s StepDone notice.
2. **B-378** (Q-378 changed by Crown). What changed:
   - curses come from quest steps too (`FHiddenQuestStep::Rewards`);
   - the player can clear one (`ClearCurse`, `wor.Curse.Clear`);
   - a cleared or lifted curse comes back (`CheckRegain`, `RegainCurses`);
   - a cure found where the curse was given isn't undone by that same event (`BeginEvent`).
   - In `dev`, B-377b moved the step counting into `CountStepMet`. Put `GiveStepRewards(Quest, Step, …UCurseSubsystem)` there, right after `Entry->RunCount = 0`. The B-377b branch shows the resolution.
3. **B-378b**: the placeholder hub service `UCurseClearWidget` (`wor.UI.ClearCurse`) and a spec case.
   - In `CurseSubsystem.h/.cpp`, keep both B-378b's penalty cache and the new code. The pattern is `bPenaltyDirty = true;` followed by `LiftedThisEvent.Add`.
4. **B-377b** and **B-381**: each only has merge commits for the above (B-378b merged forward; the resolutions are on the branches). B-377b also drops a doubled ER-12h row.
5. **B-388** (new, UNBUILT, cut from B-377): main and side quests, part 1.
   - What it adds: `EQuestKind` with `Title` and a step `Goal`; `GetVisibleQuests`; `StartQuestById` (`wor.Quest.Give`); the journal's Quests page; `WoRGameNames::GetQuestKindName`; QuestJournalSpec.
   - Merge it after B-377b. The same `CountStepMet` note applies: its visible StepDone notice goes there.
   - No save change.
- The trial branch has all of these cherry-picked. Its `HiddenQuestSubsystem.h/.cpp` show the combined result.

## Update, 23:45 UTC: Crown's confirmations, B-381 activations, and new row B-389 (D-168)

- **Docs only, merge into dev again:**
  - Q-379 confirmed (`feature/B-379-hidden-counters-cloud`, one commit);
  - Q-380 (4) confirmed (`feature/B-380-apparel-cloud`, one commit).
- **B-381 activations** (Crown, MemoryCatalogue §3.11). Merge `feature/B-381-gear-quests-cloud` into dev again. What changed:
  - the `WornItemState` condition (subject 30, with fields `SocketElement`, `StoneType`, `MinTier`);
  - `FWoRConditionContext::BoundItem`;
  - `FWornItemState` / `FMemoryEventMember::WornStates` (`MemoryConditions::GetWornStates`: weapons, adapter, apparel);
  - `StartGearQuests` checks each item with its own state;
  - a spec case;
  - Q-381 confirmed.
  - The branch merged B-380b in (for the apparel socket). Since B-380b came from dev, the quest subsystem took B-377b's resolved version, so B-381 now equals B-377b plus apparel plus the activations, and the dev merge should be close to clean.
- **New row 69: B-389** (`feature/B-389-resistance-by-type-cloud`, UNBUILT, cut from dev after B-387). D-168, resistance by damage type:
  - `FResistanceBonus::PhysicalDamage` / `SpellDamage` (appended, damage-only, summed by `Accumulate`);
  - `WoRResistance::GetScopeLineResistance`;
  - `FDamageMathInputs::ResistanceScopeLine`, added to the element line in step 3 (kept on GAS);
  - the debug prints;
  - ResistanceLinesSpec;
  - Q-389, and ED-20 / ED-27.
  - **No save change.** The new fields are tagged UPROPERTYs, so old data reads 0. The existing damage and resistance specs must pass untouched: that is the no-balance-change proof.
- The trial branch has all of these cherry-picked.

## Update, 2026-11-09: the GAS plan (B-042f, B-042g, B-043a) and the PIE-gated cleanups

- **B-042f** is merged into dev (desktop, built, full suite green). Its PIE (`docs/PIE/B-042f.md`, `wor.Effects.Compare` 0 differ at UseGAS 0 and 1) is the gate for row 70.
- **Row 70: B-042g part 1** (`feature/B-042g-gas-readers-default-cloud`, UNBUILT, on B-042f): **merge only after B-042f's comparison passes.** The B-042g row is now part 1 of 6 (new rows B-042g2..g6, not written).
- **Row 71: B-043a** (`feature/B-043a-costs-gas-cloud`, UNBUILT, on B-042f, so it merges straight onto dev): skill costs as Instant GEs behind `wor.Costs.UseGAS` (default 0). B-043 is sliced into B-043a–h; B-214 now waits on B-043b, B-236 on B-214.
- **Rows 72 / 73: B-273b and B-064a (Ice)**, written, reviewed and fixed; B-064a is stacked on B-273b.
- **Cleanups** are in their own section at the end ("PIE-gated cleanups"): each merges only after Crown's PIE of the job it cleans up.
- The trial branch has rows 70–73 and the cleanups C1–C7 cherry-picked.

## Update, 2026-11-09 (later): round 2 and a fresh trial branch

- **New trial branch `integration/cloud-2026-11-09`**: current dev plus no-ff merges of rows 74–77 and cleanups C1–C7, in that order. No code conflicts; doc conflicts only. A cross-merge review found no code breakage (PASS WITH NOTES). The old `integration/cloud-2026-11-08` is superseded.
- **When resolving Backlog conflicts, take the branch's row for its own job** (and for rows it deliberately edits). A plain "keep dev's row" union left B-153 and B-172 BLOCKED and dropped B-042g2's note on B-043a; the trial branch has the fix (cbe9a118).
- After B-042g2 merges, PIE checks that set `wor.Effects.UseGAS` are only for before it (B-043b check 10, B-064c check 10 now say so; older sheets B-042c/h/k/o/x/y/g/i too).
- Row 78, B-043c, is written and reviewed (stacked on row 76). The trial branch has it.

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
| 60 | `docs/damage-defence-sources-cloud` | `docs/Q-164b-amp-cloud` (contains it; merge this one) | – | docs (DesignDecisions, Rules 02 ED-50). The trial branch merged only `docs/Q-164b-amp-cloud`; this branch adds the comparison doc on top. |
| 61 | `feature/B-382-amp-lines-cloud` | row 60 | **30** | **save** (29 → 30; add the `30: B-382's Amp lines…` note to the one comment). **DesignDecisions Q-164b:** keep only the answered version; a union also keeps the old OPEN heading and options A–D (drop them). Backlog: keep the REVIEW rows (B-367b, B-374 come back as TODO from this branch's older base). Docs (Backlog, DesignDecisions Q-164b, Rules 02 ED-50 / ED-50b, DamageCalculator changelog). Code: `FEquipmentStatBonus` (B-380's apparel lines sit beside it; no overlap), `LoadoutComponent::GetActiveStatBonus` `AddScaled` (one added line), `ActionExecutor` `PreviewActionDamage` / `FActionDamagePreview`, `DamageCalculator::GetStatusEffectDamageModifier`. |
| 62 | `feature/B-383-damage-scopes-cloud` | B-382 | **31** | **save** (30 → 31). | Backlog: the same doubled rows (keep REVIEW; B-382's row with "save v30"). Code in effect / GAS / damage-calculator files only (`EDamageScope`, `FSkillEffectPayload`, `FActiveSkillEffect`, `WoREffectContext`, `WoREffectSpecBuilder`, `SkillEffectManager`, `DamageCalculator`). Docs: Backlog (B-383, B-384), D-166, Rules 02, PIE. |
| 63 | `feature/B-384-one-damage-channel-cloud` | B-383 | 31 | Code: `DamageCalculator.cpp/.h`, `DamageScopeDebug`, a new spec. Docs: Backlog (the B-384 row goes from BLOCKED to REVIEW; keep the REVIEW row, and B-367b's REVIEW row again), D-167, Rules ED-50c, PIE. **Balance shift:** stacked damage boosts now add (PIE B-384 has the before / after numbers). |
| 64 | `feature/B-385-scope-gas-followups-cloud` | B-384 | 31 | Code in DamageCalculator / DamageMath / GAS execution / mirror only. Docs: Backlog, PIE. |
| 65 | `feature/B-380d-apparel-lore-socket-cloud` | B-380c (row 59) | 29 | Code: PoolSubsystem, InventoryComponent, LoadoutRulesDebug, ApparelSpec. Docs: Q-380d / Q-380c (a union can copy them twice: keep one each), Backlog (B-380d REVIEW, new B-380e TODO), Rules CL-50b. |
| 66 | `feature/B-386-apparel-amp-cloud` | B-380d **and** B-385 (it merges B-385 in) | 31 | Small code (ApparelData, FApparelEntry). Docs: Q-386, CL-50c; Backlog doubled rows again (keep REVIEW). |
| 67 | `feature/B-387-crystal-amp-cloud` | B-386 | 31 | Code: FAmpBonus.h (AmpRules), LoadoutComponent `GetActiveStatBonus`, AmpSpec. Docs: Q-387, ED-50d. **Balance note:** worn crystals from D tier up add flat Amp to their element. |
| 68 | `feature/B-388-main-side-quests-cloud` | B-377 (merge after B-377b, see the 23:00 update) | – | Code: HiddenQuestData (EQuestKind, Title, step Goal), HiddenQuestSubsystem (GetVisibleQuests, StartQuestById, visible notices; put them in `dev`'s `CountStepMet`), MemoryJournalWidget (Quests page), WoRGameNames (GetQuestKindName), QuestJournalSpec. Docs: Backlog B-388, Q-388, ER-12l, PIE B-388 and its Pending row, HiddenQuestSystem changelog, session entry. | Backlog / Rules / PIE README: add the rows next to dev's (B-377b, ER-12k, the B-38x Pending rows). |
| 69 | `feature/B-389-resistance-by-type-cloud` | dev (after B-387) | – | Code: FResistanceBonus (two appended lines), ResistanceQuery (`GetScopeLineResistance`), DamageMath / DamageCalculator (step 3), the Resistance / Stat debug prints, ResistanceLinesSpec. Docs: Backlog B-389, Q-389 (and D-168 already in dev), ED-20 / ED-27, PIE B-389 and its Pending row, ResistanceSystem / DamageCalculator changelogs, session entry. | Every existing damage / resistance spec must pass unchanged (no balance change). **Q-389 changed by Crown (2026-11-09): an intended balance change** — the class's Physical column counts for damage (Generic −15% physical damage taken, Resonator +15%); every existing spec should still pass (their defenders are Casters). |
| 70 | `feature/B-042g-gas-readers-default-cloud` | B-042f (in dev) | – | **Merge only after B-042f's PIE comparison passes.** Code: EffectIdentity (PhysicalType, EffectValue), WoREffectSpecBuilder (`GetEffectsOfType`), mirror `Matches` and fingerprint, SkillEffectManager `GetEffectsByType` at UseGAS 1, GASEffectsByTypeSpec. Docs: Backlog (B-042g part 1 + new rows g2..g6), Q-042g, PIE. |
| 71 | `feature/B-043a-costs-gas-cloud` | B-042f (in dev) | – | Code: tags (`Effect.Family.Cost/Backlash`, `SetByCaller.Cost.*`), WoRGameplayEffects (`UWoRGE_EnergyCost`, `UWoRGE_HPBacklash`), new `GAS/WoRActionCosts`, ActionExecutor `SpendEnergy`, ActionExecutorCosts backlash, `WoRCostDebug`, GASCostsSpec. Docs: Backlog (B-043 sliced a–h, B-214 re-pointed), Q-043a, PIE. If it merges before row 70, the Backlog / DesignDecisions / GASSystem conflicts are row unions. |
| 72 | `feature/B-273b-hit-kind-cloud` | dev | – | The pair becomes the hit kind: `DealtTypeOverride` + `GetHitKind()` on the hit structs, `AddStatusBuildupForHit`, the bar remembers the dealt type, `BarCapTriggerResolver` by hit kind (an Ice row with no break falls back to Water's), GAS damage / buildup executions read `Resist_<ElementType>`. No number changes for existing callers. Commits 0798ec4f + 7ec15d25 only build together. Reviewed; fixed. |
| 73 | `feature/B-064a-ice-cloud` | row 72 | – | Ice (D-029, D-090, Q-064a): `ESkillEffectType::IceBreak` (appended), its BreakTable row (Water's slow + 10% max HP), `USpellData::bDealsIce` (rule 75: Water or Generic), the class table's Ice column, Water Amp skips Ice, spec `WoR.Elements.Ice`. New row B-064c (the rest); Q-064a2 OPEN (lore: the break's name). Reviewed; fixed. |
| 74 | `feature/B-173-turn-start-energy-cloud` | dev (after the D-089 docs merge) | – | Energy refill at each turn start (D-089 changed; Q-173: every turn kind): `Combat/Mechanics/TurnStartEnergy`, CombatConstants, the orchestrator's turn-start step, `TurnStartEnergyDebug`, spec `WoR.Energy.TurnStart`. Docs: Backlog, Rules ET-17 / ET-29, Q-173, PIE. Reviewed; fixed. |
| 75 | `feature/B-064c-ice-rest-cloud` | dev | – | Ice, the rest (Q-064c): Ice column in `ResistanceColumns` and its structs (old data reads 0, no save bump), `bKeyedIce` on resistance buffs / Wards (Ice-keyed covers Ice and Water), Water boosts and StatusMultiplier skip Ice, AI estimate by hit kind. Net format of `FWoREffectIdentity` changes (both ends same build). Reviewed; fixed (debug readouts). |
| 76 | `feature/B-043b-ability-base-cloud` | dev | – | GAS stage 4 part 2 (Q-043b): `UWoRGameplayAbility`, granting at fight start, `wor.Abilities.UseGAS` (default 0) routes actions through abilities; costs still paid once by the executor. API from UE 5.5+ memory: watch the first compile (`GetDynamicSpecSourceTags`, InstancingPolicy in the constructor). Reviewed; fixed. B-214 can start after it. |
| 77 | `feature/B-042g2-usegas-switch-delete-cloud` | dev | – | **Merge only after Crown's full PIE fight at `wor.Effects.UseGAS 1` on dev (its sheet, section A).** Deletes the UseGAS switch (GAS reads always), the parity specs and the enum-side spec halves (D-140). Review BLOCKED on five specs; fixed (trackers on spec holders; HP-based asserts). Run the full suite: its sheet lists the specs to watch. Conflicts likely with row 75 (ResistanceQuery / SkillEffectManager): resolve keeping both. |
| 78 | `feature/B-043c-melee-ability-tasks-cloud` | row 76 (B-043b) | – | GAS stage 4 part 3 (Q-043c): a skill's impacts reach its ability as `Event.Combat.Impact` (`UActionExecutor::RouteImpactFrame`, `WaitGameplayEvent`), else resolved as before; the montage stays in the executor (moved to B-043f). Behind `wor.Abilities.UseGAS`. Reviewed; fixed. |
| 79 | `feature/B-043d-projectile-abilities-cloud` | dev (B-043c in) | – | GAS stage 4 part 4 (Q-043d): projectile / burst arrivals and dodges through the spell's ability (`Event.Combat.Projectile.Arrived/.Dodged`), exactly once, executor fallback; behind `wor.Abilities.UseGAS`. Reviewed; fixed. |
| 80 | `feature/B-170a-action-pips-cloud` | dev | – | Action pips 1 (Q-170a): actions on the character, no play change. **Q-170a differs from ActionPips.md §3.1** (an additive layer set, not a stat entry): Crown may veto. Reviewed; fixed. |
| 81 | `feature/B-397-hidden-quests-shown-cloud` | dev | – | D-171: hidden quests listed once found, current goal only, private discovery notice; Q-377's placeholder notices superseded. Reviewed; fixed. |
| 82 | `feature/B-170b-turn-rotation-cloud` | row 80 (B-170a) | – | Action pips 2 (Q-170b2): fixed round-robin rotation behind `wor.Turns.ActionPips` (default 0, read per fight); speed hooks keep broadcasting; first-striking side leads every round (Crown may change). Reviewed; fixed. |
| 83 | `feature/B-043e-spell-shapes-abilities-cloud` | row 79 (B-043d) | – | GAS stage 4 part 5 (Q-043e): AOE / Instant hits and support casts through the spell's ability (`Event.Combat.SpellResolve.*`), bodies moved unchanged, executor fallback; behind `wor.Abilities.UseGAS`. Reviewed (spec gaps noted in its PIE sheet). |
| 84 | `feature/B-170c-multi-action-turns-cloud` | row 82 (B-170b) | – | Action pips 3 (Q-170c2): several actions per turn behind `wor.Turns.ActionPips`, `ActionCost` (≥1) on skills and items, `EActionType::EndTurn` (appended), ritual arming continues the turn, switching free. Reviewed; fixed (ClampMin 1 per the plan). |
| 85 | `feature/B-394-cast-vfx-source-cloud` | dev | 31 | D-170 cast VFX source rule (Q-394): `CastVisuals` resolver; play unchanged until art is set; no adapter + not a Refractor keeps the skill's visual. Reviewed; fixed. |
| 86 | `feature/B-392-two-guards-cloud` | dev | **32** | D-170 two guards (Q-392): `EGuardStyle`, the guard picks the parry montage; **save v32** (older saves wiped). Behaviour: any authored weapon `ParryMontage` now plays. Reviewed; fixed. |
| 87 | `feature/B-393-parry-vfx-cloud` | row 86 | **33** | D-170 parry landing VFX (Q-393): weapon guard sparks, parry-effect cosmetic tinted by its element source, priority setting; **save v33**; unarmed Guard no longer sparks. Reviewed; fixed. |
| 88 | `feature/B-391-battle-hud-cloud` | dev | – | Placeholder battle HUD (Q-391): new C++ HUD + command menu behind `wor.UI.NewHUD` (default 1, old HUD at 0), menu keys + InputMap rows; B-391b holds damage numbers and the old HUD's missing pieces. Reviewed; fixed. |
| 89 | `feature/B-043g-ritual-ability-cloud` | dev (B-043f in) | – | GAS stage 4 part 7 (Q-043g): a ritual's fire runs through the same skill ability, cost-free (arm pays once, B-043f); behind `wor.Abilities.UseGAS`. Reviewed (PASS WITH NOTES). |
| 90 | `feature/B-402-side-on-defence-view-cloud` | dev | – | Side-on defence view (Q-402): low side-on shot framing defenders and attacker; no shake / punch while defending (dropped, not replayed). Reviewed; fixed. |
| 91 | `feature/B-405-boss-framing-kill-shots-cloud` | dev | – | Boss framing and kill shots (Q-405): low turn view against Boss / MiniBoss; a kill-shot hold before FinishCombat (nothing can start during it); the camera viewer is the local player. Review BLOCKed (actions during the hold); fixed. |
| 92 | `feature/B-398-turn-view-setting-cloud` | dev | **34** | Turn-view setting (Q-398): Centred Wide default, Over-the-Shoulder option, saved per player (`FSavedSettings`) → **save v34**. **Save merge rule:** whatever order rows 86, 87 and 92 merge in, keep `LATEST = 34` and order the comment 31, 32 (B-392), 33 (B-393), 34 (B-398); drop the "32 and 33 are taken" prefix. Reviewed; fixed. |
| 93 | `feature/B-403-shot-pattern-tuning-cloud` | dev | – | Shot-pattern tuning (Q-403): melee rush with motion blur, side-on impact framing that keeps the caster, still spread; `wor.Camera.ShotPreview`. FOV punch cut (8/14 → 5/10) listed for Crown. Reviewed; fixed. |
| 94 | `feature/B-407-short-signature-cameras-cloud` | dev | – | Short signature cameras (Q-407): `SignatureCameraCap` cuts a player/team signature sequence back at the cap; `wor.Presentation.Validate` warns on long ones. Reviewed; fixed. |
| 95 | `feature/B-399-calm-menus-cloud` | row 92 | – | Calm menus (Q-399): a sub-menu steps the turn camera one step in, cancel steps back, nothing moves while choosing; `OnMenuDepthChanged`, `wor.Camera.MenuDepth`. **Stacked on row 92; merge after it.** Reviewed (PASS WITH NOTES); fixed. |
| 96 | `feature/B-400-target-cut-cloud` | row 95 | – | Target cut with several enemies (Q-400): the menu's highlighted target (hover / focus on a target button), Over-the-Shoulder cuts to a front medium shot of it, the other styles re-aim; `wor.Camera.TargetCycle`. **Stacked on row 95.** New row B-400b: B-391's new HUD doesn't highlight yet. Reviewed (PASS WITH NOTES); fixed. |
| 97 | `feature/B-406-boss-cinematics-cloud` | dev | – | Per-boss cinematics on trial (Q-406): `UBossData::OpeningSequence` and `FBossPhase::CinematicSequence` (soft, empty = none); turn 1 waits for the opening, a phase cinematic holds the next action request; HUD hidden (`OnCinematicHudHidden`); named-attack front shot; 20 s cap; `wor.Camera.BossCinematic`. New row B-406b. Reviewed (PASS WITH NOTES); fixed. |
| 98 | `feature/B-404-wide-beats-cloud` | row 92 | – | Wide beats and party handover (Q-404): between turns (someone acted, the turn passes to another hero) the camera goes wide, holds, then blends 0.5 s behind the next hero; `OnTurnHandover(From, To)` once per turn start (a fight ending mid-beat drops it); `wor.Camera.Handover`. **Stacked on row 92.** Reviewed (PASS WITH NOTES); fixed (beat-path specs). |
| 99 | `feature/B-170d-action-effects-cloud` | dev | – | Action pips 4 (Q-170d2): `GainActions` / `LoseActions` / `MaxActionsBuff` / `MaxActionsDebuff` (appended to `ESkillEffectType`; shown as Max Actions Up / Down), waiting changes per fight, Ice / Water / Wind break rows with the switch on, Extra Action shown as Extra Turn, Emerald's F..S ladder; validator warnings. Review BLOCKed (authored Value lost on the cast path); fixed and re-reviewed (PASS WITH NOTES, notes fixed). No code conflicts on the trial branch. |
| 100 | `feature/B-401-no-reset-between-actions-cloud` | row 98 (+ dev merged in) | – | No reset between actions (Q-401): when the same hero is asked again in one turn, the camera blends back (0.35 s) to the turn view it started on, after a moving shot ends; no wide beat; counted in `wor.Camera.Print`. **Stacked on row 98.** Reviewed (PASS WITH NOTES); fixed. |
| 101 | `feature/B-170e-action-pips-ui-cloud` | row 88 (+ dev merged in) | – | Action pips placeholder UI (Q-170e2): party-card pips + Max Actions with progress, costs on every skill / spell / item and greyed with the reason, End Turn button and key (Backspace / View; `EPieMenuCategory::EndTurn` appended), `ActionPipsText`. **Behaviour change to built B-170c:** End Turn is now refused outside an action-pips fight (it was a pass); Q-170c2 updated. **Stacked on row 88.** Reviewed (PASS WITH NOTES); fixed. |
| 102 | `feature/B-400b-new-hud-target-highlight-cloud` | row 96 + row 88 merged in | – | The new battle HUD's target rows highlight their enemy on hover, so B-400's target view works in the default HUD. **Merge after rows 88 and 96.** Reviewed (PASS WITH NOTES); fixed. |
| 103 | `feature/B-170f-ai-actions-cloud` | row 99 | – | Action pips 6 (Q-170f2): the AI spends or banks (`AI/AIActionPips.h`), skips picks it can't pay, holds only for a pick it can pay in Energy, scores the action effects and Emerald by Crown's ladder; Easy stops early 30% (deckbuilder parity); `CanContinueTurn` re-asks the AI. **Dropped for Crown to confirm:** the Emerald look-ahead in action-pips fights (Q-170f2). **Stacked on row 99.** Reviewed (PASS WITH NOTES); fixed. No code conflicts on the trial branch. |
| 104 | `feature/B-391b-hud-pieces-cloud` | row 88 (+ dev merged in) | – | Battle HUD part 2 (Q-391b): floating damage numbers (element colour, size / brightness by multiplier, grey immune, green heals) from a new replicated `FCombatHitShown` (`SendHit` / multicast signature changed; `OnHitShown` kept for Blueprints; `SendHeal` from `BroadcastHealing`); Broken Darkness card shows "Absorb x / y, overload z%". New row B-391c (status buildup bar, weapon name, durability, absorb cue). **Stacked on row 88.** Reviewed (PASS WITH NOTES); fixed. |
| 105 | `feature/B-406b-boss-cinematic-skip-cloud` | row 97 | – | Boss cinematics, the rest (Q-406b): skip key `IA_Combat_SkipCinematic` (Tab / Menu; moved off Space in review: PIE must confirm the editor's Guard / Dodge / pause aren't on it), local skip in co-op; a ritual fire waits for a phase cinematic. New row B-406c (hero shot per boss turn after the trial; CommonUI binding). **Stacked on row 97.** Reviewed (PASS WITH NOTES); fixed. |
| 106 | `feature/B-170g-turn-speed-retire-cloud` | row 103 | – | Action pips 7 (Q-170g2): Turn Speed retired for new authoring only (validator **warnings**, not errors, until B-170h; "(retired)" labels; `// OBSOLETE (B-170h)` flags; `wor.Data.ListTurnSpeed` incl. reaction triggers); rules pages ET-03..19 and SB-22/23/29 carry the action-pips rule first. Nothing removed. **Stacked on row 103.** Reviewed (PASS WITH NOTES); fixed. |
| 107 | `feature/B-391c-hud-pieces-2-cloud` | row 104 | – | Battle HUD part 3 (Q-391c): status buildup bar with cracks on every card, gear line and tooltip on party cards, AD / WD durability header on the command menu, "Absorbed" cue (`SHOW_ABSORB_CUE`, Crown may veto). Summons: no join event exists, listed as a limitation. **Stacked on row 104.** Reviewed (PASS WITH NOTES); fixed. |
| 108 | `feature/B-234d-host-hand-over-3-cloud` | dev | – | Host hand-over part 3 (Q-234d): after a handed-over run the new host's own party and pool come back from its own slot (written at once at run end, so a crash before the hub keeps the run's gains; restored at the hub), the run's upgrade-memory raises merged in; the join order crosses a seamless travel on the PlayerState (`CopyProperties`), and a destroyed old PlayerState no longer drops its player's id. **Touches saves: run PIE 5 / 5b with three instances.** Reviewed and re-reviewed (PASS WITH NOTES); fixed. |
| 109 | `feature/B-032d-per-impact-defense-cloud` | dev | – | Per-impact defense for every spell shape (Q-032d): multi-entry / multi-hit spells get one defense window per arrival behind `wor.Combat.PerImpactDefense` (default 1; 0 = the old lumped hit); spell windows stay open past the cast animation for projectiles in flight; abilities missing Impact notifies land fallback impacts (not abilities with projectile entries); early closes land each remaining impact with its own entry's flags (fixes the old close's short damage). **For Crown:** damage totals hold, but per-hit effects (Lifesteal, Burn, Double Hit, crits, ED-22 floor, reflect) now count arrivals, not HitCount. B-032b now waits on its PIE. Reviewed (PASS WITH NOTES); fixed. No code conflicts on the trial branch. |

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
| 30 | B-382 | Amp lines on gear (`FEquipmentStatBonus::Amp`, inside `FSavedGear::StatBonus`) |
| 31 | B-383 | The Spell Amp line (`FAmpBonus::Spell`, same place) |

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

## Fixes the merges need (from wor-reviewer on the trial branch)

These problems only exist once two branches meet. Apply each one at the row named:

| After row | Fix | Trial commit |
|---|---|---|
| 38 (B-363) | **Compile error.** The `TrialRestSpot.cpp` hunk drops `ReviveMiniBoss`'s closing `}`, so `StepAway` starts inside it. Put the brace back. | `9a0d94d3` |
| 53 (B-378b) | `DefaultGame.ini`: the merge brings back the old `EvolutionItemData` scan (`CookRule=Unknown`) beside B-278's AlwaysCook one. Delete the old line. Check that every asset type appears once on the `+` side. | `9a0d94d3` |
| 57 (B-380) | **Compile error with no textual conflict.** B-357c removed `const FCombatLoadout &Loadout = GetActiveLoadout();` from `GetActiveResistanceBonus`, `GetActiveEffects`, `GetActiveEffectsGathered` and `GetActiveConditionalEffectsGathered`. B-380's apparel loops in those functions read `Loadout`. Put the line back before each apparel loop. | `9a0d94d3` |
| 46 (B-187e) | **A client's rest left the host "at a rest spot".** The host could then buy pillar points and switch skin anywhere. `WoRRestSpot::Rest(World, Id, bLocalPlayerRests = true)` now takes a flag, and `ATrialRestSpot::Interact` passes false for a client's press. **Level up** is hidden in a client's menu: it read the client's own empty run. TODO: a server RPC. | `5e77a3d2` |

**Follow-ups:** the reviewer marked these nice-to-have. They are not done on the trial branch; they could become rows.

- **Hand-over save copy (B-234b with B-377 / B-377b / B-378 / B-379).** `CaptureInto` writes this player's journal, quests, curses and counters into the copy's slot only. After a hand-over they don't reach the player's own save. Copy them back as the wallet and forms already are.
- **Stale PIE command.** `docs/PIE/B-370.md` and `docs/PIE/B-371.md` still say `wor.Camera`; after B-349 it is `wor.Camera.Print`. Fix them when merging B-370 and B-371.
- **B-371 on clients.** B-371's camera listens to `OnProjectileLaunched`, which fires only on the server (B-231). Clients never see the Cast shot. Broadcasting it from B-231b's client receive path would fix this.
- **Stale comment.** `RestSpotWidget.h`: the `LeaveButton` comment still says "hidden on a client until B-187e". It is relabelled now.

## After the last merge

- `Backlog.md` has one row per job (the REVIEW row).
- Rules 01 has one CL-28b (B-351b) and one CL-28c (B-357b/c). Rules 02 has one ED-37 / 37b / 37c. Rules 05 has one ER-12h and one ER-12i.
- `DesignDecisions.md` already has two headings each for Q-155, Q-321 and Q-042i in `dev`. They are not from these merges; leave them.
- The first full build of the combined code is the real check: the reviewer read it but nothing was compiled.

## PIE-gated cleanups (merge last, each only after its gate)

Deletions of code a finished job made obsolete. CLAUDE.md: no deleting code before it is verified in PIE. Each branch is cut from dev, UNBUILT, and **merges only after Crown's PIE of its gate job**. Each PIE sheet starts with the Blueprint reference checks (Find in Blueprints / Reference Viewer) where a deleted symbol was Blueprint-visible.

| # | Branch | Gate (Crown's PIE of) | What it deletes |
|---|---|---|---|
| C1 | `feature/B-152-stone-wrappers-delete-cloud` | B-021 | The twelve stat-stone consumable wrappers and their dispatch cases; the DefenseStone / CritStone branches in Amber's and Opal's handlers. Reviewed (PASS WITH NOTES, fixed). |

| C2 | `feature/B-153-loadout-class-field-delete-cloud` | B-022 | `ULoadoutComponent::CharacterClass` (a BlueprintReadWrite UPROPERTY: Blueprint search first). Reviewed, PASS WITH NOTES. |
| C3 | `feature/B-154-ring-weapon-managers-delete-cloud` | B-024 | `URingManager`, `UWeaponManager`, `UActionExecutor::GetRingManager` / `GetWeaponManager`. Reviewed; fixed (weapon-query spec case kept, CoopStateOwnership). Recommend one `-DisableUnity` build. |
| C4 | `feature/B-172-legacy-validators-delete-cloud` | B-028, **and** B-351 check 10, B-380 check 4, B-196-spell-cap check 3 (they still run `wor.Loadout.CompareRules`) | The `*Legacy` loadout validators and `wor.Loadout.CompareRules`. Reviewed; fixed (gate widened). |
| C5 | `feature/B-309-unified-loadout-old-path-delete-cloud` | C4 merged, and B-301..B-308, B-312 | **Stacked on C4; merge after it.** Safe subset only: `DoWeaponStatsApply`, `PrimarySlotCost`. The old `FCombatLoadout` fields / entry types stay (live writers, saves); the row lists them. Reviewed; fixed. |
| C6 | `feature/B-034b-charge-manager-delete-cloud` | B-034, plus the editor Blueprint check (sheet check 1) | `UInfusionChargeManager` and its VFX handlers, hold-time constants; fight-subsystem count 10 → 9 (another branch adding a fight subsystem first changes that number). Reviewed; fixed (docs). |
| C7 | `feature/B-317-store-old-copies-delete-cloud` | B-312, B-313, B-313b, B-314, B-316 | First part (Q-313b): the crystal inventory's pouch copy, `BattleItemsTaken` and the post-fight hand-back. New row B-317b (co-op store move). Wearers / per-member copies / enemy `ItemSlots` stay (live readers, saves, editor move). Reviewed, PASS WITH NOTES. |

**Docs-only, merge any time:** `chore/B-032b-blocked-cloud` (B-032b BLOCKED on new row B-032c: the lumped spell path still serves multi-entry / multi-hit spells), `chore/B-165-blocked-cloud` (B-165 needs the D-067 spec first, best written on the desktop).

**Not written:** B-299 (waits for all of B-042g). All C-rows touch the same doc lines (Backlog, PIE README Pending, LoadoutSystem changelog, the session's "Cloud writer: PIE-gated deletions" heading): resolve by keeping both rows / bullets and one heading. The trial branch has C1–C7 cherry-picked.

## Update, 2026-11-09 (afternoon): after B-043b / B-043c landed

Rows 79–81 written and reviewed. Being written: B-170b (on row 80) and B-043e (on row 79). Rows 74–77 and C1–C7 still wait on the desktop.

The trial branch `integration/cloud-2026-11-09` now also has current dev and rows 79–84 merged (doc conflicts only; a duplicated-row slip in the B-043a..c rows is fixed). **Backlog conflicts: use a row union keyed by job id, never 'keep both blocks'.**

**Save versions after rows 86–87:** 32 (B-392) then 33 (B-393). Every other open cloud branch stays at 31 or lower.

## Update, 2026-11-09 (evening)

- **Do not merge `feature/B-043f-orchestrator-abilities-cloud`.** The desktop built B-043f itself (`feature/B-043f-ability-commits-cost`, in dev) with Q-043f's answer C (the montage stays in the executor). The cloud branch holds only docs: an OPEN Q-043f, a BLOCKED row and a re-pointed B-043g. All of these are superseded by dev. Its PIE baseline sheet idea is covered by dev's `docs/PIE/B-043f.md`.
- **New trial branch `integration/cloud-2026-11-09b`.** It is current dev (B-043b..f) plus the 18 unmerged cloud branches: rows 74–77 (without B-043b), 80–82, 84–88, 81, and C1–C7.
  - No code conflicts.
  - A cross-merge review found no code breakage.
  - It did find Backlog status cells overwritten by the trial merges. Those are restored from each branch's tip.
  - It supersedes `integration/cloud-2026-11-09`.
- **B-170e must target B-391's `UBattleCommandMenuWidget`** for its End Turn button, because `wor.UI.NewHUD` defaults to 1.
- B-043g (rituals) is being written from dev.

## Update, 2026-11-09 (night)

- **Now in dev, built by the desktop:** B-043g, B-064c, B-170a.
- **New trial branch `integration/cloud-2026-11-09c`.** It is current dev plus every still-unmerged cloud branch: rows 74, 76–77, 81–82, 84–88, 90–92, and C1–C7. It supersedes 09b.
- **Code conflicts the real merges will meet, as resolved on the trial branch:**
  - **B-402 then B-405** (`CombatCameraTypes.h`, `ECombatCameraMode`): keep B-402's Defence comment, add a comma after `Defence`, then B-405's appended `KillShot`.
  - **B-405 then B-398** (`CombatCameraDebug.cpp`, `GetCameraStateString`): the format string gets both new fields (`style %s | %s | … | %s | %s`), and the argument list already has both.
  - **Saves:** B-392 (32), B-393 (33), B-398 (34). Keep `LATEST = 34` with the comment in order.
- Backlog conflicts: take each job's row from its own branch (row union by job id). The trial branch has the restored rows.


## Update, 2026-11-09 (late night): rows 93–95 on the trial branch

`integration/cloud-2026-11-09c` now also has rows 93 (B-403), 94 (B-407) and 95 (B-399). The real merges will meet these code conflicts:
- **B-403 after B-398** (`CameraFramingChecks.h`): keep B-398's `THIRD_LINE` / `ProjectToScreen` / `GetThirdsCell`, then B-403's `GetRequiredPoints` comment (impact needs the caster too).
- **B-407 after B-405** (`CombatCameraDirector.h`): keep both blocks (B-405's kill-shot API and `BreakApartEffect`, then B-407's `GetSignatureCapString`).
- **B-399 after B-405** (`CombatCameraDirector.cpp`): the turn view is `ComputeView(..., MenuDepth)` then `FBossFramingRules::ApplyHeight` for the Turn mode. **Also** wrap B-399's two other `ComputeTurnView(..., MenuDepth)` calls (`StepTurnViewForMenu`, `GetTurnLayoutString`) in `ApplyHeight`; git won't flag those, and without the wrap a sub-menu step against a boss jumps back to eye level. Headers, `CombatCameraDebug.cpp` and `CommandMenuRules`: keep both sides.
- **B-400 after B-405** (`CombatCameraDirector.cpp`): B-400 replaced B-399's menu-step body with `MoveTurnCamera(End, Seconds)`. Take B-400's side, then send every turn-view move through B-405's height. The trial branch adds `GetTurnViewWithHeight(Frame)` (`ComputeTurnView(..., MenuDepth)` then `ApplyHeight`) and uses it in `StepTurnViewForMenu`, the picker-close move and `CutBackFromTargetView`. `CombatCameraDebug.cpp`: the format string gets one more `| %s` (both B-400's target view and B-405's boss framing are in the arguments).
- **B-400 with B-391:** the new HUD needs B-400b before the target view follows it.
- **B-406 after B-405 / B-399 / B-400** (`CombatCameraDirector.cpp`): keep B-406's `BossCinematicPreviewDefaultSeconds` inside `CombatCameraTiming` (before B-399's `MenuStepSeconds` and the closing brace), not in B-400's `CombatCameraTargetShape`. `EndCombat`: both B-405's kill-shot undo and B-406's cinematic end and phase unbinds. Headers: keep both. `CombatCameraDebug.cpp`: one more `| %s` for `GetBossCinematicString`.
- **B-406 with B-391** (`CombatHUD.cpp`, not flagged by git): `GetAllWidgets` must also return `BattleHUD` and `BattleMenu`, or the new HUD stays on screen during a boss cinematic.
- **B-404 after B-405** (`CombatCameraDirector.cpp`, not flagged by git): in `HandleWideBeatEnd`, blend to `GetTurnViewWithHeight(BuildFrame(To))` (B-405's fight height) instead of `PendingTurnChange.TurnView`. `CombatCameraDebug.cpp`: the state string gains B-404's `GetHandoverString` field (17 fields with all camera rows in).
- **Check every resolved camera file's braces** after a keep-both resolution: two slips on the trial branch (a lost `});` in `CombatCameraSpec.cpp` and a lost `}));` in `CombatCameraDebug.cpp`) came from blocks that git aligned on a shared closing line. A quick count of `{` vs `}` per file catches them.
- **B-401 after B-405** (`CombatCameraDirector.cpp`): in `StepShot` keep B-403's motion-blur reset, then B-401's pending return. `ReturnToTurnView` must land on `GetTurnViewWithHeight(BuildFrame(Actor))` instead of the plan's plain `TurnView` (not flagged by git).
- **B-170e with B-170d:** docs only. `ActionPips.md` keeps one specs line listing B-170c, d and e, and the known-limit lines from both.
- **B-400b:** its base merge of B-391 into B-400 lost a `}` after `CycleIndex` in `CombatCommandMenuSubsystem.h` (fixed on the branch). On the trial branch, `BattleHUDSpec.cpp`'s hunks interleave with B-170e's: keep B-170e's cases and add B-400b's hover case at the end of the same Describe.
- **Doc tools:** never resolve `docs/Design/InputMap.md` (or any changelog table) with a row union keyed by the first cell: its changelog rows all start with the date, so they collapse into one. Keep both sides, then remove duplicate lines.
- **B-391b with B-170e** (`BattleHUDWidget.cpp`, `BattleHUDSpec.cpp`): keep both sides in each hunk. In `AddCard`, B-391b's `bShowEP` line, then B-170e's `OnActionsChanged` binding. In `RefreshPools`, B-391b's absorb line, then B-170e's actions line. `Initialise`: both. Check the spec's Describe/It nesting after.
- **B-406b after B-391** (`CombatPlayerController.cpp/.h`, `InputMap.md`): B-406b's branch (from dev) still has the old `CombatMenuKeys::FBinding` / `GetBindings` list that B-391 replaced with `GetMenuKeys`. Keep B-391's version and add only B-406b's skip-key constants, action, binding and functions. InputMap: B-391 / B-170e / B-400 rows plus the skip row.
- **B-170g with B-170e** (docs): `docs/Rules/04-effects-and-turns.md` take B-170g's rows; `ActionPips.md` one specs line listing B-170c..g.
- **B-391c with B-170e** (`BattleHUDWidget.cpp`): card rows are B-391c's buildup and gear lines, then B-170e's actions line; the debug string keeps both. Other files: keep both include sets.

## Update, 2026-11-12: six rows built into dev

- **Now in dev (desktop, built, full suite green):** row 74 (B-173), 81 (B-397), 85 (B-394), 86 (B-392, **save v32**), 99 (B-170d), plus the desktop's own B-170i (Crown's Reflex regain rework, Q-170i). Dev is at `22158fd7`.
- **Saves:** dev is now at 32. Next in order: B-393 (33), then B-398 (34). No renumbering.
- **Trial branch `integration/cloud-2026-11-09c`** has dev merged in (`e0c32389`). Code merged with no conflicts. Docs were resolved as below, and the real merges will meet the same:
  - **Backlog / PIE README:** row union by job id. Take dev's row where it says "in dev"; otherwise keep the cloud branch's row.
  - **B-170g after B-170i** (`docs/Rules/04-effects-and-turns.md`): keep B-170g's action-pips layout, but ET-18's regain must read B-170i's rule ("1 + one per whole step of Reflex value", Q-170i), not the old "1 (2 or 3 at Reflex thresholds)". Git won't flag the row inside B-170g's table. ET-29: dev's (B-173).
  - **B-170e/f/g after B-170i** (`ActionPips.md`): keep both changelog lines, in order d, e, f, g, i.
  - **B-170f's AI spec after B-170i:** "Hero's bank is the regain" expects a regain of 1 (`AI_PIPS_REGAIN`). That holds under B-170i only while the test hero has less Reflex value than one step (15 × Body). If it fails on the build, read the regain from `GetActionRegain()` instead of the constant.
- **Stacked rows on B-170d** (B-170f, then B-170g): their base is now in dev, so they merge straight onto dev.

## Update, 2026-11-12 (later): seven more rows in dev

- **Now in dev (desktop, built):** B-393 (**save v33**), B-391, B-402, B-405, B-398 (**save v34**), B-403, B-407. Dev is at `53b98d79`. Every save bump in this plan is now in dev.
- **Trial branch** has dev merged in again (`integration/cloud-2026-11-09c`). The remaining camera and HUD rows will meet these code conflicts when they land on dev:
  - **B-399** (`CombatCameraDirector.cpp`, `GetTurnLayoutString`): dev's line is `ApplyHeight(ComputeTurnView(TurnViewStyle, Frame), …)`. Add B-399's `MenuDepth` inside it, or use `GetTurnViewWithHeight` once B-400 has added it.
  - **B-400 / B-404 / B-406** (`CombatCameraDebug.cpp`, `GetCameraStateString`): add one `| %s` field per row on top of dev's string, ending with 17 fields: B-400's target view after B-398's layout, B-404's handover after the shot string, B-406's boss cinematic last. Count the specifiers against the arguments.
  - **B-406 / B-404 / B-401** (`CombatCameraDirector.h`): their public block goes after dev's B-407 API, with no overlap.
  - **B-391b / B-391c / B-170e / B-400b** (`BattleHUDWidget.cpp`, `BattleCommandMenuWidget.cpp`): keep dev's `BattleHUDButton.h` include (the desktop's B-391 fix) beside each row's own includes.

## Update, 2026-11-12 (midday): B-399, B-400, B-404, B-406 in dev; new trial branch

- **Now in dev (desktop, built, full suite green):** B-399, B-400, B-404, B-406 (`8624aa65`). All the camera rows are in except B-401 and B-406b.
- **New trial branch `integration/cloud-2026-11-12`** replaces `integration/cloud-2026-11-09c`. Dev now has the desktop's built and review-fixed copies of the camera and HUD rows, and the old trial can't follow it cleanly. The new branch is dev at `8624aa65` plus the 18 cloud branches still open, merged in this order. Each merge is one commit with its resolution in the message:
  1. B-042g2 (row 77; merge only after its PIE gate)
  2. B-401
  3. B-170e
  4. B-400b
  5. B-170f
  6. B-391b
  7. B-406b
  8. B-170g
  9. B-391c
  10. B-234d
  11. B-032d
  12. C1 to C5 in order (B-152, B-153, B-154, B-172, B-309)
  13. C6 (B-034b)
  14. C7 (B-317)
- **Code conflicts the real merges will meet against today's dev** (the earlier rules for these rows still hold where they aren't repeated here):
  - **B-401:** `CombatCameraDirector.cpp`, `.h`, `CombatCameraTypes.h` and `CombatCameraSpec.cpp`, keep both sides. In `StepShot`, B-403's motion-blur reset comes first, then B-401's pending return. In the spec, close B-406's Describe (`});` twice) before B-401's opens. **Not flagged by git:** `ReturnToTurnView` must land on `GetTurnViewWithHeight(BuildFrame(Actor))`, not `Plan.TurnView`. B-401's branch still has the plain plan.
  - **B-400b:** `CombatCommandMenuSubsystem.h`, take dev's placement of `CameraDepthFor` and `CycleIndex` and drop the branch's copy higher up. `BattleCommandMenuWidget.cpp`: both include sets. `BattleHUDSpec.cpp`: B-170e's two cases, then B-400b's hover case. Each case needs the shared menu-widget setup lines, which git places between the hunks.
  - **B-391b** and **B-391c** with B-170e: as in the 2026-11-09 rules above. Also keep dev's `BattleHUDButton.h` include. In B-391c's debug string, close the gear block before B-170e's actions block opens.
  - **B-406b:** as in the 2026-11-09 rule. Tab and Menu clash with none of B-391's menu keys.
  - **Docs:** `docs/Rules/04-effects-and-turns.md` must not be resolved by a line union, because the old and new action-pips rows both survive. Use the trial's file. Also watch for a partial second copy of Q-391 in `DesignDecisions.md`.
- B-170f, B-170g, B-234d, B-032d and every C-row merged with docs-only conflicts. No symbol a C-row deletes is used by code that dev added since (only comments name `URingManager` / `UWeaponManager`).
- UNBUILT: braces and parens balance in every resolved file, and there are no conflict markers or duplicate Backlog / PIE / Q rows. The desktop build is the real check.

## Update, 2026-11-12 (afternoon): B-401 and B-170e in dev

- **Now in dev (desktop, built):** B-401 (with the boss-height return as this plan asked) and B-170e (with Crown's hide-End-Turn setting, Q-170e2). Dev is at `8179c9fc`.
- **Trial branch `integration/cloud-2026-11-12`** has dev merged in (`727cb03e`). Code merged with no conflicts, docs only. Watch for a second, older copy of Q-170e2 in `DesignDecisions.md`: keep dev's (Crown's answer).
- **Still open:** 16 cloud branches, in this order:
  1. B-042g2 (PIE-gated)
  2. B-400b
  3. B-170f
  4. B-391b
  5. B-406b
  6. B-170g
  7. B-391c
  8. B-234d
  9. B-032d
  10. C1 to C7

  The midday rules for each still hold.
