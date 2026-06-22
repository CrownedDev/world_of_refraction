# Classes

**Status:** [Live] — your class is fixed at creation and shapes everything you can do. Entry-point overview; each class has its own page. Owning code: `ECharacterClass`, `CharacterClassHelpers`.

## The three classes

| Class | Magic source | Weapons | Innate element | Signature |
|---|---|---|---|---|
| **[Generic](../Archetypes/Generic.md)** | borrowed (equipped crystals) | **dual-wield** | none | weapon abilities; tanky vs physical, soft vs magic |
| **[Refractor](../Archetypes/Refractor.md)** | innate spells | one weapon | **one fixed element** | weather leader; spell-pool budget grows with mastery |
| **[Resonator](../Archetypes/Resonator.md)** | [rings](../Gear/Rings.md) | one weapon | none (element from rings) | element-swap flexibility; fragile (rings wear/break) |

## What class gates

- **Dual-wield** — Generic only (`CharacterClassHelpers::CanDualWield`).
- **Weapon abilities** — Generic.
- **Innate element** — Refractor (chosen at creation); Generic/Resonator have none.
- **Rings** — the Resonator's spell source (6-slot-equivalent budget; see [Rings](../Gear/Rings.md)).
- **Resistances** — each class has an innate element/physical resistance profile (`ClassInnateResistanceTable`; see [Resistance](../Status/Resistance.md)).

## Special identities

- **[Reality](../Archetypes/Reality.md)** — a Refractor element variant: any-element casting, +10% stats, hard-counters Broken Darkness.
- **[Broken Darkness](../Archetypes/BrokenDarkness.md)** — absorb-to-charge, rotating element pool, forbidden Light/Void cost HP.

## Entry points

- `ECharacterClass` (Generic / Caster[Refractor] / Resonator).
- `CharacterClassHelpers` — `CanDualWield`, `CanUseAbilities`, `UsesRings`, etc.

## Related

- The five archetype pages above · [Stats](./Stats.md) · [Elements](../Magic/Elements.md) · [Resistance](../Status/Resistance.md)
