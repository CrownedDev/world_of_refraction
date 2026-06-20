# Broken Darkness — Deterministic Strain Trigger

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> Replaces the random `RollForBreak` transformation chance with a deterministic, persistent strain
> accrual. When shipped, updates [`docs/Architecture/BrokenDarkness.md`](../Architecture/BrokenDarkness.md)
> and this moves to `docs/Design/Completed/`.

## What changes

Today an innate-Darkness Caster who **overreaches** (casts above stat requirements, or casts infused)
rolls a **random** chance to break into BD — tier-keyed (S=1.5% … E/F=0%), infusion-multiplied
(L1×1.5, L2×2.0), independent each cast, no memory.

This replaces the dice with **strain**: each overreaching cast adds to a hidden, persistent strain
accumulation. At 100% the character deterministically breaks into BD. Same accrue-until-break shape as
crystal durability wear — applied to the caster instead of a crystal.

## Locked rules

- **Deterministic** — no roll. Overreach accrues; the break is predictable.
- **Hidden** — no UI bar. BD arrives as a sudden snap from pushing too far (matches the fiction).
- **Persistent** — does not reset between combats. Overreach accumulates across the whole run until it
  gives.
- **Stored as a percentage** (0–100%), NOT a raw number — so pool changes (leveling MaxEP) never grant or
  steal progress. 80% full stays 80% full when the pool resizes.
- **Resets to 0 on break** (once BD, the strain accounting is spent).
- **Resets to 0 on revert** BD→Darkness (clean slate, ready for the next BD run).
- **Eligibility unchanged** — only innate-Darkness Casters who aren't already BD accrue strain (same gate
  as the current roll).
- **Trigger gate unchanged** — strain accrues on the same events the roll fired on: a cast above stat
  requirements, an L1/L2-infused spell, or an over-requirement Darkness-infused ability. A within-means,
  uninfused cast adds nothing.

## The formula

```
StrainPerCast = TierStrain × (1 + SpellDamageFrac) × (1 − DefenceFrac) × InfusionMult
StrainAdded%  = StrainPerCast ÷ CurrentMaxEP
Accrued%     += StrainAdded%
Break into BD when Accrued% ≥ 100%
```

One lever per pillar:

| Pillar | Stat | Effect on strain | Range |
| ------ | ---- | ---------------- | ----- |
| **Mind** | Spell Damage | **raises** strain per cast (pushing magic harder strains more) | frac 0 → 0.5 |
| **Body** | Defence | **lowers** strain per cast (physical resilience endures it) | frac 0 → 0.5 |
| **Spirit** | Max Energy | the **pool** — bigger vessel absorbs more overreach | 50 → 1000 |

- `SpellDamageFrac = CalculateSpellDamage() − 1.0`
- `DefenceFrac = CalculateFlatDefense()`
- `CurrentMaxEP = CalculateMaxEnergy()`
- `InfusionMult` — L0 ×1.0, L1 ×1.5, L2 ×2.0 (inherited from the old roll)

MaxEP only matters **at the moment of a cast**, to convert that cast's flat strain into a percentage
slice. Spending EP never touches strain — the two bars are independent. MaxEP is the *fuse length*, not
the fuse.

## Tier-strain curve

Anchored at the **floor** (base stats: MaxEP 50, SpellDmg 1.0, Defence 0): an S-rank cast = 1.5 casts to
break → S-strain = 50 ÷ 1.5 = **33.3**. Lower tiers inherit the existing BD transform-chance ratios
(S=1.5%, A=1.0%, B=0.6%, C=0.3%, D=0.1%, E/F=0), so current balance feel is preserved.

| Spell tier | TierStrain | Casts to BD (floor) |
| ---------- | ---------- | ------------------- |
| S | 33.3 | 1.5 |
| A | 22.2 | 2.25 |
| B | 13.3 | 3.75 |
| C | 6.7 | 7.5 |
| D | 2.2 | 22.5 |
| E / F | 0 | never |

Abilities use their own tier (consistent with the tier-scaling consolidation flip).

## How it feels across builds (S-rank, no infusion)

| Build | MaxEP | SpellDmg | Defence | Strain/cast | Casts to BD |
| ----- | ----- | -------- | ------- | ----------- | ----------- |
| Floor | 50 | 1.0 | 0 | 33.3 | 1.5 |
| Mid | 400 | 1.2 | 0.3 | 28.0 | ~14 |
| Tank (EP+Def) | 800 | 1.0 | 0.5 | 16.7 | ~48 |
| Glass cannon | 150 | 1.5 | 0 | 50.0 | ~3 |

A fragile, magic-heavy caster snaps into BD fast; an energy/defence-invested one endures a long run of
overreach. All three levers point the right way.

## Build scope (for the survey)

- **New persistent strain field** (percentage). Must survive between combats → lives in **save/persistent
  state**, not transient runtime only. Exact home (character save / inventory data) confirmed at survey.
- **New strain constants** — the tier-strain table + floor anchor, own namespace (mirror
  `BrokenDarknessConstants`).
- **`AddStrain(ActionTier, InfusionLevel)`** on `UBrokenDarknessManager` — computes the % slice, accrues,
  triggers `TriggerTransformation()` at ≥100%.
- **Repoint trigger sites** — `ActionExecutor::CheckBrokenDarknessBreak` calls `AddStrain` instead of
  `RollForBreak`. Same eligibility/trigger conditions.
- **Reset hooks** — strain → 0 in both `TriggerTransformation` and `RevertTransformation`.
- **Retire (wrap, don't delete until verified)** — `RollForBreak`, `GetBaseBreakChance`,
  `GetInfusionMultiplier`, `BREAK_CHANCE_*` constants.
- **Debug tooling** — since the bar is hidden, ship a console readout (current strain %, projected
  casts-to-break for a given tier) mirroring `WoR.AbsorptionSnapshot`.

Cross-system → survey-first.

## Open / carry-over

- **Persistence home** — confirm where per-character run state is saved.
- **Decay** — locked as monotonic (only rises until break); no per-battle recovery. Flagged here in case
  PIE shows it needs a slow bleed-off.
- **Revert trigger** — the BD→Darkness trigger (healer/item) is still unbuilt; the strain reset hangs off
  `RevertTransformation` whenever that trigger lands.

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: deterministic persistent strain replaces the random BD transform roll. Pool = MaxEP, stored as %, strain = TierStrain × (1+SpellDmg%) × (1−Defence%) × InfusionMult, floor = 1.5 S-casts, hidden, resets on break + revert. Not yet built. | (tbd) |
