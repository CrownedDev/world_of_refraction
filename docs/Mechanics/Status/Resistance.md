# Resistance

**Status:** [Live] — for **status buildup**. Owning code: `UStatusBuildupManager::GetTotalStatusResistance`, `ClassInnateResistanceTable`.

> **Scope — read this.** "Resistance" here means **status-buildup resistance**: it **slows how fast your status bar fills**, per element and physical type. It does **NOT** reduce raw damage — **elemental DAMAGE weakness/resistance is [Stub]** (`ElementMultiplier` hardcoded `1.0`, `DamageCalculator.cpp:208`; see [Elements](../Magic/Elements.md)).

## What it does

When you're hit, incoming **buildup** is reduced by your total resistance for that element/physical type before it adds to the bar (see [Status buildup](./StatusBuildup.md)). Higher resistance → enemies cap your bar slower.

## Sources (summed)

`GetTotalStatusResistance` aggregates several sources, including:

- **Spirit** — the `Resistance` substat (a Spirit stat; see [Stats](../Character/Stats.md)).
- **Gear** — `BonusResistance` (equipment) + attached **Resistance Stone**.
- **Class-innate profile** — each class/element has a per-element + per-physical-type row (`ClassInnateResistanceTable`): e.g. Generic resists physical but is soft to magic; Resonator the inverse; Broken Darkness broadly fragile.
- **Effects** — `ResistanceBuff` / `ResistanceDebuff` skill effects shift it (and you can debuff an enemy's resistance to pressure them faster).

## Entry points

- `UStatusBuildupManager::GetTotalStatusResistance` — the single aggregate.
- `ClassInnateResistanceTable` — per-class/element/physical rows.

## Related

- [Status buildup](./StatusBuildup.md) (what it slows) · [Status effects](./StatusEffects.md) · [Elements](../Magic/Elements.md) (damage-vs-status split) · [Stats](../Character/Stats.md) (Spirit) · [`../Architecture/ResistanceSystem.md`](../../Architecture/ResistanceSystem.md)
