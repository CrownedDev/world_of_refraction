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
| B-008 | `#if !UE_BUILD_SHIPPING` around all `FAutoConsoleCommand`s and test actors' bodies | CC | S | — | Shipping config compiles without them | REVIEW (in dev) |
| B-094 | Guard false positive: `Scripts/claude/guard.sh:22` matches `rm` inside any word (so a read-only `git ls-tree -r … Content/…` was blocked). Match `rm` only as a command word, e.g. `(^|[;&|[:space:]])rm[[:space:]]` | CC | S | — | `git ls-tree -r HEAD Content` passes; `rm -rf Content/x` is still blocked | REVIEW (in dev) |
| B-046 | **Game rules reference (docs only):** one rules reference for this game in `docs/Rules/`, shaped like the deckbuilder's `docs/reference/` (index page + one page per topic: classes and loadouts, elements and damage, status bars, effects and turns, economy and runs, party). Gather every rule from `DesignDecisions.md` (answered/decided), the design docs and the code (cite file:line); where they disagree, raise a D-xxx, never pick. CLAUDE.md points to it as this game's rules entry point | CC | M | — | Index + pages written; every rule cites its source; conflicts listed as decisions | REVIEW (in dev) |
| B-047 | **Best-practice audit (research only):** compare each area with Epic's guidance and the **Lyra** sample: data assets (naming, validation), save/versioning, player-facing text (FText, localisation-ready), input (Enhanced Input + CommonUI), Content naming/folders, test coverage gaps, performance, co-op readiness. Write `docs/analysis/BestPractice_2026-10.md`: per area, what we do, what the standard does, the gap, a recommendation. Each fix → a backlog job; each judgement call → a D-xxx | CC | M | B-046 | Doc written; jobs and decisions added | IN PROGRESS (`docs/B-047-best-practice-audit`) |
| B-009 | Defense timing → world time (`DefenseSystem.cpp:68,212,422`) | CC | S | — | Window survives pause; matches the old timing in PIE | REVIEW (in dev) |
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
| B-027 | One authority check (`HasServerAuthority` ×2 + raw `HasAuthority`) **B-047 adds:** also `EconomyService`'s raw `HasAuthority`. | CC | S | — | Single helper used everywhere | TODO |
| B-028 | One loadout validator (three today) over a slot view + 2 adapters | CC | L | B-007 | Same findings on the test inventory | TODO |
| B-029 | One AI damage estimator, resolving element like execution | CC | M | B-001 | AI estimate equals executor result in a test | TODO |
| B-030 | `ResetForCombat()` on every per-fight subsystem, called from `ReleaseCombatResources` | CC | M | B-001 | No state survives between two fights (test) | TODO |
| B-031 | Delete `SubmitAction` sync router + `ApplyDamage` fallback | CC | M | B-001 | Only the async path remains; rituals still fire | TODO |
| B-032 | ActionExecutor DRY: one per-impact resolve, one reflect block, merge `ApplyHit`/`ApplyDamage` | CC | L | B-031 | Same damage in a test fight | BLOCKED (B-031) |
| B-033 | God-file splits (move-only): Orchestrator debug → `Debug/Combat/`; ActionExecutor montage runner + cost resolution | CC | M | B-003 | Move-only diff; compiles | BLOCKED (B-003) |
| B-034 | Delete the dead `InfusionChargeManager`; infusion level held on the pending action, not the VFX component | CC | M | B-003 | VFX still shows level; ChargeManager gone | BLOCKED (B-003) |
| B-035 | Flatten `ApplyBetweenCombatCrystalDestruction` (nesting 8) | CC | S | B-001 | Same results; nesting ≤4 | TODO |
| B-117 | Stale-text sweep found by B-046 (`docs/Rules/` CL-C4, CED-6, C3-5, C4-10, Cp-1, Cp-5): comments and docs the rulings already settle (class comment and `GetClassDescription`, `Classes.md`/`Generic.md`, `FusionStones.md`, `Loadout.md` evolution cost, `Mechanics/README.md` gear cap, `RequirementGap.md` StatusMultiplier pillar, removed requirement penalty and Evolution backlash comments, `FRingInventoryEntry.h:155`, InfusionConstants wrong numbers, `StatusBuildup.md` drain, `TurnManager.md`, `PartySystem.md`, `EncounterComponent.h:3-8`); drop the unread InfusionConstants and ring caps. Comment/doc only | CC | S | B-046 | Each listed text matches the code and rulings; build + tests unchanged | TODO |

## Phase 2 — GAS migration (see `Design/GASMigration.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-040 | GAS stage 1: native GameplayTags + enum↔tag maps + `wor.CompareTags` | CC | M | B-004 | Tag queries equal enum queries | BLOCKED (B-004) |
| B-041 | GAS stage 2: AttributeSets + ASC on `ACombatCharacter`; getters read attributes; `wor.CompareStats` | CC | L | B-040, B-020 | Identical stats snapshot | BLOCKED (B-040) |
| B-044 | **Effects overhaul review (research only, no code):** write `docs/analysis/EffectsOverhaul.md` comparing (1) our effect system (`SkillEffectManager`, `FSkillEffect`, `SkillEffectSystem.md`), (2) the deckbuilder's effect model (`wor-deckbuilder/docs/reference/04-effects.md`, `05-triggers-conditions.md`: verb, trigger, condition, Lasts, stacks) and (3) **GAS GameplayEffects** as the standard (duration policy, periodic, stacking by source/target + refresh policy, tags, modifiers, execution calcs). Crown (2026-10-26): use the standard where it already does the job, don't reinvent it; custom code only for real gaps (e.g. turn-based durations). Also cover what D-031's nested descriptions need from the data (trigger → nested conditions → effects). End with a feature table (ours / deckbuilder / GAS / gap) and a plan for B-042, plus the open questions for Crown as D-028 | CC | M | — | Doc written; D-028 raised with recommendations | TODO |
| B-045 | Description generator per D-031: item/skill text as the deckbuilder's tree (trigger → nested conditions → effects) and short form, generated from data; live-lighting query for fights; tests from the deckbuilder's text fixtures **B-047 adds (if D-061 is B):** generated text is `FText` built from `FText::Format` patterns kept in one place. | CC | M | B-044 | Tests: same data gives the same text; condition lines report held / not held | BLOCKED (B-044) |
| B-042 | GAS stage 3: turn tracker, effect-family GE classes + spec builder, SkillEffectManager → ASC facade; `wor.CompareEffects` | CC | L | B-041 | Effect tests + snapshot identical | BLOCKED (B-041) |
| B-043 | GAS stage 4: abilities per delivery shape, montage/event tasks, costs, rituals | CC | L | B-042 | Every skill plays identically in PIE | BLOCKED (B-042) |

## Phase 3 — Combat & element correctness

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-060 | Item use reports real success (`ItemExecutor.cpp:169`) | CC | S | — | Failed use doesn't consume a charge | TODO |
| B-061 | Stun rule per D-001 | CC | S | D-001 | Matches the answer | REVIEW (in dev) |
| B-092 | Command menu while stunned (D-001): offer only Attack + Defend. The pie menu has no Defend option today, so add one (`EPieMenuCategory`, `CombatCommandMenuSubsystem::BuildMainMenuButtons`) | CC | S | B-061 | A stunned player sees only Attack and Defend; Defend works from the menu | REVIEW (in dev) |
| B-110 | Menu closed after a refused player action: `ConfirmActionWithTargets` calls `Close()` after `SubmitConfirmedAction`. An invalid player action makes `SubmitActionAsync` re-request synchronously (`CombatOrchestrator.cpp:518-522`), so that `Close()` shuts the menu that was just reopened. Drop the trailing `Close()` (`HandleActionExecuted` already closes) or close before submitting. Found in the B-092 review | CC | S | — | A refused pick (e.g. a stunned actor's spell via a stale menu) leaves the re-opened menu usable | TODO |
| B-093 | Key bindings for **Defend, Infusion and Infusion level** (Enhanced Input actions, keyboard + gamepad) alongside the menu (Crown, 2026-10-26: menu for now, keys later). Fills the commented-out `InfusionAction`/`InfusionLevelAction` placeholders in `CombatPlayerController.h:51-71`; fits CommonUI input routing | CC | M | B-092 | Each key works on your turn (and on gamepad); menu still works | TODO |
| B-062 | Effect identity: once-per-match per character (D-011); capped-status effects refresh from the same source, add from a different one (D-012); instants recorded | CC | M | D-011, D-012, D-051, D-053, D-055 | Tests for each rule | BLOCKED (D-051, D-053, D-055) |
| B-063 | Status caps per D-014/D-030: every full bar lasts 2 of the holder's own turns (shared turn-count helper); Wind pushes the holder's next two turns down; Water reduces healing (not blocks); BD-Darkness `DrainEnergy` case; Void `RandomSkill` minimal | CC | M | D-014, D-030, D-051, D-055 | Tests: 2-holder-turn duration; Wind delay; Water heal reduction | BLOCKED (D-051, D-055) |
| B-112 | A full **Light** bar does nothing: its cap resolves to `CritChanceDebuff`, which `ApplyTriggeredSkillEffect` does not handle (`SkillEffectManager.cpp:2476`, default at `:2555`). D-030 lists Light = crit down. Handle it (2 of the holder's turns, with B-063's counter) | CC | S | B-063 | Test: a full Light bar lowers crit chance for 2 holder turns | BLOCKED (B-063) |
| B-113 | Status Multiplier counted twice on infused hits? `GetChargeStatusMultiplier` already includes it (`ActionExecutor.cpp:4696-4719`) and `AddStatusBuildup` multiplies again (`StatusBuildupManager.cpp:389-394`), against the header's "callers that include it skip". Confirm in a test, then apply it once | CC | S | — | Test: an infused hit's buildup equals base × charge × Status Multiplier once | TODO |
| B-114 | AI buildup estimate uses the authored value: `UAbilityData::CalculateStatusBuildup` ignores `StatusBuildup` and uses 5 per hit (`AbilityData.cpp:82-94`); the real hit uses the authored value. Make the estimate read the same number (or fold into B-029) | CC | S | — | Test: the AI estimate equals the buildup a real hit adds | TODO |
| B-064a | Ice per D-029: append `Ice` damage type (Water-keyed, no crystals/rings/innate); resistance + trigger reach per the deckbuilder; Ice bar cap → timed turn-speed debuff (Freeze). Survey every element switch first | CC | M | B-064 | Tests: Ice skill needs a Water source; Ice Res resists Water; full Ice bar slows turns | BLOCKED (B-064) |
| B-064 | Element damage per D-005 (C): the resistance table also scales damage via `ElementMultiplier`; `OnDamageDealt` carries element | CC | M | D-005, D-036 | Tests: weak/resistant/neutral; damage numbers can tint by element | BLOCKED (D-036) |
| B-095 | Resistance table structure: move `ClassInnateResistanceTable` (hard-coded C++) into a designer-editable data asset, same values; debug print button | CC | M | B-064 | Same numbers in tests before/after; editable in the Details panel | BLOCKED (B-064) |
| B-065 | Cleanse per D-015: cleanse and Iolite skip effects that came from gear (origin mark), yours or the enemy's | CC | S | D-015 | Tests: cleanse keeps own gear effects; Iolite keeps enemy gear buffs | TODO |
| B-066 | Aura element inheritance for buff types; Cleanse payload wired (Roadmap Design Gaps) | CC | M | — | `DA_Spell_Aura` and `DA_Spell_Cleanse` do something | TODO |
| B-067 | Small fixes: TurnManager null checks, DoubleHit totals, corpse return-anim, anim-wait hang, GI deref | CC | M | B-001 | Each with a regression test or PIE note | TODO |

## Phase 4 — Gameplay loop

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-070 | `TrialRunSubsystem::ActiveTrial` → strong ref | CC | S | — | Works in a packaged build | TODO |
| B-071 | Party into battle per D-003/D-004: spawn party-list members at encounter start with their own `FPartyMember.PawnClass`; per-member Player/AI control (leader always Player), switchable on the member's turn (D-026); a Generic/Resonator with no gear is left out (D-016); keep the sphere gather for co-op players | CC | M | D-003, D-004, D-044, D-045 | 3v1 fight from a recruited party, one member on AI | BLOCKED (D-044, D-045) |
| B-076 | Find characters during a trial (D-026): reuse the hub recruit flow; they join for that run only and arrive ungeared; gear them from the shared run inventory | CC | M | B-071 | Recruit in a trial, gear them from the run inventory, they fight in the next encounter and are gone after the run | BLOCKED (B-071) |
| B-072 | Win/lose routing per D-002 | CC | S | D-002, B-003 | Matches answer | BLOCKED (B-003) |
| B-073 | Rewards per D-006 (A): each win grants Gold (run-only); Prisms + in-run upgrade materials tallied and paid at run end, win or lose, scaled by progress and difficulty; one payout multiplier (default 1.0) for later passives | CC | M | D-006, D-046, D-048 | Tests: payout on win and on failure, nothing banked mid-run; shop purchasable after a run | BLOCKED (D-046, D-048) |
| B-096 | Passive effects that raise the run-end payout (gear / evolution passives feed B-073's multiplier) | CC | S | B-073 | A passive changes the payout in a test | BLOCKED (B-073) |
| B-074 | Encounter Composition Arc 2: `UEncounterData`, multi-enemy battles, AI formation | CC | L | B-001 | 1v3 fight from data | TODO |
| B-075 | Recruit duplicate fix; currency clamp; shop soft-lock; hint cleanup on EndPlay | CC | S | — | Each verified | TODO |
| B-115 | D-013: **durability and absorption carry over** between fights in a run. Only HP/EP are saved today, so wear and the +10 repair never reach the next fight (`docs/Rules/05-economy-and-runs.md` Cx-3) | CC | M | B-001 | Test: crystal durability and BD absorption after fight 1 are what fight 2 starts with | TODO |
| B-116 | D-021 run start: **buy your loadout from your pool** with a starting Gold budget; unspent Gold carries into the run; slot limits (1 weapon / 2 rings / 1 evolution early). Budget and price curve as named constants `// TODO: tune`. Update `Resources_Design.md` §draft | CC | L | B-073, B-091 | Test: a run starts with only the bought gear and the leftover Gold | BLOCKED (B-073) |

## Phase 5 — Gear

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-080 | Skill damage type per D-007: Physical by default, the infusion source's element when infused; fix infused abilities using innate instead of the source (`ActionExecutor.cpp:1450`) | CC | M | D-007, D-037 | Tests: Generic + fire crystal deals Fire; uninfused deals Physical | BLOCKED (D-037) |
| B-097 | D-027 cleanup: find callers of the unused class gates `CanUseAbilities`/`CanUseSpells` (`ECharacterClass.h`, `CharacterData.h`); remove or flag them. No behaviour change: classes keep their slots and rules | CC | S | B-080 | No caller left, or each one listed in the hand-off; tests unchanged | BLOCKED (B-080) |
| B-098 | Weapons stop giving spells (D-027): survey weapon spell slots + weapon-crystal spells in data/saves, then the crystal keys the weapon's abilities instead | CC | L | B-097 | Survey in the hand-off first; no weapon offers a spell; saved loadouts still load | BLOCKED (B-097) |
| B-099 | Generic + evolution keeps its secondary weapon (D-027): `HasSecondaryEquipment` / `HasRingInSecondary` return false when the primary is an evolution (`LoadoutComponent.cpp:2469`, `:2503`), against `FCombatCapabilities.cpp:206`, which already expects "Evolution + Secondary Weapon". Make every path agree | CC | S | — | Test: Generic with an evolution primary and a secondary sword attacks and uses the sword's abilities | TODO |
| B-081 | Gear effects follow a mid-fight swap per D-008: on weapon toggle / ring switch, remove + disarm the old gear's effects, apply + arm the new | CC | M | D-008 | Tests: ring switch changes effects; swapping back doesn't stack | TODO |
| B-082 | Ring fusions per D-009: the stat-stone half of an elemental fusion on a ring applies its attached self-passive (today stone reads go through the weapon only) | CC | S | D-009 | Test: DefenseStone + Fire crystal ring raises Defense and keeps its Fire spells | TODO |
| B-083 | D-010/D-027: correct the stale class comment in `ECharacterClass.h` (Resonator budget of 5, no same-element cap; Generic/Refractor slots as in D-027). Comment-only | CC | S | — | Comment matches D-027 and the code | TODO |
| B-084 | Dismantle equipped per D-016: confirm warning, then remove from every loadout and dismantle; Generic/Resonator with no gear is left out of the fight; the warning says upgrades are lost | CC | S | D-016 | Tests: no ghost copy after dismantle; gearless Generic refused at fight start | TODO |
| B-085 | Loadouts reference owned gear by `PersistentID` (no copies) | CC | L | B-090 | Wear/dismantle stay in sync | BLOCKED (B-090) |

## Phase 6 — Persistence

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-090 | Save/persistence keystone: `USaveGame`, stable IDs, two-phase load; character state survives hub ↔ trial ↔ battle **B-047 adds:** build on `ULocalPlayerSaveGame` (version stamp + `HandlePostLoad` upgrade switch); async save at fight/trial exit; asset refs saved as `FPrimaryAssetId`; first step registers the primary asset types and fixes the `Map` scan path in `DefaultGame.ini`; `wor.PrintSave` debug command. | CC | L | B-041, D-060 | Quit and reload keeps inventory, currency, party, HP | BLOCKED (B-041, D-060) |
| B-091 | One shared pool per player (D-020): characters equip from it, no per-pawn copy (`InitializeFromPool`); one item, one wearer; per-character gear presets drawn from the pool, clashes flagged on the party screen | CC | M | D-020 | Tests: party of 3 with one sword has one sword | TODO |

## UI rebuild — CommonUI, mockups first (D-024)

Runs alongside the phases above. **Per screen group:** mockup (CC, no editor needed) → Crown approves → build on CommonUI (CC+UE) → Crown PIE → retire the old widgets. The old UI keeps working until its replacement passes PIE. Gameplay logic stays in C++ (`refactor/bp-logic-to-cpp`, B-002/B-003); the new widgets sit on those C++ bases.

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-100 | Mockup tool per D-025: plain + annotated 1280×720 drafts drawn from the deckbuilder kit (`UI_DeckbuilderReuse.md` §5), versioned `vN/` | CC | S | D-025 | One test draft renders | REVIEW (in dev) |
| B-101 | Mockup the **combat HUD**: party cards, turn-order strip, command diamond (incl. Defend), target selection, enemy plate, status icons, damage numbers, BD gauges (screens 31–40, Expedition 33 layout, `UI_DeckbuilderReuse.md` §1–2). Draw all four header-bar candidates plus a crystal-themed one (D-017); lean into crystal themes (D-024) | CC | M | B-100 | Plain + annotated drafts committed; listed for Crown in `docs/sessions/<today>.md` | REVIEW (in dev) |
| B-102 | Approve the combat HUD mockup. **v1 approved as the layout for now (Crown, 2026-10-26): "could be more crystal-themed, but fine for the time being".** The crystal pass and header-bar pick come later in v2 (B-111). Reality badge: **indigo**; Void stays **violet** | Crown | S | B-101 | v1 noted here | DONE (v1, 2026-10-26) |
| B-111 | Combat HUD mockup **v2** (Crown's v1 review, 2026-10-26): draw the **four header-bar candidates plus a crystal-themed one** (D-017, `UI_DeckbuilderReuse.md` §3); **lean harder into the crystal look** (crystal light, faceted shapes, element gems, D-024); the stunned chip reads **"Stunned · 2 turns"** (D-030: 2 of the holder's own turns). **Broken Darkness on the party card (Crown, 2026-10-26):** no separate pips or gold bar; the EP value shows the absorption stacks as a multiplier, "89 (x3)" (stacks = `CurrentAbsorptionStacks`, max 3), and **overload sits on the EP bar itself** (bar in the absorbed element's colour up to full, dark intensifying variants past full: D-024 note) (it *is* EP above max, one field `CurrentEP`, up to 1.30 × MaxEP: `BrokenDarkness.md` Energy). Sera is BD so her element reads **Darkness**, not Void. Reality badge **indigo** crystal, Void **violet** (Crown, 2026-10-26). **Not urgent** (Crown: v1 is fine for now): do it after the code jobs ahead of it, before B-104's asset import | CC | M | B-101 | `combat/hud/v2/` plain + annotated drafts committed; B-102 back to Crown | TODO |
| B-103 | CommonUI base layer in C++: root layout with layer stacks (game / menu / modal), activatable screen base, button base with the four jobs (play / confirm / danger / navigate), input routing for keyboard + gamepad | CC | M | B-004, B-003 | Builds; an empty test screen opens and closes with Back; tests for the stack | BLOCKED (B-004) |
| B-104 | Import the kit pieces the approved mockup uses (`SourceArt/` + `Content/UI/Vendor/`, `UI_DeckbuilderReuse.md` §4), fonts, CommonUI styles; `CREDITS.md` entries | CC+UE | M | B-102 | Assets in the Content Browser with the §4 import settings | TODO |
| B-105 | Build the combat HUD on CommonUI from the approved mockup, alongside the old one (switchable) **B-047 adds (if D-061 is B):** menu and HUD text through `LOCTEXT`/`FText::Format` (plurals), no `FText::FromString` on literals. | CC+UE | L | B-103, B-104 | Fight playable on the new HUD; PIE file written | BLOCKED (B-103) |
| B-106 | Retire the old combat widgets (after Crown's PIE pass of B-105) | CC+UE | S | B-105 DONE | Old widgets gone; fight still works | BLOCKED (B-105) |
| B-107 | Mockup **results + loot + retry** (44–46), then repeat B-102→B-106 for them | CC | M | B-102 | As B-101 | TODO |
| B-108 | Mockup **pause + settings + confirm dialog** (17, 43, 6, 8), then repeat B-102→B-106 | CC | M | B-102 | As B-101 | TODO |
| B-109 | Mockup **hub HUD + shop + party** (10–13), then repeat B-102→B-106 | CC | M | B-102 | As B-101 | TODO |

Later groups follow `UI_ScreenInventory.md` order; CC adds their rows when B-109 is approved.

## Best-practice follow-ups (B-047, `docs/analysis/BestPractice_2026-10.md`)

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-118 | Drop the hard-coded test-asset fallbacks in the orchestrator's debug commands: the four `LoadObject` calls in `CombatOrchestrator.cpp:1970,2053,2391,2569` (one points at a file not in git) become a logged error. Flag first, remove after PIE | CC | S | — | No `/Game/` string paths outside test actors; a debug command with no skill set logs an error instead of loading a test asset | TODO |
| B-119 | Run the `DataValidation` commandlet in `Scripts/ue/test.sh` (fail on errors) | CC | S | — | A deliberately broken DA fails `test.sh`; current content passes, or its errors are listed | TODO |
| B-120 | Data-asset validation and debug buttons: `IsDataValid` for `UTrialData`, `UCosmeticsData`, `UStanceData`, `UInfusionDisplayData`; CallInEditor Print buttons on CharacterData, SkillDataBase, InventoryData, TrialData, StanceData, CosmeticsData via the existing `*Debug` libraries (CLAUDE.md rule) | CC | S | — | Each validates in the editor; Details panel shows Debug > Print; Output Log shows the dump | TODO |
| B-121 | Untrack the 566 orphaned `Content/__ExternalActors__`/`__ExternalObjects__` files (`git rm --cached`) and narrow `.gitignore:16-17` to the vendor/template paths. First confirm in the editor that no tracked map uses one-file-per-actor | CC+UE | S | — | `git ls-files Content/__External*` empty (or only tracked maps' actors); all maps open with their actors | TODO |
| B-122 | Content duplicate/junk cleanup in the editor (Reference Viewer, then Fix Up Redirectors): duplicate hub/combat game modes, the second `BP_CommonInputData`, both `NewBlueprint`, `Data/Testing/` merged into `Testing/`. Covers `Roadmap.md:98`, `Resources_Design.md:1250` | CC+UE | S | — | One of each remains; maps and PIE work; no redirectors left | TODO |
| B-123 | `docs/Conventions/ContentNaming.md`: one-page prefix and folder convention (Epic's list + `DA_<Kind>_`, `IA_`/`IMC_`, `L_` for new maps, one `Testing/` root); CLAUDE.md links it | CC | S | D-058 | Doc exists and matches D-058's answer | BLOCKED (D-058) |
| B-124 | Save tags cover item identity: `FWeaponInventoryEntry`/`FRingInventoryEntry` tag only ID/tier/quality (`FWeaponInventoryEntry.h:59-72`), not the asset, crystal, spells or rolls; fix the tags (or drop them per D-059) and add a round-trip test through an `ArIsSaveGame` archive | CC | S | D-059 | Test: a rolled weapon round-trips with the same asset, crystal, rolls and GUID | BLOCKED (D-059) |
| B-125 | Settings save split: per-machine `UGameUserSettings` subclass + per-player `ULocalPlayerSaveGame` (the Lyra split) | CC | S | B-108 | An options change survives a restart | BLOCKED (B-108) |
| B-126 | Player-facing names that work in cooked builds: `GetDisplayNameTextByValue` returns raw enum names once metadata is stripped (`CharacterPanelWidget.cpp:627`, `SpellData.cpp:178`, `SkillEffectDisplayNames.h:283`). One explicit `FText` name table (where it lives per D-062) for `ESpellElement`/`ESkillEffectType` (+ `SkillEffectDisplayNames`, `ItemIdentity::GetTypeName`), with a test | CC | S | D-062 | Test: every element and status has a non-empty name that is not its enum identifier | BLOCKED (D-062) |
| B-127 | Asset display text to `FText`: `Name`/`Description` on `EquipmentDataBase`, `SkillDataBase`, `CharacterData` and the smaller assets (`EquipmentDataBase.h:64,70`, `SkillDataBase.h:125,128`, `CharacterData.h:119,146`); readers updated; ≤3 files per commit. Spot-check one asset first that authored text survives | CC+UE | M | D-061 | Authored names survive in the editor; build + tests pass; no asset shows "Unnamed" | BLOCKED (D-061) |
| B-128 | CommonUI glyph assets: keyboard + one gamepad controller data, `UCommonInputPlatformSettings` with a matching default gamepad, Click/Back actions set | CC+UE | S | B-103, B-104 | Glyphs switch between keyboard and gamepad on the test screen | BLOCKED (B-103, B-104) |
| B-129 | Input tidy-up: named constants for mapping-context priorities; drop the hard-coded `ConstructorHelpers` asset path in `AExplorationPlayerController` (config soft path instead) | CC | S | B-003 | No literal priorities or `/Game/` input paths in C++ | BLOCKED (B-003) |
| B-130 | `docs/Design/CoopStateOwnership.md`: per system, where its state lives today, who should own it in co-op (server / GameState / PlayerState / client), and what breaks first (battle `OpenLevel` drops a client; fight state in GameInstance subsystems; client defense input never reaches the server) | CC | S | — | Doc covers every GameInstance subsystem with per-fight state | TODO |
| B-131 | `UCharacterDataComponent` actually replicates: `SetIsReplicatedByDefault(true)`; MaxHP/MaxEP replicated or documented as derived; note in code that the `Server*` functions are not RPCs | CC | S | B-027, D-065 | Test or PIE note: HP is replicated on a listen client | BLOCKED (B-027, D-065) |
| B-132 | Replace the `GetFirstPlayerController`/`GetPlayerController(…, 0)` lookups (pool, economy, rewards) with the instigating controller or the party leader | CC | M | B-030 | No player-0 lookups outside UI and debug code; rewards go to the instigator in a test | BLOCKED (B-030) |
| B-133 | Test harness: `FWoRTestWorld::SpawnCombatantWithLoadout`; bring `docs/Architecture/AutomationTests.md` up to date (6 specs, 35 tests) | CC | S | — | Helper used by one spec; doc lists every spec | TODO |
| B-134 | `WoR.Effects` baseline spec: record today's apply / stack / refresh / tick-down / expiry / clear behaviour and the re-entrancy regression, so B-042 and B-062 have an "identical before/after" to compare against | CC | M | B-133 | Spec passes on `dev`; listed in AutomationTests.md | BLOCKED (B-133) |
| B-135 | Specs for status buildup (fill, resistance, drain, cap), defense matching (bands, consume, multi-impact) and the grid (rows, movement) | CC | M | B-133 | Three specs pass on `dev` | BLOCKED (B-133) |
| B-136 | `WoR.Infusion.Cost` spec: charge levels and commit costs for spells and abilities (pins today's numbers; D-035 may change them) | CC | M | B-133 | Spec passes; documents the x2.25/x3.0 ability case | BLOCKED (B-133) |
| B-137 | `WoR.Economy` spec: cart cost, upgrade/downgrade refund round-trip, dismantle | CC | S | B-133 | Spec passes on `dev` | BLOCKED (B-133) |
| B-138 | Weapon mesh follows a new `ULoadoutComponent::OnActiveWeaponChanged` delegate; drop the 10 Hz polling tick (`WeaponMeshComponent.cpp:14-31`) | CC | S | — | No tick on the component; the mesh still swaps on weapon switch (PIE) | TODO |
| B-139 | Anim instance cleanup: remove the function-local `static int32 LogCounter` and the dead `Printf` (`CombatAnimInstance.cpp:46-68`), cache the loadout component; remove the empty `InfusionVFXComponent::TickComponent` | CC | S | — | No mutable statics or per-frame lookups in the anim update; build + tests pass | TODO |
| B-140 | Async preload of equipped skills' VFX at combat start, released at combat end; no `LoadSynchronous` at cast (`ActionExecutor.cpp:3452-3454, 3511, 3620, 5320, 5827`) | CC | M | B-030, B-142 | First cast has no load hitch (PIE / Insights); `LoadSynchronous` only as a logged fallback | BLOCKED (B-030, B-142) |
| B-141 | Per-hit logs to `Verbose` on `LogWoRCombat` (`ActionExecutor.cpp:584-591, 1744, 2635-2644` and the like) | CC | S | B-006 | A normal fight logs no per-hit Warning/Display lines | TODO |
| B-142 | Unreal Insights baseline of the demo fight (CPU frame, game thread, loads), saved under `docs/analysis/` | Crown | S | — | Trace summary committed | TODO |

## Later (not scheduled)
- Co-op: replicated combat state, intent RPCs, `ServerTravel`, arena as streamed level instance (`ArchitectureReview_2026-10.md` §1.6, §1.9).
- Real-time field abilities (movement, first strike) on GAS (`GASMigration.md`).
