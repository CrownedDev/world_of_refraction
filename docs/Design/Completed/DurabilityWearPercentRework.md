# Durability Wear — Percent-of-Max Rework

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> This supersedes the flat-integer wear model once shipped. Until then, the live mechanic is
> [`docs/Mechanics/Gear/DurabilityWear.md`](../../Mechanics/Gear/DurabilityWear.md) — that doc stays authoritative
> until this builds, at which point it is rewritten and this moves to `docs/Design/Completed/`.

## What changes

Wear stops being a flat integer (`gap × 3`, `+12` for an L2 spell, etc.) and becomes a **percentage of the
crystal's max durability**. Casts-to-break then stays consistent across crystal tiers instead of swinging with
the crystal's absolute durability value.

The **direction is unchanged**: wear only applies when the action out-tiers the crystal
(`TierGap = ActionTier − CrystalTier`, mismatch only when `> 0`). Crystal tier ≥ action tier → 0 wear.

## Locked rules

### Mismatch (per tier the action exceeds the crystal)

| Action type     | Per-gap wear |
| --------------- | ------------ |
| Spell           | **7%** of max |
| Ability / Attack | **3%** of max |

### Infusion add-on (flat, added on top of mismatch)

| Action type     | L1   | L2   |
| --------------- | ---- | ---- |
| Spell           | **+5%** | **+10%** |
| Ability / Attack | **none** | **none** |

- **Spells strain crystals inherently** — they create from nothing, so infusion costs extra on top of mismatch.
- **Abilities/attacks only strain a *mismatched* crystal.** A matched (or overtiered) crystal infuses an
  ability for **free** — the strain is "powerful weapon channelling through a weak crystal," which the
  weapon-tier mismatch term already captures. There is no flat infusion add-on for abilities.

### Action-tier source (wear convention — unchanged)

- **Spell** → `SpellData->Tier` (the spell's own tier, regardless of catalyst).
- **Ability / Attack** → `Weapon->Tier` (action inherits the weapon's tier).

> Note: this is the **wear** convention. The **damage** tier-gap system uses the skill's own tier for
> abilities — a deliberate, documented split (`PhaseRunnerCombatRework.md`). The two systems are independent;
> do not unify them.

### Ceiling — REMOVED

The old 45%-of-max single-cast ceiling is **gone**. The **one-shot-gap rule** (`SUBSTAT_ONE_SHOT_GAP`, gap ≥ 4)
is now the sole shatter governor: a sufficiently overtiered action can shatter a crystal in one cast, and
nothing clamps it. The floor (minimum wear) is retained.

### Final wear

```
TotalPct  = MismatchPct + InfusionPct          // from the tables above
FinalWear = round(TotalPct × MaxDurabilityForTier(CrystalTier))
```

## Wear tables (% of crystal max durability, plain / no infusion)

Rows = action tier, columns = crystal tier. Upper-right triangle (crystal ≥ action) = 0.

### Spell — 7% per gap

| Spell ↓ / Crystal → | F  | E  | D  | C  | B  | A  | S |
| ------------------- | -- | -- | -- | -- | -- | -- | - |
| F                   | 0  | 0  | 0  | 0  | 0  | 0  | 0 |
| E                   | 7  | 0  | 0  | 0  | 0  | 0  | 0 |
| D                   | 14 | 7  | 0  | 0  | 0  | 0  | 0 |
| C                   | 21 | 14 | 7  | 0  | 0  | 0  | 0 |
| B                   | 28 | 21 | 14 | 7  | 0  | 0  | 0 |
| A                   | 35 | 28 | 21 | 14 | 7  | 0  | 0 |
| S                   | 42 | 35 | 28 | 21 | 14 | 7  | 0 |

Add infusion on top of any cell: **+5% (L1), +10% (L2).**

### Ability / Attack — 3% per gap

(Action tier = the **weapon's** tier.)

| Ability ↓ / Crystal → | F  | E  | D  | C  | B  | A  | S |
| --------------------- | -- | -- | -- | -- | -- | -- | - |
| F                     | 0  | 0  | 0  | 0  | 0  | 0  | 0 |
| E                     | 3  | 0  | 0  | 0  | 0  | 0  | 0 |
| D                     | 6  | 3  | 0  | 0  | 0  | 0  | 0 |
| C                     | 9  | 6  | 3  | 0  | 0  | 0  | 0 |
| B                     | 12 | 9  | 6  | 3  | 0  | 0  | 0 |
| A                     | 15 | 12 | 9  | 6  | 3  | 0  | 0 |
| S                     | 18 | 15 | 12 | 9  | 6  | 3  | 0 |

No infusion add-on for abilities/attacks.

## Worked examples (F-tier crystal, max durability 30)

| Action                | Total % | Wear (round) | Casts to break |
| --------------------- | ------- | ------------ | -------------- |
| S spell, plain        | 42      | 13           | ~2.4           |
| S spell, L1           | 47      | 14           | ~2             |
| S spell, L2           | 52      | 16           | ~2             |
| S ability (S weapon)  | 18      | 5            | ~5.5           |
| Matched spell, plain  | 0       | 0            | never (no mismatch) |
| Matched ability       | 0       | 0            | never (no mismatch) |

S-spell-L2 at 52% (>45%) is exactly why the old ceiling was removed — it would have clipped this case.

## Build scope (for the survey)

Model swap on `BreakCalculator` Step A. Touches:

- `DurabilityConstants.h` — replace flat wear constants (`WEAR_PER_TIER_MISMATCH`, `SPELL_L1/L2_WEAR`,
  `ABILITY_L1/L2_WEAR`) with the percent constants (spell 7%/gap, ability 3%/gap, spell infusion +5/+10).
- `BreakCalculator.cpp/.h` — `CalculateDurabilityWear` returns `round(pct × MaxDurability)`; the substat
  ceiling clamp is removed (floor + one-shot-gap retained). Return contract / detailed structs may need the
  percent surfaced for UI.
- Substat power/control modulation — **carry-over decision needed at build time:** does the geared-caster
  power factor still multiply the percent-derived base? If yes, a powerful caster pushes past the authored %
  (and with no ceiling, shatters more readily). Resolve before staging.
- `BreakCalculatorDebug` + `WOR_` debug suite + worked-example regen.

Survey-first (cross-system, examples + debug tables regen).

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: percent-of-max wear model, spell 7%/ability 3% per gap, spell infusion +5/+10, abilities no infusion add-on, ceiling removed (one-shot-gap sole shatter governor). Not yet built. | (tbd) |
