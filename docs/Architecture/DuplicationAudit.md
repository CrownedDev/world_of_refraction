# Duplication Audit — full-project, system-by-system

**Date:** 2026-06-16. **Supersedes all prior contents** (fresh rebuild on `feature/realtime-defense`). The
previous 8-pass audit is preserved in git history (commit `588ffc4e`) and recoverable if needed.

**Method.** Per system, two axes: **DEFINITION-side** dup (same data/threshold authored in parallel places)
and **CHECK/LOGIC-side** dup (same computation re-implemented instead of called). Findings are **symbol-led**
(name + `file:line`). The high-value catch is **hand-mirrored logic that must stay manually in sync** —
especially an AI path re-implementing a player-path calc — ranked first.

**Survey only.** No source edits. Consolidation is a separate per-item job after triage. Items flagged
**VERIFY — may be intentional** are *not* recommended for merge. Crown's standing no-merge decisions
(value-collisions, the USpellData/UAbilityData/UWeaponAttackData type split, deliberate asymmetries) are
excluded.

Confidence legend: **SAFE** (clear mechanical dup, low risk) / **VERIFY** (real dup but extraction needs
care or may be intentional) / **RISKY** (cross-system or semantically entangled).

---

## Pass 1 — Defense / real-time windows

| # | Duplicated symbol / logic | Sites | Why it's a real dup | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| 1 | **The per-impact spell resolve block** (drain → `ResolveImpactDefense` → re-find `Ctx` → `if(Ctx){ StashHitFlags; capture bDefended; ApplyOneImpact; tally record/abort }` → `CloseDefenseWindow`) | `SpawnAOEEffect` `ActionExecutor.cpp:3520–3545`; `ResolveInstantSpell` `:3622–3629`; `OnProjectileImpact` `:3847–3866` | **The code itself says so** — `ResolveInstantSpell`'s comment: *"Byte-identical to SpawnAOEEffect's converted branch"* (`:3603`). AOE & Instant are line-for-line identical; projectile differs only by `ImpactOrdinal` (vs `0`), the `bLastArrival` close gate, and `GetCurrentSkillData()` (vs `Spell`). **Proven drift-risk:** the B1 stash and the B2 tally each had to be edited into all three identically. | Extract `ResolveSpellImpact(Target, ImpactOrdinal, CastEntryIndex, Caster, Skill, bCloseNow)` that all three call. | 3 sites, same system, **sync-critical** | **VERIFY** (projectile's burst-specifics — ordinal/`bLastArrival` — need parameterizing) |
| 2 | **"Fully defended" predicate** `bResolvedSuccess && (ResolvedDefenseType == Parry \|\| == Dodge)` | `AllInterruptSucceeded` `:2473–2474`; AOE record `:3565–3566`; Instant record `:3667–3668`; projectile record `:3929–3930` | The 3 spell `bDefended` lines are byte-identical (B2-tally introduced); `AllInterruptSucceeded` inlines the same predicate. It encodes the *definition of "the defender fully avoided this hit"* — add a new fully-avoiding defense type and all 4 must change together. | Extract `static bool IsFullDefense(EDefenseType, bool bSuccess)` (free fn) or `FPendingDefenseContext::WasFullyDefended()`. | 4 sites, same system, **sync-critical** | **SAFE** |
| 3 | **Split-fraction / slice math** `bUseSplit ? Split[i]/100 : even(+remainder)` | `ResolveImpactDefense` BaseSlice `:1896–1900`; `ApplyOneImpact` damage slice `:1937–1943`; `ApplyOneImpact` BuildupFraction (B0) `:1949` | **Comment admits the mirror:** `ResolveImpactDefense:1885` — *"Same FloorToInt + 0.01 epsilon as ApplyOneImpact so the split math is identical on both sides."* Three computations of the same `bUseSplit`/`Split[i]/100`/even-fallback fraction over different totals (base vs reduced vs buildup). | Extract `ComputeImpactFraction(ImpactIndex, HitCount, Split)` (returns the [0,1] share); keep `FloorToInt+epsilon` at the call site or a `SliceOf(total, fraction)` companion. | 3 sites, same system, **sync-critical** | **VERIFY** (the totals differ; extract only the *fraction* logic, not the whole slice) |
| 4 | **Guarded resolved-difficulty fetch** `Table.IsValidIndex(i) ? Table[i] : FDefenseDifficultyTriple()` | melee `:5288–5291` (`ResolvedDifficulty[Index]`); AOE `:3524–3527`, Instant `~:3612`, projectile `:3831–3834` (all `ResolvedCastDifficulty[CastEntryIndex]`) | Same guard-then-default-to-Easy idiom, 4 sites. Mostly folds out if #1 is extracted (the spell three collapse to one); melee remains a 2nd site. | `GetResolvedDifficultyAt(const TArray<FDefenseDifficultyTriple>&, int32)` helper. | 4 sites (→2 after #1), same system | **SAFE** |
| 5 | **`EnsureResultResolved`** — stale parallel resolver | `ActionExecutor.cpp:1821–1859` | Explicitly marked *"OBSOLETE (Stage 3): superseded by `ResolveImpactDefense` — no live callers"* (`:1817`). A second implementation of "compute the defense result + stash on context," kept un-deleted pending PIE-verify. Dead parallel code, not a live dup. | Delete after confirming zero callers (the incremental-removal "remove once proven" step). | 1 site, dead | **VERIFY** (confirm no callers before removal) |

### Checked and CLEAN (no action)
- **AI defense path does NOT re-implement player defense math** (the high-value catch — clean here). `UAIDecisionManager::ChooseDefenseType` (`AIDecisionManager.cpp:411`), `GetDefenseAccuracy(EAIDifficulty)` (`:482`), `CalculateDefenseReactionDelay` (`:499`) operate on **`EAIDifficulty`** (the AI's *skill* level) — a different concept from the attack's per-impact **`EDefenseDifficulty`**. The AI only *decides what to submit* via `SubmitDefenseInput`; the window/result are owned by `UDefenseSystem` (`MatchAndConsumeInput` / `CalculateDefenseResult`). No mirrored player calc. *(Deliberate asymmetry — excluded.)* *(resolved 2026-06-19: `GetDefenseAccuracy` / `CalculateDefenseReactionDelay` / `ScheduleDefenseDecision` deleted; AI defense reworked to per-impact synthesis via `TrySynthesizeImpactDefense` + `CalculateDefenseDelta`, judged by the shared matcher — the asymmetry the entry notes is preserved.)*
- **Damage-reduction math is centralized.** `UDefenseSystem::CalculateDefenseResult` is the single block/parry/dodge reduction; called by `ResolveImpactDefense` (`:1915`), the close path, and the dead `EnsureResultResolved`. Not re-implemented.
- **Block/parry buildup multipliers** (`BLOCK_BUILDUP_MULTIPLIER` / `PARRY_BUILDUP_MULTIPLIER`, `CombatConstants.h:325–326`) are named constants applied in one place (`ApplyOneImpact:2006–2012`); the lumped tail routes through the same `ApplyOneImpact`, so no second application site.
- **Window duel math** (`GetEffectiveDefenseInputWindow`, Reflex/speed terms) lives once in `UDefenseSystem`; callers pass through it, none re-derive it.

**VERDICT: MIXED.** Reduction math, the duel window, and AI/player roles are cleanly separated (CONSOLIDATED). But the **per-impact spell resolve block is triplicated** (#1, code-acknowledged "byte-identical"), and two small predicates/calcs are hand-mirrored across 3–4 sites (#2, #3) with comments admitting they must stay in sync — the genuine consolidation targets. Plus one dead parallel resolver (#5) to retire.

---

*Next pass (on your go): Pass 2 — Stat composition / damage calculation.*
