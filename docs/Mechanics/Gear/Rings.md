# Rings

**Status:** [Built · No UI] — the model is live in combat once a loadout is configured, but there's no ring-equip screen. Resonator's spell-source gear. Owning code: `URingData`, `LoadoutComponent` (`ESlotKind::ResonatorRing`).

## What a ring is

A ring is **element-bearing spell-carrier gear** for the [Resonator](../Archetypes/Resonator.md):

- **Element comes from the ring's attached crystal** — `URingData` has **no `Element` field**; a ring is a crystal-bearing slot (`ESlotKind::ResonatorRing`) and the attached crystal supplies the element. Swap the crystal/ring → swap your element. See [Socketing](./Socketing.md).
- **Carries spells** — `PresetSpells` (locked on conjured rings via `bSpellsLocked`) or customisable spell slots up to `GetMaxSpells()`. See [Equipment slots](./EquipmentSlots.md) for capacity.
- **Infusion mode** — `InfusionMode` (Balanced / Physical / Status) sets how an infused cast through this ring splits its charge bonus. See [Infusion](../Magic/Infusion.md).
- **Engravings** — inherited `BaseStatBonus` / `GeneratedStatBonus` (per-instance roll). See [Per-instance rolls](./PerInstanceRolls.md).

## Ring types (shop taxonomy)

Four cost-composed types, locked 2026-07-11 (canonical table with cost
formulas: [`Resources_Design.md` §5.1c](../../Design/Resources_Design.md)):

- **Template** — a bare ring; you buy and attach your own crystal after (`DA_Ring_Generic`). Cheapest.
- **Crystal** — ships with a tier-matched element gem attached, no spells; you fill the slots (the 9 gem rings, Garnet … Iolite).
- **Themed** — gem plus 1–2 bundled Generic spells matching a concept (the 27 themed rings — Emberflame, Tidecaller, Zephyr, …). The bundled spells are **priced into the shop cost**.
- **Evolution** — ships with an evolution crystal attached. None authored yet — banked as an intended type.

All four sell at the hub **Jeweler** — see [`MerchantShopSystem`](../../Architecture/MerchantShopSystem.md).

## Equipping cost — a budget, not a slot count

There is **no fixed ring-slot count and no per-element cap.** Equipped rings fit a **unified slot-cost budget**: a normal ring costs **1**, an evolved ring **2**, shared with the primary slot, and the total must fit `LoadoutConstants::LOADOUT_TOTAL_BUDGET` (`LoadoutComponent.cpp`). You can stack same-element rings freely — the only limit is the budget. See [Loadout](./Loadout.md).

## Fragility

Rings **wear with use and break** at 0 durability (a broken ring stops working until repaired). That's the Resonator's trade — total element flexibility, disposable tools. Wear is a **percent of the ring crystal's max durability**, so casts-to-break is consistent across crystal tiers (a higher-tier ring crystal lasts the same number of casts, just with more absolute durability) — and a sufficiently over-tiered cast can shatter it outright. Luck can skip a wear event. See [Durability & wear](./DurabilityWear.md).

## Entry points

- `URingData` — `bSpellsLocked`, `PresetSpells`, `InfusionMode`, `GetMaxSpells()` (no `Element` field).
- `LoadoutComponent` — `ESlotKind::ResonatorRing`, ring slot-cost validation (`LOADOUT_TOTAL_BUDGET`).

## Related

- [Resonator](../Archetypes/Resonator.md) · [Loadout](./Loadout.md) · [Equipment slots](./EquipmentSlots.md) · [Socketing](./Socketing.md) · [Durability & wear](./DurabilityWear.md) · [Per-instance rolls](./PerInstanceRolls.md)
