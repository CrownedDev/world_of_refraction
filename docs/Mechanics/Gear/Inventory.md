# Inventory

**Status:** [Built · No UI] — owned items exist in data; no in-game inventory screen. Owning code: `UInventoryComponent`, `InventoryConstants`.

## What it is

Your **ownership warehouse** — everything you possess: weapons, rings, crystals, fusions, evolution items, and consumables, with quantities. Distinct from your [Loadout](./Loadout.md): inventory is *what you own*, loadout is *what you bring to a fight*.

- **Per-instance gear** — owned weapons/rings/evolutions are **instances** (each with its own rolled stats), not just asset references. Two of the same weapon can carry different rolls. See [Per-instance rolls](./PerInstanceRolls.md).
- **Stackable consumables** — crystals/stones/items carry quantities (consumed on use).
- **Capacity** — `InventoryConstants` (e.g. `MAX_RING_INVENTORY_SLOTS`) bounds storage; this is **inventory capacity**, separate from the loadout slot-cost budget.

## Entry points

- `UInventoryComponent` — owned `Rings`, weapons, crystals, items; `RemoveItemCount`, `SavedLoadouts`.
- `InventoryConstants` — capacity constants, `GetRingSlotCost`.

## Related

- [Loadout](./Loadout.md) · [Per-instance rolls](./PerInstanceRolls.md) · [Items](../Items/Crystals.md) · [`../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md)
