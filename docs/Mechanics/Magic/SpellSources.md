# Spell Sources

**Status:** Live — reference doc (created 2026-06-22). Describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`README.md`](../README.md), [`../Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md) (infusion *source choice* is a different axis — see below), [`DurabilityWear.md`](../Gear/DurabilityWear.md) (wear math), [`../Architecture/BrokenDarkness.md`](../../Architecture/BrokenDarkness.md) / [`Items/Crystals.md`](../Items/Crystals.md) (break checks), [`../Architecture/LoadoutSystem.md`](../../Architecture/LoadoutSystem.md) (slot resolution).

## Concept

Every cast has an **origin** — where the spell came from. `ESpellSource` records that origin so post-cast
logic can route the right consequence: durability wear, a break check, or item consumption. It is resolved
once per cast from the spell's loadout slot.

## `ESpellSource` (`Skills/Definitions/ESpellSource.h`)

| Source | Origin | Post-cast consequence |
|---|---|---|
| **Innate** | Caster's innate-element spell | None — no risk, no wear |
| **RingCrystal** | A Resonator ring's crystal | **Break check** after cast |
| **Evolution** | An evolution-item slot | Evolution wear (TBD per design) |
| **Item** | A consumable | **Consumed** on use |
| **WeaponCrystal** | A weapon-attached crystal | Durability **wear at commit** |

## Resolution

- The active spell slot resolves to its source via `ULoadoutComponent::ResolveSpellSource` (the loadout knows which slot a spell occupies and what holder backs it).
- The resolved source is read by `UActionExecutor` during the cast to drive `ApplyCommitCosts` (wear) and the post-cast break/consume handlers.

> **Not the same as infusion source.** `ESpellSource` is the spell's *origin* (post-cast routing).
> `EInfusionSourceOption` is the player/AI's *infusion choice* for the cast (which channel pays the infusion
> cost). A spell's origin is 1:1 bound to its allowed infusion sources, but the two enums are distinct — see
> [`../Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md).

## Integration points

- **Durability** — WeaponCrystal/RingCrystal sources feed `UCrystalManager::ProcessPostCastWear` (see [`DurabilityWear.md`](../Gear/DurabilityWear.md)).
- **Break** — RingCrystal triggers a break roll after the cast resolves.
- **Inventory** — Item sources are routed through the item executor and removed from inventory on use.

## Known TODOs

- No dedicated doc existed before this; sources were only referenced in passing by `InfusionSystem.md`.
- **Evolution** post-cast wear is marked TBD in the enum — confirm the evolution wear path against [`DurabilityWear.md`](../Gear/DurabilityWear.md) before documenting it as final.
