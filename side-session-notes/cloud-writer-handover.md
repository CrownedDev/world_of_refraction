# Cloud writer handover (2026-11-08, second part, after the brain widened the rules) — IN PROGRESS

The brain (Crown's coordinating session) widened the rules at 06:37 UTC: stack on my own UNBUILT branches where a job depends on one, pre-write the co-op jobs (headless specs + PIE sheet), write the CC part of CC+UE jobs; still skip B-340 and B-042f. Slicing L / M jobs into part 1 + a new row, as before.

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-232b | `feature/B-232b-menu-on-commander-cloud` (**on** `feature/B-228b-client-combat-hud-cloud`) | REVIEW part 1 (UNBUILT): `CommandMenuRules::OpensHere`; the host's menu opens only for members it commands; a client opens its own member's menu from `OnRep_TurnState`; TurnStateSpec case; Rules PA-37c, PIE B-232b. The rest is new row **B-232c** (run readers on `URunStateComponent::Get*`, a recruit fills a spot) | none |
| B-235 | `feature/B-235-personal-prism-drops-cloud` | REVIEW part 1 (UNBUILT): per-player Prism drops at one spot (`UTrialRunSubsystem::DropPrismsForPlayer` / `TakeDropForPlayer` / `GetTotalDroppedPrisms`), every player's wallet drops on a wipe, the pickup gives each their own and stays for the others; DefeatSpec case; Rules ER-12b, PIE B-235. Only the host's drop is saved (a client's run copy is B-234). The rest is new row **B-235b** | none |
| B-230 | `feature/B-230-replicated-status-bars-cloud` | REVIEW part 1 (UNBUILT): new replicated `UStatusBarStateComponent` on every `ACombatCharacter` (follows the buildup manager on the server); a client's character panel draws from it; StatusBarStateSpec; Rules PA-37d, StatusBuildup changelog, PIE B-230. Effects already reach clients (B-318). The rest (grid rows) is new row **B-230b** | none |
| B-233 | `feature/B-233-sessions-null-cloud` | REVIEW part 1 (UNBUILT): OnlineSubsystem / Null / Utils plugins in the .uproject, modules in Build.cs, `DefaultPlatformService=Null`; new `UWoRSessionSubsystem` (host with `?listen`, find, join, leave; `MAX_PLAYERS` 3; `wor.Session.Host/Find/Join/Print`); new doc `docs/Architecture/OnlineSessions.md`; Rules PA-37e, PIE B-233 (LAN on one PC). The rest (EOS, invites, a lobby screen) is new row **B-233b** | **Q-233**: Null now, EOS later (needs Crown's EOS product ids: a Crown action) |
| B-351c | `feature/B-351c-one-list-readers-cloud` (**on** `feature/B-351b-stone-socket-carry-cloud`) | REVIEW part 1 (UNBUILT): the readers were mostly done already (`GetAvailableAbilities` reads `FCombatLoadout`, on the one list since B-351; stone bonuses read the stone socket); `LogActiveLoadout` on the one list and both sockets, new `wor.Loadout.PrintActive`, SkillSlotListSpec case (a stone slot's pick reaches `FCombatLoadout::GetAllAbilities`). The crystal spell list (~69 `AssignedSpells` reads), the deletion, the editor move and the gear screen are new row **B-351d** | **Q-351d = A**: two parts, as Q-351 / Q-351c |
| B-357b | not started | **Left for the desktop:** "only while that item is infused at level 2" has no item state behind it (infusion is a per-action charge today), so the gear rule needs a model decision with the code, and the one resolve path touches ~220 reads: best done with a compiler | — |

**Review fixes (second part):** B-232b: the menu check needs a roster entry on both sides; a refused client pick reopens the client's menu (`Client_MenuPickRefused`); the client's menu turn is forgotten at fight end; the menu's weapon switch / move row / "Let AI play" still act locally on a client: listed on B-232c. B-235: the pickup message shows only for this machine's player, the hint shows only this machine's own amount, `TakeDropForPlayer` clears the spot with the last drop, "nothing taken" is Verbose; leftovers on B-235b. B-230: no 0 / 0 bar before the first update, new `wor.Status.Bars`. B-233: the LocalUserNum overloads (no net-id dereference), Host refuses while a session exists, one call of each kind at a time, a refused join clears its handler, hosting uses `OpenLevel(..., "listen")` (seamless ServerTravel opens no socket), `Deinitialize`, `wor.Session.Leave`, new `WoRSessionSpec`.

---

# Cloud writer handover (2026-11-08 night run) — STOPPED: no cleanly writable job left

wor-reviewer read B-348b / B-349 / B-355b too (no blockers; fixes pushed: B-355b shares the phase fight state, B-348b write-back only for a stamped pawn + level drops count the head-start, B-349 `wor.Turns.PrintBelt`). Last review (B-351b / B-357 / B-228b): fixed and pushed: B-357 two ClassLimitsSpec cases that would fail with the adapter cost 3, and its storage-size note; B-228b now owns the client HUD in UTurnStateComponent (the orchestrator is server-only) and waits for both sides; B-351b comment. Open notes for the builder: B-351b has no spec of the fight-to-fight carry itself (Snapshot → Apply), and the party debug print does not show the stone socket. Earlier: wor-reviewer read B-367b and B-374 (no blockers), and B-368 and B-370 (three must-fixes in the B-370 director: an instant hand-off under a cinematic, arm turns, movement off; plus play rates and an unused B-368 helper): all fixed and pushed. its fixes are pushed on both branches (stale comments; the school audit also lists Conjuration damage spells as "check: conjured, or Destruction"; PIE check for the new `School_Destruction` button id).

Overnight cloud writer, Crown asleep. Pre-writes only (UNBUILT); the desktop writer builds, tests, reviews and merges. All branches cut from `origin/dev` at `ef63ea66`.

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-367b | `feature/B-367b-perfect-guard-only-cloud` | REVIEW (UNBUILT): `GuardRules::StopsInterruptableHit` and `FlawlessParry::IsPerfectHit` are Perfect Guard only; GuardSpec / FlawlessParrySpec updated; Rules ED-37/37b/37c, Q-168, Q-367, CombatOrchestrator changelog, PIE B-367b | none (Crown's correction, no question) |
| B-374 | `feature/B-374-six-schools-cloud` | REVIEW (UNBUILT): six `ESpellSchool` values (Manipulation, Impairment appended; Destruction un-retired), menu enum + mapping, `BuildSchoolButtons` public static in school order, LOCTEXT names, `USpellDataDebug::ProposeSchool` / `IsSchoolMismatch`, SpellSchoolSpec rewritten; Rules CL-42, LoadoutSystem changelog, PIE B-374 (asset re-tagging is the editor step). Note: one docs commit says it carries Q-374 + changelog but only had CL-42; the next commit adds them | **Q-374 = A**: first-match damage → heal/cleanse → control (stun/silence/skip/random) → debuff/buildup → buff; Conjuration never flagged (D&D 5e Enchantment, Skyrim Illusion/Conjuration) |
| B-368 | `feature/B-368-notify-validator-cloud` | REVIEW (UNBUILT): new `Combat/Presentation/NotifyValidation.h/.cpp` (Impact 0..HitCount-1 in order, Cast/VFX ranges, `FirstDefendableSeconds`, `CinematicWindowFits`, `MIN_DEFENSE_READ_SECONDS` 0.75), `wor.Presentation.Validate` (`Debug/Combat/NotifyValidationDebug.cpp`), `NotifyValidationSpec`; Rules ED-37d, CombatCamera changelog, PIE B-368. Builder risk: the spec sets notify times with `FAnimNotifyEvent::SetTime` on an unlinked montage | none (no design question) |
| B-370 | `feature/B-370-cinematic-window-cloud` | REVIEW (UNBUILT): `UCombatCinematicWindow` notify state; `ECombatCameraMode::Defence`; `FCombatCameraRules` `MinDefenseReadSeconds` / `ReadWindowTiming` / `ResolveWindowEnd` / `SelectHandOffMode` / `ComputeDefenceView`; the director times the Defence hand-off from action start; `wor.Camera` prints the window; specs in CombatCameraSpec; Rules ED-42c, CombatCamera changelog, PIE B-370. **Merge note:** B-368 and B-370 both append a CombatCamera.md changelog row at EOF (trivial conflict) and each carry a 0.75 s read-time constant: after both merge, make `NotifyValidation::MIN_DEFENSE_READ_SECONDS` read `FCombatCameraRules::MinDefenseReadSeconds` and let the director use `NotifyValidation::FirstDefendableSeconds` | **Q-370b = A**: wind-up end = the RitualCastMontage leg end until B-373; the Defence view comes with or without a sequence (Expedition 33) |
| B-369 | `feature/B-369-impact-vfx-outcome-cloud` | REVIEW (UNBUILT): new `Combat/Presentation/ImpactVisuals.h/.cpp` (`OutcomeFor`, `Choose` fallback chain, `SpawnForImpact` at the HitPoint socket, `wor.ImpactVFX.Log`); `UCosmeticsData` Impact Visuals fields (empty until art); hooked in `UActionExecutor::ApplyOneImpact`; `ImpactVisualsSpec`; Rules ED-44 (new section), CombatOrchestrator changelog, PIE B-369. reviewed; fixes pushed (projectile-only skip, body mesh socket, VFX-notify warning) | **Q-369b = A**: the target's CosmeticsData; a spell arrival adds nothing to its own impact; a melee VFX notify playing the Impact entry should be removed (Lyra GameplayCues, Sekiro, Expedition 33) |
| B-337e | `feature/B-337e-condition-migration-cloud` | REVIEW (UNBUILT): `FSkillConditionGroup` (AllOf / AnyOf) as `FSkillEffect::ConditionGroup`, filled on load from the old `Conditions` (`MigrateConditionGroup`); `SkillConditions::ToAuthored` / `GroupOf` / group `ToSets`; the impact path and the after-action gate read it; parity spec extended; `wor.Conditions.ListOld`; Rules ET-32b, ConditionSystem changelog, PIE B-337e (resave is the editor step). The runtime `FActiveSkillEffect::Conditions` keeps the old form until sweep row 46 | **Q-337e = A**: All of / Any of sets (not one set) so OR rows keep their result; read-only and rebuilt on edit until the sweep (Gameplay Tag query shape). Review fixes pushed (drift, reaction assets, resave commandlet) |
| B-348b | `feature/B-348b-save-head-starts-cloud` | REVIEW (UNBUILT): `FPartyMember::HeadStart*` + `FSavedPartyMember` (save **v21**: renumber if another branch took it), `UPartySessionSubsystem::SetMemberHeadStart`, the component seeds from / writes back to the member; SaveSpec round-trip; Rules CL-37b, SaveSystem changelog, PIE B-348b | **Q-348b = A**: a mid-fight Broken Darkness transformation lasts the fight only, not saved (FF6 / FF9 Trance) |
| B-340 | not started | **Skipped on purpose:** it splits `DesignDecisions.md` and moves `Backlog.md` rows, which every open cloud branch edits; best done by the desktop writer after these branches merge | — |
| B-349 | `chore/B-349-console-names-cloud` | REVIEW (UNBUILT): 40 flat console names renamed to `wor.<System>.<Verb>` (table in `docs/PIE/B-349.md`), `wor.PrintCombatTuning` removed (duplicate of `wor.Tuning.Print`); 91 docs updated; ~70 small commits (≤3 files). **Merge note:** the other cloud branches still write `wor.Camera` (B-370 docs / debug line) — rename to `wor.Camera.Print` after merging | **Q-349 = A**: one plain `Print` per system, other reads get their own verb; system = the owner's existing name (Unreal console groups, Lyra) |
| B-355b | `feature/B-355b-boss-moment-rules-cloud` | REVIEW (UNBUILT): `UFightTriggers::StartBoss` queues the one list's AtMoment rules (`BossRules::GetRules`), `Fire` → new `BossTriggers::FireMomentRule` (conditions read at the moment; a failing rule keeps waiting); `wor.Boss.Print` lists fight-start / moment rules; AtMoment warning gone; FightTriggersSpec case; Rules ER-60b, BossPhases changelog, PIE B-355b. Left: `BossInfusionHits` (needs per-target infusion hit reports) and the old-type retirement after PIE | **Q-355b = A**: conditions read when the moment comes (FFXIV / Elden Ring boss scripts) |
| B-351b | `feature/B-351b-stone-socket-carry-cloud` | REVIEW part 1 (UNBUILT): `RemoveCrystalFromGear` clears the stone socket too; a stone over an authored element-socket stone is refused; `FCarriedAttachment` / `FSavedCarriedAttachment::StoneSocket` (save **v22**; 21 is B-348b) through the carry and the run save; TwoSocketsSpec cases; Rules CL-28b, Loadout changelog, PIE B-351b. The rest became a new backlog row **B-351c** (CC+UE) | **Q-351c = A**: two parts, as Q-351 sliced B-351 |
| B-357 | `feature/B-357-evolution-one-cost-cloud` | REVIEW slice 1 (UNBUILT): `EVOLUTION_SLOT_COST` 2 everywhere; evolved adapter 2 → 3; budget specs. The rest is new row **B-357b** (resolve path, passives at infusion L2) | **Q-357 = A**: one cost 2 (the deckbuilder charges an Evolution its own cost wherever it attaches); B-357 in parts |
| B-042f | not started | **Skipped:** its finish line is a PIE-side `wor.Effects.Compare` (0 differ) and the B-134 snapshot on both paths; the stones already compose through `UWoRGE_StatSources` (earlier B-042 jobs). The desktop writer should judge what code is left | — |
| B-228b | `feature/B-228b-client-combat-hud-cloud` | REVIEW (UNBUILT): `ClientCombatHUD::Next` (pure), `UTurnStateComponent::UpdateClientCombatHUD` from `OnRep_TurnState`, `ACombatOrchestrator::BuildClientCombatHUD` (runs `OnCombatStartedUI`), `TeardownCombatHUD` public; TurnStateSpec case; Rules PA-37b, TurnState doc, PIE B-228b (2-player PIE) | none |

**Why the run stopped:** every remaining job in order needs something the cloud can't do well: B-232b (menu routing needs B-228b merged first), B-230 / B-233 / B-235 (two-player co-op, L-sized, need 2-player PIE to get right), B-340 (splits files every open branch edits: do it after these merge), B-042f (finish line is a PIE comparison), B-351c / B-357b (new rows; B-351c is CC+UE). The editor steps are in each job's `docs/PIE/` sheet.

---

# Cloud writer handover (2026-11-04) — STOPPED at a checkpoint (user, usage 67%); the main writer continues solo

The cloud writer pre-writes backlog jobs as **UNBUILT** branches (no Unreal in the cloud). The Unreal writer builds, tests and merges them. Decisions and backlog rows for the simplicity audit live on **`docs/D-128-136-simplicity-round2`** (rounds 1 to 3: D-120 to D-149, Q-21-audit). That branch carries round 1's `docs/D-120-127-simplicity-round1` too.

## Branches pushed (all UNBUILT unless the Backlog says otherwise), with their base

| Job | Branch | Base | State |
|---|---|---|---|
| B-293d + Q-293e | `chore/B-293d-decided-balance` | dev | merged to dev by the desktop |
| B-223b | `feature/B-223b-personal-level-drops` | dev | reviewed, fixed |
| B-325 | `feature/B-325-combat-tier` | dev | reviewed, fixed (sim weapon instance tier fix) |
| B-326 | `chore/B-326-flat-requirement-penalty` | B-293d | reviewed, fixed |
| B-327 | `fix/B-327-zone-health-once` | dev | spec only (no bug) |
| B-328 | `feature/B-328-enemy-hp-no-zone` | B-325 + B-327 + round-1 docs | D-120 widened applied |
| B-228 | `feature/B-228-turn-state-gamestate` | dev | reviewed, fixed; B-228b (client HUD) queued |
| B-229 | `feature/B-229-input-rpcs` | B-228 | reviewed, fixed |
| B-343 | `chore/B-343-guard-dodge-only` | B-229 + round-2 docs | not reviewed |
| B-313 | `refactor/B-313-pouch-per-member` | dev | reviewed; B-313b queued |
| B-314 | `refactor/B-314-run-layer` | B-313 | reviewed, fixed; B-314b queued |
| B-347 | `feature/B-347-free-run-loadout` | B-314 | reviewed, fixed; B-347b queued |
| B-316 | `feature/B-316-give-request-buyfor` | B-347 | buy-for slice, reviewed, fixed; B-316b queued |
| B-339 | `feature/B-339-inventory-screen` | B-316 | reviewed, fixed |
| B-354 | `feature/B-354-tier-caps-retire` | B-313 | reviewed; fixes queued with its writer |
| B-338 | `chore/B-338-delete-item-storage` | round-2 docs | done |
| B-330 | `feature/B-330-pillars-only` | round-1 docs | reviewed, fixed |
| B-331 | `feature/B-331-one-speed` | B-330 | reviewed, fixed; B-331b queued |
| B-332 | `docs/B-332-pie-sweep` | round-1 docs | docs only |
| B-333 | `feature/B-333-seven-balances` | round-1 docs | reviewed, fixed; B-333b queued |
| B-334 | `feature/B-334-infusion-costs` | round-1 docs | reviewed, fixed |
| B-335 | `chore/B-335-per-instance-rolls-obsolete` | round-1 docs | reviewed, fixed |
| B-344 | `feature/B-344-difficulty-sets-ai` | round-2 docs | not reviewed |
| B-345 | `feature/B-345-one-ai-path` | B-344 | reviewed, fixed |
| B-341 | `feature/B-341-turn-rules-trim` | round-2 docs | reviewed, fixed |
| B-342 | `feature/B-342-three-rows` | round-2 docs | reviewed, fixed |
| B-350 | `chore/B-350-test-actors-out` | round-2 docs | reviewed, fixed (actors flagged, editor step) |
| B-348 | `chore/B-348-one-save-version` | round-2 docs | reviewed, fixed; B-348b queued |
| B-346 | `feature/B-346-party-seats` | round-2 docs | reviewed; fixes queued with its writer |
| B-337 | `feature/B-337-conditions` | round-2 docs | reviewed; fixes queued; B-337b queued |
| B-351 | `feature/B-351-two-sockets` @ 425a1787 | round-2 docs | with D-145; both review must-fixes and should-fixes done; B-351b: authored-stone editor filters, sockets/skill-list debug helper |
| B-352 | `feature/B-352-fusions-retire` @ 6c846615 | B-351 (fixes merged) | review fixes mostly done; left: merge the stacked comment in `CrystalTypeHelpers.h`, list the remaining fusion branches for the sweep, PIE note on the Blueprint Fusion pin |
| B-353 | `feature/B-353-stat-stone` @ 253b0ef8 | B-352 (before its fixes) | job done, not reviewed; left: merge the B-351 / B-352 fixes in |
| B-354 | `feature/B-354-tier-caps-retire` @ 5ce0c408 | B-313 | review fixes done |
| B-359 | `feature/B-359-weather-baseline` | round-2 docs | done, not reviewed (review stopped) |
| B-358 | `feature/B-358-one-physical-resistance` | round-2 docs | done, not reviewed (review stopped) |
| B-356 | `feature/B-356-three-schools` @ 7bddbcd3 | B-345 | done, not reviewed; editor step: re-tag Conjuration spells (Q-356) |
| B-355 | `feature/B-355-boss-rules` @ 7f3531e8 | B-337 | code + spec done, not reviewed; left: Backlog row, PIE sheet, changelog, session entry, B-355b row (moment triggers still on the old list) |
| B-337 | `feature/B-337-conditions` @ a5f1304e | round-2 docs | review fixes done; B-337b queued |
| B-346 | `feature/B-346-party-seats` @ 96679a3d | round-2 docs | review fixes NOT started (PartyControl widget "Let AI finish" row, move to `SetMemberAISeat`, PIE console note, OBSOLETE flags on `CanHandToAI` / `CreateHandToAIButton`, nits) |
| B-232 | `feature/B-232-run-state-party` | B-229 | slice done, not reviewed; B-232b queued (menu on the commanding machine, other run readers, recruit fills a spot); `SeatRules.h` copied from B-346 |

**Not reviewed at all:** B-232, B-343, B-344, B-353, B-355, B-356, B-358, B-359.

**Save version order:** B-348 (11), then B-333 (12), then B-351 (next free). Each bumps LATEST = MIN (D-138).

## Queue still to pre-write (not started)
B-357 (evolution placement, D-147 incl. the L2 correction), B-336 (world subsystems, D-129), B-349 (console names, D-139), B-340 (docs moves, D-132; do last), B-042i / B-042j2 (GAS; skip if the Unreal writer started them), and the follow-up rows (B-228b, B-313b, B-314b, B-316b, B-331b, B-333b, B-337b, B-347b, B-348b, B-351b, B-355b to be written).

## Open items for Crown
- CLAUDE.md edits waiting on the user's go-ahead (not made on a peer's request): D-121 (GAS-only rule), D-138 (Saves note), D-139 (debug-tools / console naming), D-140 (Tests rule), plus `ConditionSystem.md` in the doc list.
- Defaults to confirm (Q- entries answered by the reference-backed default): Q-313, Q-314/b/c, Q-316, Q-325, Q-326, Q-328, Q-330, Q-331, Q-333, Q-334 (L1 infusion EP equals L0, no HP), Q-341/b/c, Q-342, Q-344, Q-345, Q-347, Q-348, Q-351 (stone socket costs no slot), Q-354, Q-356 (Conjuration → Enhancement), Q-358 (physical columns fold by average), Q-359 (a phase switches to the boss's element), Q-232 (B-232 slice; a player's own character is its pawn's).
- B-350: no nightly balance runner exists (nothing sets `WOR_BALANCE`); GASParity left the default run.
