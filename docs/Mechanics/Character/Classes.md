# Classes

**Status:** [Live] — your class is fixed at creation and shapes everything you can do. Entry-point overview; each class has its own page. Owning code: `ECharacterClass`, `CharacterClassHelpers`.

## The three classes

| Class | Magic source | Weapons | Innate element | Signature |
|---|---|---|---|---|
| **[Generic](../Archetypes/Generic.md)** | borrowed (a ring or evolution it wears) | **dual-wield** | none | two weapon slots; tanky vs physical, soft vs magic |
| **[Refractor](../Archetypes/Refractor.md)** | innate spells | one primary slot (weapon, ring or evolution) | **one fixed element** | weather leader; spell-pool budget grows with mastery |
| **[Resonator](../Archetypes/Resonator.md)** | [rings](../Gear/Rings.md) | one primary slot (weapon or evolution) | none (element from rings) | element-swap flexibility; fragile (rings wear/break) |

## What class gates

A class is made by its slots, not a skill gate (D-027): any class with a weapon gets that weapon's
abilities (a weapon crystal makes them elemental; **weapons never give spells** — not built yet, B-098: today
a weapon crystal still offers its spells), and spells come from
rings, evolutions and a Refractor's innate pool.

- **Dual-wield** — Generic only (its secondary weapon slot).
- **Innate element** — Refractor (chosen at creation); Generic/Resonator have none.
- **Rings** — the Resonator's spell source, sharing a budget of 5 with its primary; any mix of elements (D-010; see [Rings](../Gear/Rings.md)).
- **Resistances** — each class has an innate element/physical resistance profile (`ClassInnateResistanceTable`; see [Resistance](../Status/Resistance.md)).

## Special identities

- **[Reality](../Archetypes/Reality.md)** — a Refractor element variant: any-element casting, +10% stats, hard-counters Broken Darkness.
- **[Broken Darkness](../Archetypes/BrokenDarkness.md)** — absorb-to-charge, rotating element pool, forbidden Light/Void cost HP.

## Entry points

- `ECharacterClass` (Generic / Caster[Refractor] / Resonator).
- `CharacterClassHelpers` — `CanDualWield`, `CanUseAbilities`, `UsesRings`, etc.

## Related

- The five archetype pages above · [Stats](./Stats.md) · [Elements](../Magic/Elements.md) · [Resistance](../Status/Resistance.md)
