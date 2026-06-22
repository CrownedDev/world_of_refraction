# Crystal Wear

## Overview

Equipment crystals (refined or evolution) accrue **durability wear** on each
post-cast event — tier mismatch, infusion levels — modulated by the caster's
real power output and control. Wear is a **percent of the crystal's max
durability for its tier**, so casts-to-break is consistent across tiers (a 7%
spell costs ~7 casts on any crystal, not "more casts on bigger crystals"). At 0
durability the crystal breaks; the between-combat sweep clears broken slots, then
`REPAIR_PER_BATTLE` (10) restores the survivors.

The pipeline is three files: `UBreakCalculator` (pure formula),
`DurabilityConstants` (tunables), `UCrystalManager` (live entry points + WOR_
debug suite, `UGameInstanceSubsystem`).

## Formula

**Wear percent** — `UBreakCalculator::CalculateWearPercentOfMax(CrystalTier, ActionTier, InfusionLevel, bIsSpell)` is the single source of the percent (base amount, detailed results, and all UI/debug derive from it — they can't drift):
- Tier mismatch: `SPELL_WEAR_PCT_PER_GAP (0.07)` / `ABILITY_WEAR_PCT_PER_GAP (0.03)`
  per tier the action is above the crystal (negative/zero gaps → 0).
- Infusion: spells add `SPELL_L1/L2_INFUSION_PCT (0.05/0.10)`; **abilities add nothing**
  (`ABILITY_L1/L2_INFUSION_PCT = 0`).
- Stacks additively (0–1 fraction). Within-tier uninfused → 0%.

**Base wear** — `CalculateDurabilityWear(...)` = `round( percent × max_durability_for_crystal_tier )`.

**Substat modifier** — `CalculateDurabilityWearWithSubstats(..., SpellDamageFrac, StatusMultiplierFrac, EfficiencyFrac, ResistanceFrac)`:

```
final = max( floor, base × power_factor / control_factor )      // NO ceiling
        then: if (tier_gap > 0 && final < 1) final = 1          // min-1 mismatch floor

power_factor   = clamp( 1 + (SpellDamageFrac + StatusMultiplierFrac) × AMP, PF_MIN, PF_MAX )
control_factor = clamp( 1 + (EfficiencyFrac   + ResistanceFrac)      × AMP, CF_MIN, CF_MAX )
floor          = FLOOR_FRAC × base
```

`base == 0` short-circuits — but a **real mismatch** (`tier_gap > 0`) whose base
rounds to 0 still floors to **1** (a mismatch always costs something); matched /
over-spec (`tier_gap ≤ 0`) returns exactly 0. **No ceiling**: shatter emerges
naturally from the percent math — a sufficiently over-tiered action produces wear
≥ durability and breaks the crystal in one cast (the old explicit 45%-of-max cap
and the `tier_gap ≥ ONE_SHOT_GAP` branch were removed; the branches collapsed to a
single `max(floor, raw)` path). `power_factor` is now clamped at **both** ends
(`PF_MAX = 3.0`), symmetric with control — a geared caster amplifies wear at most
×3, not unbounded; shatter stays driven by tier mismatch, not raw gear.
`EfficiencyFrac` is **inverted**: callers pass
`1 - GetEvolutionModifiedEfficiencyMultiplier()` so a disciplined caster's *lower*
multiplier reads as *positive* control input.

`FDurabilityWearWithSubstatsResult` surfaces **`WearPercentOfMax`** (the 0–1
authored fraction, for UI / `WouldBreakCrystal` preview) and **`bMinFloored`**
(true when the min-1 floor fired).

## Constants (`Durabilityconstants.h`)

| Constant | Value | Role |
|---|---|---|
| `SPELL_WEAR_PCT_PER_GAP` | 0.07 | Spell wear (% of max) per tier of mismatch |
| `ABILITY_WEAR_PCT_PER_GAP` | 0.03 | Ability/attack wear (% of max) per tier of mismatch |
| `SPELL_L1_INFUSION_PCT` / `_L2_` | 0.05 / 0.10 | Spell infusion wear (% of max), L1 / L2 |
| `ABILITY_L1_INFUSION_PCT` / `_L2_` | 0.0 / 0.0 | Abilities have **no** infusion wear add-on |
| `SUBSTAT_AMP` | 5.0 | Sensitivity of factors per 1.0 of substat fraction |
| `SUBSTAT_POWER_FACTOR_MIN` / `_MAX` | 0.4 / 3.0 | Power factor clamp (**MAX added this rework**, ≤×3 amplify) |
| `SUBSTAT_CONTROL_FACTOR_MIN` / `_MAX` | 0.5 / 3.0 | Control factor clamp |
| `SUBSTAT_FLOOR_FRAC` | 0.25 | Final wear ≥ 25% of base (when base > 0); on top, min-1 on mismatch |
| `REPAIR_PER_BATTLE` | 10 | Between-combat auto-repair per surviving crystal |
| `CUSTOM_SPELL_WEAR` | 2 | Flat int; **defined but unused** by the live caller |
| ~~`SUBSTAT_CEIL_FRAC`~~ | 0.45 | **Retired** — 45%-of-max ceiling removed; constant now unreferenced (kept in header, flagged for a dead-constant sweep) |
| ~~`SUBSTAT_ONE_SHOT_GAP`~~ | 4 | **Retired** — one-shot branch collapsed (shatter is now intrinsic); constant now unreferenced |

Numbers are first-pass — tune in-engine via the WOR_ suite.

## Entry points

Two live paths on `UCrystalManager`. Both read the four crystal-modified
fractions off the caster's `UCharacterDataComponent` (gear-inclusive) and
fall back to the base formula if `CharacterData` is missing.

- `ProcessPostCastWear(Actor, Holder, FRuntimeAttachedItem &Attachment, ActionTier, InfusionLevel, bIsSpell)`
  — refined crystals AND elemental fusions (see *Fusion wear* below). Called from
  three sites in `UActionExecutor::ApplyCommitCosts`. Includes a Luck-skip roll and
  broadcasts `OnCrystalBroken` / `OnCrystalDurabilityChanged` per cast.
- `ProcessPostCastEvolutionWear(Actor, ULoadoutComponent*, ActionTier, InfusionLevel, bIsSpell)`
  — standalone primary-slot evolution (`PrimarySlotType == Evolution`, e.g.
  Broken Darkness). Called from `UActionExecutor::ExecuteSpellAsync` after
  `SpendEnergy` succeeds. Writes via `ULoadoutComponent::ApplyWearToActivePrimaryEvolution(_, bForceWear=true)`.
  Now rolls the **same Luck-skip** as the refined path (shared `LUCK_BREAK_SKIP_MAX`
  ceiling, early return before durability is applied — added so Luck protects all
  crystal types consistently). Still no per-cast broadcast (between-combat sweep
  clears broken slots via `ClearBrokenPrimaryEvolution`). The `bForceWear` flag
  bypasses the per-asset `bCanBreak` gate — BD's mechanic is intrinsic, so
  per-asset opt-in would silently fail.

## Fusion wear

Elemental fusions (gem + stone half) wear and break in production. The
`ProcessPostCastWear` gate is `IsCrystal() || (IsFusion() && Fusion.HasGemHalf())`
— augmented (stone+stone) fusions never wear, preserving augment-stone no-wear
semantics. Wear math keys off the **gem half's tier** (matching
`FFusionAttachment::GetMaxDurability`'s keying — the stone half's tier already
feeds durability via the two-tier matrix bonus, so wear-keying it too would
double-count). Cadence is identical to crystals: per infused action through that
catalyst.

**Break = dead stat, at the break instant.** The `bBroke` branch fires, for
fusions only, `UTurnManager::OnActorSpeedChanged` + `RecomputeMaxPools`
unconditionally — the attachment is already broken there, so the stat-read
guards (`GetAttachedStonePercent` / `...ForType` IsBroken early-returns) read 0
and the recompute lands the loss immediately. Weapon breaks need this hook;
ring breaks also recompute via RingManager's auto-switch (idempotent double).
Plain crystals carry no stat percents, so their breaks need no hook.

`FBrokenCrystalPayload` carries an appended `FFusionId` (Kind == Fusion; the
gem half doubles as `CrystalId` for Kind-unaware listeners). RingManager's
break consumer resolves the `FUSION gem+stone` name, and both debug commands
handle fusions: `DebugBreakActiveCrystal` accepts elemental fusions (same gate
as production), `DebugForceWearActiveCrystal` labels the FUSION branch with a
truthful wears/never-wears string.

## EP-vs-wear cost split

`UActionExecutor::CalculateActionEnergyCost` decides what a cast pays:

| `ESpellSource` | Non-BD | BD |
|---|---|---|
| `Innate` | Full EP | Full EP |
| `Evolution` (primary-slot) | Full EP + HP backlash + self-status | **0 EP**, durability wear — *except* `SpellInfusionLevel ≥ 1` with `SelectedSource == Innate` (Darkness conversion): full EP **plus** wear |
| `RingCrystal` | 0 EP, wear | 0 EP, wear |
| `WeaponCrystal` | 0 EP, wear | 0 EP, wear |

The BD Innate-source carve-out captures *absorbed energy converting the spell's
element* — the only case where a BD evolution cast pays EP. Full energy-model
detail: `BrokenDarkness.md`.

## Breakability (`EBreakability`)

Who can wear down (and thus break) an evolution crystal is a 3-state enum
(`EBreakability.h`, rework 6-1), authored per evolution asset:

| Value | Meaning |
|---|---|
| `Unbreakable` | No one — never wears, not even Broken Darkness. |
| `Breakable` | Any class wears it down. |
| `BDBreakable` (default) | Only Broken Darkness / Reality wielders wear it (the prior `bCanBreak = false` behaviour). |

The wear pipeline gates on this via the `bForceWear` flag — `BDBreakable` crystals
only take wear when the caster `IsBrokenDarkness()` or is Reality
(`CrystalManager::ProcessPostCastEvolutionWear`). `GetBreakabilityString` gives the
inline debug label.

## Debug suite

`UGameInstanceSubsystem` is not in the `FExec` chain, so the live entry points
live on `ACombatOrchestrator` and forward to the manager. Three methods on
`UCrystalManager`:

- `WOR_WearTable()` — substat-modified prediction table (rows F→S) for the
  active combatant. No wear applied. Worst-case envelope: S-tier, L2, Spell.
  Powered by `UBreakCalculatorDebug::PrintWearTableWithSubstats`.
- `WOR_SimCast(ActionTier, InfusionLevel)` — runs the real `ProcessPostCastWear`
  path once on the active combatant's primary weapon crystal. Logs BEFORE /
  PREDICT / AFTER — decisive end-to-end check vs the prediction table.
- `WOR_CrystalState()` — prints the equipped primary-weapon crystal: type,
  tier, `bCanBreak`, current/max durability.

`ACombatOrchestrator` exposes four `CallInEditor` buttons under category
`Debug|CrystalWear` (Details panel during PIE): `DebugCrystalState`,
`DebugWearTable`, `DebugSimCast_S_L2` (worst-case live wear),
`DebugSimCast_Matched_L1` (isolates infusion-only wear by resolving the
equipped crystal's own tier).

Two `UFUNCTION(Exec)` console commands on `UCrystalManager` for raw testing:
`DebugBreakActiveCrystal` (force-routes through the production pipeline) and
`DebugForceWearActiveCrystal Amount` (raw `FRuntimeAttachedItem::ApplyWear`,
bypasses tier math and Luck-skip).

## Cross-links

- `BrokenDarkness.md` — full BD energy model and the wear-as-cost rationale.
- `CharacterDataSystem.md` — the four crystal-modified pillar accessors and
  case-B `ApplyEvolutionPillarModifier` branch.
- `LoadoutSystem.md` — `ApplyWearToActivePrimaryEvolution` / `ClearBrokenPrimaryEvolution`
  writers and `FCombatLoadout::PrimaryEvolution` shape.
- `CombatOrchestrator.md` — between-combat destruction + repair sweep.
- `InfusionSystem.md` — the infusion system as a whole; durability is one of its three cost axes.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-27 | Initial documentation — substat wear modifier, two-path entry points (refined vs case-B primary evolution), BD EP-vs-wear cost split with Innate-conversion carve-out, WOR_ debug suite | feature/crystal-wear-substat-modifier |
| 2026-06-11 | Fusion wear pipeline: elemental fusions wear + break in production (gate widened, gem-half tier keys wear, augmented fusions stay never-wear). Break fires speed-notify + `RecomputeMaxPools` at the break instant — the production trigger for the broken-fusion stat guards. `FBrokenCrystalPayload.FusionId` appended; RingManager + both debug commands fusion-aware. | feature/fusion-wear-pipeline |
| 2026-06-18 | Infusion rework: documented the `EBreakability` enum (6-1, Unbreakable/Breakable/BDBreakable default) and its `bForceWear` gate. Confirmed the EP-vs-wear split still reflects crystal-zero-EP (6-2-2 — crystal sources pay durability, not EP). Cross-links `InfusionSystem.md`. | feature/realtime-defense |
| 2026-06-22 | **DurabilityWearPercentRework** (4 commits). Wear is now **percent-of-max** (spell 7%/gap, ability 3%/gap, spell infusion +5/+10, abilities none) via the shared `CalculateWearPercentOfMax` helper, so casts-to-break is tier-consistent. `PowerFactor` capped at `SUBSTAT_POWER_FACTOR_MAX (3.0)`. The 45%-of-max **ceiling removed** (branches collapsed; shatter is intrinsic to the percent math); `SUBSTAT_CEIL_FRAC` + `SUBSTAT_ONE_SHOT_GAP` now unreferenced. Added a **min-1 mismatch floor** (`tier_gap>0 → ≥1 wear`, robust even on the `base==0` early return). Luck wear-skip **extended to the evolution path**. Surfaced `WearPercentOfMax` + `bMinFloored` on `FDurabilityWearWithSubstatsResult` for UI/preview; debug suite + `WOR_SimCast` regenerated to the percent model. | feature/durability-percent-wear |
