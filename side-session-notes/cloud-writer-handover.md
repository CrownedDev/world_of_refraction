# Cloud writer handover (2026-11-09, fourteenth part: the visual and camera phase) — IN PROGRESS

**Written, pushed, reviewed and fixed** (merge plan rows 85–95). All are on the current trial branch **`integration/cloud-2026-11-09c`** (dev plus every unmerged cloud branch; it supersedes 09b):
- **B-394:** cast VFX source rule (Q-394).
- **B-392:** two guards. Save v32.
- **B-393:** parry landing VFX, stacked on B-392. Save v33.
- **B-391:** placeholder battle HUD and command menu, behind `wor.UI.NewHUD` (default 1). New row **B-391b** holds the damage numbers and the old HUD's missing pieces.
- **B-402:** side-on defence view; shake / punch while defending are dropped, not replayed (Q-402).
- **B-405:** boss framing (low turn view) and kill shots; nothing can start during the kill-shot hold (Q-405).
- **B-398:** turn-view setting, Centred Wide default, saved per player. Save v34.
- **B-403:** shot-pattern tuning (melee rush with motion blur, side-on impact). **Q-403 lists the FOV punch cut (8/14 → 5/10) for Crown.**
- **B-407:** short signature-spell cameras (`SignatureCameraCap`, 2.0 s, TODO: tune).
- **B-399:** calm menus (a sub-menu steps the turn camera in), stacked on B-398.

**Merge rules:** keep `LATEST = 34` whatever order rows 86, 87 and 92 merge in. The camera rows conflict with one another; the merge plan's late-night update gives each resolution. The one git won't flag: B-399's menu-step calls need B-405's `ApplyHeight` wrap.

**Superseded:** the desktop built its own **B-043f** (Q-043f C) and B-043g. My cloud `feature/B-043f-orchestrator-abilities-cloud` is docs only: **don't merge it**.

**Also written, reviewed and fixed (rows 96–100, all on trial 09c):** B-400 (target cut, on B-399; new row B-400b wires B-391's HUD), B-406 (per-boss cinematics on trial; B-406b holds the skip key and the rest), B-404 (wide beats and `OnTurnHandover`, on B-398), B-170d (action effects and break rows, from dev; its review BLOCK is fixed), B-401 (no reset between actions, on B-404).

**Being written / reviewed now:** B-170e (action-pips placeholder UI, on B-391 with dev merged in; in review) and B-170f (AI spend or bank, on B-170d).

**Still writable after those:** B-391b (on B-391), B-400b (on B-400 + B-391), B-406b. B-170g follows B-170f. B-170h waits for Crown's PIE; B-043h waits for PIE.

**Trial-merge lesson:** after a keep-both resolution, count braces per file; two slips were found and fixed that way. Every camera row's turn-view move goes through `GetTurnViewWithHeight` on the trial branch; the merge plan lists each place.

**Not cloud work:** B-395 (the slice fight: Crown's loop, animations reviewed one at a time) and B-214 (held by Crown).

# Cloud writer handover (2026-11-09, thirteenth part: after B-043b / B-043c landed) — STOPPED: the next parts wait for builds

The desktop built and merged B-043b and B-043c. Crown answered Q-170..Q-170h, which unblocked B-170a..g, and D-171 became B-397.

These are written, pushed, reviewed and fixed, and they are on the trial branch `integration/cloud-2026-11-09` (merge plan rows 79–84):

- **B-043d** (from dev): projectile and burst spells go through their ability (Q-043d).
- **B-043e** (on B-043d): AOE, Instant and support spells go through their ability (Q-043e). Two spec gaps are noted in its PIE sheet.
- **B-397** (from dev): hidden quests are listed once found, showing the current goal only, with a private discovery notice.
  - Q-377's placeholder notices are marked superseded.
  - The older PIE sheets now carry notes about this.
- **B-170a** (from dev): action pips 1, actions on the character.
  - **Q-170a differs from the plan:** action points are an additive layer set, not a stat entry. Crown may veto.
- **B-170b** (on B-170a): a fixed rotation behind `wor.Turns.ActionPips`.
  - Q-170b2: the side that strikes first leads every later round. Crown may change this.
- **B-170c** (on B-170b): several actions per turn.
  - `ActionCost` is at least 1. That is the plan's minimum; the writer brief had wrongly said 0.
  - `EActionType::EndTurn` is appended.
  - Q-170c2.

**Why I stopped:**
- **B-043f** (the orchestrator through abilities, with the montage moved into `PlayMontageAndWait`) is the big rewrite. It should wait for B-043d and B-043e to build.
- **B-170d** would be the fourth unbuilt layer on B-170a/b/c.
- **B-214** is held by Crown until after the visual phase.
- Rows 74–77 and the cleanups C1–C7 are still waiting on the desktop.

# Cloud writer handover (2026-11-09, twelfth part: after the desktop merged the GAS / Ice batch) — STOPPED: what's left needs a build first

The desktop merged these into dev: B-043a, B-273b + B-064a (with the review fixes), B-042g part 1 (after B-042f's PIE passed), B-390, B-396, and the two blocked-job notes. Crown also changed D-089 (Energy), which unblocks B-173. Newly writable:

- **B-173** (`feature/B-173-turn-start-energy-cloud`, from dev). Pushed, reviewed, fixed (Q-173: every turn kind refills).
  - Energy refills at the start of each of a character's turns: 10% of Max EP, at least 1, TODO: tune.
  - The refill goes through `ServerGainEnergy`'s gates, so Broken Darkness and a dormant Resonator gain nothing.
  - Debug: `wor.Energy.Print`. Spec: `WoR.Energy.TurnStart`.
- **B-042g2** (`feature/B-042g2-usegas-switch-delete-cloud`). Pushed. The review BLOCKed it over five specs; all are fixed.
  - It makes GAS reads permanent.
  - **Merge only after Crown's full fight at `wor.Effects.UseGAS 1` on dev.**
- **B-064c** (`feature/B-064c-ice-rest-cloud`). Pushed, reviewed, fixed. The rest of Ice: the resistance column, Ice-keyed buffs and Wards, and Water boosts skipping Ice.
  - Note for Crown: a Status Multiplier buff granted by an Ice spell no longer boosts that spell's own buildup.
- **B-043b** (`feature/B-043b-ability-base-cloud`). Pushed, reviewed, fixed. Adds the ability base class and granting, behind `wor.Abilities.UseGAS` (default 0).
  - The UE API is written from memory of 5.5 or later, so watch the first compile.
  - B-214 can stack on it.
- **New trial branch `integration/cloud-2026-11-09`.** It is dev plus everything unmerged (rows 74–77 and C1–C7). The cross-merge review found no code breakage, and the doc-row damage is fixed.
- **B-043c** (`feature/B-043c-melee-ability-tasks-cloud`, stacked on B-043b). Pushed, reviewed, fixed. A skill's impacts reach its ability as `Event.Combat.Impact`; the montage stays in the executor until B-043f.

**Why I stopped here.** B-043d..h would stack a fourth, fifth, ... unbuilt GAS layer on B-043b/c, whose UE API use is written from memory. One compile of B-043b/c first is the cheaper path. B-214 (field skills, L) can stack on B-043b, but it needs the map pawn's ASC, which the survey couldn't find, and most of its tables are Crown's (`// TODO: Crown`), so it waits too. Everything else on the queue needs a compiler, PIE or Crown.

# Cloud writer handover (2026-11-09, eleventh part: the brain's GAS / Ice / cleanup plan) — STOPPED: the plan is done; no writable job left

**The brain's plan (2026-11-09), in order, and where each item stands.** Nothing was built.

1. **B-042f.** Merged into dev by the desktop (built, suite green). Its PIE (`wor.Effects.Compare` 0 differ at UseGAS 0 and 1) gates B-042g.
2. **B-042g part 1** (`feature/B-042g-gas-readers-default-cloud`, on B-042f). Pushed, reviewed, fixed, and on the trial branch. Merge only after B-042f's comparison passes. B-042g is cut into six parts; rows B-042g2..g6 are not written.
3. **B-043, sliced into B-043a–h.** Part 1 is **B-043a** (`feature/B-043a-costs-gas-cloud`, on B-042f):
   - Skill EP costs and the infusion HP backlash are paid as Instant GEs behind `wor.Costs.UseGAS`.
   - Q-043a (reference-backed default): SetByCaller from the one cost formula; the MMC comes with B-043b.
   - Reviewed and fixed: a blocked GE pays the old way, and the ASC must belong to the payer.
   - On the trial branch; merge-plan row 71.
4. **B-273b + B-064a (Ice).** Both are pushed, reviewed and fixed, and are on the trial branch (merge plan rows 72 and 73).
   - B-273b (`feature/B-273b-hit-kind-cloud`, from dev): the hit kind carries a dealt-type override. Buildup, the break trigger and the GAS executions read it, and no existing number changes.
   - B-064a (`feature/B-064a-ice-cloud`, on B-273b), Ice:
     - IceBreak: Water's slow for 1 turn plus 10% of max HP;
     - `bDealsIce` on Water or Generic spells (deckbuilder rule 75);
     - the class table's Ice column;
     - Water Amp doesn't boost Ice;
     - spec `WoR.Elements.Ice`.
   - Q-064a was approved by the reference-backed default (Elden Ring's Frostbite).
   - **Q-064a2 is OPEN (lore, Crown's):** the Ice break's name ("Ice break" is a placeholder), and whether Water's "Frozen" / "Frost Bite" move to Ice.
   - New row B-064c holds the rest: the Ice column in ResistanceColumns (the asset, gear, saves), Ice-keyed resistance buffs, the Water StatusMultiplier on Ice, AI / VFX by hit kind, Ice abilities and tuning.
5. **Cleanups.** Each has its own branch from dev and merges only after Crown's PIE of its gate (merge plan section "PIE-gated cleanups", rows C1–C7). All are reviewed, have their findings fixed, and are on the trial branch:
   - C1: B-152 (gate B-021).
   - C2: B-153 (gate B-022).
   - C3: B-154 (gate B-024).
   - C4: B-172. Its gate is B-028 plus three PIE checks that still run the deleted `wor.Loadout.CompareRules`.
   - C5: B-309, stacked on B-172. Only the safe subset: the old `FCombatLoadout` fields still have live writers and are what the save stores, so they need their own job first.
   - C6: B-034b (gate B-034, plus an editor Blueprint check).
   - C7: B-317, first part. New row B-317b covers the co-op store move. The wearers, the per-member copies and the enemies' `ItemSlots` stay.

   **Blocked,** with docs-only branches:
   - B-032b → new row B-032c. The lumped spell path still serves multi-hit spells, so per-impact defense needs building first, which changes behaviour.
   - B-165 needs the D-067 spec first, written on the desktop.

   **Not writable:** B-299, which waits for all of B-042g.
6. **B-214 / B-236: not writable after part 1.** Field skills need B-043b (the ability base, granting and tags), and B-236 waits on B-214. B-214 is re-pointed to B-043b.

**Writable jobs left: none from the brain's list.** What's left needs a compiler, Crown's PIE, or Crown's answer:
- B-042g2..g6 wait on B-042g part 1 merging, after B-042f's PIE.
- B-043b..h are stacked work for after B-043a builds.
- B-299 waits for B-042g.
- B-032c is a behaviour change for Crown to approve.
- B-165 needs a spec run on the desktop.
- B-064c can start once B-064a builds.
- Q-064a2 is Crown's lore call.

# Cloud writer handover (2026-11-08, tenth part: confirmations, B-381 activations, B-389 / D-168) — STOPPED

**The brain's three messages from 21:57–21:59 UTC are all done.** Everything is pushed, reviewed by wor-reviewer with its findings fixed, and cherry-picked onto the trial branch. Nothing was built.

- **Confirmed by Crown, marked on their branches:**
  - Q-379 (B-379, and B-381);
  - Q-380 (4) (B-380);
  - Q-381 (B-381).
- **B-381 activations** (Crown, MemoryCatalogue §3.11). A gear quest's start can now read the worn item's own state with a `WornItemState` condition: the socket's crystal or evolution element, a given evolution, a stone type, a minimum tier, and the upgrade level.
  - Weapons, the adapter and apparel all count; B-380b was merged in for the apparel socket.
  - Spec: an apparel piece with a Fire crystal starts the quest; Water or an empty socket doesn't.
  - The review BLOCKed it. Fixed: the condition was checked after the "no event" guard, and a broken crystal still counted.
- **New row B-389: resistance by damage type** (Crown's D-168). Cut from dev after B-387.
  - Defence stays as it is, as the All line.
  - Gear gains Physical and Spell damage lines. They are summed from every source and added to the element line under the ±50% cap.
  - Old data deals the same damage.
  - **Q-389** (reference-backed default): the class table's buildup Physical column (Generic +15 / Resonator −15) does not count for damage, because that would be a balance change.
  - The review passed with notes; all fixed.
  - Left for later: scoped ResistanceBuff effects, DoT ticks (element only), and rolling the new lines on gear.
- **Merge plan:** the 23:45 update and new row 69.

**Writable jobs left:** none. What's left needs a compiler. Q-378's cure question is closed (the brain, 2026-11-09: Crown says a cleared curse is regainable, as built; marked on B-378 and the trial branch). **Q-389 changed by Crown (2026-11-09):** damage reads the class table's Physical column, so a Generic takes 15% less physical damage and a Resonator 15% more (an intended balance change). Built on B-389 with spec cases, Q-389 answer, ED-27, PIE checks 7–8; reviewed (PASS WITH NOTES, fixed); on the trial branch; merge plan row 69 flags it. Note: a Generic or Resonator turning Broken Darkness loses that line (the BD row's physical column is 0); the Generic boss "Lord" now takes 15% less physical damage.

# Cloud writer handover (2026-11-08, ninth part: Q-187e vote, Q-377 confirmed, Q-378 changed, B-388) — STOPPED

**The brain's four messages (21:42–21:45 UTC) are all done.** Every branch below is pushed, reviewed by wor-reviewer, and has its review findings fixed. Nothing was built.

**Q-187e (1) changed by Crown: reviving the mini-boss is a vote.** On `feature/B-187e-client-rest-menu-cloud`:
- New: `Trial/ReviveVote` (rules), a replicated `FReviveVote` on `AWoRGameState`, `ServerCastReviveVote`, the vote buttons in the rest menu, `wor.Trial.Vote` / `wor.Trial.ReviveVote`, and ReviveVoteSpec.
- The review BLOCKed it, and every finding is fixed:
  - a compile error (`TObjectPtr` from `FindByPredicate`);
  - a late answer could revive twice: the vote now acts once;
  - a pass could reload the level mid-fight: **Q-187e2** (reference-backed default) makes a passed vote wait for the fight to end;
  - `Cast` is renamed `Answer`;
  - refusals now say why.
- Q-187e (2) was confirmed by Crown: Leave stays personal.

**Q-377 confirmed by Crown (all A).** On `feature/B-377-hidden-quests-cloud`: the two notices are in one place (`HiddenQuestNotice`, `// TODO: Crown's text`). Merged forward into B-378, B-378b, B-377b and B-381.

**Q-378 changed by Crown (curses).** What was built:
- On `feature/B-378-curses-cloud`:
  - a quest step gives a curse (`FHiddenQuestStep::Rewards`);
  - CurseHeld already works on quest steps;
  - the player clears one (`ClearCurse`, `wor.Curse.Clear`);
  - a cleared or lifted curse comes back when its giver is met again (`CheckRegain`, `RegainCurses`);
  - specs: cursed, the step moves; cleared, it doesn't; regained, it moves again; a memory met again regains it.
  - The review BLOCKed it. Fixed: `CurseSubsystem.h` didn't compile; a cure found where its curse was given was undone by that same event (`BeginEvent`).
- On `feature/B-378b-curse-penalty-cloud`: the placeholder hub service `UCurseClearWidget` (`wor.UI.ClearCurse`, refused during a run), a spec case and PIE checks 5–8.
- **Flag for Crown** (in Q-378's notes): as built, a curse lifted by its cure can come back too. Should a cure be permanent?

**New row B-388: main and side quests, part 1.** On `feature/B-388-main-side-quests-cloud` (UNBUILT, cut from B-377):
- `EQuestKind` (Hidden / Main / Side), a `Title`, and a `Goal` per step;
- `GetVisibleQuests` and `StartQuestById` (`wor.Quest.Give`);
- notices for visible quests;
- the journal's Quests page, and `WoRGameNames::GetQuestKindName`;
- QuestJournalSpec;
- Q-388 (reference-backed default: Skyrim's one quest record), ER-12l and PIE B-388.
- Content is Crown's; it is placeholder text for now.

**Trial branch:** all of the above is cherry-picked. One slip happened and was fixed in its own commit: `HiddenQuestSubsystem.h` got emptied while a conflict was being resolved, and it has been rebuilt.

**Merge plan:** the 23:00 update lists the dev re-merges in order (B-377 → B-378 → B-378b → B-377b / B-381 → B-388), plus the B-187e re-merge. New row 68 is B-388.

**Writable jobs left:** none. What's left needs a compiler, Crown's text, or Crown's answer on the cure question.

# Cloud writer handover (2026-11-08, eighth part: Q-187d changed by Crown) — STOPPED

**The brain (21:36 UTC):** Crown changed Q-187d (1): only the character(s) that use the rest spot rest (the presser's own; solo: the whole party). Points (2) and (3) stay A.

**Done on `feature/B-187d-co-op-rest-cloud`:**
- `UPartySessionSubsystem::RestMembersWhere`.
- `WoRRestSpot::RestsMember`: the presser's claimed members; for the host, also every member no player claims.
- `Rest(World, Id, Presser)`.
- RestSpotSpec case: a client rests only its own; the host rests its own and the unclaimed; solo rests everyone.
- Q-187d answer (Crown), ER-09b, PIE B-187d (check 1 changed, new 1b).

**Merged forward:** into `feature/B-187e-client-rest-menu-cloud`. One conflict: B-187e's `bLocalPlayerRests` became `bPresserIsHost`.

**Trial branch:** the four commits are cherry-picked; a full merge of the desktop-reviewed B-187e conflicts in 15 files.

**Note for the desktop:** B-187d and B-187e were already in `dev`, so merge them again. `dev` has also moved on a lot (B-386 is in), so the trial branch is behind `dev`.

# Cloud writer handover (2026-11-08, seventh part: the brain's list after B-384) — STOPPED: all five items done; what is left wants a compiler or Crown

**The brain (20:56 UTC), in order:** (1) the GAS follow-ups from B-382/B-383; (2) B-380d if writable; (3) apparel Amp lines; (4) crystal Amp by tier; (5) re-scan the Backlog.

| Job | Branch | State | Decisions |
|---|---|---|---|
| B-385 | `feature/B-385-scope-gas-followups-cloud` (on B-384) | REVIEW (UNBUILT). Reviewed, fixed and re-reviewed (pass). At UseGAS 1, Defence keeps its scope: an additive correction (this hit's layers minus all of them), added to the composed attribute and in the GAS damage execution, so it still works when the layers floor to 0. The mirror compares and fingerprints the scope; `wor.Damage.Scopes` reads held effects. ScopeGASSpec; PIE B-385 | — |
| B-380d | `feature/B-380d-apparel-lore-socket-cloud` (on B-380c) | REVIEW (UNBUILT). Reviewed and fixed. The apparel socket gives the old crystal back to storage; socketing the same crystal again is a no-op; an evolution must be owned, belong to the piece's owner (apparel gets an owner when banked) and not be in another socket; `IsEvolutionInUse` also counts apparel. ApparelSpec; CL-50b; PIE B-380d. **Split:** lore-loadout apparel (banking authored apparel, the save, then stripping unowned apparel) moved to new row **B-380e**, which needs a build | Q-380d (default: the crystal comes back whole; worn or broken is Crown's call); Q-380c (default: banked at first draw like starting weapons, built in B-380e) |
| B-386 | `feature/B-386-apparel-amp-cloud` (on B-380d, with B-385 merged in) | REVIEW (UNBUILT). Review: pass with notes, notes fixed. Apparel carries Amp lines (the deckbuilder's HP · Amp · Def). ApparelSpec; CL-50c; PIE B-386 | Q-386 (deckbuilder rule) |
| B-387 | `feature/B-387-crystal-amp-cloud` (on B-386) | REVIEW (UNBUILT). Review: pass with notes, fixed. A worn crystal (weapon, adapter or apparel socket) and the seated evolution grant Amp to their element by tier: F0 E0 D1 C1 B2 A2 S3. Broken ones and gear-socket evolutions (D-147) give none. AmpSpec; ED-50d; PIE B-387 (balance note: it stacks, so rerun the balance report) | Q-387 (deckbuilder ladder) |

**Re-scan (item 5):** nothing else was unblocked by today's decisions:
- The MemoryCatalogue rows are written (B-375 to B-381).
- D-164 to D-167 and Q-164b are covered by B-380 to B-387.
- B-380e needs a build.
- Still open for later rows: B-377b's notes (a friend's curses and counters read on the host; a client's quest-end memory reaching the host's known set).

**Merge plan:** rows 64-67 added. Save stays 31.

**Trial branch:** all merged. The docs union doubled Q-380c/d and some Backlog rows; fixed on the trial branch.

# Cloud writer handover (2026-11-08, sixth part: Crown's Amp answer, damage scopes, one damage channel) — STOPPED: all three rows written; what is left wants a compiler or Crown

**The brain (19:37 UTC):** Crown answered **Q-164b = A, as an addition**, then sent a follow-up: give every damage and defence boost a scope (D-166).

| Job | Branch | State | Decisions |
|---|---|---|---|
| B-382 | `feature/B-382-amp-lines-cloud` (on `docs/damage-defence-sources-cloud`) | REVIEW (UNBUILT, save **v30**), reviewed and fixed. `FAmpBonus` lines (All / Physical / Spell / nine elements, -5..+5) on `FEquipmentStatBonus`; worn gear sums them raw (`GetActiveStatBonus`); they add flat to the skill's base before the multipliers, once per action, damaging actions only (`GetActionAmp`, `PreviewActionDamage`); `wor.Damage.Amp`; AmpSpec; ED-50, PIE B-382. Review fix: the save bump to 30 (D-138) | Q-164b answered (Crown): A, as an addition; defaults taken (small numbers, no status buildup, crystal Amp by tier later) |
| B-383 | `feature/B-383-damage-scopes-cloud` (**on** B-382) | REVIEW (UNBUILT, save **v31**), reviewed and fixed. `EDamageScope` All / Physical (not a spell) / Spell / Element on the eleven damage / defence boost types (payload and runtime, GAS identity); `GetTotalStatModifierForHit` (GAS `SumMagnitudeForHit`); every read in `UDamageCalculator` goes through it (`GetDefenderDefenseForHit`, `GetScopedTransientDamageFactor`, which is shared with the stat-cap correction: the review's must-fix); effect text names the scope; Amp's Physical = not a spell, plus a Spell line; `wor.Damage.Scopes`; DamageScopeSpec; D-166, ED-50b, PIE B-383. New row **B-384**, since answered and written | D-166 (Crown) |
| B-384 | `feature/B-384-one-damage-channel-cloud` (**on** B-383) | REVIEW (UNBUILT), reviewed and fixed. The four all-damage % channels add into one (`GetAllDamagePercent`: DamageBuff − Debuff incl. difficulty, ModifyDamageDealt, the hit's Raw or Spell axis; each scoped), applied once. The old multiplied path stays behind `wor.Damage.FoldAllDamage 0` (OBSOLETE until PIE). `wor.Damage.Scopes` prints the one line. AllDamageFoldSpec; D-167, ED-50c; PIE B-384 has the balance shift and before / after numbers (×1.728 → ×1.6 for three +20%). Review fixes: at the stat cap the Raw / Spell buffs no longer fight the cap correction (they count only in the uncapped sum: **reference-backed default, flagged for Crown**); the spec's cvar toggle is safe | D-167 (Crown: fold them); the stat-cap rule is the flagged default |

**Left for a later pass (reviewer's nice-to-haves):**
- At UseGAS 1 the Defense attribute ignores scope (TODO in `GetDefenderDefenseForHit` and `WoRDamageExecution`).
- `FWoRMirroredEffect::Matches` does not compare the scope.
- `wor.Damage.Scopes` reads the enum store, not the held effects.
- No GAS / serialize spec for the scope.

**Merge plan and trial:** rows 61-63 added (save 30 and 31; B-384 bumps none). `integration/cloud-2026-11-08` merges both. A docs union there brings back Q-164b's old OPEN lines and older TODO Backlog rows; both are fixed on the trial branch and noted in the plan.

# Cloud writer handover (2026-11-08, fifth part: the brain's merge list) — STOPPED: the brain's four items are done; what is left wants a compiler or Crown

**The brain (13:05 UTC), in order:** (1) B-380c; (2) a merge plan; (3) a trial integration branch; (4) Amp, Q-164b. **14:03 UTC:** Crown reshaped (4) into a comparison, `docs/analysis/DamageAndDefenceSources_2026-11-08.md`, which covers damage and defence sources against the deckbuilder's Amp / Def lines, with a recommendation in Q-164b.

| Item | Branch | State |
|---|---|---|
| B-380c | `feature/B-380c-apparel-slots-saved-cloud` (**on** B-380b) | REVIEW (UNBUILT, save **v29**), reviewed and fixed. Apparel is saved in loadouts and custom loadouts; inventories draw worn apparel (`HandlePoolGearChanged`); a crystal can be attached in play (`AttachToApparel`, `wor.Loadout.SocketApparel`); `NotifyApparelChanged` re-syncs stats; lore loadouts refuse apparel. The gear screen is left to the PIE sheet; new row **B-380d** |
| Merge plan | `docs/cloud-merge-plan-2026-11-08` | `docs/Merge/CloudBranches-2026-11-08.md`: 60 branches top to bottom, each with base, save version, expected conflicts and how to resolve them; the save table (21-29 already rise in order, so no renumbering); the B-368 / B-370 constants; the B-232d + B-235c chest hint; the fixes the merges need (from wor-reviewer); follow-ups. Skip B-361b (already in dev) |
| Trial merge | `integration/cloud-2026-11-08` (from origin/dev `dba4a0f5`) | **Trial only: never merge into dev.** All 60 merged `--no-ff` with conflicts resolved, plus fix commits: the save-version comment written once; FCombatCameraRules as the one source of the read-time constants; a client's chest hint key; doubled Backlog / Rules rows dropped (CL-28b clash → CL-28c); two compile errors that only appear when branches meet (ReviveMiniBoss's brace; B-357c + B-380's `Loadout` local); the doubled EvolutionItemData scan; a client's rest no longer leaves the host at a rest spot; Level up hidden on a client. wor-reviewer ran and its must-fixes are applied. Not compiled |
| Q-164b + comparison | `docs/damage-defence-sources-cloud` (**on** `docs/Q-164b-amp-cloud`) | `docs/analysis/DamageAndDefenceSources_2026-11-08.md`: the hit formula with code lines, every damage and defence source (scope, stacking), the deckbuilder's Amp / Def and its one sum, a side-by-side table. Defence already matches (resistance = element Def); the gap is that nothing raises one element's damage, and four all-damage % channels duplicate each other. Q-164b stays **OPEN** for Crown; it recommends A (keyed Amp lines in one sum, flat into the base per action). ED-50 updated. **Merge this branch instead of `docs/Q-164b-amp-cloud`** (row 60 of the plan): it contains it |

**Why it stopped (14:40 UTC):** the brain's list is finished. What is left needs a compiler or Crown:
- the first build of `integration/cloud-2026-11-08`;
- the branch-by-branch merges, following the plan;
- Crown's Q-164b answer;
- the desktop jobs (B-380d, B-234d, B-357d, B-351e, B-233b, B-107-109; editor, VRoid and profiler work).

# Cloud writer handover (2026-11-08, fourth part: the brain sent more) — STOPPED: the brain's list is written; what is left wants a compiler

**Why it stopped (13:40 UTC):** every row the brain sent is written, reviewed with wor-reviewer and fixed, and the split rows are written too (B-377b, B-378b, B-380b, B-187e). Left: **B-380c** (apparel loadout slots saved, inventories drawing worn apparel, attaching a crystal in play, stat re-sync, gear screen: deep in the loadout / inventory sync code, better with a compiler), plus the older desktop jobs (B-234d, B-357d, B-351e, B-233b, B-107-109, editor / VRoid / profiler). Every branch is UNBUILT: build, run the specs, then PIE.

**The brain (10:46, 10:56, 10:57 UTC):** (1) B-235d part 3 → row **B-235e**, then Crown's change (MemoryCatalogue §3.5: in co-op a PriorMemory reads the team's memories together) applied on it; (2) **B-187d**; (3) Crown agreed `docs/Design/MemoryCatalogue.md` → new rows **B-375** MemoryDebug, **B-376** "party has", **B-377** hidden quests, **B-378** curses, **B-379** hidden counters, stacked B-235e → B-375 → B-376 → B-377 → B-378 → B-379. Save versions on the stack: B-377 **v25**, B-378 **v26**, B-379 **v27**, B-377b **v28** (after the cloud's 21-24; renumber at merge).

**The brain (11:58 UTC):** D-164 apparel and D-165 gear quests → **B-380** apparel part 1 (from dev; part 2 **B-380b**: owned store, save, re-sync, gear screen) and **B-381** gear quests (on B-379).

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-235e | `feature/B-235e-own-memory-finds-cloud` (**on** B-235d) | REVIEW (UNBUILT), reviewed and fixed: a client's own map spot (overlap only on the walker's machine) and fragment are its finds, reported to the host (`ServerReportFoundMemory`; one for everyone shared from the host); worn-item and reaction memories go to the wearer's player (`MemoryUnlock::UnlockFoundBy`, FightTriggers too); the host knows each friend's memories (`KnownMemoryIds`, journal reported from `ClientInitialize`) and in co-op a PriorMemory reads the team's memories together (Crown, MemoryCatalogue §3.5: `GetTeamMemoryIds`; `FWoRConditionContext::KnownMemoryIds`); co-op CheckEvent lets PickReceivers decide who met it; a client refuses a memory for none of its characters. Spec case; ER-12e, PIE B-235e. Commit 92cfecde does not build alone | none new (Q-235d, Crown) |
| B-187d | `feature/B-187d-co-op-rest-cloud` (**on** B-187c, B-235c merged in) | REVIEW (UNBUILT, **merge-resolved without a compiler**: both run resets kept; the chest keeps B-235c's per-player key and B-073's tally), reviewed and fixed: a client's rest press goes to the host (HubInteract) and opens its own menu (`Client_OpenRestMenu`; Leave and earnings hidden on a client); after a revive every player stands beside the spot (`TrialReturn::GetReturnStand`, 150 cm alternating, the stand when blocked, dropped after travel); a chest tallies once on its first open. RestSpotSpec case; ER-09d, PIE B-187d. New row **B-187e** (a client's host-only menu actions). Commit bc23997b does not build alone | **Q-187d = A/A/A**: whole party restored (BG3 / DOS2), chest tallies once (personal loot), regroup side by side (Monster Hunter, DRG) |
| B-073 (fix) | `feature/B-073-run-rewards-cloud` | Review of B-187d found a compile error from B-073: a second `UTrialRunSubsystem::GetPayoutMultiplier()` (same signature as dev's difficulty one). Renamed B-073's to `GetRunPayoutMultiplier()` (new commit; merged into B-187d). The other B-073 descendants (B-187, B-187b, B-187c, B-234c, B-363, B-364) pick it up when B-073 merges first; none call the new name | — |
| B-375 | `feature/B-375-memory-debug-cloud` (**on** B-235e) | REVIEW (UNBUILT), reviewed and fixed: `MemoryDebug::GetWhyString` and `wor.Memory.Why <Id>` (scope, each condition met / not yet per player present, who would get it); MemoryDebugSpec; ER-12f, PIE B-375 | none |
| B-376 | `feature/B-376-party-has-cloud` (**on** B-375) | REVIEW (UNBUILT), reviewed and fixed: `PartyHas` → `CharacterPresent` (appended), `FMemoryEvent::Present` kept through `FilterParty` (pair memories solo or across players); spec case; ER-12g, PIE B-376 | none |
| B-377 | `feature/B-377-hidden-quests-cloud` (**on** B-376) | REVIEW (UNBUILT, save **v25**), reviewed and fixed: `UHiddenQuestData`, `UHiddenQuestSubsystem` (start from `OnMemoryRecorded`, steps from `CheckEvent`, `EndRun` from `ExitTrial`, end memory to its player alone), `FSavedHiddenQuests`, `wor.Quest.Print`; HiddenQuestSpec; HiddenQuestSystem.md; ER-12h, PIE B-377. New row **B-377b** (co-op friends' quests; the journal saved). Note: the journal is not saved yet | **Q-377 = A**: a step can need several events in one run (that count resets), the player's own depth starts it, notices say nothing (Monster Hunter, Hades, SLF); (4) the starting event does not do step 1 |
| B-378 | `feature/B-378-curses-cloud` (**on** B-377) | REVIEW (UNBUILT, save **v26**), reviewed and fixed: `UCurseSubsystem` (Curse / RemoveCurse rewards on `OnMemoryRecorded`; a lifted curse stays lifted), `CurseHeld` condition (a friend's part on the host never reads the host's curses), `FSavedCurses`, `wor.Curse.Print`; CurseSpec; CurseSystem.md; ER-12i, PIE B-378. New row **B-378b** (the penalty and `UCurseData`) | **Q-378 = A**: by a memory's reward id, lifted by RemoveCurse, several, per player (SLF's Lycagon's Mark, Elden Ring) |
| B-379 | `feature/B-379-hidden-counters-cloud` (**on** B-378) | REVIEW (UNBUILT, save **v27**): `UHiddenCounterSubsystem` (per character and all; Parries = a Perfect Guard and Kills, from `ABattleGameMode`; a friend's character's on their machine via `Client_AddCounter`), `CounterReached` condition, `FSavedCounters`, `wor.Counter.Print` / `wor.Counter.Add`; HiddenCounterSpec; HiddenCounterSystem.md; ER-12j, PIE B-379. reviewed and fixed (`WoRConditions::Compare` qualified: a compile error; parries via `GuardRules::IsPerfectGuard`); a friend's counter milestone waits for B-377b | **Q-379 = A**: Parries and Kills first, per character and all, the player's own save, checked at the next memory event (SLF hidden stats, Monster Hunter hunt counts) |
| B-380 | `feature/B-380-apparel-cloud` (from **dev**) | REVIEW (UNBUILT) part 1, reviewed and fixed, then Crown's D-164 answers applied: `UApparelData` (HP and Defence lines only), `FApparelEntry` + `ApparelRules` (2 slots, no budget, a crystal or evolution in the socket keys it and adds its element's resistance, an evolution its passives (Crown), `CRYSTAL_ELEMENT_RESISTANCE` 5 TODO tune; at merge with B-357b gate the apparel evolution's passives at charge L2), its effects in the three loadout effect gatherers, `FCombatLoadout::Apparel` (out of the Slots mirror the budget walks), its lines in `GetActiveStatBonus` and resistance in `GetActiveResistanceBonus`, `wor.Loadout.Print` (new), `wor.Loadout.WearApparel` (placeholder); ApparelSpec; Q-380, CL-50, PIE B-380. New row **B-380b** (owned store, save, re-sync, gear screen) | **Q-380**: all answered by Crown (2 slots, the crystal's / evolution's element resistance, effects, an evolution allowed); default: lines tier-scaled |
| B-381 | `feature/B-381-gear-quests-cloud` (**on** B-379) | REVIEW (UNBUILT): `FBoundQuest`, `BoundQuests` on `UEquipmentDataBase` / `UEvolutionItemData`, `UHiddenQuestSubsystem::StartGearQuests` (the event's worn gear and primary evolution, this player's own characters); HiddenQuestSpec case; Q-381, ER-12k, PIE B-381. Reviewed (the duplicate B-380 row dropped) | **Q-381 = A**: on the item like bound memories, worn first, the wearer's player (NieR: Automata weapon stories) |
| B-187e | `feature/B-187e-client-rest-menu-cloud` (**on** B-187d) | REVIEW (UNBUILT), reviewed and fixed: a client's Revive shows the host's answer (`Client_OpenRestMenu(bCanRevive, RevivalNumber)`) and asks the host (`ServerReviveMiniBoss`: refused mid-fight or beyond 600 cm of the rest stand); a client's Leave leaves the session only (`disconnect`); Q-187e, ER-09e, PIE B-187e (no spec: network paths) | **Q-187e = A/A**: Revive asks the host; Leave leaves alone (Monster Hunter abandon, DRG) |
| B-378b | `feature/B-378b-curse-penalty-cloud` (**on** B-378) | REVIEW (UNBUILT): `UCurseData` (Title / Description for Crown, StatPenalty, ResistancePenalty), the held curses' penalty added for the holder's own local-party characters in `GetActiveStatBonus` / `GetActiveResistanceBonus`; CurseSpec case; ER-12i, PIE B-378b. Reviewed and fixed (curse assets cooked AlwaysCook; the penalty added flat after the tier sum; the penalty cached; the gather waits for the registry) | Q-378 note: own party characters only, flat, summed |
| B-377b | `feature/B-377b-coop-quests-journal-save-cloud` (**on** B-381) | REVIEW (UNBUILT, save **v28**), reviewed and fixed: the journal saved (`FSavedJournal`; `Restore` announces nothing); a client reports its quests (`ServerReportQuests`), the host checks them on the friend's own part (`CheckFriendsQuests` → `Client_QuestStepMet(QuestId, StepIndex)`, counted only for that step), a friend's worn gear starts theirs (`Client_StartQuest`); the host's own quests read its own part. HiddenQuestSpec cases; ER-12h, PIE B-377b. Left: a friend's curses / counters read on the host; a client's quest end memory into the host's known set | none new |
| B-380b | `feature/B-380b-apparel-store-save-cloud` (**on** B-380) | REVIEW (UNBUILT) part 1: owned apparel in the pool (`OwnedApparel`, `AddApparelToPool` mints the id), saved in the one gear list with its crystal / evolution and roll (no new save field), `WearApparel` banks it; ApparelSpec case; CL-50, PIE B-380b. New row **B-380c** (loadout apparel slots saved, inventories drawing it, attaching a crystal in play, stat re-sync, the gear screen). Reviewed and fixed (RemoveGear takes apparel; a bad socket loads empty) | none |

# Cloud writer handover (2026-11-08, third part: the brain unblocked B-073) — STOPPED: no cleanly writable job left

**Why it stopped:** what is left needs the desktop or a merge: B-187d (co-op rest press: B-187c and B-235c conflict in `TrialMapFeature.cpp` / `TrialRunSubsystem.cpp`), B-234d (a pool reset after a handed-over run: needs the pool's internals with a compiler), B-357d (the L2 action's passives and ~200 old reads: compiler), B-351e (after Crown's PIE and an editor step), B-233b (EOS ids), B-107-109 (Crown's B-102 review), B-127 / B-032b / B-034b / B-066 / B-240 / B-237 / B-215 / B-373 (editor first), B-042g (after B-042f's PIE), B-340, and the VRoid / profiler jobs. Every branch here is UNBUILT: build, run the specs, then PIE.

The brain (08:32 UTC): B-073 was stale-blocked (D-048 answered 2026-10-26); write it and its chain (B-187 → B-363 → B-364, B-234b, B-235d's payout), then B-357b and B-351d in parts with reference-backed defaults. Still skip B-233b, B-107-109, B-127, B-340, B-042f, editor / VRoid / profiler jobs.

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-073 | `feature/B-073-run-rewards-cloud` | REVIEW (UNBUILT): `Trial/RunRewards` (win run Prisms now; hub Gold + Gear / Skill Essence tallied per win and chest; the run's end pays the tally scaled by difficulty, a +25 % boss bonus and the multiplier, to every player in full); `UTrialRunSubsystem::RecordFightReward` / `RecordChestReward` / `ExitTrial` pays; save **v23** (`FSavedRun::Tally*`); `wor.Trial.PrintRewards`; `RunRewardsSpec`; Rules ER-06..09, PIE B-073. Covers B-235d's payout (every player in full) | **Q-073 = A**: Gear + Skill Essence (D-094's Unknown essence retired with D-125), boss bonus for "further", first numbers (Nightreign / Hades encounter-plus-clear) |
| B-187 | `feature/B-187-rest-spot-cloud` (**on** B-073) | REVIEW part 1 (UNBUILT): `ATrialRestSpot` + `WoRRestSpot::Rest` (full HP / EP, revive, last rest spot), placeholder `URestSpotWidget` (inventory, leave with confirm), `LeaveRunEarly` (half the tally); `wor.Trial.Rest`; `RestSpotSpec`; Rules ER-09b, PIE B-187 (placing the actor in a map is an editor step; the console works without). Mini-boss rest spots / revival: new row **B-187b** | **Q-187 = A**: leaving pays half (Darkest Dungeon retreat), every use rests (Hades) |
| B-363 | `feature/B-363-levelling-at-rest-spots-cloud` (**on** B-187, B-073 / B-187 review fixes merged in) | REVIEW (UNBUILT): points placed only at a rest spot (`UTrialRunSubsystem::IsAtRestSpot`; set by `Rest`, cleared by a fight, `RestoreRun` and `WoRRestSpot::StepAway` when the rest menu / level screen closes); `WoRLevelDrops::GetPointPrice` / `TryBuyPoint` (run Prisms on ER-24's ladder per character; optional wallet); level screen's Purchase point, rest menu's Level up; vendor `BuyWorldStat` OBSOLETE; LevelDropsSpec cases (banked not placed, price spent); Rules ER-24b, PIE B-363 | none (Q-156 already answered) |
| B-364 | `feature/B-364-over-the-pillar-cap-cloud` (**on** B-363, its review fixes merged in) | REVIEW (UNBUILT): `Character/OverCapRules.h` (3 per pillar, +5 % each); over-cap points in the run's levels; a point placed at 7 becomes one (refused at 3; MaxHP / EP recomputed); `GetEvolutionModified{Mind,Body,Spirit}` × `ScaleFor` (classic and GAS paths); buying stops only when every pillar is full; level screen and `wor.WorldStat.Print` show them; LevelDropsSpec cases; Rules ER-24c, PIE B-364. Not saved with the run (like the run levels). **Flag for Crown (Q-157):** the multiplier scales the whole pillar, so turn speed, the damage multiplier and the pillar's MaxHP / EP part rise too | none (Q-157 already answered) |
| B-234b | `feature/B-234b-host-hand-over-cloud` (**on** B-234) | REVIEW part 1 (UNBUILT): `AWoRGameState::HostOrder` (join order, replicated; `AWoRPlayerState::OnSetUniqueId` adds a joiner); `UWoRHostHandOverSubsystem` (on a lost host the next player adopts its copy and hosts, the others find and join its session; `wor.Coop.PrintSuccession`, `wor.Coop.SimulateHostDrop`); `USaveSubsystem::AdoptCheckpoint` (writes then go to the copy slot, never the new host's own); `UWoRSessionSubsystem::FindSessionIndexByOwner` / `OnSessionsFound`; `UPartySessionSubsystem::ForgetLocalParty`; `HostSuccessionSpec`; Rules PA-37h, PIE B-234b (3 standalone instances; the default map must be the hub). Planned hand-over at a rest spot + pool reset: new row **B-234c** (needs B-234b and B-187: not stacked yet) | **Q-234b = A, A, A**: join order (Halo 3 host migration), own save never written while hosting the copy (Borderlands 2 / Diablo III guest saves), 3 s × 10 search |

| B-235d (payout) | `feature/B-235d-private-memories-cloud` | Backlog note only: the full run-end payout to every player is B-073's (`RunRewards::PayRunEndToPlayers`) | — |
| B-235d (part 2, Crown's changed Q-235d) | `feature/B-235d-private-memories-cloud` | REVIEW (UNBUILT), reviewed twice and fixed: Crown changed Q-235d (after Shangri-La Frontier): a memory goes to the player whose character found it, another player only if it is for their character and they met it themselves, everyone's when it lists no character. `MemoryUnlock::UnlockFor` / `ShouldFriendReceive` / `ShouldHostReceive` / `GetRemotePlayers`; `MemoryConditions::PickReceivers` / `FilterParty` read each player's own part of an event; the host's journal only when the host found it (solo unchanged); a later finder still gets it; `wor.Memory.Sent`; spec cases; Q-235d, ER-12e, PIE 1-8. The first 'confirmed A' build (share on claim) was replaced. Left (part 3): a client's own map-spot / fragment / gear / reaction finds count as the host's; PriorMemory for a friend reads the host's journal; an earlier-run memory. Commits bea4cf2d and ec861bf7 do not build alone (the head should) | **Q-235d: Crown, 2026-11-08** |
| B-357b | `feature/B-357b-evolution-resolve-path-cloud` (**on** B-357) | REVIEW part 1 (UNBUILT): `Equipment/Crystals/EvolutionPlacement.h` (character / weapon / adapter; gear passives only at charge L2), `ULoadoutComponent::GatherAttachedEvolutions` (the one resolve path), the three effect readers on it (same effects as before), `wor.Loadout.PrintEvolutions`; `EvolutionPlacementSpec`; Rules CL-28b, PIE B-357b. The L2 action's passives, gear evolutions without stats / resistance and the ~200 remaining reads: new row **B-357c** | **Q-357b = A / A**: "infused at L2" is per action (Elden Ring's full-charge effects; the deckbuilder's WhenAttached rows), in parts |
| B-351d | `feature/B-351d-crystal-spell-lists-cloud` (**on** B-351c) | REVIEW part 1 (UNBUILT): a crystal's one spell list (`SpellPicks` / `bSpellSlotsActive` on the item; `GetSpellSlots` / `ReplaceSpell` / `RestoreSpell` on weapon and adapter entries; `GetAllSpells` follows once edited, so its readers do; `FGearInstance`, save (no bump), `CopyOwned`, `WriteLoadoutToOwned`); `wor.Loadout.PrintSpellSlots` / `ReplaceSpell` / `RestoreSpell`; `SpellSlotListSpec`; Rules CL-20c, PIE B-351d. Deletion after PIE, editor move of stones, gear screen: new row **B-351e** | none (Q-351d already answered) |

| B-187b | `feature/B-187b-mini-boss-rest-spots-cloud` (**on** B-187) | REVIEW part 1 (UNBUILT): a beaten mini-boss's place gets a rest spot (`UTrialRunSubsystem::MarkEncounterBeaten`, `ATrialRestSpot::SpawnForMiniBoss`); the rest menu's Revive (3 per run, free; the trial reloads); the revived team one tier step higher per revival for HP and level drops (`TrialGate::StepTier`, `ApplyEnemyRankHealth` TierSteps); `wor.Trial.PrintRevivals`; `MiniBossReviveSpec`; Rules ER-09c, PIE B-187b | **Q-187b = A×4**: 3 revivals, one tier step each, reload, co-op later (Hades' Heat, Elden Ring NG+, Darkest Dungeon) |
| B-187c | `feature/B-187c-revive-return-and-save-cloud` (**on** B-187b) | REVIEW part 1 (UNBUILT): after a revive the local player comes back where it rested (`ATrialGameMode::RestartPlayer`); marks and revivals saved (**save v24**); spec cases; PIE B-187c. Co-op press: new row **B-187d** (needs B-235c's map-feature RPC) | none |

| B-372 | `feature/B-372-camera-framing-checks-cloud` (**on** B-371) | REVIEW CC part (UNBUILT): `Combat/Camera/CameraFramingChecks.h` (on screen with a 5 % margin, line of sight and camera clearance on the camera channel, per-moment required points), `wor.Camera.CheckFraming 1` logs each shot; spec `WoR.Camera.Framing` (every code-built shot framed on an empty arena); Rules ED-42e, PIE B-372 (the screenshots are the editor step) | none |

| B-234c | `feature/B-234c-planned-host-hand-over-cloud` (**on** B-234b, **with B-187 merged in**) | REVIEW part 1 (UNBUILT): the rest menu's Hand over host (a listen-server host with another player); `BeginPlannedHandOver` (sync save → copies, `AWoRGameState::Multicast_BeginPlannedHandOver`, leave after 2 s, rejoin the next host); clients hand over as on a drop; HostSuccessionSpec case; PIE B-234c. Pool / party back to the player's own after a handed-over run, the order across travel: new row **B-234d** | none |

| B-357c | `feature/B-357c-gear-evolutions-passives-only-cloud` (**on** B-357b) | REVIEW part 1 (UNBUILT): **play change (D-147)**: an evolution on a weapon or adapter no longer adds pillar percent or resistance (`WoREvolutionPlacement::GivesStats`, the pillar layer and the resistance sum on the resolve path); spec cases; PIE B-357c. The L2 action's passives and the remaining old reads: new row **B-357d** | none (D-147) |

| B-348c | `feature/B-348c-broken-darkness-persists-cloud` (**on** B-348b) | REVIEW (UNBUILT): **Crown changed Q-348b**: Broken Darkness persists in and out of the run (`FPartyMember::bBrokenDarkness`, banked at fight end, seeded like a born-BD, saved v21 with B-348b); the run's strain carries fight to fight (`RunStrain`, `ApplyCarriedStrain`) and clears at the run's end; `BrokenDarknessPersistSpec`, SaveSpec case; PIE B-348c. The un-transform path (gap 4.5) still unbuilt | Crown's answer |

**Crown's answers applied (brain, 08:38-09:03 UTC; new commits, no force-push):**
- **Q-374** (B-374): stun / apply stun / skip turn / silence propose **Impairment**; **Manipulation** = changing what an enemy does (Random Skill now); Conjuration never flagged; `ProposeSchool`, SpellSchoolSpec, CL-42, Q-374.
- **Q-370b** confirmed (B-370); reference fight **Expedition 33, Maelle vs Painted Maelle** (camera behaviour only) in Q-370b, FeelGuide and B-157's row; **cinematic sequences optional** (big moments only) also in Q-371 / B-371's row (merged forward into B-372).
- **Q-369b changed** (B-369): three layers: the impact is the **skill's** (Impact entry; new `EVFXRole::CritImpact`, else the impact larger), the **element default** as fallback in a new `UImpactVisualDefaults` asset (`/Game/Data/Combat/DA_ImpactVisualDefaults`, editor step), the **defence outcome** the defender's cosmetic slot (CosmeticsData keeps only the guard spark / dodge whiff); ImpactVisualsSpec, ED-44, PIE (the notify advice), Q-369b; merged forward into B-231c2.
- **Q-337e** (B-337e): the deckbuilder's nested shape (trigger, then indented All of checks: `GetConditionTreeString`, `wor.Conditions.Tree`); Any of collapsed (`AdvancedDisplay`), kept for migrated OR rows; `wor.Conditions.ListAnyOf` as the sweep step (sweep row 46, PIE B-337e); ET-32b, Q-337e.
- **Q-357 part (1) changed** (B-357): frame + attachment costs (`WEAPON_FRAME_COST` 1, `ADAPTER_FRAME_COST` 0, `CRYSTAL_ATTACHMENT_COST` 1, `EVOLUTION_SLOT_COST` 2): evolved adapter back to **2**; **Generic budget 6 → 5** (D-110 note); ClassLimitsSpec / LoadoutBudgetSpec, CL-03 / CL-10 / CL-12, PIE B-357; merged forward into B-357b → B-357c. B-351b/c/d already had the evolved adapter at 2.
- **Q-357 final** (brain, 09:29 UTC; supersedes the line above where they differ): an evolution **alone** in the slot costs **3** (`EVOLUTION_ALONE_SLOT_COST`; was 2); on gear +2; no one-evolution-per-loadout rule. Specs (Refractor: evolution alone / evolved weapon fit; Resonator: 5 crystal adapters, weapon + 2 evolved adapters, evolution alone + evolved adapter fit), Rules CL-04 / CL-10 / CL-12, D-110, Q-357, PIE B-357 (data validation lists loadouts the 2 → 3 pushes over); merged into B-357b → B-357c.
- **Q-349** and **Q-355b** marked confirmed by Crown on their branches. **Q-348b** → B-348c above.

**Review (Crown-change commits: B-348c / B-357 / B-369 / B-337e), fixed:** B-348c must-fixes: `UBrokenDarknessManager::BeginPlay` no longer resets state (the character component's BeginPlay runs first and had already flipped the member and applied its strain), the spec builds the pawn in that order with a real manager; a break or revert writes the record at once; any BD character carries its absorption pool. B-357 must-fix: a Generic over its 5 (two evolved weapons, a budget-6 save) is cleared by dropping the secondary weapon; `GENERIC` reads `LOADOUT_TOTAL_BUDGET`; merged into B-357b → B-357c. B-369 must-fix: the impact defaults asset is cooked (an `ImpactVisualDefaults` AlwaysCook rule in DefaultGame.ini), kept in a `TStrongObjectPtr`, looked up once; path as a namespace constant; a Print button; the runner warns on a CritImpact notify; merged into B-231c2. B-337e: OR'd events read "When: any of the events below". **Open / flag for Crown:** a born Broken Darkness character still starts each fight in BD (its nature), so a revert lasts until its next spawn (noted in Q-348b); `RevertTransformation` still has no gameplay caller (gap 4.5).

**Review (B-372 / B-187c / B-234c), fixed:** B-234c must-fix: the hand-over call now rides each player's PlayerState after its copy's chunks (`Client_BeginPlannedHandOver`; the GameState multicast could arrive first and leave a partial copy); a failed save stops it; a level change during the wait drops it. B-372: Close / Wide styles and a blocking wall in the spec, the wind-up's height scales with the style (a Close wind-up framed the caster off screen), the PIE check ignores the framed actors, named clamps, Q-372. B-187c: the save spec goes through the bytes, the revivals print shows the stands, a PIE note that the revived fight may start at once.

**Review (B-351d / B-187b), fixed:** B-351d must-fixes: the spec's gem had durability 0 (read as broken: its lists were empty); a picked spell on a broken crystal now greys out (`GetSpellBlockedReason`, `IsSpellFromBrokenRing`); an adapter's picks need a spell source; a copy keeps its picks only on the same crystal; the inventory print shows spell picks. B-187b: only a team whose own rank is a mini-boss's leaves a rest spot (an elite caught with it doesn't); only the revived team's members get its tier steps; the orchestrator gets each enemy's steps at spawn (no run read mid-fight, D-064); merged forward into B-187c. Also: four PIE rows of this part had gone under **Done** (B-357b, B-351d, B-187b, B-187c): moved to Pending. **Open for the builder:** no console command for an adapter's spell list yet (weapon only); no spec of the spell picks' save round trip; the mini-boss rest spot is spawned on the server only and doesn't replicate (B-187d).

**Merge notes (third part):** B-073 → B-187 → B-363 → B-364 and B-187 → B-187b → B-187c both edit `TrialRunSubsystem.h/.cpp`, `TrialRestSpot.*` and `RestSpotWidget.*` (expect small conflicts merging the two lines); saves v23 (B-073) and v24 (B-187c) sit on top of v21 / v22 (B-348b / B-351b): renumber at merge. B-234b is on B-234 (on B-233); B-234c is on B-234b and merges B-187 (so it needs both stacks). B-187d (co-op rest press) would need B-187c + B-235c, which conflict in `TrialMapFeature.cpp` / `TrialRunSubsystem.cpp`: left unwritten for a merge on the desktop. B-357b on B-357. B-351d on B-351c (on B-351b).

**Review (B-234b / B-357b), fixed:** B-234b must-fixes: the successor leaves the dead session first and hosts after the delay (HostSession refused while one existed), and adopts the copy only once hosting succeeded (`OnSessionCreated`); the step resets so a later drop hands over again; a failed rejoin searches again (`OnSessionJoined`); the copy gets `HandlePostLoad` (D-060); the spec uses its own slot (`AdoptCheckpointFrom`), never the real copy. B-357b: LogWoR, a prefixed constant, the row's spec list narrowed. **Open for the builder:** after a seamless travel the join order is rebuilt as players arrive (TODO, B-234c); `bPlayingHostCopy` stays on for the session by design (the player's own save is untouched until a restart); `ForgetLocalParty` removes index 0 (fine until remote parties exist).

**Review (B-363 / B-364), fixed:** the rest-spot flag now ends when the rest menu or level screen closes (StepAway) and on a resumed run, and the form subsystem's older flag follows it; `TryBuyPoint` takes a wallet and the specs cover banking and the price spent; a Body sub-stat spec; a stray Backlog cell. **Open for the builder:** an over-cap or bought point while points still wait can exceed what fits (buying isn't capped by waiting points); refusal tests match English text.

---

# Cloud writer handover (2026-11-08, second part, after the brain widened the rules) — STOPPED: no cleanly writable job left

**Why it stopped:** every remaining row is blocked or needs the desktop: B-233b (EOS: Crown's EOS product ids, Q-233), B-234b (hand-over: needs B-187's rest spot; B-187 waits on B-073, BLOCKED on D-048), B-235d's payout (B-073), B-351d (crystal spell lists: ~69 reads, needs a compiler), B-357b (left for the desktop), B-127 (editor spot-check first), B-107-109 (Crown's B-102 review), and the editor / VRoid / profiler jobs. Every branch here is UNBUILT: build, run the specs, then PIE.

**Last review (B-235c / B-235d / B-231c / B-231c2), fixed:** B-235c only chests and the Prism pickup go to the host (lore / memory features showed and recorded on the host's side), chests key by player state; B-235d a memory is shared once, when the host's run newly unlocks it (the "even if the host had it" path never ran and FightTriggers re-sent it every fight); B-231c an attached effect falls back to the caster's spot. **Open for the builder:** telegraph tells and the Guard / Perfect Guard / Dodge bursts go by the Unreliable VFX multicast (a dropped one loses a defence cue: consider a reliable variant); check a client's chest item reaches its inventory on its screen; one B-235d docs commit touched 4 files (no history rewrite).

The brain (Crown's coordinating session) widened the rules at 06:37 UTC: stack on my own UNBUILT branches where a job depends on one, pre-write the co-op jobs (headless specs + PIE sheet), write the CC part of CC+UE jobs; still skip B-340 and B-042f. Slicing L / M jobs into part 1 + a new row, as before.

**Stacks (merge the base first; each later branch already contains its base):**
- B-228b → B-232b → B-232c → B-232d → B-232e (B-232c's and B-232d's review fixes are merged forward into the next branch).
- B-230 → B-231 → B-231b → B-231c → B-231c2 (B-231c2 also merges B-369: merge B-369 and B-231c first) ; B-231 → B-230b (a side branch off B-231).
- B-233 → B-234.
- B-235 → B-235b → B-235c → B-235d.
- B-351b → B-351c. B-370 → B-371.
- Merge notes: save versions v21 (B-348b) / v22 (B-351b) — renumber if dev moved; several branches append `docs/Rules/06-party.md` PA-37 rows and `docs/Architecture/TurnState.md` changelog rows (trivial conflicts); `AWoRPlayerState` gains RPCs on three stacks (B-232e, B-234, B-235b-d) in different spots; B-349 renames `wor.Camera` (B-370 / B-371 docs) and other commands.

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-232b | `feature/B-232b-menu-on-commander-cloud` (**on** `feature/B-228b-client-combat-hud-cloud`) | REVIEW part 1 (UNBUILT): `CommandMenuRules::OpensHere`; the host's menu opens only for members it commands; a client opens its own member's menu from `OnRep_TurnState`; TurnStateSpec case; Rules PA-37c, PIE B-232b. The rest is new row **B-232c** (run readers on `URunStateComponent::Get*`, a recruit fills a spot) | none |
| B-235 | `feature/B-235-personal-prism-drops-cloud` | REVIEW part 1 (UNBUILT): per-player Prism drops at one spot (`UTrialRunSubsystem::DropPrismsForPlayer` / `TakeDropForPlayer` / `GetTotalDroppedPrisms`), every player's wallet drops on a wipe, the pickup gives each their own and stays for the others; DefeatSpec case; Rules ER-12b, PIE B-235. Only the host's drop is saved (a client's run copy is B-234). The rest is new row **B-235b** | none |
| B-230 | `feature/B-230-replicated-status-bars-cloud` | REVIEW part 1 (UNBUILT): new replicated `UStatusBarStateComponent` on every `ACombatCharacter` (follows the buildup manager on the server); a client's character panel draws from it; StatusBarStateSpec; Rules PA-37d, StatusBuildup changelog, PIE B-230. Effects already reach clients (B-318). The rest (grid rows) is new row **B-230b** | none |
| B-233 | `feature/B-233-sessions-null-cloud` | REVIEW part 1 (UNBUILT): OnlineSubsystem / Null / Utils plugins in the .uproject, modules in Build.cs, `DefaultPlatformService=Null`; new `UWoRSessionSubsystem` (host with `?listen`, find, join, leave; `MAX_PLAYERS` 3; `wor.Session.Host/Find/Join/Print`); new doc `docs/Architecture/OnlineSessions.md`; Rules PA-37e, PIE B-233 (LAN on one PC). The rest (EOS, invites, a lobby screen) is new row **B-233b** | **Q-233**: Null now, EOS later (needs Crown's EOS product ids: a Crown action) |
| B-351c | `feature/B-351c-one-list-readers-cloud` (**on** `feature/B-351b-stone-socket-carry-cloud`) | REVIEW part 1 (UNBUILT): the readers were mostly done already (`GetAvailableAbilities` reads `FCombatLoadout`, on the one list since B-351; stone bonuses read the stone socket); `LogActiveLoadout` on the one list and both sockets, new `wor.Loadout.PrintActive`, SkillSlotListSpec case (a stone slot's pick reaches `FCombatLoadout::GetAllAbilities`). The crystal spell list (~69 `AssignedSpells` reads), the deletion, the editor move and the gear screen are new row **B-351d** | **Q-351d = A**: two parts, as Q-351 / Q-351c |
| B-371 | `feature/B-371-shot-patterns-cloud` (**on** `feature/B-370-cinematic-window-cloud`) | REVIEW (UNBUILT): `FSkillCameraTuning` on `USkillDataBase::CameraTuning`; `FCombatCameraRules::ComputeShot` / `BlendShot` / `UsesShotPatterns` / `ImpactMomentFor`; new `UActionExecutor::OnProjectileLaunched`; the director plays wind-up / Cast / Impact / Spread for a skill without its own `CameraSequence` (60 Hz timer only while a shot moves; `ImpactShake` class property); `TemplateSequences` OBSOLETE; CombatCameraSpec cases; Rules ED-42d, PIE B-371 (Crown approves the look = B-157). Reviewed; fixes pushed | none (Q-371 already answered) |
| B-278 | `feature/B-278-asset-manager-rules-cloud` | REVIEW CC part (UNBUILT): AlwaysCook rules for every scanned / path-loaded type (+ the loot pool's item types), `MapsToCook` (five levels), the player-form mesh directory; `CookRulesSpec`; new doc `docs/Architecture/Packaging.md` (PSO steps); PIE B-278 is a packaged-build check on the desktop | none |
| B-189 | `feature/B-189-element-map-look-cloud` | REVIEW CC part (UNBUILT): new `TrialMapLook` (per-element sky tint / fog / sun table, placeholder values), applied when the active trial's level loads; `wor.Trial.Look <Element>`; `TrialMapLookSpec`; Rules ER-10c, Weather changelog, PIE B-189 (screenshots) | none (placeholder colours, Crown tunes) |
| B-231 | `feature/B-231-actions-server-multicast-cloud` (**on** `feature/B-230-replicated-status-bars-cloud`) | REVIEW part 1 (UNBUILT): new replicated `UCombatPresentationComponent` on every `ACombatCharacter`: the server's montages (executor legs, item, defense) and each resolved hit (`OnHitShown`, reliable) multicast to clients; `wor.Net.PrintPresentation`; `CombatPresentationSpec`; Rules PA-37f, PIE B-231. Projectiles / VFX / telegraphs / damage-number widget: new row **B-231b** | none |
| B-234 | `feature/B-234-checkpoint-copies-cloud` (**on** `feature/B-233-sessions-null-cloud`) | REVIEW part 1 (UNBUILT): after each successful host save, the save (zlib, 2 KB chunks, max 128) goes by Client RPC on each remote `AWoRPlayerState`; the client reassembles (`FCoopCheckpointAssembler`), decompresses and keeps it in slot `WoRCoopCheckpoint`; `wor.Coop.PrintCheckpoint`; `CoopCheckpointSpec`; Rules PA-37g, PIE B-234. Hand-over: new row **B-234b** (needs B-187's rest spot) | none |
| B-232c | `feature/B-232c-run-readers-cloud` (**on** `feature/B-232b-menu-on-commander-cloud`) | REVIEW part 1 (UNBUILT): `ECombatFreeCommand`; a client's weapon / adapter switch, row step and "Let AI play" go through `ACombatPlayerController::RequestFreeCommand` → server checks like a pick → `Client_FreeCommandApplied` (the client's loadout copy follows); spec cases; Rules PA-37c2, PIE B-232c. Run readers / recruit / row replication: new row **B-232d** | none |
| B-230b | `feature/B-230b-grid-slot-replicated-cloud` (**on** `feature/B-231-actions-server-multicast-cloud`) | REVIEW (UNBUILT): new replicated `UCombatGridSlotComponent` on every `ACombatCharacter`; `UCombatGridSubsystem` publishes each change and its position reads fall back to it (a client's empty grid); Move / Swap only for held actors; a client's menu greys the row step; `wor.Grid.PrintSlots`; `GridSlotSpec`; Rules PA-37d2, PIE B-230b | none |
| B-235b | `feature/B-235b-personal-rewards-cloud` (**on** `feature/B-235-personal-prism-drops-cloud`) | REVIEW part 1 (UNBUILT): the B-235 leftovers: `Client_ShowFeatureMessage` (the pickup message on the picker's screen), `WoRDefeat::GetPresentDroppedPrisms` (a departed player's drop no longer holds the pickup); DefeatSpec case; Rules ER-12c, PIE B-235b. Rewards (instanced loot, level drops, memories, payout): new row **B-235c** | none |
| B-232d | `feature/B-232d-run-readers-cloud` (**on** `feature/B-232c-run-readers-cloud`, B-232c's review fixes merged in) | REVIEW part 1 (UNBUILT): `URunStateComponent` replicates found features, run memories and waiting levels (`PullRunProgress` on each `UTrialRunSubsystem` change); readers `HasFoundFeature` / `GetRunMemories` / `GetUnspentLevels`; the chest hint, memory spot / fragment and level-drop screen use them; RunStateSpec case; Rules PA-37c3, PIE B-232d. Recruit: new row **B-232e** | none |
| B-232e | `feature/B-232e-recruit-ai-seat-cloud` (**on** `feature/B-232d-run-readers-cloud`) | REVIEW (UNBUILT): a run recruit is an AI seat in a networked session (`PartySeats::RecruitIsAISeat`, D-150 #3); a client's recruit E-press goes to the host (`AWoRPlayerState::ServerRecruit` → `InviteForClient`: invites only, in a run, the pawn in the trigger); TrialRecruitSpec case; Rules PA-37c4, PIE B-232e | none (D-150 decides it) |
| B-231b | `feature/B-231b-cosmetic-projectiles-cloud` (**on** `feature/B-231-actions-server-multicast-cloud`) | REVIEW CC part 1 (UNBUILT): `FCosmeticProjectileLaunch`; `ASkillProjectile::MakeCosmeticLaunch` / `InitializeCosmetic` (no collision, bursts only); `UCombatPresentationComponent::SendProjectile` multicasts each launch and clients fly a copy; spec case; Rules PA-37f2, PIE B-231b. Impact visuals / telegraphs / stance montages / damage-number widget: new row **B-231c** | none |
| B-231c | `feature/B-231c-client-telegraphs-cloud` (**on** `feature/B-231b-cosmetic-projectiles-cloud`) | REVIEW CC part 1 (UNBUILT): `FCosmeticVFX`; every VFX the runner spawns (`SpawnVFXArrayEntry`: cast / ritual effects, telegraph tells) is multicast (Unreliable) and spawned on clients attached to the same socket or at the spot, tinted; spec case; Rules PA-37f3, PIE B-231c. Left in the row: B-369 impact visuals (after B-369 merges), stance montages, damage-number widget (editor) | none |
| B-235c | `feature/B-235c-instanced-chests-cloud` (**on** `feature/B-235b-personal-rewards-cloud`) | REVIEW part 1 (UNBUILT): chests per player (D-103: `GetFeatureIdFor` keys and seeds by player; the host / solo roll unchanged), `ShowMessageTo`; a client's map-feature E goes to the host (`AWoRPlayerState::ServerInteractFeature`, `IsWithinReach`); TrialMapFeatureSpec case; Rules ER-12d, PIE B-235c. Rest: new row **B-235d**. Also fixed on B-235b: `ATrialMapFeature::ShowMessage` was protected (compile error) | none (D-103 decides it) |
| B-231c (part 2) | `feature/B-231c2-impact-visuals-clients-cloud` (B-231c **+ B-369 merged in**) | REVIEW (UNBUILT): `ImpactVisuals::SpawnForImpact` sends its burst through the target's `SendVFX`, so the outcome bursts show on clients. Merge it after both B-369 and B-231c | none |
| B-235d | `feature/B-235d-private-memories-cloud` (**on** `feature/B-235c-instanced-chests-cloud`) | REVIEW part 1 (UNBUILT): `MemoryUnlock::QualifiesFor` / `ShareWithPlayers`; `AWoRPlayerState::Client_UnlockMemory` (a friend's own journal, private notice); MemorySourcesSpec case; Rules ER-12e, PIE B-235d. Payout waits on B-073 (BLOCKED, D-048) | **Q-235d = A**: by the memory's CharacterDepth, everyone when it lists no one (BG3 / DOS2 origin beats) |
| B-127, B-107-109, B-121/122, B-142/143, B-206 | not started | **Skipped:** B-127's first step is an editor spot-check (FString to FText on authored assets, ~270 reads); B-107-109 wait on Crown's B-102 review; the rest are editor / VRoid / profiler jobs | — |
| B-187 / B-363 / B-364 | not started | **Blocked:** B-187 depends on B-073 (BLOCKED, D-048); B-363 on B-187; B-364 on B-363 | — |
| B-357b | not started | **Left for the desktop:** "only while that item is infused at level 2" has no item state behind it (infusion is a per-action charge today), so the gear rule needs a model decision with the code, and the one resolve path touches ~220 reads: best done with a compiler | — |

**Review fixes (second part):** B-232b: the menu check needs a roster entry on both sides; a refused client pick reopens the client's menu (`Client_MenuPickRefused`); the client's menu turn is forgotten at fight end; the menu's weapon switch / move row / "Let AI play" still act locally on a client: listed on B-232c. B-235: the pickup message shows only for this machine's player, the hint shows only this machine's own amount, `TakeDropForPlayer` clears the spot with the last drop, "nothing taken" is Verbose; leftovers on B-235b. B-230: no 0 / 0 bar before the first update, new `wor.Status.Bars`. Review (B-232d / B-232e / B-231b), fixed: B-232d the readers use the GameState copy only on a client (host / solo read the run: a spent memory spot came back), the copy's OnRep refreshes map features on a client (`RefreshFromRun`), resets notify; B-231b a copy needs no target and always ends (life span, default speed); B-232e PIE notes. Review (B-232c / B-230b / B-235b), fixed: B-232c the client's copy resets its turn switch before following the server (must-fix: the 2nd switch drifted) and on each new turn, a refused hand-off reopens the menu; B-230b replicated slots read only on a client, clear empties first, one shared row-edge text; B-235b placement vs removal rule documented, the departed-player drop moved to B-235c. Earlier review (B-278 / B-189 / B-231 / B-234), fixed: B-278 CharacterForm AlwaysCook (listed by type); B-189 a look doesn't stack intensity, baked suns skipped; B-231 hits Reliable, cached component; B-234 compressed + capped copies (a big save would overflow the reliable buffer), sent only after the host's write, LogWoR. B-351c: `wor.Loadout.PrintActive` renamed `wor.Loadout.Print` (D-139), same character lookup as the *Skill commands. B-371: the Cast shot once per pattern, the impact shot unfollowed, a sequence stops a held pattern, skip-after-hit spares the first frame, no half-started pattern. B-233: the LocalUserNum overloads (no net-id dereference), Host refuses while a session exists, one call of each kind at a time, a refused join clears its handler, hosting uses `OpenLevel(..., "listen")` (seamless ServerTravel opens no socket), `Deinitialize`, `wor.Session.Leave`, new `WoRSessionSpec`.

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
