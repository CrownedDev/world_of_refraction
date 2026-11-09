# Backlog: finished rows

Rows moved out of [`Backlog.md`](Backlog.md) once finished (`DONE`, `DROPPED`, `SUPERSEDED`, `REVERTED`; D-132, B-340). `REVIEW` rows waiting on PIE stay in the backlog.

## Phase 0 — Foundations (unlocks verified, autonomous work)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-005 | Fill `UE_ROOT` (local env var) and run `Scripts/ue/build.sh` + `test.sh` once | Crown | S | — | Both scripts work locally; hooks active | DONE (local env, 2026-10-26) |
| B-011 | Add the 11 missing Architecture docs to CLAUDE.md's list; fix the stale `InnateElement` gotcha | CC | S | — | CLAUDE.md matches `docs/Architecture/` | DONE (this branch) |

## Phase 1b — DRY audit 2 (`analysis/DryAudit_2026-10-28.md`; Crown: DRY is a priority)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-243 | **A2: one active-weapon / ring rule** (`GetActiveWeapon` `:2528` vs `GetActiveWeaponLoadout` `:3179`; asset getters read entry getters; one "find the active loadout" preamble for the 49 copies in `LoadoutComponent.cpp`); `IsValid()` vs `!= nullptr` made one check | CC | S | — | Specs: each getter pair agrees in every class / slot case | SUPERSEDED (D-110 unified loadout; if its UNBUILT branch builds green, merge it first as the safety net) |
| B-244 | **A1: one "which gear counts" visitor** (`ForEachContributingGear`, `LoadoutComponent.cpp:4082, 4241, 4343, 4434, 4516`, debug `SkillEffectManager.cpp:520`); stats, resistances and effects read the same set (**Q-244**) | CC | M | B-243 | Spec per class: the same gear feeds stats, resistances and effects | SUPERSEDED (D-110 unified loadout; if its UNBUILT branch builds green, merge it first as the safety net) |
| B-257 | **Obsolete code with no job yet** (left by B-148, B-167, B-202, B-216, Q-159, B-118) and the dead `EnsureResultResolved` (`ActionExecutor.cpp:1770`): list each with the PIE check that unlocks its deletion; delete only what's already PIE-passed | CC | S | — | A list in the audit doc; deletions only after PIE | DONE (docs only): `DryAudit_2026-10-28.md` §E lists each piece with the PIE pass that unlocks it; none of those passes is done yet, so nothing was deleted |

## Phase 1d — Simplify elements and effects (`analysis/SimplifyAudit_2026-10-28.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-270 | **S4: one infusion-source resolver** (`ResolveInfusionSource`, replacing 5 drifted switches); a broken ring or crystal gives no element (CL-24) through every path; one evolution lookup; `WeaponStatsApply` / `DoWeaponStatsApply` (no callers) flagged obsolete | CC | M | B-243, B-245 | Spec: every source option resolves the same in menu, AI and execution; broken gives none | SUPERSEDED (D-110 unified loadout; if its UNBUILT branch builds green, merge it first as the safety net) |

## Phase 1e — Rules pinned by tests (`analysis/RulesTraceability_2026-10-29.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-281 | **Rules-page hygiene**: renumber the duplicate ED-42 (ED-42b); flip the stale "not built / Conflict" rows shipped by B-225, B-045, B-090, B-085 / B-091, B-076 (ET-11, ET-33/34, ET-50, ER-04, ER-17, ER-40..42, PA-15..17); PA-08 says up to three enemies; function-name citations instead of stale line numbers on pages 02 and 04; add rule rows for D-101 (weapon Power) and D-102 (level drops) | CC | S | — | Index count matches; no stale rows | DONE (docs only): `chore/B-281-rules-hygiene` merged; ED-42b, stale rows flipped after checking the code or spec, function citations on pages 02 / 04, ED-43 (D-101) and ER-61 (D-102), index 276 rules; left for a later pass: ER-15 / ER-50 / ER-53 / ER-56 re-status and the line citations on 01 / 03 / 05 / 06 |

## Owned-items store (D-111, `analysis/InventoryArchitecture_2026-10-29.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
| B-340 | **Docs moves** (D-132): `docs/Mechanics/` folds into `docs/Rules/` or goes to `docs/archive/`; the May overviews to the archive; `DesignDecisions.md` split into open and archive files; finished backlog rows to `Backlog_Done.md` (REVIEW rows waiting on PIE stay); links fixed | CC | S | — | No broken relative links; Rules is the only rule source | DONE (docs only): `chore/B-340-docs-moves` (2026-11-09) |
|---|---|---|---|---|---|---|
| B-315 | **Store 4: owner on the entry** (re-scoped by D-117): the owning member lives on the store entry (the wearer is always the owner; B-321 retires the wearers map); the shared chest is an entry location (Q-321) | CC | M | B-314 | Specs green; one instance per item | DROPPED (D-122: the owner is the inventory component itself) |
| B-331 | **One Speed** (D-123): Action Speed and Spell Speed merge into Speed (the sub-stat, the defense-window maths, `StatModifierTable`, the stone kind; D-088's Ice → Speed) | CC | M | B-330 | Specs: one Speed feeds both windows; old assets map both speeds via `GetTotalSpeed` (saves are B-331b) | REVERTED (B-331r, D-151, 2026-11-04: the two speeds stay separate; the merge's effect undone by a new commit). Was REVIEW (in dev): built on the desktop 2026-11-04 (doc-only dev merge); slice 1 of 2 (Q-331): `UCharacterData::Speed` (Body) = the old Action + Spell Speed points, one window factor for attacks and spells; the stone kind, the effect pair and play-rate are B-331b |
| B-331b | **One Speed, part 2** (D-123, Q-331): play-rate on Speed; `ESubStat::Speed` appended; one Speed stone (old two map to it); one Speed buff/debuff pair (Ice and Water slow it, D-088 wording); one gear `BonusSpeed`; `StatModifierTable`, GAS attributes, `AddPillarPercent`, the tie-break, UI names; assets and gear fold the old points into Speed (save version bump, D-138); the old fields stay OBSOLETE until the sweep; `StatModifierTable` and the GAS attributes get Speed; a spec case for the gear bonuses and the stones feeding the window (review) | CC | M | B-331 | Specs: one stone, one effect and one gear field each feed play-rate and the window; old gear and saves read as Speed | DROPPED (D-151, 2026-11-04: the two speeds stay separate; every item here was the speed merge) |

## Phase 3 — Combat & element correctness

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-221b | **SUPERSEDED by D-117 / B-321** (gear is per character, so no member wears another's piece). Was: Q-091 changed by Crown, part 2 (split from B-221): a saved loadout naming a piece another member wears **keeps it and shows that slot empty** until it's free (no strip); every reader of the active loadout skips such a piece (weapon resolution, stats, `ValidateLoadout`, save); then retire the clash-on-activate step (`ClaimActiveSharedGear` strip, the party panel clash line) after PIE. Also the trial-recruit gear button (`UPartyControlWidget::CycleRunGear`) skips pieces another member wears | CC | M | B-221 | Spec: member B's saved loadout names A's sword; B's slot reads empty; A takes it off; B's slot has it again without re-picking | SUPERSEDED (D-117, B-321) |
| B-095 | Resistance table structure: move `ClassInnateResistanceTable` (hard-coded C++) into a designer-editable data asset, same values; debug print button | CC | M | B-216 | Same numbers in tests before/after; editable in the Details panel | DONE (folded into B-216: `UResistanceProfileData`, Details-panel buttons Print / Compare / Reset) |

## Phase 4 — Gameplay loop

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-072 | Win/lose routing per D-002 | CC | S | D-002, B-003 | Matches answer | DROPPED (D-155 replaces D-002's lose half; the loss routing is B-362) |
| B-116 | D-021 run start: **buy your loadout from your pool** with a starting Gold budget; unspent Gold carries into the run; slot limits (1 weapon / 2 rings / 1 evolution early). Budget and price curve as named constants `// TODO: tune`. Update `Resources_Design.md` §draft | CC | L | B-073, B-091 | Test: a run starts with only the bought gear and the leftover Gold | DROPPED (D-137: the run loadout is free; B-347) |

## Phase 5 — Gear

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-083 | D-010/D-027: correct the stale class comment in `ECharacterClass.h` (Resonator budget of 5, no same-element cap; Generic/Refractor slots as in D-027). Comment-only | CC | S | — | Comment matches D-027 and the code | DONE (already fixed by B-117, 476c92ad; checked 2026-10-26, nothing to play-test) |
