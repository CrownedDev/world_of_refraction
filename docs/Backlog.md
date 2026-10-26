# Backlog — the single work queue

**What this is:** every planned job, in order, small enough for one branch (≤3 files per commit), each with a "done when" check. Claude Code works top-down through it (`.claude/skills/work-backlog`). Crown reviews branches, play-tests and answers decisions.

**Sources (don't re-plan from them; plan here):**
- `analysis/CodebaseReview_2026-10.md` (§5 bugs, §5.4 DRY)
- `analysis/ArchitectureReview_2026-10.md` (§2 simplification, §3 order)
- `Design/GASMigration.md`
- `Design/UI_ScreenInventory.md`, `Design/UI_DeckbuilderReuse.md`
- `Gaps/Roadmap.md`, `TODO.md` (historical; this file supersedes them as the queue)

**Status values:**
- `TODO`
- `IN PROGRESS (branch)`
- `REVIEW (in dev)`: done by Claude, built + tested + reviewed, merged into `dev`; waiting for Crown's PIE pass (`docs/PIE/B-xxx.md`) and the `dev → main` merge. Counts as met for jobs that depend on it.
- `BLOCKED (D-xxx | B-xxx | Crown)`
- `DONE (commit)`

**Who:**
- **CC:** Claude Code.
- **Crown:** needs a judgement call or a play-test.
- **CC+UE:** Claude Code with the Unreal editor open and unreal-mcp connected (asset work). Skipped while the editor is closed.

**Rules:**
- Pick the **first `TODO` whose dependencies are `DONE`** and whose decisions aren't `OPEN`.
- A blocked job never stops the run: mark it, skip it, take the next.
- Phase 0 jobs marked Crown gate everything that needs a build; until they're done, CC works only on jobs that don't touch the files of the two pending branches.

---

## Phase 0 — Foundations (unlocks verified, autonomous work)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-001 | Build + PIE `fix/combat-hang-arc-a`; merge to main | Crown | S | — | Merged; a win resets status bars; a stunned actor's turn passes (superseded by B-061: a stunned actor now acts, Attack/Defend only) | REVIEW (in dev) |
| B-002 | Merge `dev` into `refactor/bp-logic-to-cpp`, resolve the orchestrator conflict | CC | S | B-001 | Branch builds clean (Crown confirms) | REVIEW (branch `refactor/B-002-merge-dev`; B-003 merges it) |
| B-003 | Build + cut over `refactor/bp-logic-to-cpp` (follow `Refactor/BPLogicToCpp_ReviewNotes.md`); merge | Crown | M | B-002 | Native HUD, results, hub PC working in PIE | BLOCKED (B-002) |
| B-004 | Enable **CommonUI** + **GameplayAbilities** plugins; commit `.uproject` | Crown | S | — | Plugins on; project opens | TODO |
| B-005 | Fill `UE_ROOT` (local env var) and run `Scripts/ue/build.sh` + `test.sh` once | Crown | S | — | Both scripts work locally; hooks active | DONE (local env, 2026-10-26) |
| B-006 | Log categories: `LogWoR`, `LogWoRCombat`, `LogWoREffects`, `LogWoRAI`, `LogWoRLoadout`, `LogWoRUI`, `LogWoREconomy`; new header + cpp | CC | S | — | Declared and defined; used by new code; `LogTemp` migrated file-by-file when touched | REVIEW (in dev) |
| B-007 | First Automation Spec tests: DamageCalculator (Pattern-P, defense clamp, crit), TierGap/RequirementGap scaling, TurnManager ordering | CC | M | B-005 | `test.sh` runs them headless and they pass | REVIEW (in dev) |
| B-008 | `#if !UE_BUILD_SHIPPING` around all `FAutoConsoleCommand`s and test actors' bodies | CC | S | — | Shipping config compiles without them | TODO |
| B-094 | Guard false positive: `Scripts/claude/guard.sh:22` matches `rm` inside any word (so a read-only `git ls-tree -r … Content/…` was blocked). Match `rm` only as a command word, e.g. `(^|[;&|[:space:]])rm[[:space:]]` | CC | S | — | `git ls-tree -r HEAD Content` passes; `rm -rf Content/x` is still blocked | TODO |
| B-009 | Defense timing → world time (`DefenseSystem.cpp:68,212,422`) | CC | S | — | Window survives pause; matches the old timing in PIE | TODO |
| B-010 | Per-battle `FRandomStream` (crit, BD, AI, random targets), seeded at combat start, seed logged | CC | M | B-001 | Same seed gives the same fight in a test | TODO |
| B-011 | Add the 11 missing Architecture docs to CLAUDE.md's list; fix the stale `InnateElement` gotcha | CC | S | — | CLAUDE.md matches `docs/Architecture/` | DONE (this branch) |

## Phase 1 — Simplify & DRY (low-risk, mostly not blocked)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-020 | Stat-modifier table: `ESubStat → {Buff, Debuff, Modify}` + `GetNetModifier(Actor, Stat)`; replace the ~20 hand-written Buff−Debuff pairs | CC | M | B-007 | `wor.CompareStats` identical before/after | TODO |
| B-021 | Stone row table (`{Type, Stat, Buff/Debuff, Label}`); fold the 14 `Execute*BuffEffect` wrappers and the parallel switches | CC | M | — | Every stone gives the same result (test) | TODO |
| B-022 | `GetActiveLoadout()` returns const ref; `LoadoutComponent::CharacterClass` becomes a getter over CharacterData | CC | S | — | Compiles; 25 call sites unchanged in behaviour | TODO |
| B-023 | AI uses the live enum values: `Stun` (not `SkipTurn`), `CleanseSelf`/`CleanseAllies` (not `Cleanse`) | CC | S | B-001 | AI detects stun; picks cleanse spells | TODO |
| B-024 | Fold WeaponManager + RingManager into LoadoutComponent | CC | S | — | Two subsystems gone; same behaviour | TODO |
| B-025 | EconomyService: one shared preamble helper (null/authority/components) | CC | S | — | 18 copies gone | TODO |
| B-026 | InventoryComponent weapon/ring attach/remove → one template | CC | M | — | ~500 → ~250 lines; tests for attach/remove | TODO |
| B-027 | One authority check (`HasServerAuthority` ×2 + raw `HasAuthority`) | CC | S | — | Single helper used everywhere | TODO |
| B-028 | One loadout validator (three today) over a slot view + 2 adapters | CC | L | B-007 | Same findings on the test inventory | TODO |
| B-029 | One AI damage estimator, resolving element like execution | CC | M | B-001 | AI estimate equals executor result in a test | TODO |
| B-030 | `ResetForCombat()` on every per-fight subsystem, called from `ReleaseCombatResources` | CC | M | B-001 | No state survives between two fights (test) | TODO |
| B-031 | Delete `SubmitAction` sync router + `ApplyDamage` fallback | CC | M | B-001 | Only the async path remains; rituals still fire | TODO |
| B-032 | ActionExecutor DRY: one per-impact resolve, one reflect block, merge `ApplyHit`/`ApplyDamage` | CC | L | B-031 | Same damage in a test fight | BLOCKED (B-031) |
| B-033 | God-file splits (move-only): Orchestrator debug → `Debug/Combat/`; ActionExecutor montage runner + cost resolution | CC | M | B-003 | Move-only diff; compiles | BLOCKED (B-003) |
| B-034 | Delete the dead `InfusionChargeManager`; infusion level held on the pending action, not the VFX component | CC | M | B-003 | VFX still shows level; ChargeManager gone | BLOCKED (B-003) |
| B-035 | Flatten `ApplyBetweenCombatCrystalDestruction` (nesting 8) | CC | S | B-001 | Same results; nesting ≤4 | TODO |

## Phase 2 — GAS migration (see `Design/GASMigration.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-040 | GAS stage 1: native GameplayTags + enum↔tag maps + `wor.CompareTags` | CC | M | B-004 | Tag queries equal enum queries | BLOCKED (B-004) |
| B-041 | GAS stage 2: AttributeSets + ASC on `ACombatCharacter`; getters read attributes; `wor.CompareStats` | CC | L | B-040, B-020 | Identical stats snapshot | BLOCKED (B-040) |
| B-042 | GAS stage 3: turn tracker, effect-family GE classes + spec builder, SkillEffectManager → ASC facade; `wor.CompareEffects` | CC | L | B-041 | Effect tests + snapshot identical | BLOCKED (B-041) |
| B-043 | GAS stage 4: abilities per delivery shape, montage/event tasks, costs, rituals | CC | L | B-042 | Every skill plays identically in PIE | BLOCKED (B-042) |

## Phase 3 — Combat & element correctness

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-060 | Item use reports real success (`ItemExecutor.cpp:169`) | CC | S | — | Failed use doesn't consume a charge | TODO |
| B-061 | Stun rule per D-001 | CC | S | D-001 | Matches the answer | REVIEW (in dev) |
| B-092 | Command menu while stunned (D-001): offer only Attack + Defend. The pie menu has no Defend option today, so add one (`EPieMenuCategory`, `CombatCommandMenuSubsystem::BuildMainMenuButtons`) | CC | S | B-061 | A stunned player sees only Attack and Defend; Defend works from the menu | TODO |
| B-093 | Key bindings for **Defend, Infusion and Infusion level** (Enhanced Input actions, keyboard + gamepad) alongside the menu (Crown, 2026-10-26: menu for now, keys later). Fills the commented-out `InfusionAction`/`InfusionLevelAction` placeholders in `CombatPlayerController.h:51-71`; fits CommonUI input routing | CC | M | B-092 | Each key works on your turn (and on gamepad); menu still works | TODO |
| B-062 | Effect identity: per-actor fires-once, instants recorded, refresh vs stack per D-012 (or via GAS stage 3) | CC | M | D-011, D-012 | Tests for each rule | BLOCKED (D-011) |
| B-063 | Wind SkipTurn enforced per D-014; BD-Darkness `DrainEnergy` case; Void `RandomSkill` minimal | CC | M | D-014 | Each element cap does something | BLOCKED (D-014) |
| B-064 | Element damage per D-005; `OnDamageDealt` carries element | CC | M | D-005 | Damage numbers can tint by element | BLOCKED (D-005) |
| B-065 | Cleanse vs gear effects per D-015 | CC | S | D-015 | Test | BLOCKED (D-015) |
| B-066 | Aura element inheritance for buff types; Cleanse payload wired (Roadmap Design Gaps) | CC | M | — | `DA_Spell_Aura` and `DA_Spell_Cleanse` do something | TODO |
| B-067 | Small fixes: TurnManager null checks, DoubleHit totals, corpse return-anim, anim-wait hang, GI deref | CC | M | B-001 | Each with a regression test or PIE note | TODO |

## Phase 4 — Gameplay loop

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-070 | `TrialRunSubsystem::ActiveTrial` → strong ref | CC | S | — | Works in a packaged build | TODO |
| B-071 | Party into battle per D-003/D-004: spawn party-list members at encounter start with their own `FPartyMember.PawnClass`; per-member Player/AI control (leader always Player); keep the sphere gather for co-op players | CC | M | D-003, D-004 | 3v1 fight from a recruited party, one member on AI | TODO |
| B-076 | Find characters during a trial: reuse the hub recruit flow in a trial; what happens to them after the run per D-026 | CC | M | B-071, D-026 | Recruit in a trial, they fight in the next encounter | BLOCKED (D-026) |
| B-072 | Win/lose routing per D-002 | CC | S | D-002, B-003 | Matches answer | BLOCKED (B-003) |
| B-073 | Prisms/currency rewards per D-006 | CC | S | D-006 | Shop purchasable after a win | BLOCKED (D-006) |
| B-074 | Encounter Composition Arc 2: `UEncounterData`, multi-enemy battles, AI formation | CC | L | B-001 | 1v3 fight from data | TODO |
| B-075 | Recruit duplicate fix; currency clamp; shop soft-lock; hint cleanup on EndPlay | CC | S | — | Each verified | TODO |

## Phase 5 — Gear

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-080 | Generic weapon-crystal spells per D-007 | CC | S | D-007 | Matches answer | BLOCKED (D-007) |
| B-081 | Gear effects re-gathered on swap per D-008 | CC | M | D-008 | Ring switch changes effects | BLOCKED (D-008) |
| B-082 | Ring stones per D-009 | CC | S | D-009 | Matches answer | BLOCKED (D-009) |
| B-083 | Resonator ring rules per D-010 | CC | S | D-010 | Validator enforces | BLOCKED (D-010) |
| B-084 | Dismantle equipped per D-016 | CC | S | D-016 | Matches answer | BLOCKED (D-016) |
| B-085 | Loadouts reference owned gear by `PersistentID` (no copies) | CC | L | B-090 | Wear/dismantle stay in sync | BLOCKED (B-090) |

## Phase 6 — Persistence

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-090 | Save/persistence keystone: `USaveGame`, stable IDs, two-phase load; character state survives hub ↔ trial ↔ battle | CC | L | B-041 | Quit and reload keeps inventory, currency, party, HP | BLOCKED (B-041) |
| B-091 | Pool: account vs character per D-020; wire or drop `UPoolSubsystem` | CC | M | D-020 | One inventory model | BLOCKED (D-020) |

## UI rebuild — CommonUI, mockups first (D-024)

Runs alongside the phases above. **Per screen group:** mockup (CC, no editor needed) → Crown approves → build on CommonUI (CC+UE) → Crown PIE → retire the old widgets. The old UI keeps working until its replacement passes PIE. Gameplay logic stays in C++ (`refactor/bp-logic-to-cpp`, B-002/B-003); the new widgets sit on those C++ bases.

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-100 | Mockup tool per D-025: plain + annotated 1280×720 drafts drawn from the deckbuilder kit (`UI_DeckbuilderReuse.md` §5), versioned `vN/` | CC | S | D-025 | One test draft renders | TODO |
| B-101 | Mockup the **combat HUD**: party cards, turn-order strip, command diamond (incl. Defend), target selection, enemy plate, status icons, damage numbers, BD gauges (screens 31–40, Expedition 33 layout, `UI_DeckbuilderReuse.md` §1–2). Note the open header-bar pick | CC | M | B-100 | Plain + annotated drafts committed; listed for Crown in `docs/sessions/<today>.md` | BLOCKED (B-100) |
| B-102 | Approve the combat HUD mockup (or ask for a vN+1) | Crown | S | B-101 | Approved version noted here | BLOCKED (B-101) |
| B-103 | CommonUI base layer in C++: root layout with layer stacks (game / menu / modal), activatable screen base, button base with the four jobs (play / confirm / danger / navigate), input routing for keyboard + gamepad | CC | M | B-004, B-003 | Builds; an empty test screen opens and closes with Back; tests for the stack | BLOCKED (B-004) |
| B-104 | Import the kit pieces the approved mockup uses (`SourceArt/` + `Content/UI/Vendor/`, `UI_DeckbuilderReuse.md` §4), fonts, CommonUI styles; `CREDITS.md` entries | CC+UE | M | B-102 | Assets in the Content Browser with the §4 import settings | BLOCKED (B-102) |
| B-105 | Build the combat HUD on CommonUI from the approved mockup, alongside the old one (switchable) | CC+UE | L | B-103, B-104 | Fight playable on the new HUD; PIE file written | BLOCKED (B-103) |
| B-106 | Retire the old combat widgets (after Crown's PIE pass of B-105) | CC+UE | S | B-105 DONE | Old widgets gone; fight still works | BLOCKED (B-105) |
| B-107 | Mockup **results + loot + retry** (44–46), then repeat B-102→B-106 for them | CC | M | B-102 | As B-101 | BLOCKED (B-102) |
| B-108 | Mockup **pause + settings + confirm dialog** (17, 43, 6, 8), then repeat B-102→B-106 | CC | M | B-102 | As B-101 | BLOCKED (B-102) |
| B-109 | Mockup **hub HUD + shop + party** (10–13), then repeat B-102→B-106 | CC | M | B-102 | As B-101 | BLOCKED (B-102) |

Later groups follow `UI_ScreenInventory.md` order; CC adds their rows when B-109 is approved.

## Later (not scheduled)
- Co-op: replicated combat state, intent RPCs, `ServerTravel`, arena as streamed level instance (`ArchitectureReview_2026-10.md` §1.6, §1.9).
- Real-time field abilities (movement, first strike) on GAS (`GASMigration.md`).
