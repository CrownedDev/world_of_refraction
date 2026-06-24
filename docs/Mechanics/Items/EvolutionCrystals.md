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

## Lifecycle: tier, attach, level, remove, wear, break

Evolution is the **third gear type** (§5.3b) — it carries a per-instance **tier** and goes through the
full leveling/economy lifecycle. Owned evolutions live in `UEvolutionInventoryComponent::Entries`
(`FEvolutionInventoryEntry`: `Item`, `Tier`, `Quality`, `CurrentDurability`, `FGuid InstanceID`).

**Reference model (attach = reference, not move).** Slotting an evolution does **not** consume the
owned entry — the owned `FEvolutionInventoryEntry` **persists** in `Entries`; the slot just *references*
it. So a slotted evolution still counts toward your owned cap wherever it is.

- **Primary slot** — referenced by `FSavedLoadout::PrimaryEvolutionInstance` (a GUID); at loadout
  inflation the owned entry's tier + rolled state is copied onto `FCombatLoadout::PrimaryEvolution`
  (the runtime carries the GUID too — `GetActivePrimaryEvolutionInstance`).
- **Weapon/ring (gear) attach** — `UInventoryComponent::AttachEvolutionToWeapon/AttachEvolutionToRing`
  (Spiritualist): writes the host's `AttachedItem.Evolution` referencing the owned entry by
  `InstanceID` + copies its leveled `Tier` + rolled state. **One evo, one slot** is enforced
  (`IsEvolutionSlottedAnywhere` — rejects if already in any primary or gear slot).

**`InstanceID` = the keystone.** On a slotted `FEvolutionAttachment`: a **valid** `InstanceID` means
**player-attached** (references an owned entry — levelable indirectly, removable, breaks → dismantle);
an **invalid** `InstanceID` means **authored-locked** (baked into the weapon/ring asset, can't be
removed/leveled, breaks → just clears).

**Level / downgrade** — `UEconomyService::LevelUpEvolution`/`DowngradeEvolution(InstanceID)` write the
owned `FEvolutionInventoryEntry.Tier` (Gear essence + ½ Reality up; half-Gear refund down, floored at
base). Levelable in **inventory** or **primary slot** (the entry persists, so the change propagates to
the slot on next inflation); **gear-attached is frozen** (not levelable while attached). See
[`../Leveling.md`](../Leveling.md).

**Remove:**
- **From primary → destroyed + dust.** `RemovePrimaryEvolution` self-resolves the slotted GUID, clears
  the slot, then `DismantleEvolution` (element essence at the gear amount, leveled tier) + frees a cap slot.
- **From weapon/ring → returns to inventory + 10% wear.** `RemoveEvolutionFromWeapon/Ring` copies the
  worn runtime durability back onto the owned entry, applies 10% removal wear, un-references the slot
  (entry persists). If the 10% wear drops durability to 0 it **breaks → forced dismantle** instead.
  Rejects authored-locked attachments (invalid `InstanceID`).

**Wear + break (combat).** Evolutions default `EBreakability::Breakable` (any wielder wears them).
Primary-slot evos wear via `UCrystalManager::ProcessPostCastEvolutionWear`; gear-attached evos wear via
`ProcessPostCastGearEvolutionWear` (routed at the WeaponCrystal infusion cast, since the attached evo
provides the weapon's spells). On the combat-end sweep
(`ACombatOrchestrator::ApplyBetweenCombatCrystalDestruction`) a broken evolution **forced-dismantles**:
player-attached / primary → `DismantleEvolution` (essence + free cap slot); authored-locked → clears
only (no owned entry to dismantle).

**5-per-run cap.** `UEvolutionInventoryComponent::CountRunEvolutions` = owned `Entries` **+**
authored-locked gear evolutions (gear `AttachedItem` with **invalid** `InstanceID` — not in `Entries`).
Player-attached gear evos are skipped (already counted in `Entries` — reference model). Enforced at
`AddInstance` (can't acquire a 6th); dismantle/sell free a slot.

**Durability persistence.** `FEvolutionInventoryEntry.CurrentDurability` is the persisted value (seeded
to `MaxDurability` at mint); `FEvolutionAttachment.CurrentDurability` is the runtime copy. Attach copies
entry→attachment; gear-remove copies the worn attachment value back → wear survives attach/detach.

## Reachability

The pillar modifier and carried spells **work in combat once a loadout is configured**, but **socketing an evolution yourself has no UI** — loadouts are data/Blueprint-configured today. [Built · No UI]

## Entry points

- `UEvolutionItemData` — the asset (pillar modifiers, carried spells, base `Tier`, `Breakability`).
- `FEvolutionInventoryEntry` — the owned instance (`Tier`/`Quality`/`CurrentDurability`/`InstanceID`).
- `FEvolutionAttachment` — the slotted runtime form (`Tier`/`InstanceID`/durability + rolled bonuses).
- `UCharacterDataComponent::ApplyEvolutionPillarModifier` — the live pillar-mod apply.
- `UEconomyService::LevelUpEvolution`/`DowngradeEvolution`/`DismantleEvolution`/`RemovePrimaryEvolution`,
  `UInventoryComponent::AttachEvolutionToWeapon`/`AttachEvolutionToRing`/`RemoveEvolutionFromWeapon`/`RemoveEvolutionFromRing`,
  `UEvolutionInventoryComponent::AddInstance`/`RemoveInstance`/`CountRunEvolutions`.
- See [`../Leveling.md`](../Leveling.md), [`../../Architecture/TierOnInstance.md`](../../Architecture/TierOnInstance.md),
  [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md).

## Related

- [`Crystals.md`](./Crystals.md) — refined crystals (spells, not pillar mods).
- [`AugmentStones.md`](./AugmentStones.md), [`FusionStones.md`](./FusionStones.md) — the other attachables.
- [`../../Architecture/LoadoutSystem.md`](../../Architecture/LoadoutSystem.md), [`../../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).
