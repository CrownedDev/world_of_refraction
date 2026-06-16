# Duplication / Shadow-Logic Audit (2026 rebuild)

**Date:** 2026-06-16
**Branch:** `feature/realtime-defense`
**Status:** SURVEY — no edits made. This is a map, not a refactor. Nothing in `Source/` was changed.

> **Supersedes** the 2026-06-08 whole-tree audit (`feature/weapon-stones`). That version
> was organised by stat/damage/cost/enum *concern*; this rebuild walks the project
> **system-by-system** per Crown's triage list and is built up one pass at a time. Findings
> from the old audit (A1 TurnSpeed-skips-crystal, A2 split-efficiency, B1 stat-formula twins,
> B5 AI infusion-mult re-encode, B7 StatusMultiplier 3-copy, etc.) are **not lost** — they
> re-surface under the relevant new-system pass below (B5→Pass 7, B7→Pass 6, A1→Pass 5, …).

## Method

For each system: find **DEFINITION-side** duplication (same data/threshold authored in
parallel places) and **CHECK/LOGIC-side** duplication (same computation re-implemented
instead of called). Every finding leads with the symbol name + `file:line`. The high-value
catch is **hand-mirrored logic that must stay manually in sync** — especially an AI path
re-implementing a player-path calc. Those rank first.

Per-item columns: real-dup rationale · proposed consolidation · blast radius (# sites /
cross-system? / sync-critical?) · confidence (**SAFE** / **VERIFY** / **RISKY**).
**VERIFY = may be a deliberate asymmetry; do not recommend a merge until Crown confirms.**

Out of scope (Crown's standing decisions): value-collisions (same number, different concept),
`USpellData`/`UAbilityData`/`UWeaponAttackData` unification, documented deliberate asymmetries.

## Pass status

1. ✅ Defense / real-time windows
2. ✅ Stat composition / damage calculation
3. ✅ Gear / augment stones / fusion
4. ✅ Items / crystals / evolution
5. ✅ Turn manager / belt
6. ✅ Status buildup / resistance
7. ✅ Skill effects
8. ✅ Broken Darkness — **this pass (audit complete)**

*(Candidate systems not on Crown's list, flagged not folded: **Infusion / charge** — L1/L2
multiplier mapping re-encoded in AI vs `ActionExecutor` (old-audit B5); **Loadout / class
active-slots** — class→slots encoded twice (old-audit B3). Both will be reached via Pass 3/7
or listed explicitly if they fall outside.)*

---

## Pass 1 — Defense / real-time windows

**Canonical owner:** `UDefenseSystem` (`Private/Combat/Defense/DefenseSystem.cpp`). Player input
(`ACombatPlayerController` → `SubmitDefenseInput`) and AI input (`UAIDecisionManager::ScheduleDefenseDecision`
→ `SubmitDefenseInput`) converge on the **same** resolve chain: `MatchAndConsumeInput` (timing) →
`CalculateDefenseResult` (reduction/reflect). The per-impact resolve (`ActionExecutor::ResolveImpactDefense`)
and the lumped close (`DefenseSystem::CloseDefenseWindow`) both call that same `CalculateDefenseResult`.

### Ranked findings

| # | Duplicated symbol / logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| D-1 | **Parry-reflect apply+broadcast body** — `ApplyReflectedDamage(attacker, dmg)` immediately followed by `OnParryReflect.Broadcast(defender, attacker, dmg)` | wrapper `UDefenseSystem::ApplyParryReflect` `DefenseSystem.cpp:412-413`; **inlined** at `CloseDefenseWindow` `DefenseSystem.cpp:180-181` | **YES** — the inline is a byte-for-byte copy of the wrapper's two-line body. Two reflect-emit paths: per-impact (count-based) calls `ApplyParryReflect`; lumped close inlines it. A future change to "what a reflect emits" (e.g. an SFX cue, a second delegate) must be made in both or they desync. | Route the lumped inline through `ApplyParryReflect(State.Attacker.Get(), Defender, Result.ReflectedDamage)`. The wrapper already exists; delete the 2 inline lines. | 1 site, intra-system, not numeric-sync. Tiny. | **SAFE** |
| D-2 | **Defense reduction percentages stated in AI prose** — "Dodge 100% > Parry 70% + reflect > Block 50%" | comments `AIDecisionManager.cpp:449` (and `:448-449`, `ChooseDefenseType` priority note) vs the authored UPROPERTYs `BlockReduction=0.5` / `ParryReduction=0.7` / `ParryReflect=0.3` `DefenseSystem.h:419-421` | **NO** — comments only, not a second computation. The AI never computes a reduction; it picks a type and lets `CalculateDefenseResult` reduce. | None needed (it's documentation). If desired, reword the AI comment to not restate tunable numbers. | doc-drift only | **VERIFY** (benign — comment, not logic) |

### Confirmed CLEAN (single-owner — no dup)

- **`CalculateDefenseResult`** (`DefenseSystem.cpp:488-533`) — the sole reduction/reflect calc; reads the `BlockReduction`/`ParryReduction`/`ParryReflect` UPROPERTYs (the old 2026-06-08 "uses literal 0.5/0.3 instead of its UPROPERTYs" finding is **FIXED** — `:511/:517/:518` now read the properties). Called by both the per-impact path (`ActionExecutor.cpp:1855`) and the lumped close (`DefenseSystem.cpp:166`). One owner, both consumers route to it.
- **Reflect amount** — computed once in `CalculateDefenseResult:518` (`BaseDamage * ParryReflect`), carried on `FDefenseResult.ReflectedDamage`, applied (never recomputed) at `ApplyParryReflect`/the inline. Single computation.
- **Defense input window formula** — `GetEffectiveDefenseInputWindow` (`DefenseSystem.cpp:445-482`), `window = max(MIN, base + ReflexTerm − SpeedTerm)`, single owner; `MatchAndConsumeInput` calls it (`:323`). No second implementation.
- **`DefenseDifficultyMultiplier`** (`DefenseDifficulty.h:58`) — sole tier→multiplier map; the per-type `TypeTier`/`TypeMult` lambdas in `MatchAndConsumeInput` (`:328-338`) are the only resolver. One owner.
- **`MINIMUM_DEFENSE_WINDOW`** / `IMPOSSIBLE_WINDOW_FLOOR` / `DEFENSE_AFTER_GRACE_SECONDS` — single constants in `CombatConstants.h`; every floor (`MatchAndConsumeInput:362`, `GetEffectiveDefenseInputWindow:481`) reads the shared constant.
- **AI defense decision** — `ChooseDefenseType` / `GetDefenseAttemptChance` / `GetDefenseAccuracy` / `CalculateDefenseReactionDelay` (`AIDecisionManager.cpp:411+`, `:465+`) are genuinely **AI-only** (the player reacts in real time — there is no player twin to mirror). The AI does **not** re-implement `MatchAndConsumeInput` or `CalculateDefenseResult`; it submits a typed/timed input and lets the shared chain judge it. **No AI-mirrors-player catch in this system.**

### Explicitly NOT duplication (noted to pre-empt re-flagging)

- **AI reaction delay (`WindowDuration` seed) vs the actual acceptance window (`GetEffectiveDefenseInputWindow × DifficultyMult`)** — the AI times its press against the legacy `WindowDuration` seed while the impact-time match uses the per-impact effective window. These are different quantities, but this is the **known Stage-4 AI-degradation integration gap** (`RealTimeDefenseRework.md:540`, §4 per-hit-AI), not a duplicated formula. Tracked there; out of scope for this audit.

### VERDICT — Pass 1: **CONSOLIDATED**

Both halves are single-owner: the reduction/reflect/window/difficulty **definitions** live once
(UPROPERTYs + `CombatConstants`), and the **checks** (`MatchAndConsumeInput`, `CalculateDefenseResult`,
`GetEffectiveDefenseInputWindow`) are shared by player and AI alike. The only genuine dup is **D-1**, a
two-line reflect-emit inline that a one-line call collapses (SAFE). No hand-mirrored sync-critical logic,
no AI re-implementation of the player path.

---

## Pass 2 — Stat composition / damage calculation

**Canonical owners:** asset stat formulas `UCharacterData::Calculate*` (`CharacterData.h:417-629`);
crystal/equipment-aware twins `UCharacterDataComponent::GetEvolutionModified*` /
`GetEffective*` (`CharacterDataComponent.cpp`); damage pipeline `UDamageCalculator::CalculateDamage`
(`DamageCalculator.cpp`). **DEFINITION side is consolidated** — every `*_PER_POINT` / `*_BASE` /
`*_CAP` tuning constant lives once in `CombatConstants.h` and all formula bodies reference it, so no
*value* drift exists. All duplication below is **CHECK/LOGIC-side — formula *shape* written twice.**

### Ranked findings

| # | Duplicated logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| **P2-1** | **Pattern-P compose+clamp shape** — `Clamp(min(stat, STAT_MULT_CAP) × (1+EquipTerm) × Stone × Transient, STAT_MODIFIER_MIN, STAT_MODIFIER_MAX)` | live damage **`DamageCalculator.cpp:147-153`** (Step 2.5 spell) + **`:177-181`** (Step 2.6 raw); effective getters **`CharacterDataComponent.cpp:841-843`** (`GetEffectiveSpellDamage`) + **`:907-909`** (`GetEffectiveRawDamage`) | **YES — the high-value catch.** The four input *terms* (`GetEquipment*Term`/`GetStone*Factor`/`GetTransient*Factor`) are DRY single-source, but the **compose+clamp expression is hand-written in both places**. The live path can't call the getter (it's mid-pipeline, applies the Pattern-P target as a `target/raw` correction at `:152`). The getters are read by **AI threat** (`AIDecisionManager.cpp:937,945`) and BD/display. So *actual damage* and *AI-estimated/displayed damage* share a shape that must stay byte-identical — the comments at `:835`/`:905` and `:134`/`:164` explicitly promise "byte-identical" / "Mirrors GetEffective…". A shape edit (stone moves inside the clamp, ceiling changes) silently desyncs live vs AI/display. **Sync-critical, cross-system, AI-vs-live** — the BD-bug pattern at the damage layer. | Extract `ComposePatternP(StatBase, EquipTerm, Stone, Transient) → clamped mult`. `GetEffective*` returns it directly; `DamageCalculator` computes `Correction = ComposePatternP(...) / RawProduct`. Input terms already shared; only the compose/clamp shape needs one home. | 4 sites; `DamageCalculator` ↔ `CharacterDataComponent`; **sync-critical** | **VERIFY** (mechanical, but touches the live damage number — PIE damage-parity test before/after) |
| **P2-2** | **Asset `Calculate*` ↔ component `GetEvolutionModified*` formula shape** — `(BASE +) Pillar × TotalPoints × PER_POINT`, capped | SpellDamage `CharacterData.h:454` ↔ `CharacterDataComponent.cpp:725`; RawDamage `:606` ↔ `:736`; FlatDefense `:495` ↔ `:761`; StatusMultiplier `:443` ↔ `:1176`; Resistance `:625` ↔ `GetCrystalResistanceStatCapped:1187`; MaxHP/MaxEP `:588`/`:597` ↔ inline `RecomputeMaxPools` | **YES, but deliberate input asymmetry** (old-audit B1). Asset reads RAW `GetEffective{Mind,Body,Spirit}` (no actor → no crystal/equipment); component reads `GetEvolutionModified{…}` (crystal × equip × transient). The **input differs by design**; the **formula shape is copied**. Agrees today via shared constants; a *shape* edit (new base term, cap added/removed, pillar swap) diverges silently. Largest latent surface, but lower urgency than P2-1 (asset family is the documented no-context fallback; twins aren't both on the live damage path the way P2-1's are). | Generalise the **`CalculateTurnSpeedWithSpirit(SpiritValue)` pattern** (`CharacterData.h:554` — asset owns ONE body, caller supplies the pillar) to all seven: `CalculateXWith(PillarValue)`, called by both the raw asset wrapper and the component twin. One shape, two callers. | 7 stats × 2 files; **BP-exposed** (`UFUNCTION` — `.uasset` graphs ref by name) | **RISKY** (renames/signature changes need `FunctionRedirect`s; verify no Blueprint break) |

### Confirmed CLEAN / resolved since 2026-06-08

- **Crit chance** — `GetCriticalChance` → `GetEvolutionModifiedCritChance` → `GetLuckModifiedChance` single chain; AI routes to `GetCriticalChance` (no re-impl). One owner.
- **Defender flat defense** — `GetDefenderFlatDefense` (`DamageCalculator.cpp:367` reads `GetEvolutionModifiedFlatDefense`) sole owner.
- **Attacker damage multiplier** — `GetAttackerDamageMultiplier:323` sole owner (Spell→`GetEvolutionModifiedSpellDamage`, else→Raw).
- **Pattern-P input terms** — `GetEquipment{Spell,Raw}DamageTerm` / `GetStone*Factor` / `GetTransient*Factor` are single-source helpers; both the getters and `DamageCalculator` Step 1 DRY-source them (only the *compose shape* is the P2-1 dup, not the terms).
- **RESOLVED — old A1 (TurnSpeed skips the crystal layer):** `TurnManager.cpp:400` now passes `GetEvolutionModifiedSpirit()` into `CalculateTurnSpeedWithSpirit` — TurnSpeed gets the crystal pillar. The "no crystal twin" gap is closed.
- **RESOLVED — old B7 (StatusMultiplier 3-copy):** `GetSourceStatusMultiplierFactor` was retired; `AddStatusBuildup` + BD overload + crystal-wear now all call `GetEffectiveStatusMultiplier` (`CharacterDataComponent.cpp:1203`) — one live superset owner. (The asset `:443` and component-stat `:1176` remain as the P2-2 shape twins, not a live 3rd copy.)

### VERDICT — Pass 2: **MIXED**

Definition side (all tuning constants) is fully consolidated; core damage routing (crit / flat-defense /
attacker-mult) is single-owner and clean; two prior live findings (A1, B7) are now resolved. **But** the
CHECK side carries **P2-1** — a sync-critical, AI-vs-live Pattern-P shape mirror (the top item this audit
hunts for) — plus **P2-2**, the broad asset↔component formula-shape twin family (latent, BP-exposed).

---

## Pass 3 — Gear / augment stones / fusion

**Canonical owners:** stone % + fusion-half math `CrystalEffectTable` (`GetAttachedStonePercent`,
`GetDamageStoneBasePercent`); durability `UBreakCalculator::CalculateDurabilityWear`; per-class active-gear
sum `ULoadoutComponent::GetActiveStatBonus` (`LoadoutComponent.cpp:3065`); variant dispatch
`FRuntimeAttachedItem`. The **stone/fusion/wear *value* layer is consolidated** (single tables/calculators);
the duplication is in the per-class gear **selection** logic and a repeated stone-multiplier expression.

### Ranked findings

| # | Duplicated logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| **P3-1** | **Per-class active-gear slot resolution** — the `switch (CharacterClass) { Generic: {bHasSecondaryWeapon … Weapon/Ring branches} Resonator: {…} Caster/default: {…} }` skeleton that decides WHICH loadout entries are active | **`GetActiveStatBonus` `LoadoutComponent.cpp:3073-3143`** (pulls `.StatBonus`); **`GetActiveResistanceBonus` `:3177-3242`** (pulls `.ResistanceBonus`); **`GetActiveEffects` `:3283+`** (pulls `GetStartingEffects()`) | **YES — the high-value catch.** Three functions carry a byte-identical class→active-slot switch, differing only in which field they read off the selected entry. The code *admits* it: `:3175` "Mirrors GetActiveStatBonus's per-class slot resolution exactly"; `:3274` "Coverage mirrors GetActiveStatBonus's per-class gear selection." **Sync-critical:** add a class, or change which slots a class uses (e.g. Resonator gains a secondary), and all three must change in lockstep or stat-bonus / resistance / starting-effects disagree about which gear is live. This is old-audit **B3, worsened** — 3 copies, not 2. | Extract the per-class **selection** once — e.g. `GetActiveContributingEntries()` returning the active weapon/ring/evolution entries for this class — and have the three consumers project their field (`.StatBonus` / `.ResistanceBonus` / starting-effects). Must preserve the deliberate evolution exceptions (the post-switch evo tail differs: effects EXCLUDE weapon/ring-attached evolutions; stats/resistance include them). | 3 functions, intra-`LoadoutComponent`; **sync-critical** (class→slots) | **VERIFY** (extraction is clear, but the per-function evolution-tail exceptions must be preserved, not blindly merged) |
| **P3-2** | **Attached-stone multiplier expression** — `1.0f + CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::X) / STAT_PERCENT_DIVISOR` | ~10 consumers, each its own `ESubStat`: SpellDamage `CharacterDataComponent.cpp:804`, RawDamage `:875`, ActionSpeed `:937`, SpellSpeed `:974`, Efficiency `:1019`/`:1153`, Reflex `:1071`, StatusMultiplier `:1223`, Resistance `:1270` **+ `StatusBuildupManager.cpp:234`**, Luck `:691` | **YES, but low-severity** (old-audit §6 "no central apply-stone dispatcher"). The 3-token compose shape repeats per stat; the *value* is protected — `GetAttachedStonePercent` (fusion-aware) and `STAT_PERCENT_DIVISOR` are single-source, so no value drift. Each consumer is a genuinely distinct application point. | Tiny helper `StoneMultiplier(Att, ESubStat) → 1 + pct/DIVISOR` (and an additive `StoneFraction` variant for the Resistance `+=` sites) to collapse the repeated expression to one call. | ~10 sites; `CharacterDataComponent` + `StatusBuildupManager`; **not** sync-critical (value-protected) | **SAFE** (mechanical convenience helper; behaviour-neutral) |
| P3-3 | **Variant dispatch chain** — `if (IsAugmentStone()) … if (IsCrystal()) … if (IsEvolution()) … if (IsFusion()) …` | ~6 accessors in `FRuntimeAttachedItem.cpp` (`IsBroken:19`, `GetElement:61`, `GetCurrentDurability:93`, `GetMaxDurability:115`, `ApplyWear:154`, `RepairBetweenCombats:178`) | **NO** — idiomatic discriminated-union dispatch; each accessor delegates to a *different* variant method, bodies genuinely differ. Not logic duplication. Noted only as an enum-fanout hazard: a **new attachment `Kind` requires touching ~6–8 accessors** (the §8-style "new enum value, many switches" class). | (optional) switch-on-`Kind` like `operator==`/`FromAttachedItem` already do, for consistency; not a correctness need. | ~6–8 methods, intra-struct | benign |

### Confirmed CLEAN

- **Fusion math** — fusion-half resolution (gem vs stone, two-tier durability) lives in `CrystalEffectTable::GetAttachedStonePercent` + `FFusionAttachment` (`GetMaxDurability`/`ApplyWear`/`RepairBetweenCombats`); consumers never re-implement it. `GetStone*Factor` comments confirm "fusion-aware … returns 0 for non-stone/non-fusion." Single fusion-aware owner.
- **Durability wear** — `UBreakCalculator::CalculateDurabilityWear` sole owner; all callers (incl. debug) route to it.
- **Stone % + DamageStone base** — `CrystalEffectTable` single table; `GetDamageStoneBasePercent` two-path (attached vs consumable) is the documented intentional split (carried from old §6 (c)).
- **`EquipmentDataBase::GetCombinedStatBonus` `:243`** — asset-level attachment combine (the data asset's own stone), a **different layer** from the runtime per-class `GetActiveStatBonus`; not part of P3-1.

### VERDICT — Pass 3: **MIXED**

The stone / fusion / wear **value** layer is fully consolidated (single tables + calculators, fusion math
owned once). But gear **selection** carries **P3-1** — a sync-critical class→active-slot switch hand-mirrored
across three `LoadoutComponent` functions (old B3, now 3 copies) — plus **P3-2**, the value-safe stone-multiplier
expression repeated ~10×.

---

## Pass 4 — Items / crystals / evolution

**Canonical owners:** slot caps `CrystalEffectTable::ResolveSpellSlotCap` / `GetAttachmentSlotsForTier`;
wear `UBreakCalculator`; consumable + attached stone % `CrystalEffectTable`; efficiency
`UCharacterDataComponent::GetEffectiveEfficiencyMultiplier`. The **crystal / wear / slot-cap / efficiency
*value* layer is fully consolidated** (and old A2 is now resolved). The duplication is in the
**loadout-entry assembly logic** — the merge + validate code that turns gear + crystal + assigned skills
into the live ability/spell lists, copied across `FWeaponLoadoutEntry` and `FRingLoadoutEntry`.

### Ranked findings

| # | Duplicated logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| **P4-1** | **Sequential-override-merge algorithm** — "walk Presets, replace slot *i* with `Overrides[idx]` when non-null, append surplus overrides, then `SetNum(cap)`" | **`FWeaponLoadoutEntry.cpp` `GetAllAbilities:46-74`, `GetAllSpells:86-117`, `GetAugmentStoneAbilities:174-207`; `FRingLoadoutEntry.cpp` `GetAllSpells:51-82`** | **YES — 4 byte-identical copies** of the same loop, differing only in element type (`UAbilityData*`/`USpellData*`) and the cap/locked gate. A change to override-merge semantics (null handling, slot mapping, ordering) must hit all 4 or weapon-abilities / weapon-spells / augment-abilities / ring-spells silently diverge. | Template helper `MergeSequentialOverride<T>(Presets, Overrides, Cap) → TArray<T>`; the per-call locked-short-circuit + cap source stay at the call site. | 4 sites, 2 files; latent-sync (merge semantics) | **SAFE** (type-generic, mechanical; loop is identical) |
| **P4-2** | **Validate-skills skeleton** — ownership check → (type predicate) → count-cap | weapon `ValidateAbilities:212` ≈ `ValidateAugmentStoneAbilities:256` (ownership + weapon-type + dual-weapon + cap); spell `ValidateSpells` weapon `:313` ≈ ring `:110` (ownership + element-match + cap) | **YES, partial** — the ownership-loop + count-cap core is shared; the per-type predicate genuinely differs (abilities check `RequiredWeaponType` + `bRequiresDualWeapon`; spells check `Element` match). The two weapon ability-validators are near-identical (differ only in array + locked/attachment gate + cap source). | Extract the shared ownership+count core taking a per-entry predicate lambda; keep the ability-vs-spell predicate at the call site. Lower-priority than P4-1 (more genuine per-type variation). | 4 validators, 2 files | **VERIFY** (real per-type differences — extract the core only, don't force-merge the predicates) |
| P4-3 | **Locked-preset count** | `FWeaponLoadoutEntry::GetLockedAbilityCount:13` ≈ `FRingLoadoutEntry::GetLockedSpellCount:11` (comment `:18` admits "Mirrors FWeaponLoadoutEntry::GetLockedAbilityCount") | **YES, trivial** — same `if (locked) return Presets.Num(); return 0;` shape, weapon-abilities vs ring-spells. | Folds out naturally if P4-1/P4-2 extraction lands; not worth a standalone change. | 2 tiny fns | benign |

### Confirmed CLEAN / resolved since 2026-06-08

- **RESOLVED — old A2 (split efficiency):** `GetEffectiveEnergyCostEfficiencyMultiplier:4031-4033` documents and confirms that energy-cost, BD overload drain (`CombatOrchestrator.cpp:1169`), and crystal-wear all now route through the unified gear-inclusive `GetEffectiveEfficiencyMultiplier` (`CharacterDataComponent.cpp:1122`). The bare asset `CalculateEfficiencyMultiplier` is an unreachable defensive fallback. The "efficiency means two numbers" split is closed.
- **Slot caps** — `CrystalEffectTable::ResolveSpellSlotCap` / `GetAttachmentSlotsForTier` single owners; every loadout entry routes the *value* through them (so the P4-1/P4-2 dups are over the *cap value*-protected merge/validate *shape*, not the cap math).
- **Wear** — `UBreakCalculator` single owner; `CrystalManager::ProcessPostCastWear`, `InfusionCostHelper`, and debug all route to it. The SimCast prediction (`CrystalManager.cpp:634`) re-runs the *same* calculator; its documented prediction-vs-live divergence (`:667`) is the random luck-skip inside `ProcessPostCastWear`, not a formula mirror — benign.
- **Consumable damage-stone** — `ItemExecutor.cpp:518` reads the same `GetDamageStoneBasePercent` table as the attached path (documented intentional two-path, old §6(c)). Consumable potion buffs apply as skill-effects the getters fold in — no stat-math re-implementation.
- **Evolution stat bonus** — composed via `FEquipmentStatBonus::Accumulate` (Base + Generated); single combine path (the per-class *selection* of whether to include it is the Pass-3 P3-1 concern, not a value dup here).

### VERDICT — Pass 4: **MIXED**

The crystal / wear / slot-cap / consumable / efficiency **value** layer is fully consolidated (single
tables + calculators), and a third prior live finding (**A2**) is now resolved. But loadout-entry **assembly**
carries **P4-1** — the sequential-override-merge algorithm copied 4× across the weapon/ring entry files
(SAFE template-extract) — and **P4-2**, the validate-skills skeleton mirrored weapon↔ring and
abilities↔augment (VERIFY, partial).

---

## Pass 5 — Turn manager / belt

**Canonical owner:** `UTurnManager` (`Private/Combat/TurnManager.cpp`). "Belt" = the materialized
turn-order belt (`TurnBelt`, horizon `TURN_BELT_HORIZON=16`), not an item hotbar (consumables are
Pass 4). **This is the cleanest pass — and an exemplar:** the very preview-vs-live mirror this audit
hunts for is *deliberately engineered out*.

### The exemplary pattern (what the other systems' mirrors should imitate)

`AdvanceSimState` (`TurnManager.cpp:478`) is the **single** scheduler step, parameterised by the state
array it operates on, and its own comment says so (`:482-484`): "THE scheduler step: the live path
(`AdvanceToNextTurn`, real state) and the belt fill (`RebuildBelt`, scratch state) both run through here —
the only place a turn is selected."

- **Live advance:** `AdvanceToNextTurn:169` → `AdvanceSimState(Combatants, PendingTurns, …)` (real state).
- **Belt / preview fill:** `RebuildBelt:675` → `AdvanceSimState(Scratch, ScratchPending, …)` (scratch copy, replayed 16×).
- **`PreviewTurnOrder:464`** is a pure **slice of the materialized belt** (`:466` "no forward sim"). The UI strip (`TurnOrderStripWidget`) and AI lookahead (`AIDecisionManager.cpp:1729`) both read it.

So turn selection, pinned/bonus/execution firing, debt accrual, and round-rollover exist **once**. Contrast
P2-1 (live damage re-derives the Pattern-P shape the effective-getters own) and the Pass-4 SimCast wear
prediction — those are the anti-pattern; `AdvanceSimState` is the pattern done right.

### Findings

| # | Item | Sites | Real dup? | Notes | Confidence |
|---|---|---|---|---|---|
| P5-1 | **Stone-multiplier shape (TurnSpeed)** — `1 + GetAttachedStonePercent(Att, ESubStat::TurnSpeed)/STAT_PERCENT_DIVISOR` | `TurnManager.cpp:242-243` (the `GetEffectiveSpeed` lambda inside `CalculateSpeedRatios`) | **YES — an 11th instance of P3-2**, not a new dup. Value-protected (shared lookup + divisor). | Folds into the P3-2 `StoneMultiplier(Att, ESubStat)` helper; cross-referenced, not a separate fix. | **SAFE** |
| P5-2 | **Buff/debuff fold idiom** — `1 + (Buff − Debuff [+ Mod])/STAT_PERCENT_DIVISOR` | `GetEffectiveSpeed:229` (Turn) vs the same idiom in `GetTransient*Factor` (Pass 2) and Efficiency (`:1166`) | **NO** — generic "fold this stat's transient buff/debuff" idiom; each site reads its **own** `ESkillEffectType` pair. Shape recurs but the inputs are genuinely per-stat. | Idiomatic, not duplication. Noted only so it isn't re-flagged. | benign |

### Confirmed CLEAN

- **Scheduler step** — `AdvanceSimState` single owner (live + belt + preview all route through it). No second turn-selection implementation.
- **Tie-break comparator** — the World-level total → Body → Mind → Spirit ordering (`TurnManager.cpp:361-376`) lives once; both real and scratch advances use it.
- **Effective speed** — computed once in `CalculateSpeedRatios::GetEffectiveSpeed`; the belt/preview consume the precomputed `SpeedRatio`, never a second effective-speed formula.
- **TurnSpeed crystal layer** — `CacheActorStats` caches raw World levels for **tie-break** (`:415-417`); the speed value passes `GetEvolutionModifiedSpirit()` into `CalculateTurnSpeedWithSpirit` (the old-A1 fix). Two distinct uses of Spirit, by design — not a dup.
- **Consumable TurnSpeedStone** — `ItemExecutor::ExecuteTurnSpeedStoneEffect:446` applies a `TurnSpeedBuff` skill-effect that `GetEffectiveSpeed` folds in; no turn-speed math re-implemented.
- **Bonus vs Execution turns** — both pinned types are selected in the one `AdvanceSimState` winner-pick; no parallel scheduler.
- **InventoryComponent belt-refresh guard** (`:205,:317`) — a `GetAttachedStonePercent(…TurnSpeed) > 0` **presence** check (does equipping change turn order), reusing the canonical lookup; not a formula re-impl.

### VERDICT — Pass 5: **CONSOLIDATED**

Turn-order / belt logic is single-owner throughout, and the live↔preview unification via `AdvanceSimState`
is the model pattern the audit's other mirrors violate. Only carry-overs: P5-1 (an 11th P3-2 stone-multiplier
site, folds into that helper) and the benign P5-2 buff/debuff idiom.

---

## Pass 6 — Status buildup / resistance

**Canonical owner:** `UStatusBuildupManager` (`Private/Skills/Effects/StatusBuildupManager.cpp`).
The **buildup pipeline and the resistance aggregation are both single-owner**, and old **B7**
(StatusMultiplier 3-copy) is confirmed resolved. The two items below are (a) an AI estimate that is
deliberately coarse, and (b) a resistance twin the code *itself documents as a kept-separate exception*.

### Ranked findings

| # | Item | Sites | Real dup? | Notes / proposed action | Confidence |
|---|---|---|---|---|---|
| **P6-1** | **AI status-buildup estimate vs live apply** | estimate `UAbilityData::CalculateStatusBuildup` `AbilityData.cpp:90-103` (used by AI `AIDecisionManager.cpp:817,853,2012`) vs live `UStatusBuildupManager::AddStatusBuildup:289` | **NO — different scope, not a pipeline mirror.** The estimate is offense-only: `BASE_PER_HIT × CalculateStatusMultiplier() × HitCount`. It uses the **asset** `CalculateStatusMultiplier` (no crystal/gear/transient — the P2-2 asset-vs-component split) and omits the defender-side `(1 − GetTotalStatusResistance)`, the BD absorption-stack amp, and decay that the live path applies. So the AI **under-estimates** buildup for geared casters and ignores target resistance. | This is an **AI-accuracy** question, not a duplication to extract: the estimate intentionally doesn't have a defender to resist or a live actor to read gear from. If tighter AI scoring is wanted, route the estimate through `GetEffectiveStatusMultiplier` and a representative resistance — but that's a design call, not a dedupe. | **VERIFY — likely intentional** (AI estimate coarseness; do not "merge") |
| **P6-2** | **Resistance base-stat composition shape** — `ModifiedSpirit × GetTotalResistance() × RESISTANCE_PER_POINT (+ BonusResistance×RPP + ResistanceStone)` | `GetCrystalResistanceStatCapped` `CharacterDataComponent.cpp:1187` (buildup general), `GetEffectiveResistance` `:1243` (wear control), asset `CalculateResistance` `CharacterData.h:625` | **NO — deliberate divergent twins, self-documented.** The three share the base shape but diverge **by design**: buildup-general is 0.5-capped / multiplicative / element-matched / clamped [−1,1]; wear-control is uncapped / additive / element-agnostic. The code spells it out at `:1253-1255`: *"Different value, different consumer — do NOT merge (twin-trap EXCEPTION: kept separate by design)."* | **Exemplary** — this is the right way to handle a P2-2-style shape sibling that must NOT be unified: document the intentional divergence at the site. No action. (Crown's "different concept stays separate" exclusion, made explicit in-code.) | benign (do not merge) |
| P6-3 | **Stone-multiplier shape (Resistance)** — `1 + GetAttachedStonePercent(Att, Resistance)/DIVISOR` (+ additive `/DIVISOR` variant) | `StatusBuildupManager.cpp:234`, `CharacterDataComponent.cpp:1270` | **YES — further P3-2 instances** (12th/13th), value-protected. | Folds into the P3-2 `StoneMultiplier`/`StoneFraction` helpers; cross-ref, not a new fix. | **SAFE** |

### Confirmed CLEAN / resolved

- **Buildup pipeline** — `AddStatusBuildup:289` is the single 6-step amp/reduce site; every producer (ActionExecutor, ItemExecutor, SkillEffectManager DoT, debug) routes to it.
- **Resistance aggregation** — `GetTotalStatusResistance:203-278` composes all six sources (crystal-Spirit general + equip + attached stone + element/physical effect + class-innate + gear per-category + ModifyStatusResist) in **one** function with the final clamp. No second status-resistance reducer.
- **RESOLVED — old B7 (StatusMultiplier 3-copy):** `:284-287` documents the retirement of `GetSourceStatusMultiplierFactor`; live buildup (`:367`), BD overload bake (`CombatOrchestrator.cpp:1159-1160`), and crystal-wear all read the single `GetEffectiveStatusMultiplier` — "lockstep by construction."
- **Trigger resolution** — `BarCapTriggerResolver::ResolveTrigger(Element, PhysicalType)` single owner (`:301`, `:134`); no second element/physical→trigger switch.
- **Threshold / decay** — `STATUS_EFFECT_THRESHOLD` / decay constants referenced only inside `StatusBuildupManager`; centralized.
- **Offense vs defense separation** — attacker amps (StatusMultiplier, BD stack) applied *above* the resistance reduction (`:351-393`), deliberately kept out of `GetTotalStatusResistance`. Clean layering, not a split.

### VERDICT — Pass 6: **CONSOLIDATED**

The buildup pipeline, resistance aggregation, StatusMultiplier amp (B7-resolved), and trigger resolution are
all single-owner. No new sync-critical mirror. The only items are **P6-1** (an AI offense-estimate that is
coarse by construction — VERIFY as intentional, not a dedupe) and **P6-2** (a resistance shape-twin the code
*explicitly* keeps separate — the model for handling a P2-2-style sibling that must not be merged). P6-3 is
two more P3-2 stone-multiplier sites.

---

## Pass 7 — Skill effects

**Canonical owners:** classification `SkillEffectClassification::IsBuff/IsDebuff`
(`ESkillEffectType.h:261/333`); stat-modifier aggregation `USkillEffectManager::GetTotalStatModifier`;
DOT formula `FActiveSkillEffect::ApplyDOT` (`ActiveSkillEffect.h:444`); charge multipliers
`UActionExecutor::Get{Spell,Ability}Charge{Damage,Status}Multiplier` (`ActionExecutor.cpp:4303-4360`).
**Effect classification + application + stat aggregation are consolidated** (old **B6** is resolved). The
one live finding is old **B5** — the AI re-encodes the charge level→multiplier mapping.

### Ranked findings

| # | Duplicated logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| **P7-1** | **Charge level→multiplier mapping** ("L2 → ×`CHARGE_L2_DAMAGE_MULT`; L1 → ×`SPELL_L1_BUILDUP_MULT`") re-encoded in the AI estimate | live: `GetSpellChargeDamageMultiplier:4316` / `GetAbilityChargeDamageMultiplier:4342` (called at `:1040,:1283`), buildup inline `ActionExecutor.cpp:1110`; **AI re-applies inline** `AIDecisionManager.cpp:729,793,1904,1992` (`× CHARGE_L2_DAMAGE_MULT`) + `:1926,2015` (`× SPELL_L1_BUILDUP_MULT`) | **YES — old B5, still live.** The `*_MULT` **constant** is shared (no value drift), but the **level-gating** (which level earns which multiplier) is duplicated: the AI scores actions with a hand-mirrored copy of the live assembly's charge mapping. Change the charge model (L1 gains a damage mult, add L3, retune which level gets what) and the AI estimate silently diverges from what the action actually does. Sync-critical for **AI↔live parity** — there's even a design doc tracking it (`AIExecutionParity_Design.md:51`: "AI re-applies by hand"). | Extract the level→multiplier mapping into a **free function** (e.g. `InfusionCharge::DamageMultiplier(level)` / `StatusMultiplier(level)` / `BuildupMultiplier(level)` in `InfusionConstants`/`InfusionCostHelper`). The `ActionExecutor` getters become thin wrappers; the AI calls the same free function (no subsystem ref needed — that friction is *why* it was inlined). | ~6 AI sites; AI ↔ ActionExecutor; **sync-critical (AI parity)** | **VERIFY** (behaviour-neutral — constant already shared; the win is the mapping has one home) |
| P7-2 | **`IsBuff`/`IsDebuff` per-struct methods** | `FSkillEffect::IsBuff:171`/`IsDebuff:177`; `FActiveSkillEffect::IsBuff:506`/`IsDebuff:512` | **NO — old B6 RESOLVED.** Both structs now *delegate* ("Delegates to the shared single-source classifier") to `SkillEffectClassification::IsBuff/IsDebuff`. The per-struct switches that could disagree are gone; one classifier owns the type→category map. | None. Residual: `IsBuff` and `IsDebuff` are two complementary switches (a new effect type is added to the correct one, `default:false`) — but single-owner each, the minimal correct form. | n/a | resolved / benign |
| P7-3 | **Buff/debuff-pair read idiom** — `GetTotalStatModifier(XBuff) − GetTotalStatModifier(XDebuff)` | ~13 sites across `CharacterDataComponent.cpp` (SpellDamage `:821`, Raw `:892`, ActionSpeed `:942`, SpellSpeed `:979`, Reflex `:1082`, Efficiency `:1164`, StatusMult `:1232`, MaxHP/EP `:628-631`, pillar `:546`, …) | **NO** — the P5-2 idiom; each site reads its **own** Buff/Debuff `ESkillEffectType` pair and the pairing is name-obvious. Not a computation copy. | (optional) a `BuffDebuffNet(Owner, BuffType, DebuffType)` convenience could collapse the call pair, but the per-site pairing is intentional and clear. | ~13 sites | benign |

### Confirmed CLEAN / resolved

- **RESOLVED — old B6 (IsBuff/IsDebuff two switches):** consolidated into `SkillEffectClassification` (`ESkillEffectType.h:259-`); `FSkillEffect` and `FActiveSkillEffect` both delegate. Sign-aware types (`ModifyStatusResist`) handled once, in the classifier.
- **Stat-modifier aggregation** — `GetTotalStatModifier` single owner; every transient buff/debuff read (the ~13 P7-3 sites + Turn/BD) routes through it. No second stacking aggregator.
- **Effect application** — `ESkillEffectType` apply switch single-owned in `SkillEffectManager` (old §8); display names in `SkillEffectDisplayNames`; concern-split, each map owned once.
- **DOT damage** — `FActiveSkillEffect::ApplyDOT:444` (`BaseDOTDamage × WeaponInfusionMultiplier`) sole owner; `SkillEffectManager.cpp:402` routes to it. Buildup-from-effect routes to `AddStatusBuildup` (Pass 6). No second DOT/buildup formula.
- **Infusion stat-modifier mapping** — `UEvolutionItemData::MapToInfusionModifiers` (`EvolutionItemData.cpp:225`) is the single per-substat `× InfusionMultiplier` map; both the instance getter and the static both route through it.

### VERDICT — Pass 7: **MIXED**

Effect classification (B6 resolved → one `SkillEffectClassification`), application, stat aggregation, and the
DOT/infusion-mapping formulas are all single-owner. The one live finding is **P7-1** (old **B5**): the AI
re-encodes the charge level→multiplier mapping inline — value-safe today (shared constant) but a sync-critical
AI↔live parity hazard, cleanly fixed by a shared free-function mapping. P7-3 is the benign P5-2 read idiom.

---

## Pass 8 — Broken Darkness

**Canonical owner:** `UBrokenDarknessManager` (`Private/Combat/Mechanics/BrokenDarknessManager.cpp`).
The **live BD machinery is consolidated** — old **B8** (overload energy bake) is resolved, the BD-stack
status wrapper was retired, and the requirement check is shared. The one finding is a **dead-duplicate
absorption formula** (two formulas + two rate-constant sets for "how much BD absorbs").

### Ranked findings

| # | Duplicated logic | Sites | Real dup? | Proposed consolidation | Blast radius | Confidence |
|---|---|---|---|---|---|---|
| **P8-1** | **Two absorption-energy formulas + two rate-constant sets** | **LIVE:** `OnDefenseResolved:786` → `CalculateAbsorptionEnergy:841` = `AttackEnergyCost × BrokenDarknessConstants::PARRY/BLOCK_ABSORPTION_MULT`. **DEAD:** `OnSuccessfulParry:346` / `OnSuccessfulBlock:372` = `DamageBlocked × ParryAbsorptionRate(1.0)/BlockAbsorptionRate(0.5)` (member UPROPERTYs `BrokenDarknessManager.h:357/361`) | **YES — dead-duplicate, a real trap.** The damage-based pair is **unwired** (no callers — only defs/decls; `BrokenDarkness.md:328` "OnSuccessfulParry / OnSuccessfulBlock **unwired**"). It encodes a *different* absorption answer than the live energy-cost path, backed by a *separate* rate-constant set. A maintainer wiring up the obvious-looking `OnSuccessfulParry` would silently get the wrong formula + wrong tuning. **Doc mismatch too:** the design docs (`BrokenDarkness_ReactiveDefense.md:12,23`, `RealTimeDefenseRework.md:484,715`) describe absorption as `ParryAbsorptionRate 1.0 / BlockAbsorptionRate 0.5` — the **dead** path's members — while the live path uses `PARRY/BLOCK_ABSORPTION_MULT`. | Delete the unwired pair + the redundant `Parry/BlockAbsorptionRate` members (non-defense absorption already has a live entry point, `GrantAbsorptionEnergy:398`). If a damage-based source is ever wanted, route it through `CalculateAbsorptionEnergy`, not a second formula. Reconcile the design docs to the live constants. | 2 dead fns + 2 member constants; doc-vs-code | **VERIFY** (BD methods may be `BlueprintCallable` — confirm no `.uasset` caller before removal) |
| P8-2 | **BD break-chance infusion mapping** `GetInfusionMultiplier(level)` | `BrokenDarknessManager.cpp:71` (sole caller `RollForBreak:142`) | **NO** — a BD-local `InfusionLevel → break-chance-multiplier` map; a **different domain** from the P7-1 charge damage/buildup mapping. Single owner, one caller. | None — distinct concept (break chance ≠ damage). Noted so it isn't conflated with P7-1. | n/a | benign |

### Confirmed CLEAN / resolved

- **RESOLVED — old B8 (overload energy bake):** `CombatOrchestrator.cpp:1162/1169` reads the canonical `GetEffectiveStatusMultiplier` + `GetEffectiveEfficiencyMultiplier` (the T3-consolidated single getters) and passes the product to `ProcessOverloadTick`; the self-status pass uses `bSkipBaseStatAmp` to avoid double-count. "Lockstep by construction" — no longer the easy-to-desync rare path the 2026-06-08 audit flagged.
- **BD-stack status multiplier** — `GetElementStackStatusMultiplier:674` single owner; the old thin wrapper `DamageCalculator::GetBDStackStatusMultiplier` was **retired** (`DamageCalculator.cpp:472`), and `StatusBuildupManager.cpp:383` now calls the BD manager directly. One hook.
- **Requirement check** — `DoesSpellExceedRequirements:186` routes to the shared `FWorldStatRequirements::GetTotalDeficit` (the consolidated World-Stat helper); no hand-mirrored comparison (confirmed in the World-Stat probe).
- **Transform/absorb/forbidden-cast gates** — all guard on the single `bIsTransformed` flag; no parallel state.

### VERDICT — Pass 8: **MIXED**

Live BD (overload bake B8-resolved, stack-mult wrapper retired, requirement check shared) is consolidated.
The one finding is **P8-1**: a dead, unwired absorption formula with its own rate-constant set that
contradicts the live energy-cost path *and* the design docs — a removal/reconcile, not a refactor.

---

## Audit complete — cross-pass summary

All 8 systems surveyed. **Old-audit live findings: A1, A2, B6, B7, B8 all RESOLVED since 2026-06-08;
B3 worsened; B5 still live.** No source was changed in this audit.

### Priority-ranked backlog (for separate triage)

| Rank | ID | System | What | Sync-critical? | Confidence |
|---|---|---|---|---|---|
| 1 | **P2-1** | Damage | Pattern-P compose-shape mirror: live `DamageCalculator` ↔ `GetEffective*` (AI/display read it) | **Yes — AI↔live** | VERIFY |
| 2 | **P3-1** | Gear | Per-class slot resolution hand-mirrored ×3 (`GetActiveStatBonus`/`ResistanceBonus`/`Effects`) — old B3 | **Yes — class→slots** | VERIFY |
| 3 | **P7-1** | Skill effects | AI re-encodes charge level→multiplier mapping inline — old B5 | **Yes — AI↔live** | VERIFY |
| 4 | **P8-1** | Broken Darkness | Dead-duplicate absorption formula + split rate constants (+ doc mismatch) | latent/trap | VERIFY |
| 5 | **P4-1** | Items | Sequential-override-merge copied 4× (weapon/ring entries) | latent | SAFE |
| 6 | **P2-2** | Stat | Asset↔component formula-shape twin family (7 stats) | latent | RISKY (BP) |
| 7 | **P4-2** | Items | Validate-skills skeleton mirror (weapon↔ring, ability↔augment) | latent | VERIFY |
| 8 | **P3-2** | Gear | Stone-multiplier expression repeated 13× (incl. P5-1, P6-3) | value-safe | SAFE |
| 9 | **D-1** | Defense | Parry-reflect emit inlined vs `ApplyParryReflect` | low | SAFE |

**Deliberate / do-not-merge (verified intentional):** P6-2 (resistance shape-twin, self-documented),
P2-2 asset family (no-context fallback by design), DamageStone two-path, the discriminated-union dispatch.
**AI-coarseness (likely intentional, not dedupe):** P6-1 (AI buildup estimate).

### The pattern worth internalising

The audit's recurring hazard is **shape-drift across a live↔mirror boundary** (P2-1, P3-1, P7-1, P8-1):
shared *constants* protect value drift, but a copied *formula/mapping shape* silently diverges on the next
edit. The codebase already contains the antidote in two forms — **`AdvanceSimState`** (Pass 5: one step,
replayed for live + scratch) and **`GetEffectiveResistance`'s self-documented twin-trap exception**
(Pass 6). Every rank-1–4 item is "make it look more like `AdvanceSimState`."

---

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-06-16 | Rebuilt the audit system-by-system (supersedes the 2026-06-08 concern-organised version). Pass 1 (Defense / real-time windows) complete: CONSOLIDATED, one SAFE latent (D-1 parry-reflect inline). Passes 2–8 pending. | `feature/realtime-defense` |
| 2026-06-16 | Pass 2 (Stat composition / damage calculation) complete: MIXED. P2-1 Pattern-P compose-shape mirror (live `DamageCalculator` ↔ `GetEffective*` read by AI/display — sync-critical, VERIFY); P2-2 asset↔component formula-shape twin family (latent, RISKY/BP-exposed). Constants consolidated; crit/defense/attacker-mult clean; old A1 + B7 confirmed resolved. | `feature/realtime-defense` |
| 2026-06-16 | Pass 3 (Gear / augment stones / fusion) complete: MIXED. P3-1 per-class slot-resolution hand-mirrored across 3 `LoadoutComponent` functions (old B3 worsened — sync-critical, VERIFY); P3-2 stone-multiplier expression ×10 (value-safe, SAFE); P3-3 union-dispatch fanout (benign). Fusion math / wear / stone tables confirmed single-owner. | `feature/realtime-defense` |
| 2026-06-16 | Pass 4 (Items / crystals / evolution) complete: MIXED. P4-1 sequential-override-merge copied 4× across weapon/ring entries (SAFE template-extract); P4-2 validate-skills skeleton mirror (VERIFY); P4-3 locked-count twin (benign). Slot caps / wear / stone tables single-owner; old A2 confirmed resolved (A1+A2+B7 all now closed). | `feature/realtime-defense` |
| 2026-06-16 | Pass 5 (Turn manager / belt) complete: CONSOLIDATED. `AdvanceSimState` unifies live advance + belt fill + preview (the exemplary anti-mirror); only carry-overs P5-1 (11th P3-2 stone-multiplier site) and benign P5-2 buff/debuff idiom. | `feature/realtime-defense` |
| 2026-06-16 | Pass 6 (Status buildup / resistance) complete: CONSOLIDATED. Pipeline + resistance aggregation single-owner; old B7 confirmed resolved (`GetEffectiveStatusMultiplier`). P6-1 AI buildup estimate is coarse-by-design (VERIFY, not a dedupe); P6-2 resistance shape-twin is a self-documented kept-separate exception; P6-3 two more P3-2 stone sites. | `feature/realtime-defense` |
| 2026-06-16 | Pass 7 (Skill effects) complete: MIXED. P7-1 = old B5 still live (AI re-encodes charge level→multiplier mapping inline vs the getters — value-safe, sync-critical AI parity, VERIFY: extract a shared free-function mapping). Old B6 RESOLVED — `IsBuff`/`IsDebuff` now delegate to single `SkillEffectClassification`. GetTotalStatModifier / DOT / infusion-mapping single-owner; P7-3 benign read idiom. Pass 8 pending. | `feature/realtime-defense` |
| 2026-06-16 | Pass 8 (Broken Darkness) complete: MIXED — **audit complete (all 8 passes)**. P8-1 dead-duplicate absorption formula (`OnSuccessfulParry/Block` damage-based + member rates vs live energy-cost `CalculateAbsorptionEnergy` + namespace mults; doc mismatch — VERIFY remove). Old B8 RESOLVED (overload bake reads canonical getters); BD-stack wrapper retired; requirement check shared. Added cross-pass priority backlog: rank 1–4 = P2-1, P3-1, P7-1, P8-1 (sync-critical live↔mirror). Old A1/A2/B6/B7/B8 resolved; B3 worsened; B5 live. | `feature/realtime-defense` |
