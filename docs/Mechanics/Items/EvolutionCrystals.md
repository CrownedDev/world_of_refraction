# Evolution Crystals

**Status:** mixed — the **pillar-modifier effect** is [Live] once slotted; **socketing and per-instance rolls** are [Built · No UI]. Distinct from refined crystals ([`Crystals.md`](./Crystals.md)): evolutions modify your **pillars**, not just grant spells. Deep specs: [`../../Architecture/LoadoutSystem.md`](../../Architecture/LoadoutSystem.md), [`../../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).

## What an evolution crystal does

An evolution item goes in the **primary slot** (on a weapon/ring, or standalone) and brings:

- **Pillar % modifiers** — adjusts Mind / Body / Spirit, which cascades to that pillar's substats. Applied live by `UCharacterDataComponent::ApplyEvolutionPillarModifier` (clamped). [Live]
- **Spells** — up to ~4 carried spells with fixed slots (per the asset). [Live] (cast path)
- **Forms** — special evolutions back the class identities (e.g. Broken Darkness, Reality). See [`../Archetypes/BrokenDarkness.md`](../Archetypes/BrokenDarkness.md), [`../Archetypes/Reality.md`](../Archetypes/Reality.md).

> No irreversibility: there is **no** consume/lock gate on evolving — nothing in code makes slotting an evolution permanent or one-way.

## Per-instance rolls

If `bRandomGenerateOnPickup` is set, an evolution instance rolls fresh bonuses at acquisition, stored on the **runtime** `FEvolutionAttachment` (not the asset):
- `GeneratedStatBonus` + `StatPool`/`StatMaxPool`
- `GeneratedResistance` + `ResistancePool`/`ResistanceMaxPool` (status-buildup resistance, a separate pool)

Rolls happen at pickup; the **reroll trigger and any equip/compare UI do not exist** → [Built · No UI]. See [`../../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).

## Reachability

The pillar modifier and carried spells **work in combat once a loadout is configured**, but **socketing an evolution yourself has no UI** — loadouts are data/Blueprint-configured today. [Built · No UI]

## Entry points

- `UEvolutionItemData` — the asset (pillar modifiers, carried spells).
- `FEvolutionAttachment` — runtime instance (rolled bonuses + roll pools).
- `UCharacterDataComponent::ApplyEvolutionPillarModifier` — the live pillar-mod apply.

## Related

- [`Crystals.md`](./Crystals.md) — refined crystals (spells, not pillar mods).
- [`AugmentStones.md`](./AugmentStones.md), [`FusionStones.md`](./FusionStones.md) — the other attachables.
- [`../../Architecture/LoadoutSystem.md`](../../Architecture/LoadoutSystem.md), [`../../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).
