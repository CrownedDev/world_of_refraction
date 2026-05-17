# Loadout System

## Overview

The Loadout System manages a character's **active combat loadout** — the gear,
spells, abilities and consumable items the character is *using* in battle — as
distinct from `UInventoryComponent`, which tracks what the character *owns*.

The header states the architectural split plainly:

> `InventoryComponent` = What you OWN (warehouse)
> `LoadoutComponent`  = What you're USING in battle (equipped gear)

A character can save multiple loadout configurations (`SavedLoadouts`, capped by
`MaxSavedLoadouts`, default 5). One loadout is active at a time
(`ActiveLoadoutIndex`). At battle start the active loadout is validated against
inventory (for player characters) or accepted as-is (for AI characters
initialized from a `ULoadoutData` asset).

The system is the central query surface for combat: it answers "what weapon is
active?", "what spells can I cast?", "what abilities are available?", "what
stance/animation do I use?", "what equipment stat bonuses apply?" and "what
crystals are equipped?" — resolving all of this per character class
(Generic / Caster / Resonator).

## Architecture

### `ULoadoutComponent` (`UActorComponent`)

The runtime component attached to a combatant actor. Tick is disabled
(`PrimaryComponentTick.bCanEverTick = false`) — the system is event-driven.

Key fields:

| Field | Type | Purpose |
|-------|------|---------|
| `CharacterClass` | `ECharacterClass` | Determines loadout rules; defaults to `Generic`. Set from `CharacterData` / `LoadoutData` during initialization. |
| `MaxSavedLoadouts` | `int32` | Cap on saved configurations (default 5). |
| `SavedLoadouts` | `TArray<FCombatLoadout>` | All saved loadout configurations. **Not a `UPROPERTY`** — C++-only access, not serialized via the reflection system. |
| `ActiveLoadoutIndex` | `int32` | Index of the currently active loadout. |
| `bIsReadyForBattle` | `bool` | Set true by `PrepareForBattle`; gates `UseItem`. |
| `bInitializedFromAsset` | `bool` (private) | True if initialized from a `ULoadoutData` asset (AI), false if from inventory (player). Controls whether inventory validation is skipped. |

### `FEquippedCrystalSlot` (`USTRUCT`)

A lightweight pairing of a crystal and its holder, returned by
`GetEquippedCrystals()`:

- `Crystal` — `UItemData*`, the slotted crystal.
- `Holder` — `UObject*` (the `UWeaponData` or `URingData` bearing it; `UObject*`
  because weapons and rings share no base beyond `UPrimaryDataAsset`). For an
  evolution crystal slot, `Holder == Crystal` (evolution *is* the crystal).

### `FWeaponLoadoutEntry` (`USTRUCT`)

Describes *how a weapon is used* in a loadout — distinct from
`FWeaponInventoryEntry`, which describes what is owned (weapon + crystal state).
The same owned weapon can appear configured differently across multiple saved
loadouts.

Fields:

- `WeaponEntry` — `FWeaponInventoryEntry`; the owned weapon, including
  crystal/evolution state.
- `AssignedAbilities` — `TArray<UAbilityData*>`; a **pure sequential override
  list** (max 6 via `LoadoutConstants::MAX_WEAPON_ABILITIES`). Empty by default.
- `AssignedSpells` — `TArray<USpellData*>`; sequential override list, valid only
  if the weapon has a crystal/evolution.
- `OverrideAttack` — `UWeaponAttackData*`; per-loadout attack override that, if
  set, replaces the weapon asset's `WeaponAttack`.

Notable behavior:

- `GetLockedAbilityCount()` — returns the full `PresetAbilities.Num()` when the
  weapon's `bAbilitiesLocked` is true (conjured weapons), else 0 (all
  customizable). A `// TODO` notes partial locking is not implemented.
- `GetAllAbilities()` — for locked weapons returns presets only; otherwise
  performs a **sequential override merge**: `AssignedAbilities` replace preset
  slots in order, with non-null entries beyond the preset count appended, capped
  at `MAX_WEAPON_ABILITIES`.
- `GetAllSpells()` — same sequential override merge using
  `WeaponEntry.GetSpells()` as the base list and `AssignedSpells` as overrides,
  capped at `LoadoutConstants::MAX_SPELL_SLOTS`.
- `ValidateAbilities()` — checks each assigned ability is owned and its
  `RequiredWeaponType` matches the weapon, and that count fits the customizable
  partition.
- `ValidateSpells()` — checks ownership and element match (skipped for
  "any-element" crystal sources via `ElementHelpers::IsAnySpellSource`). Note:
  this method validates `WeaponEntry.AssignedSpells`, not the entry's own
  `AssignedSpells` field.
- `InitializeFromWeapon()` — simply empties `AssignedAbilities`; presets are
  merged in at query time.

### `FRingLoadoutEntry` (`USTRUCT`)

The ring equivalent — thinner, because the inventory entry already carries the
assigned spells. It wraps a single `FRingInventoryEntry RingEntry`.

Notable behavior:

- `GetLockedSpellCount()` — returns `PresetSpells.Num()` when the ring's
  `bSpellsLocked` is true, else 0. Mirrors `FWeaponLoadoutEntry`'s ability
  locking.
- `GetCustomizableSpellCount()` — `LoadoutConstants::MAX_RING_SPELLS` minus
  locked count.
- `GetAllSpells()` — for locked rings returns presets only; otherwise
  sequential override merge of `RingEntry.GetSpells()` over `PresetSpells`,
  capped at `MAX_RING_SPELLS`.
- `GetLockedSpells()` / `GetCustomizableSpells()` — partition accessors.
- `ValidateSpells()` — ownership + element match against `RingEntry.GetElement()`.
- `IsEvolved()` — delegates to `RingEntry.IsEvolved()`; evolved rings cost 2
  loadout slots instead of 1.
- `InitializeFromRing()` — empties `RingEntry.AssignedSpells`.

### `FCombatLoadout` (referenced, defined elsewhere)

The unit stored in `SavedLoadouts`. Not in scope for this document, but the
fields the component reads include: `LoadoutName`, `PrimarySlotType`
(`EPrimarySlotType`: Weapon / Ring / Evolution), `SecondarySlotType`
(`ESecondarySlotType`: None / Weapon), `PrimaryWeapon`, `SecondaryWeapon`
(`FWeaponLoadoutEntry`), `PrimaryRing` (`FRingLoadoutEntry`), `RingLoadout`
(`TArray<FRingLoadoutEntry>` for Resonators), `ActiveRingIndex`,
`PrimaryEvolution` (`UItemData*`), `EvolutionSpells`, `InnateSpells` (Caster),
`ItemSlots` (`TArray<FItemLoadoutSlot>`), `bShowPrimary` and
`bUseWeaponParryAnimation`. It also exposes `InitializeForClass`, `Clear`,
`CreateFromAsset`, `GetAllAbilities`, `GetAllSpells`, `GetUsableItemSlots` and
`HasDuplicateItemTypes`.

## How It Works

### Initialization

1. `BeginPlay()` calls `EnsureDefaultLoadout()`, which creates a single
   `"Default"` loadout (`InitializeForClass(CharacterClass)`) if none exist.
2. `UCharacterDataComponent::BeginPlay()` then calls
   `InitializeFromCharacterData(CharacterData, Inventory)` on this component
   (after initializing the inventory). This:
   - Sets `CharacterClass` from `CharacterData->CharacterClass`.
   - If `CharacterData->DefaultLoadout` is set, calls `InitializeFromAsset()`
     (AI path, or a player template).
   - Otherwise clears `SavedLoadouts` and creates a fresh empty `"Default"`
     loadout for manual UI setup, with `bInitializedFromAsset = false`.
3. `InitializeFromAsset(ULoadoutData*)` logs any validation errors (non-blocking),
   sets `CharacterClass` from `LoadoutAsset->RequiredClass`, replaces
   `SavedLoadouts` with a single `FCombatLoadout::CreateFromAsset(...)`, marks
   `bInitializedFromAsset = true` and `bIsReadyForBattle = true` (asset-based
   loadouts are always considered ready).

### Loadout management

`CreateNewLoadout`, `CreateAndConfigureLoadout`, `DeleteLoadout`,
`DuplicateLoadout`, `RenameLoadout`, `AutoPopulateLoadout` and `ClearLoadout`
mutate `SavedLoadouts`. `DeleteLoadout` refuses to remove the last loadout and
adjusts `ActiveLoadoutIndex`. `CreateAndConfigureLoadout` populates spells,
abilities and item slots, validating each against the supplied inventory and
skipping anything unowned.

### Validation and battle prep

1. `ValidateActiveLoadout` / `ValidateLoadout` delegate to
   `GetValidationErrors()`. Asset-based loadouts short-circuit to `true` (no
   inventory check). For every error found, `OnValidationFailed` is broadcast.
2. `GetValidationErrors()` checks: primary weapon/ring/evolution ownership and
   assigned ability/spell ownership; secondary weapon ownership (Generic);
   Resonator ring-loadout ownership plus a **slot-cost budget** (1 per normal
   ring, 2 per evolved ring, via `InventoryConstants::GetRingSlotCost`) against
   `RESONATOR_RING_SLOTS_NORMAL` / `RESONATOR_RING_SLOTS_EVOLVED`; Caster innate
   spell ownership and element match; item-slot ownership; and duplicate item
   types.
3. `PrepareForBattle()` — asset-based loadouts just reset item-slot uses
   (`ResetForBattle()`) and set `bIsReadyForBattle = true`. Player loadouts must
   pass `ValidateActiveLoadout` first; failure leaves `bIsReadyForBattle = false`.

### Runtime combat queries

While in combat, callers query the component:

- **Weapon resolution** — `GetActiveWeapon()` / `GetActiveWeaponLoadout()`
  resolve the currently usable weapon per class. Key rule: for **Generic
  dual-weapon** loadouts, `bShowPrimary` is a *real gameplay toggle* (only the
  shown weapon's attacks/spells are available). For **Caster/Resonator** and
  non-dual configurations, `bShowPrimary` controls *stance display only* and all
  spells stay available. Evolution-primary characters have no weapon (except an
  evolved Generic, which still uses its secondary weapon).
- **Spell access** — `GetAvailableSpells` (whole loadout), `GetPrimarySlotSpells`
  / `GetSecondarySlotSpells`, `GetActiveSlotSpells` (respects the Generic
  dual-weapon toggle), `GetCombatSpells`, `GetWeaponResonateSpells`,
  `GetRingResonateSpells`.
- **Ability access** — `GetAvailableAbilities`.
- **Item use** — `UseItem(SlotIndex)` decrements an item slot's uses (gated by
  `bIsReadyForBattle`) and broadcasts `OnItemUsed`. `GetItemRemainingUses`,
  `GetUsableItems`, `GetUsableItemCount` support UI.
- **Stance/animation** — `GetCurrentStance` (visual, driven by `bShowPrimary`),
  `GetCurrentIdleMontage`, `GetCurrentAttack` (respects `OverrideAttack`),
  `GetCurrentAttackMontage`, plus defense/cosmetic montages
  (`GetDodgeLeftMontage`, `GetDodgeRightMontage`, `GetBlockMontage`,
  `GetParryMontage`, `GetItemUseAnimation`, `GetRingSwitchMontage`) which read
  from the sibling `UCharacterData` via `GetOwnerCharacterData()`.
- **Equipment switching** — `ToggleEquipment()` flips `bShowPrimary` and notifies
  `UTurnManager::OnActorSpeedChanged`. `SetActiveRingIndex()` changes the active
  Resonator ring and likewise notifies the turn manager.

### Crystal and stat queries

- `GetEquippedCrystals()` enumerates every crystal-bearing slot (primary &
  secondary weapon, primary ring, every Resonator ring, the primary evolution
  crystal) and returns non-empty `FEquippedCrystalSlot` entries. Crucially, it
  reads crystals from the **runtime inventory entry**
  (`WeaponEntry.AttachedCrystal`), not the asset's `SlottedCrystal` field, since
  the asset field is nulled by combat-end crystal-break cleanup.
- `FindCrystalEntryByHolder(UObject*)` returns the mutable
  `FCrystalInventoryEntry*` matching a holder asset (by identity, across primary
  / secondary weapon and the ring loadout). It is the single resolution point
  for crystal wear/repair/break writes.
- `GetActiveStatBonus(AActor*)` returns a combined `FEquipmentStatBonus` with
  per-class resolution (Generic dual weapon uses the active weapon; Generic
  ring+weapon sums both; Caster uses the primary slot only; Resonator sums the
  active ring plus the primary weapon). It uses a private `AccumulateBonus`
  helper to add field-wise.
- `GetActiveEffects(AActor*)` returns equipment-level `FSkillEffect`s per class
  (Generic active weapon; Caster primary slot; Resonator active ring). Evolution
  crystal effects are explicitly *not* included here.
- `GetActivePrimaryEvolutionCrystal(AActor*)` returns the evolution crystal
  slotted specifically in the primary *weapon* slot, or nullptr.

### Post-battle

`ConsumeUsedItems(Inventory)` removes used consumables from inventory based on
each `FItemLoadoutSlot::GetItemsToConsume()`. `GetItemsToConsume()` previews the
same list. `ResetBattleState()` clears `bIsReadyForBattle` and resets item slots.

## Integration Points

### Delegates broadcast

- `FOnLoadoutChanged OnLoadoutChanged(int32 NewLoadoutIndex)` — fired by
  `SetActiveLoadoutIndex()`. Consumers re-query `GetActiveLoadout()`.
- `FOnLoadoutItemUsed OnItemUsed(int32 SlotIndex, UItemData* Item)` — fired by
  `UseItem()`.
- `FOnLoadoutValidationFailed OnValidationFailed(const FString& Reason)` — fired
  once per error by `ValidateLoadout()`.

### Subsystems / systems it depends on

- `UTurnManager` (`UGameInstanceSubsystem`) — notified via `OnActorSpeedChanged`
  from `ToggleEquipment()` and `SetActiveRingIndex()` so cached turn-speed
  re-reads. Resolved through `GetGameInstance()->GetSubsystem<UTurnManager>()`.
- `UInventoryComponent` — sibling component; passed in for validation, battle
  prep, configuration and post-battle consumption (not cached).
- `UCharacterDataComponent` / `UCharacterData` — sibling component looked up via
  `GetOwnerCharacterData()` for class info and defense/cosmetic montages.
- Data assets it reads: `ULoadoutData`, `UWeaponData`, `URingData`, `UItemData`,
  `USpellData`, `UAbilityData`, `UWeaponAttackData`, `UStanceData`,
  `UInfusionDisplayData`.

### Systems that depend on it

- `UCharacterDataComponent` — calls `GetActiveWeapon`, `GetPrimarySlotType`,
  `GetActiveStatBonus`, `GetActivePrimaryEvolutionCrystal` for crystal-aware
  pillar/pool calculations and `HasUsableEPTarget`.
- `UCrystalManager` (per code comments) — uses `GetEquippedCrystals()` for
  combat-init crystal enumeration and `FindCrystalEntryByHolder()` for
  wear/repair/break writes.
- AI evaluation — uses `GetAllWeaponAttacks`, `GetAvailableAbilities`,
  `GetAvailableSpells`.
- Combat UI — uses loadout-management and item-slot accessors.

## Known Limitations / TODOs

- `FWeaponLoadoutEntry::GetLockedAbilityCount()` carries a
  `// TODO: Add LockedAbilityCount to WeaponData if partial locking is desired`
  — currently ability locking is all-or-nothing.
- `AutoPopulateLoadout()` carries `// TODO: Implement smarter auto-population`;
  it only assigns the first available weapon and does not auto-assign items.
- `SavedLoadouts` is **not a `UPROPERTY`** and is not serialized — there is no
  save/load persistence for player loadouts through the reflection system.
- `HasRingInSecondary()` unconditionally returns `false` after its early-out
  checks — the "ring in secondary slot" case is effectively unimplemented.
- `FWeaponLoadoutEntry::ValidateSpells()` validates `WeaponEntry.AssignedSpells`
  rather than the entry's own `AssignedSpells` field — a possible inconsistency
  worth confirming against intended design.
- `InitializeFromCharacterData()` notes that, for players with an inventory, the
  loadout is currently accepted from `DefaultLoadout` without inventory
  validation ("inventory available for future validation") — that validation
  pass is not yet wired.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
