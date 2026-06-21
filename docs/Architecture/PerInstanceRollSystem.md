# Per-Instance Roll System (U0–U4)

## Overview

Gear (weapon / ring / evolution) rolls its randomized stat and resistance bonuses **per owned instance, at acquisition** — two copies of the same item asset roll differently. The asset is a **template**: its authored `Base*` layers are the real shipped values; its `Generated*` layers are **designer PREVIEW** (a rollable sample for eyeballing the spread, **never gameplay-read** as of U4). The real rolled layer lives on the instance.

Built in five clusters on `feature/class-innate-resistance`: U0 (inert data + toggle), U1 (the loadout instance bridge), U2 (weapon/ring roll), U3 (evolution roll + read migration), U4 (preview-inert enforcement + cleanup).

## The model

```
ASSET (template, .uasset)                INSTANCE (owned, runtime)
  Base*        — authored, REAL    ──►     copied at CreateFrom*/construction
  Generated*   — designer PREVIEW   ✗      never copied (U4)
  bRandomGenerateOnPickup           ──►    ON: fresh roll at acquisition
  Stat/ResistanceMaxPoolOverride    ──►    seeds the instance MaxPools
```

- **The toggle** — `bRandomGenerateOnPickup` (on `UEquipmentDataBase` + `UEvolutionItemData`, **default OFF** = migration-safe): ON rolls fresh at acquisition; OFF ships authored Base only.
- **Preview Roll** — the asset's `Generated*` layers + the roll inputs/buttons live under the editor's *Preview Roll* category. The preview is gameplay-inert: `CreateFromWeapon/Ring` copy **Base only** (U4); evolution entry construction never touched the asset preview. The preview-side `UEquipmentDataBase::GetCombined*` helpers are retained (no gameplay caller) for future preview tooling; `UEvolutionItemData::GetCombinedResistance` was deleted (dead).

## Pool / MaxPool (the reroll-economy storage)

Each instance carries **two independent pairs** (stats + resistance): `StatPool/StatMaxPool`, `ResistancePool/ResistanceMaxPool`, on `FWeaponInventoryEntry`, `FRingInventoryEntry`, `FEvolutionInventoryEntry`, and `FEvolutionAttachment`.

- **MaxPool** = the roll's point budget. Seeded at acquisition from the per-asset override (`Stat/ResistanceMaxPoolOverride` > 0) or the default budget (`POOL_OVERRIDE_USE_TIER` = 0 sentinel). **Tier-power arc (2026-06-21):** the substat budget is now **flat** — `GetSubstatBudget` returns `FIXED_SUBSTAT_BUDGET` (~20) at every tier (per-tier `SUBSTAT_BUDGET_*` retained for reference but no longer drive the seed; higher-tier power comes from per-point VALUE scaling at `GetActiveStatBonus`, see `TierPowerScaling.md`). Resistance still uses the per-tier `RESISTANCE_BUDGET_*`.
- **The roll consumes the STORED MaxPool** — via the explicit-budget overloads `RerollSubstats(int32 Budget, …)` / `RerollResistance(int32 Budget)` (tier forms delegate) — so a future add/subtract-points lever just mutates the pool and re-rolls. Pillar percents roll alongside on the tier budget (not separately metered).
- **Pool** = the reroll charge meter, starts 0. **FUTURE economy (storage only — NOT built):** rewards fill Pool; reroll unlocks at Pool == MaxPool and spends to 0.

## The bridge (U1, shape B)

Loadouts are **designer-authored, asset-selecting** (`FSavedLoadout` inline on `UInventoryData`; inflated once per spawn by `UInventoryComponent::InitializeFromAsset` → `FCombatLoadout::CreateFromSavedLoadout`). For a per-instance roll to reach combat, the loadout must reference a specific owned copy:

- **Instance refs** — optional `FGuid`s on `FSavedLoadout` pairing with each asset pointer (`PrimaryWeaponInstance`, `SecondaryWeaponInstance`, `PrimaryRingInstance`, `PrimaryEvolutionInstance`, per-`FResonatorRingSlot` `RingInstance`). Invalid (default) = "use the asset".
- **Stable identity** — `FGuid PersistentID` on weapon/ring entries, minted **once at acquisition** (`AddWeapon`/`AddRing`); `CreateFrom*` deliberately leaves it invalid, so loadout-inflated ephemeral entries are unreferenceable. Evolution uses its existing `FEvolutionInventoryEntry::InstanceID` guid. The session-local `int32 InstanceID` (effect-ID packing) is untouched and must never be repurposed.
- **Resolution** — the `CreateFromSavedLoadout` **context overload** (inventory + evolution-inventory components): a **valid + found + asset-matching** ref copies the owned entry **wholesale** (roll, pools, guid, real crystal/spell state) instead of rebuilding from the asset; anything else falls back to the asset build (today's path, `Warning` only for set-but-dangling/mismatched refs). The zero-context signature delegates with nulls — **provably byte-identical** until a ref is set. SavedLoadout-sourced per-slot config (abilities/stances/spell overrides) applies identically on both branches.
- ⚠️ **INERT until a player equip UI exists** — nothing sets refs yet; the bridge is built ahead of the UI. Also note: owned GUIDs **re-mint every spawn** (the owned pool is itself rebuilt from the asset), so authored refs can never resolve — refs only become meaningful when set at runtime by the future equip flow.

## Per-type specifics

- **Weapon / Ring** — `AddWeapon`/`AddRing` (toggle ON): seed MaxPools, roll fresh, and **overwrite** the entry as `authored Base + fresh roll` (`ApplyPickupRoll`, shared helper). The instance's single `StatBonus`/`ResistanceBonus` is the runtime read (via `GetActiveStatBonus`/`GetActiveResistanceBonus`).
- **Evolution** — `AddInstance` (toggle ON) rolls into the owned entry's **separate** `GeneratedStatBonus`/`GeneratedResistance` (Base stays on the asset; combined at read time). The rolled state travels entry→`FEvolutionAttachment` via the U1c inflation copy, and the infusion-resolution attachment rebuild (`ResolveInfusionAttachment`, `Evolution` source case) carries it via a whole-struct copy. **Correction:** the U3 commit message claimed this whole-struct copy, but U3 only shipped the pillar/resistance accessor (`GetActivePrimaryEvolutionAttachment`) — the infusion rebuild kept a field-by-field copy (Item + durability only) that dropped `Generated*`, so rolled ints never reached infusion for the standalone slot. Fixed on `fix/evolution-infusion-roll-drop`. Reads combine `asset.Base + attachment.Generated` with **the split preserved (locked option a)**:
  - **Pillars** (`ApplyEvolutionPillarModifier`, both weapon-attached and standalone cases): rolled pillar percents are **PERMANENT** (always-on while slotted), like Base pillars.
  - **Int substats** (the infusion caller): rolled ints are **infusion-conditional**, scaled 0.5/1.0 through the same `UEvolutionItemData::MapToInfusionModifiers` mapping as Base — identical scaling by construction.
  - **Resistance** (`GetActiveResistanceBonus` evo term): `asset.BaseResistance + attachment.GeneratedResistance`, always-on, both slot cases. See `ResistanceSystem.md` (the resistance roll is one half of this system).

## Debug — `UEquipmentRollDebug`

`GetEquipmentRollString(AActor*)` / `PrintEquipmentRoll` / `LogEquipmentRoll` (`Equipment/EquipmentRollDebug.*`): for the actor's active loadout, prints **per equipped slot** — the **path marker** (`INSTANCE ROLL (ref resolved)` vs `asset Base (fallback)`, keyed on the entry's `PersistentID` validity; evolution uses an instance-state heuristic since attachments carry no guid), the instance identity, the non-zero `StatBonus`/`ResistanceBonus` values, and the `Pool/MaxPool` state — plus the `GetActiveStatBonus`/`GetActiveResistanceBonus` aggregates combat consumes. Reads via the same gameplay accessors (cannot drift). This is the bridge-proof tool: a slot printing `INSTANCE ROLL` means the per-instance roll is live in combat.

## Known Limitations / Deferred

- **Session-ephemeral.** No save system exists; the owned pool is rebuilt (and toggle-ON gear re-rolled, GUIDs re-minted) every spawn. Cross-session persistence of instance rolls is a future arc.
- **The reroll economy is storage-only.** Pool charging, the reroll trigger, and its UI are not built.
- **The player equip UI** (the thing that sets instance refs and makes the bridge carry traffic) does not exist.
- **Double-reference hazard.** A future equip UI must forbid referencing the same owned instance in two slots of one loadout (shared `int32 InstanceID` effect-ID window → apply/remove collisions). Cross-loadout sharing is safe.
- **Preview-side `GetCombined*` helpers retained** on `UEquipmentDataBase` (no gameplay caller) — delete if preview tooling never materializes.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-10 | Initial documentation — U0 (inert data + toggle + Preview Roll reframe), U1 (PersistentID, FSavedLoadout instance refs, the CreateFromSavedLoadout context overload + fallback), U2 (weapon/ring roll at acquisition, budget overloads), U3 (evolution roll, entry→attachment travel, 3-subsystem read migration, split preserved), U4 (Base-only CreateFrom*, dead-code/comment cleanup), UEquipmentRollDebug. | feature/class-innate-resistance |
| 2026-06-11 | Infusion-resolution roll drop fixed: `ResolveInfusionAttachment`'s `Evolution` case now whole-struct-copies `Loadout.PrimaryEvolution` (was Item + CurrentDurability only, dropping `Generated*` + pools). The U3 commit message overstated this as already done. Byte-identical until a rolled instance is slotted. | fix/evolution-infusion-roll-drop |
| 2026-06-21 | Tier-power arc — substat MaxPool seed is now **flat** (`FIXED_SUBSTAT_BUDGET` ~20 at every tier; per-tier `SUBSTAT_BUDGET_*` superseded as the seed). Higher-tier strength now comes from per-point VALUE scaling at `GetActiveStatBonus`, not a bigger budget. Resistance budget unchanged. | feature/tier-power-scaling |
