# Broken Darkness — Reactive Defense Changes (Planned)

**Status:** Planned. To be built after the reactive-defense per-hit foundation (Stage 3) is stable.

Captures BD-specific changes arising from the reactive per-impact defense work. This is a PLANNED-
CHANGES doc — the living BD architecture is docs/Architecture/BrokenDarkness.md; the defense-side
mechanism detail is in docs/Design/RealTimeDefenseRework.md (§8c, §13).

## Context
BD's defining mechanic: on a successful PARRY or BLOCK, it absorbs ENERGY from the attack (no damage
reflection — reflection is a dropped stale path). Energy → CurrentEP (unified, per BrokenDarkness.md),
ParryAbsorptionRate 1.0 / BlockAbsorptionRate 0.5, no passive regen, overload above MaxEP. This is
the EXISTING design.

## Planned change — per-impact energy absorption (proportional to the damage split)
With reactive defense making damage land PER IMPACT (Stage 2) and defendable PER IMPACT (Stage 3), BD
absorption becomes per-impact too:
- BD absorption stays based on the attack's ENERGY COST (matches the existing
  CalculateAbsorptionEnergy(DefenseType, AttackEnergyCost) — NOT a switch to damage-based).
- The attack's energy cost is SPLIT ACROSS the impacts, PROPORTIONAL TO THE DAMAGE SPLIT
  (ResolvedDamageSplit). An impact carrying X% of the damage carries X% of the energy cost.
- When BD PARRIES or BLOCKS a given impact, it absorbs that impact's energy-cost share × the rate:
  (AttackEnergyCost × Split[Index]%) × (ParryAbsorptionRate | BlockAbsorptionRate).
- So a multi-hit attack feeds BD energy PER landed parry/block, scaled per impact — and a bigger hit
  (bigger damage share) feeds more energy. The same ResolvedDamageSplit drives both the per-impact
  damage AND the per-impact energy absorption.

## Why
- Consistent with the per-impact model (Stage 2/3): everything resolves per impact, including BD's
  energy gain.
- Rewards defending the bigger hits in a combo (they carry more energy).
- Reflection stays dropped — BD is purely energy-absorption.

## Enabled by / depends on
- Stage 2 (done): per-impact damage apply (ResolvedDamageSplit per impact).
- Stage 3 (next): per-impact defense input/resolution (knowing WHICH impacts were parried/blocked).
- Hooks into the per-impact apply path (ApplyOneImpact): on a parried/blocked impact for a BD
  defender, grant the proportional energy-cost share × rate.

## Sequencing
Build AFTER Stage 3 (needs per-impact defense outcomes — which impacts were parried/blocked). It's a
BD-mechanic refinement layered on the per-impact defense foundation, not core defense plumbing.

## When built
Fold the resulting behavior into docs/Architecture/BrokenDarkness.md (the living architecture) and
mark this doc done.
