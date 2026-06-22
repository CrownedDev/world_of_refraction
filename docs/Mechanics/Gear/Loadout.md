# Loadout

**Status:** [Built · No UI] — loadouts are data/Blueprint-configured today; no in-game loadout screen. Owning code: `ULoadoutComponent`, `LoadoutConstants`.

## What a loadout is

Your **combat-ready set**: the primary slot (weapon / ring / evolution), plus the spells, abilities, and items you bring. It's the curated subset of your [Inventory](./Inventory.md) you actually fight with.

## The slot-cost budget

What you can equip is gated by a **unified slot-cost budget**, not fixed counts:

- **Primary slot** costs by what's in it — bare weapon 1 / crystal 2 / evolution 3 (`EVOLUTION_PRIMARY_SLOT_COST`).
- **Rings** (Resonator) cost 1 (normal) / 2 (evolved) each.
- Total (primary + rings) must fit `LoadoutConstants::LOADOUT_TOTAL_BUDGET` (`LoadoutComponent.cpp`).

Within that budget there is **no fixed ring count and no per-element cap** — spend it how you like.

Separately, each piece of gear gates **how many spells/abilities it can hold** by its tier — see [Equipment slots](./EquipmentSlots.md). Spell pools also have their own weight budget — see [Spell pool budget](../Magic/SpellPoolBudget.md).

## Class shape

- **Generic** — weapon(s) (dual-wield), abilities, crystal-borrowed magic.
- **Refractor** — a weapon + innate spells.
- **Resonator** — [rings](./Rings.md) as the spell source (budget-costed).

See [Classes](../Character/Classes.md).

## Entry points

- `ULoadoutComponent` — `ValidateLoadout`, `GetValidationErrors`, active-loadout management.
- `LoadoutConstants::LOADOUT_TOTAL_BUDGET`, `EVOLUTION_PRIMARY_SLOT_COST`; `InventoryConstants::GetRingSlotCost`.

## Related

- [Inventory](./Inventory.md) · [Rings](./Rings.md) · [Equipment slots](./EquipmentSlots.md) · [Spell pool budget](../Magic/SpellPoolBudget.md) · [Socketing](./Socketing.md) · [`../Architecture/LoadoutSystem.md`](../../Architecture/LoadoutSystem.md)
