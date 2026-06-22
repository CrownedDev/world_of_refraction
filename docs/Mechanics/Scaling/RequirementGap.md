# Requirement Gap

**Status:** Live — shipped & PIE-verified (2026-06-22, branch `feature/requirement-gap-scaling`). Reference doc — describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`TierGap.md`](./TierGap.md) (the channel-mismatch axis — same ladder shape), [`TierPower.md`](./TierPower.md) (own-tier power). Build/design history: [`RequirementGapScaling.md`](../../Design/Completed/RequirementGapScaling.md).

## Concept

Using a skill you're **under-levelled** for weakens it; being **over-levelled** for it sharpens it. The
check is **per world pillar** (Mind / Body / Spirit), not a single global penalty — each pillar's gap scales
that pillar's own substats.

This replaced the old global √-deficit penalty (one lump damage/cost penalty from the summed deficit). The
old model front-loaded the pain via √, ignored over-stat entirely, and had an unreachable 60% cap. The
shipped model is **symmetric** (over-stat rewarded, under-stat punished), **per-pillar**, and cascades
naturally through substats.

## The gap (per pillar, independent)

```
RequirementGap = RequiredLevel − CharWorldLevel     // per pillar; raw world levels 0–7
```

- Char **over** requirement → **negative** gap → boost (×>1.0). Proficiency rewarded.
- **Matched** → gap 0 → ×1.00 → authored output, unchanged.
- Char **under** requirement → **positive** gap → penalty (×<1.0). Underspec punished.

## The ladder (±5 clamp)

Shared `TierGapConstants.h` → `TierGapDamage::GetRequirementGapMultiplier(int32 Gap)`. It's the damage/status
tier-gap ladder **extended by one rung at each end**: the requirement gap can reach ±7 (world levels 0–7),
so it clamps at **±5** rather than the tier-gap's ±4.

| Gap | ≤−5 | −4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | +4 | ≥+5 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| × | **1.40** | 1.30 | 1.20 | 1.13 | 1.06 | **1.00** | 0.90 | 0.78 | 0.64 | 0.50 | **0.32** |

The −4..+4 rungs reuse the existing tier-gap constants by name; only the ±5 ends are new
(`REQ_GAP_NEG5 = 1.40`, `REQ_GAP_5 = 0.32`).

## What it scales — per-pillar substats (whole-pillar cascade)

The per-pillar multiplier converts to an additive percent (`pct = (mult − 1) × 100`) and feeds
`FActionStatModifiers::AddPillarPercent(MindPct, BodyPct, SpiritPct)` inside `ComputeActionStatModifiers`.
**Live pillar membership** (from `AddPillarPercent` — the single source of truth):

| Pillar | Substats scaled | Count |
|---|---|---|
| **Mind** | Efficiency, SpellDamage, StatusMultiplier, CritDamage, SpellSpeed | 5 |
| **Body** | Defense, ActionSpeed, RawDamage, Reflex | 4 |
| **Spirit** | Resistance, Luck | 2 |

Excluded: `TurnSpeed` (must not alter turn order) and pools (MaxHealth / MaxEnergy) — exactly as they're
already excluded from action-time modifiers.

Because the gap scales substats (not a flat damage number) the effect **cascades through the whole pillar**:
over-statting Mind makes a spell hit harder (SpellDamage), **cost less** (Efficiency), **and** crit harder
(CritDamage) — one gap, whole-pillar proficiency.

### Cost is substats-only

There is **no separate reciprocal cost term** for the requirement gap. Cost moves only because **Efficiency**
(a Mind substat) is scaled: over-stat Mind → more Efficiency → cheaper; under-stat → less → pricier. (The
reciprocal *channel* cost ladder is a different axis — see [`TierGap.md`](./TierGap.md).)

## Worked examples

Skill requires **Mind 4**, no Body/Spirit requirement:

| Char Mind | Gap | × | Effect on Mind substats |
|---|---|---|---|
| 6 | −2 | 1.13 | **+13%** — harder, cheaper, crittier |
| 4 | 0 | 1.00 | authored output (no change) |
| 2 | +2 | 0.78 | −22% — weaker, pricier |
| 7 vs req 0 | −7 → clamp −5 | 1.40 | +40% (extreme over-stat) |
| 0 vs req 5 | +5 | 0.32 | −68% (extreme under-stat) |

**777 player / 000 skill** (every pillar req 0, char level 7): gap −7 on all three → clamp → ×1.40 → all
substats +40%. Intended — proficiency means even trivial skills come off cleaner; the 000 skill's tiny base
self-limits the absolute payoff. No low-requirement carve-out.

## What's retained vs removed

- **Removed:** the `(1 − penalty)` damage / `(1 + penalty)` cost **application** at all 8 sites (SpellData,
  SkillDataBase, AbilityData ×3 shadow copies, and the AI's `DamageCalculator::CalculateAttackDamage`).
- **Retained:** `CalculatePenalty` / `GetTotalDeficit` / `DoesSpellExceedRequirements` — the **Broken
  Darkness strain gate** still reads "any deficit > 0" as its binary over-requirement trigger. Only the
  damage/cost *application* was removed; the deficit calculation itself stays.

## AI parity

Spell, ability, **and** attack estimates all run through the same
`UActionExecutor::ComputeActionStatModifiers`, so the AI sees the per-pillar requirement scaling for free —
it's the same code execution uses. The AI's duplicate attack estimator (`CalculateAttackDamage`) was
**retired** from the scoring path: attacks now route through the execution-accurate `EstimateAbilityDamage`
(they're `UAbilityData`), which also closed pre-existing tier-gap / tier-power AI attack drift.

## Source map

- Ladder + helper — `TierGapConstants.h` (`GetRequirementGapMultiplier`, `REQ_GAP_NEG5` / `REQ_GAP_5`).
- Per-pillar percents — `USkillDataBase::GetRequirementGapPillarPercents` (`SkillDataBase.cpp`).
- Wiring — `UActionExecutor::ComputeActionStatModifiers` → `AddPillarPercent`.
- Debug — `URequirementGapDebug` (`Debug/Combat/RequirementGapDebug.h/.cpp`).
