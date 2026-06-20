# Equipment Slot Tier Scaling

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> Extends the shared per-tier slot curve to the equipment containers still on flat caps. Companion to
> [`InnateSpellPoolBudget.md`](./InnateSpellPoolBudget.md) — note the two use **different models**
> (see below). BD spell pools are handled separately (all currently default to 6 — pending discussion).
> When shipped, this moves to `docs/Design/Completed/`.

## What changes

The shared slot curve `AugmentStoneConstants::ATTACHMENT_SLOTS` already drives AbilityStone ability slots
and gem-crystal weapon spell slots via `GetAttachmentSlotsForTier`. The helper is deliberately generic
("reusable for spell-slots later"). This extends it to the containers still on flat 6-caps, and **updates
the curve** to a cleaner shape.

### New curve

`ATTACHMENT_SLOTS`: `{2, 3, 3, 4, 4, 5, 6}` → **`{1, 2, 3, 4, 5, 6, 6}`**

| Tier | F | E | D | C | B | A | S |
| ---- | - | - | - | - | - | - | - |
| Slots | 1 | 2 | 3 | 4 | 5 | 6 | 6 |

Linear F→A, then S holds at 6. **A and S grant the same slot count** — S leads on power/quality, not slot
quantity (deliberate).

### Containers

| Container | Today | After |
| --------- | ----- | ----- |
| Weapon abilities | flat 6 (`MAX_WEAPON_ABILITIES`) | curve, keyed on **weapon tier** |
| Ring spells | flat 6 (`MAX_RING_SPELLS`) | curve, keyed on **ring tier** |
| Evolution spells | flat 6 (`MAX_EVOLUTION_SPELLS`) | curve, keyed on **evolution tier** |
| Weapon spells | already curve (gem-crystal tier) | unchanged path, new curve values |
| AugmentStone abilities | already curve (stone tier) | unchanged path, new curve values |

Each container keys the curve on **its own tier**. Better gear earns more slots; an F-weapon holds 1
ability, an A/S-weapon holds 6. S-tier containers stay at 6 (no change at the top); the curve only bites
below A.

## Locked rules

- Single shared curve across every slot-granting container — one edit point.
- **Flat slot-count model** — each ability/spell fills one slot regardless of *its* tier. No skill-tier
  weighting, no mastery discount. This is the **equipment** model.
- **Distinct from innate.** Innate spells use a weighted *point-budget* (skill tier = cost, per-pillar
  mastery discount). Equipment uses this *slot-count by container tier*. Two models, intentional: innate is
  the Caster's identity lever; equipment is the simpler "better gear = more room." They do not share a
  formula, only the tier vocabulary.
- **Locked skills** (conjured weapons' `bAbilitiesLocked`, locked rings' `bSpellsLocked`) — customizable
  count = `SlotCurve(containerTier) − lockedCount`, same subtraction as today against the new cap.

## Build scope (for the survey)

- **Update the constant** — `AugmentStoneConstants::ATTACHMENT_SLOTS` to `{1,2,3,4,5,6,6}`.
- **Weapon abilities** — `FWeaponLoadoutEntry::GetCustomizableAbilityCount()` and `ValidateAbilities` cap
  move from `MAX_WEAPON_ABILITIES` to `GetAttachmentSlotsForTier(weapon tier)`.
- **Ring spells** — `FRingLoadoutEntry::GetCustomizableSpellCount()` / `GetAllSpells` cap move from
  `MAX_RING_SPELLS` to the curve on ring tier.
- **Evolution spells** — `ResolveSpellSlotCap`'s evolution branch (currently returns the flat ceiling) keys
  on evolution tier instead.
- **Validation** — the loadout validators (`FSavedLoadout`, `FWeaponLoadoutEntry`, `FRingLoadoutEntry`)
  read the new caps.
- **Debug tooling** — slot readout per container (tier → slots, used/available).

Cluster it — constant + the three cap sites are >3 files; compile between.

## Open / carry-over

- **Side effect: AugmentStone + gem-spell slot counts change too** (shared curve). F-stone 2→1, etc. This
  is a real balance shift to two shipped systems. Intended as part of unifying on one curve — **confirm**,
  or split into a second constant to keep AugmentStone/gem on the old `{2,3,3,4,4,5,6}`.
- **The flat caps don't disappear** — `MAX_WEAPON_ABILITIES` etc. stay as the absolute ceiling (6) the
  curve never exceeds; they become a safety bound, not the live cap.
- **BD spell pools** — all default to 6; how the curve applies there is the next discussion (see below).

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: shared slot curve updated to {1,2,3,4,5,6,6} and extended to weapon abilities, ring spells, evolution spells (keyed on container tier). Equipment uses flat slot-count-by-tier, distinct from innate's weighted budget. Not yet built. | (tbd) |
