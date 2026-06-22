# Per-Instance Rolls

**Status:** [Built · No UI] — rolls happen at acquisition, but there's no screen to view, compare, or equip a rolled instance. Owning code: `UEquipmentBonusGenerator`, `GeneratedStatBonus`.

## What it is

When you acquire a piece of gear (weapon / ring / evolution) with `bRandomGenerateOnPickup` set, it **rolls fresh stat bonuses** for that **instance**. Two copies of the same item can roll differently — you find "good" and "bad" drops.

- **Rolled values live on the instance**, not the asset — `GeneratedStatBonus` (and `GeneratedResistance` for evolutions), separate from the authored `BaseStatBonus`.
- **Rolls at acquisition** — `UEquipmentBonusGenerator::Generate` / `GenerateWeighted` (+ `GenerateResistance`).
- Asset-side preview is **inert**; the live roll is on the owned instance.

## Reachability

The roll itself runs at pickup, but **viewing / comparing / choosing which rolled instance to equip has no UI** — `FSavedLoadout` instance-refs are built but inert until an equip screen exists. See [`../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).

## Entry points

- `UEquipmentBonusGenerator` — `Generate`, `GenerateWeighted`, `GenerateResistance`.
- `GeneratedStatBonus` / `GeneratedResistance` on the inventory/attachment instance.

## Related

- [Inventory](./Inventory.md) · [Reroll economy](./RerollEconomy.md) · [Stats](../Character/Stats.md) · [`../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md)
