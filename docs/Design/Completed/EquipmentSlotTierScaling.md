# Equipment Slot Tier Scaling

> **Status: COMPLETED — built + PIE-verified, merged to main (2026-06-20).**
> Extends the shared per-tier slot curve to the equipment containers still on flat caps. Companion to
> [`InnateSpellPoolBudget.md`](./InnateSpellPoolBudget.md) — note the two use **different models**
> (see below). BD spell pools are handled separately (all currently default to 6 — pending discussion).
> When shipped, this moves to `docs/Design/Completed/`.

## What changes

The shared slot curve `AugmentStoneConstants::ATTACHMENT_SLOTS` already drives AbilityStone ability slots
and gem-crystal weapon spell slots via `GetAttachmentSlotsForTier(FCrystalId)` (which gates on crystal
type). A new sibling, `CrystalEffectTable::SlotsForContainerTier(EItemTier)`, reads the same curve keyed on
a container's *own* tier with no crystal-type gate. This extends the curve to the containers still on flat
6-caps via that sibling, and **updates the curve** to a cleaner shape.

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

## Build scope (as built)

- **Constant + guard** — `AugmentStoneConstants::ATTACHMENT_SLOTS` set to `{1,2,3,4,5,6,6}`; the endpoint
  `static_assert` updated (`[0]==1`). New helper `CrystalEffectTable::SlotsForContainerTier(EItemTier)` added
  alongside the existing `GetAttachmentSlotsForTier(FCrystalId)` (curve shared; no crystal-type gate).
- **Weapon abilities** — `FWeaponLoadoutEntry::GetCustomizableAbilityCount()` now keys on
  `SlotsForContainerTier(WeaponEntry.Weapon->Tier)` (moved to the `.cpp` so the header avoids the
  WeaponData/CrystalEffectTable includes); `ValidateAbilities` inherits it via that function. `MAX_WEAPON_ABILITIES`
  is the no-weapon fallback.
- **Weapon native spells** — the two `ResolveSpellSlotCap(attachment, …)` sites in `FWeaponLoadoutEntry.cpp`
  (`GetAllSpells`, `ValidateSpells`) now pass `SlotsForContainerTier(weapon tier)` **as the `FlatCeiling`
  argument** — *not* a `ResolveSpellSlotCap` signature change. Gem-keying preserved: gem tier when a gem is
  attached, weapon tier as the no-gem fall-through, `MAX_SPELL_SLOTS` as the no-weapon ceiling.
- **Ring spells** — same mechanism: `FRingLoadoutEntry::GetCustomizableSpellCount()` / `GetAllSpells` pass
  `SlotsForContainerTier(ring tier)` as the `FlatCeiling` to `ResolveSpellSlotCap`. `ValidateSpells` inherits
  via `GetCustomizableSpellCount`. Gem tier overrides as before; `MAX_RING_SPELLS` is the no-ring fallback.
- **Evolution spells** — `ResolveSpellSlotCap` was **not** touched (it has no evolution branch — evolution
  has no attachment crystal). The evolution cap lives in three `LoadoutComponent.cpp` sites
  (`GetValidationErrors`, `GetValidationFindings`, the `EvolutionOverflow` trim), each now keyed on
  `SlotsForContainerTier(Loadout.PrimaryEvolution.Item->Tier)`, with `MAX_EVOLUTION_SPELLS` as the
  no-item fallback.
- **Debug tooling** — per-container slot readout added to `UInventoryDebug::LogActiveLoadout`
  (tier, used/cap for weapon abilities + spells, ring spells, evolution spells, and each Resonator ring).

The flat `MAX_*` constants are retained as absolute fallback ceilings, not the live caps.

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
| 2026-06-20 | Built across 5 clusters: curve unified to `{1,2,3,4,5,6,6}` (+ endpoint `static_assert` updated); new `CrystalEffectTable::SlotsForContainerTier(EItemTier)` helper; weapon ability + native (no-gem) spell, ring spell, and evolution spell caps keyed on each container's own tier; gem-keying preserved (gem tier when attached, container tier as the no-gem fall-through); per-container slot readout added to `InventoryDebug`. Flat `MAX_*` constants retained as fallback ceilings. Pending PIE verification. | feature/equipment-slot-tier-scaling |
