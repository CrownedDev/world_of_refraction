# Requirement Gap Scaling — Per-Pillar Substat Tier-Gap

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> Sibling of [`TierScalingConsolidation.md`](./TierScalingConsolidation.md) — shares the same tier-gap
> ladder and the `WorldLevelToTier` helper. May merge into that arc at build time. When shipped, replaces
> the √deficit penalty in `WorldStatRequirements` and this moves to `docs/Design/Completed/`.

## What changes

Today, casting/using a skill **under its requirements** applies a single global penalty:

```
Penalty   = √(TotalDeficit) × 0.10, capped 60%   // CalculatePenalty
Damage   ×= (1 − Penalty)
Energy   ×= (1 + Penalty)
```

It's binary in spirit (one lump penalty from summed deficit), front-loads the pain via √, over-stat does
**nothing**, and the 60% cap is unreachable (max real deficit 21 → 46%).

This replaces it with the **tier-gap ladder run per pillar**: your world level on each pillar is read as a
tier, gapped against the skill's required level on that pillar, and the resulting multiplier scales **that
pillar's substats**. Match → expected output. Over-stat → that pillar's substats enhanced (proficiency).
Under-stat → that pillar's substats weakened.

Requirements themselves are **unchanged** — still authored as per-pillar world levels (Mind/Body/Spirit,
0–7). The requirement *is* the world-stat threshold; nothing is tier-derived on the authoring side.

## The helper

`WorldLevelToTier(int 0–7) → EItemTier`:

| World level | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| ----------- | - | - | - | - | - | - | - | - |
| Tier | F | E | D | C | B | A | S | above-S (headroom) |

Level 7 sits above S, so a maxed pillar always clears any requirement with room to spare. The conversion
is a 1:1 label over the existing 0–7 range — the gap math below is just `level − level`, the tier framing
is for display + reusing the shared ladder.

## The gap (note the sign convention)

Per pillar, **independently**:

```
RequirementGap = RequiredLevel − CharWorldLevel        // per pillar
```

This is the **reverse sign** of the damage gap, so the *same ladder table is reused verbatim*:

- Char **over** requirement → negative gap → ladder boost (×>1.0). Proficiency rewarded.
- Char **under** requirement → positive gap → ladder penalty (×<1.0). Underspec punished.

| RequirementGap | ≤−4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | ≥+4 |
| -------------- | --- | -- | -- | -- | - | -- | -- | -- | --- |
| × | 1.30 | 1.20 | 1.13 | 1.06 | 1.00 | 0.90 | 0.78 | 0.64 | 0.50 |

(Char 6 vs req 4 → gap −2 → ×1.13. Char 2 vs req 4 → gap +2 → ×0.78. Confirms the direction.)

## What the multiplier scales

The per-pillar multiplier converts to an additive percent (`pct = mult − 1.0`) and feeds the **existing**
`FActionStatModifiers::AddPillarPercent(MindPct, BodyPct, SpiritPct)` path — the same one Evolution
pillar-mode crystals already use. It applies to that pillar's substat family:

| Pillar | Substats scaled (defers to the live pillar definition — single source of truth) |
| ------ | ------------------------------------------------------------------------------- |
| **Mind** | SpellDamage, Efficiency, CritDamage, SpellSpeed |
| **Body** | Defense, RawDamage, ActionSpeed, Reflex |
| **Spirit** | Resistance, Luck, StatusMultiplier |

Pools (MaxHealth, MaxEnergy) and TurnSpeed stay excluded, exactly as they already are from action-time
modifiers.

Because the gap feeds substats rather than a flat damage number, the effect cascades naturally: over-stat
on Mind makes a spell hit harder (SpellDamage) **and** cost less (Efficiency) **and** crit harder. One gap,
whole-pillar proficiency.

## Worked examples

Spell requires **Mind 4 (B)**, no Body/Spirit requirement:

| Char Mind level | Gap | × | Effect |
| --------------- | --- | - | ------ |
| 6 (S) | −2 | 1.13 | Mind substats +13% — harder, cheaper, critsier |
| 4 (B) | 0 | 1.00 | exactly the authored output |
| 2 (D) | +2 | 0.78 | Mind substats −22% — weaker, pricier |

**777 player, 000 skill** (no requirements, all pillars required 0): gap −7 on every pillar → clamp → ×1.30
on all three. All substats +30%. **Intended** — proficiency means even trivial skills come off cleaner;
the 000 skill's tiny base numbers self-limit the absolute payoff. No low-requirement carve-out.

## Symmetric, no carve-out

Both sides of the ladder stay live — over-stat boosts, under-stat penalizes, clamped at ±4 gap. No special
handling for low-requirement skills (their small bases handle it). Consistent with the weapon/crystal
tier-gap.

## Build scope (for the survey)

- **`WorldLevelToTier`** helper (shared with `TierScalingConsolidation`).
- **Per-pillar requirement gap** → ladder lookup → three percents → `AddPillarPercent` on the action's
  `FActionStatModifiers` at action assembly.
- **Retire (wrap, don't delete until verified)** — `CalculatePenalty` and its application in
  `SpellData::CalculateDamage`/`CalculateEnergyCost` and `SkillDataBase::CalculateDamage`/
  `CalculateEnergyCost` (the `(1 ± penalty)` lines). Output now flows through the per-pillar modifier path.
- **Keep** `GetTotalDeficit` / `DoesSpellExceedRequirements` — the BD strain trigger still reads "any
  deficit" as its binary over-requirement gate (unchanged by this).
- **AI parity** — AI previews must see the per-pillar requirement modifiers.
- **Debug tooling** — requirement-gap readout (per-pillar gap, resulting × and substat deltas).

Cross-system → survey-first. Touches the requirement/penalty path + action-mod assembly + AI preview.

## Open / carry-over

- **Cost** — substats-only, or substats **+** the explicit reciprocal cost term from
  `TierScalingConsolidation`? Efficiency (Mind) already rides the pillar scaling, so cost nudges indirectly;
  the question is whether to *also* apply the explicit reciprocal cost swing on top. **Undecided.**
- **Pillar substat membership** — the build reads the live pillar definition, not the table above; if the
  Reflex/RawDamage/etc. assignments have shifted, the live definition wins.
- **BD strain interaction** — strain still triggers on binary over-requirement. Whether strain should scale
  with *deficit depth* (a deeper underspec strains harder) is a separate future idea, not in this arc.

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: √deficit requirement penalty replaced with per-pillar tier-gap substat scaling (RequirementGap = Required − CharLevel, shared ladder, AddPillarPercent feed). Symmetric, no low-req carve-out. Not yet built. | (tbd) |
