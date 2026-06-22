# Reroll Economy

**Status:** [Built · No UI] — storage and the reroll API exist, but the fill-and-reroll **trigger and UI are unbuilt**. Owning code: `UEquipmentBonusGenerator::Reroll*`, instance `Stat/ResistancePool`.

## The intended loop

Each rolled gear instance carries **reroll pools**:

- `StatPool` / `StatMaxPool` — fills toward a reroll of the stat bonus.
- `ResistancePool` / `ResistanceMaxPool` — a separate pool for the resistance bonus (evolutions).

The design: rewards **fill** the pool; at `Pool == MaxPool` you **spend it to reroll** the instance's bonuses (drawing a fresh roll from the stored budget).

## What's built vs not

- **Built:** the pool fields on each instance + the reroll API (`UEquipmentBonusGenerator::Reroll*`).
- **Not built:** the reward flow that fills pools, the reroll trigger, and any UI. Today nothing fills or spends a pool in play.

## Entry points

- `UEquipmentBonusGenerator::Reroll*` — the reroll API.
- Instance `StatPool`/`StatMaxPool`, `ResistancePool`/`ResistanceMaxPool`.

## Related

- [Per-instance rolls](./PerInstanceRolls.md) · [Inventory](./Inventory.md) · [`../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md)
