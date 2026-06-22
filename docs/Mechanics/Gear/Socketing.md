# Socketing

**Status:** [Built · No UI] — attachment works in data/Blueprint; there is **no socket screen**, so a player can't re-socket without hand-configuring a loadout. Owning code: `FRuntimeAttachedItem`, `ULoadoutComponent` attach setters.

## What it is

Putting a **crystal, fusion, or evolution** into a weapon/ring slot so its effect comes online:

- **Crystals** — supply spells (and the element for a [ring](./Rings.md)); wear with use.
- **Fusions** — a gem-half + augment-half (+ a chosen bonus substat). See [Fusion stones](../Items/FusionStones.md).
- **Evolution items** — pillar modifiers + spells. See [Evolution crystals](../Items/EvolutionCrystals.md).

The attachment is a runtime record (`FRuntimeAttachedItem`) on the slot — `IsCrystal()` / `IsFusion()` / `IsEvolution()` discriminate what's in it. The catalyst tier of what you socket also drives **wear** and **tier-gap** (see below).

## Why what you socket matters

- **Element** of a ring/crystal spell = the socketed crystal's element.
- **Wear** — a lower-tier catalyst wears faster (see [Durability & wear](./DurabilityWear.md)).
- **Tier-gap** — action tier vs the socketed channel's tier scales output (see [Tier gap](../Scaling/TierGap.md)).

## Entry points

- `FRuntimeAttachedItem` — `IsCrystal/IsFusion/IsEvolution/IsEmpty`, the slot's attached record.
- `ULoadoutComponent` — `FindAttachedItemByHolder`, `FindSpellCatalystHolder`, attach setters.

## Related

- [Rings](./Rings.md) · [Loadout](./Loadout.md) · [Items: Crystals](../Items/Crystals.md) / [Fusion stones](../Items/FusionStones.md) / [Evolution crystals](../Items/EvolutionCrystals.md) · [Durability & wear](./DurabilityWear.md) · [Tier gap](../Scaling/TierGap.md)
