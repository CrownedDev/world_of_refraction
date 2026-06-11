# Crystal Wear

## Overview

Equipment crystals (refined or evolution) accrue **durability wear** on each
post-cast event — tier mismatch, infusion levels — modulated by the caster's
real power output and control. At 0 durability the crystal breaks; the
between-combat sweep clears broken slots, then `REPAIR_PER_BATTLE` (10)
restores the survivors.

The pipeline is three files: `UBreakCalculator` (pure formula),
`DurabilityConstants` (tunables), `UCrystalManager` (live entry points + WOR_
debug suite, `UGameInstanceSubsystem`).

## Formula

**Base wear** — `UBreakCalculator::CalculateDurabilityWear(CrystalTier, ActionTier, InfusionLevel, bIsSpell)`:
- Tier mismatch: `WEAR_PER_TIER_MISMATCH (3)` per tier the action is above the
  crystal (negative gaps → 0).
- Infusion: `ABILITY_L1/L2_WEAR (4/8)` or `SPELL_L1/L2_WEAR (6/12)`.
- Stacks additively. Within-tier uninfused → 0.

**Substat modifier** — `CalculateDurabilityWearWithSubstats(..., SpellDamageFrac, StatusMultiplierFrac, EfficiencyFrac, ResistanceFrac)`:

```
final = clamp( base × power_factor / control_factor, floor, ceiling )

power_factor   = max(  PF_MIN, 1 + (SpellDamageFrac + StatusMultiplierFrac) × AMP )
control_factor = clamp(1 + (EfficiencyFrac + ResistanceFrac) × AMP, CF_MIN, CF_MAX )
floor          = FLOOR_FRAC × base
ceiling        = (tier_gap < ONE_SHOT_GAP) ? CEIL_FRAC × max_durability_for_crystal_tier : +inf
```

`base == 0` short-circuits to `final = 0` — substats cannot manufacture wear
out of nothing. `tier_gap ≥ ONE_SHOT_GAP` lifts the ceiling so extreme
mismatch can shatter in one cast. `EfficiencyFrac` is **inverted**: callers
pass `1 - GetEvolutionModifiedEfficiencyMultiplier()` so a disciplined caster's
*lower* multiplier reads as *positive* control input.

## Constants (`Durabilityconstants.h`)

| Constant | Value | Role |
|---|---|---|
| `SUBSTAT_AMP` | 5.0 | Sensitivity of factors per 1.0 of substat fraction |
| `SUBSTAT_POWER_FACTOR_MIN` | 0.4 | Weak casters still pay meaningful wear |
| `SUBSTAT_CONTROL_FACTOR_MIN` | 0.5 | Undisciplined casters can't crater control |
| `SUBSTAT_CONTROL_FACTOR_MAX` | 3.0 | Skilled casters still pay some wear |
| `SUBSTAT_FLOOR_FRAC` | 0.25 | Final wear ≥ 25% of base (when base > 0) |
| `SUBSTAT_CEIL_FRAC` | 0.45 | Cap final at 45% of crystal max durability (sub-gap) |
| `SUBSTAT_ONE_SHOT_GAP` | 4 | Tier gap at/above which ceiling lifts |
| `REPAIR_PER_BATTLE` | 10 | Between-combat auto-repair per surviving crystal |

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
  Leaner — no Luck-skip, no per-cast broadcast (between-combat sweep clears
  broken slots via `ClearBrokenPrimaryEvolution`). The `bForceWear` flag
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

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-27 | Initial documentation — substat wear modifier, two-path entry points (refined vs case-B primary evolution), BD EP-vs-wear cost split with Innate-conversion carve-out, WOR_ debug suite | feature/crystal-wear-substat-modifier |
| 2026-06-11 | Fusion wear pipeline: elemental fusions wear + break in production (gate widened, gem-half tier keys wear, augmented fusions stay never-wear). Break fires speed-notify + `RecomputeMaxPools` at the break instant — the production trigger for the broken-fusion stat guards. `FBrokenCrystalPayload.FusionId` appended; RingManager + both debug commands fusion-aware. | feature/fusion-wear-pipeline |
