# Loadout System

## Overview

The Loadout System manages a character's **active combat loadout** — the gear,
spells, abilities and consumable items the character is *using* in battle — as
distinct from `UInventoryComponent`, which tracks what the character *owns*.

The header states the architectural split plainly:

> `InventoryComponent` = What you OWN (warehouse)
> `LoadoutComponent`  = What you're USING in battle (equipped gear)

A character can save multiple loadout configurations. `SavedLoadouts` is owned
by `UInventoryComponent` (not `ULoadoutComponent`) and capped by
`UInventoryComponent::MaxSavedLoadouts` (default 5); the loadout component
reaches into the inventory to read them. One loadout is active at a time
(`UInventoryComponent::ActiveLoadoutIndex`). At battle start the active loadout
is validated against inventory ownership.

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
| `CharacterClass` | `ECharacterClass` | Determines loadout rules; defaults to `Generic`. Set from `CharacterData` during initialization. |
| `bIsReadyForBattle` | `bool` | Set true by `PrepareForBattle`; gates `UseItem`. |

`SavedLoadouts` (`TArray<FCombatLoadout>`, `UPROPERTY()`-tagged for GC), `MaxSavedLoadouts` and `ActiveLoadoutIndex` live on **`UInventoryComponent`**, not on `ULoadoutComponent` — the loadout component is a thin facade that reads them through `GetInventoryComponent()` on every accessor.

### `FEquippedCrystalSlot` (`USTRUCT`)

A lightweight pairing of a crystal and its holder, returned by
`GetEquippedCrystals()`:

- `Kind` — `EAttachedItemKind`, the discriminator (`None` / `Crystal` /
  `Evolution` / `AugmentStone`).
- `RefinedId` — `FCrystalId` (meaningful when `Kind == Crystal` or
  `Kind == AugmentStone` — both carry an `FCrystalId` identity).
- `Item` — `UEvolutionItemData*` (meaningful when `Kind == Evolution`).
- `Holder` — `UObject*` (the `UWeaponData` or `URingData` bearing it; `UObject*`
  because weapons and rings share no base beyond `UPrimaryDataAsset`). For an
  evolution crystal slot, `Holder == Item` (evolution *is* the crystal).

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
- `AssignedAugmentStoneAbilities` — `TArray<UAbilityData*>`; a **separate**
  sequential override list for the slots an attached `AbilityStone` grants
  (parallel to `AssignedAbilities`, not merged with it). Its cap is **per-tier**,
  not a flat constant — see *stone-ability slots* below.
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
- `ValidateAbilities()` — validates each assigned (non-null) ability and
  **hard-fails** (`return false`, no soft-warning path) on any of:
  - the ability is not owned (`OwnedAbilities.HasAbility` is false);
  - `Ability->RequiredWeaponType` does not equal the weapon's `WeaponType`;
  - `Ability->bRequiresDualWeapon` is true but the weapon's `IsDualWielded()`
    is false — a dual-only ability assigned to a `Single` weapon. Dual-only
    abilities are accepted on **both** `Dual` and `OffHandShield` weapons,
    since `IsDualWielded()` is true for either.

  It also fails if `AssignedAbilities.Num()` exceeds
  `GetCustomizableAbilityCount()`. Set `bRequiresDualWeapon` on an ability
  whose animation or mechanics only make sense with a weapon in each hand.
- `ValidateSpells()` — checks ownership and element match (skipped for
  "any-element" crystal sources via `ElementHelpers::IsAnySpellSource`). Note:
  this method validates `WeaponEntry.AssignedSpells`, not the entry's own
  `AssignedSpells` field.
- **Stone-ability slots** — when an `AbilityStone` is attached, the weapon gains
  a tier-scaled pool of extra ability slots, filled from
  `AssignedAugmentStoneAbilities` (distinct from the weapon's own
  `PresetAbilities`/`AssignedAbilities`):
  - `GetAugmentStoneAbilities()` — sequential override merge over the
    stone-granted slots, same shape as `GetAllAbilities()`.
  - `ValidateAugmentStoneAbilities(OwnedAbilities)` — validates ownership and caps
    the count at `CrystalEffectTable::GetAttachmentSlotsForTier(Attachment.Crystal.Id)`
    (AbilityStone F=2, E=3, D=3, C=4, B=4, A=5, S=6; **0** when no augment stone is
    attached, so any assigned stone-abilities are then rejected).
  - Authored storage is `FSavedLoadout::PrimaryAugmentStoneAbilities` /
    `SecondaryAugmentStoneAbilities`, copied by `FCombatLoadout::CreateFromSavedLoadout`
    into the loadout entries' `AssignedAugmentStoneAbilities`.

  See `AugmentStoneSystem.md` for the slot table and the generic
  `GetAttachmentSlotsForTier` helper (reusable for crystal spell-slots later).
- `InitializeFromWeapon()` — empties `AssignedAbilities` (and
  `AssignedAugmentStoneAbilities`); presets are merged in at query time.

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
`PrimaryEvolution` (`FEvolutionAttachment` — wraps `UEvolutionItemData*` plus per-instance durability; see `FCombatLoadout.h`), `EvolutionSpells`, `InnateSpells` (Caster),
`ItemSlots` (`TArray<FItemLoadoutSlot>`), `bShowPrimary` and
`bUseWeaponParryAnimation`. It also exposes `InitializeForClass`, `Clear`,
`CreateFromSavedLoadout` (the sole runtime factory; inflates an `FSavedLoadout`
struct stored inline on `UInventoryData`), `GetAllAbilities`, `GetAllSpells`,
`GetUsableItemSlots` and `HasDuplicateItemTypes`.

## How It Works

### Initialization

`UCharacterDataComponent::BeginPlay()` drives the sequence:

1. `UInventoryComponent::InitializeFromCharacterData(CharacterData)` runs
   **first**. It reads `CharacterData->Inventory` (`UInventoryData*` — the sole
   authoring surface) and delegates to `InitializeFromInventoryAsset`, which
   populates the ownership lists (Weapons/Rings/Items/Spells/Abilities) and
   inflates each `FSavedLoadout` on the asset into a runtime `FCombatLoadout`
   via `FCombatLoadout::CreateFromSavedLoadout`. The resulting array is stored
   on the inventory component as `SavedLoadouts`. `ActiveLoadoutIndex` is
   clamped from `UInventoryData::DefaultActiveLoadoutIndex`. Missing-inventory
   characters get an empty inventory and one fallback empty `"Default"`
   loadout (soft-fail).
2. `ULoadoutComponent::InitializeFromCharacterData(CharacterData)` runs after.
   It sets `CharacterClass` from `CharacterData->CharacterClass`. If the
   inventory's `SavedLoadouts` is empty (legacy soft-fail), it seeds one empty
   `FCombatLoadout` initialized for the class.

### Loadout management

`CreateNewLoadout`, `CreateAndConfigureLoadout`, `DeleteLoadout`,
`DuplicateLoadout`, `RenameLoadout`, `AutoPopulateLoadout` and `ClearLoadout`
mutate `SavedLoadouts`. `DeleteLoadout` refuses to remove the last loadout and
adjusts `ActiveLoadoutIndex`. `CreateAndConfigureLoadout` populates spells,
abilities and item slots, validating each against the supplied inventory and
skipping anything unowned.

### Validation and battle prep

1. `ValidateActiveLoadout` / `ValidateLoadout` delegate to
   `GetValidationErrors()`. For every error found, `OnValidationFailed` is
   broadcast.
2. `GetValidationErrors()` checks: primary weapon/ring/evolution ownership and
   assigned ability/spell ownership; secondary weapon ownership (Generic);
   Resonator ring-loadout ownership plus a **slot-cost budget** (1 per normal
   ring, 2 per evolved ring, via `InventoryConstants::GetRingSlotCost`) against
   `RESONATOR_RING_SLOTS_NORMAL` / `RESONATOR_RING_SLOTS_EVOLVED`; Caster innate
   spell ownership and element match; item-slot ownership; and duplicate item
   types.
3. `PrepareForBattle()` — runs `ValidateActiveLoadout` and resets item-slot
   uses (`ResetForBattle()`). On success sets `bIsReadyForBattle = true`;
   failure leaves it `false`.

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
- **Spell source resolution** *(sweep-2)* — `ResolveSpellSource(USpellData*) const`
  walks the active loadout's spell lists in precedence order and returns which
  `ESpellSource` a spell originates from: **Innate** (including BD per-element
  pools) → **RingCrystal** (primary ring + Resonator ring loadout) →
  **WeaponCrystal** (primary + secondary weapon spell lists) → **Evolution**.
  Defaults to `Innate` with a warning if the spell isn't in any loadout
  source — a data inconsistency the AI shouldn't normally hit. Used by
  `AIDecisionManager` at the six AI cost/damage-evaluation sites so probes
  route through the correct cost model; semantically parallel to the player
  path's `UCombatCommandMenuSubsystem::MapCategoryToSpellSource`.
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
  (`WeaponEntry.AttachedItem` / `RingEntry.AttachedItem`, type
  `FRuntimeAttachedItem`), not the asset's design-time `AttachedItem`. The
  crystal-break cleanup operates exclusively on the runtime entry; the asset is
  never mutated at runtime.
- `GetCrystalEntryByHolder(UObject*)` returns the `FRuntimeAttachedItem`
  matching a holder asset (by identity, across primary / secondary weapon and
  the ring loadout). `ResetCrystalEntryByHolder` is the corresponding mutator
  used by break cleanup. These are the single resolution points for crystal
  wear/repair/break writes.
- `GetActiveStatBonus(AActor*)` returns a combined `FEquipmentStatBonus` with
  per-class resolution (Generic dual weapon uses the active weapon; Generic
  ring+weapon sums both; Caster uses the primary slot only; Resonator sums the
  active ring plus the primary weapon). Field-wise summing goes through the
  `FEquipmentStatBonus::Accumulate` struct member (the former file-private
  `AccumulateBonus`/`AccumulateResistance` duplicates were deleted).
- `GetActiveEffects(AActor*)` returns equipment-level `FSkillEffect`s per class
  (Generic active weapon; Caster primary slot; Resonator active ring). Evolution
  crystal effects are explicitly *not* included here.
- `GetActivePrimaryEvolutionCrystal(AActor*)` returns the evolution crystal
  slotted specifically in the primary *weapon* slot, or nullptr.
- `ApplyWearToActivePrimaryEvolution(Amount, bForceWear)` (BlueprintCallable)
  — writes the live `SavedLoadouts[ActiveLoadoutIndex].PrimaryEvolution` (not
  a `GetActiveLoadout` copy). Returns true if the wear broke the crystal.
  `bForceWear=true` bypasses the per-asset `bCanBreak` gate — used by Broken
  Darkness's intrinsic wear path. See `CrystalWear.md`.
- `ClearBrokenPrimaryEvolution()` (BlueprintCallable) — clears the active
  loadout's primary evolution slot if its attachment `IsBroken()`.
  `PrimarySlotType` is left as `Evolution` (slot empties; kind unchanged).
  For between-combat sweep use (`ACombatOrchestrator::ApplyBetweenCombatCrystalDestruction`).

### Post-battle

`ConsumeUsedItems(Inventory)` removes used consumables from inventory based on
each `FItemLoadoutSlot::GetItemsToConsume()`. `GetItemsToConsume()` previews the
same list. `ResetBattleState()` clears `bIsReadyForBattle` and resets item slots.

## Integration Points

### Delegates broadcast

- `FOnLoadoutChanged OnLoadoutChanged(int32 NewLoadoutIndex)` — fired by
  `SetActiveLoadoutIndex()`. Consumers re-query `GetActiveLoadout()`.
- `FOnLoadoutItemUsed OnItemUsed(int32 SlotIndex, UEvolutionItemData* Item)` — fired by
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
- Data assets it reads: `UInventoryData` (indirectly via `UInventoryComponent`),
  `UWeaponData`, `URingData`, `UEvolutionItemData`, `USpellData`, `UAbilityData`,
  `UWeaponAttackData`, `UStanceData`, `UInfusionDisplayData`.

### Systems that depend on it

- `UCharacterDataComponent` — calls `GetActiveWeapon`, `GetPrimarySlotType`,
  `GetActiveStatBonus`, `GetActivePrimaryEvolutionCrystal` for crystal-aware
  pillar/pool calculations and `HasUsableEPTarget`.
- `UCrystalManager` (per code comments) — uses `GetEquippedCrystals()` for
  combat-init crystal enumeration and `GetCrystalEntryByHolder()` /
  `ResetCrystalEntryByHolder()` for wear/repair/break writes. Also calls
  `ApplyWearToActivePrimaryEvolution` from `ProcessPostCastEvolutionWear` (the
  BD-evolution case-B wear path) and `ClearBrokenPrimaryEvolution` from the
  between-combat sweep.
- AI evaluation — uses `GetAllWeaponAttacks`, `GetAvailableAbilities`,
  `GetAvailableSpells`.
- Combat UI — uses loadout-management and item-slot accessors.

## Known Limitations / TODOs

- `FWeaponLoadoutEntry::GetLockedAbilityCount()` carries a
  `// TODO: Add LockedAbilityCount to WeaponData if partial locking is desired`
  — currently ability locking is all-or-nothing.
- `AutoPopulateLoadout()` carries `// TODO: Implement smarter auto-population`;
  it only assigns the first available weapon and does not auto-assign items.
- `HasRingInSecondary()` unconditionally returns `false` after its early-out
  checks — the "ring in secondary slot" case is effectively unimplemented.
- `FWeaponLoadoutEntry::ValidateSpells()` validates `WeaponEntry.AssignedSpells`
  rather than the entry's own `AssignedSpells` field — a possible inconsistency
  worth confirming against intended design.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-19 | Documented the `bRequiresDualWeapon` hard-fail in `FWeaponLoadoutEntry::ValidateAbilities` (dual-only abilities reject `Single` weapons). | feature/weapon-sockets-dual-flag |
| 2026-05-21 | Inventory redesign merged — `ULoadoutData` and `UCharacterData::DefaultLoadout` deleted; `UInventoryData` is the sole authoring surface and owns the inline `FSavedLoadout` array. `SavedLoadouts` / `ActiveLoadoutIndex` / `MaxSavedLoadouts` moved from `ULoadoutComponent` to `UInventoryComponent`; the loadout component now reads them via `GetInventoryComponent()`. `InitializeFromAsset` and the `bInitializedFromAsset` validation-bypass field removed; `FCombatLoadout::CreateFromSavedLoadout` is the sole runtime factory. Initialization, Validation, Field-table and Integration-Points sections updated. | feature/inventory-refactor |
| 2026-05-27 | Two new BlueprintCallable wear/clear helpers — `ApplyWearToActivePrimaryEvolution(Amount, bForceWear)` and `ClearBrokenPrimaryEvolution()` — for the case-B standalone primary-slot evolution (BD wear path). Both write the live `SavedLoadouts[ActiveLoadoutIndex]` storage. Also corrected the `FCombatLoadout::PrimaryEvolution` field-shape note (it's `FEvolutionAttachment`, not a raw `UEvolutionItemData*` — pre-existing drift surfaced by this branch). | feature/crystal-wear-substat-modifier |
| 2026-05-28 | Sweep-2 — added `ResolveSpellSource(USpellData*) const` for AI spell-source determination (precedence: Innate → RingCrystal → WeaponCrystal → Evolution). Removed dead struct-side `FCombatLoadout::Validate`/`ValidateGeneric`/`ValidateCaster`/`ValidateResonator` (`ULoadoutComponent::GetValidationErrors` is the live path; `ValidateBDSpellLoadout` retained — still shared with `FSavedLoadout::GetValidationErrors`). | feature/integration-gaps-sweep-2 |
| 2026-06-07 | Weapon-stone alignment — `FEquippedCrystalSlot.Kind` corrected to `{None, Crystal, Evolution, WeaponStone}` (`RefinedId` carries `WeaponStone` identity too); documented the per-tier stone-ability slots (`AssignedWeaponStoneAbilities`, `GetWeaponStoneAbilities`, `ValidateWeaponStoneAbilities` capped by `GetAttachmentSlotsForTier`) and the `FSavedLoadout`/`FCombatLoadout` plumbing. See new `AugmentStoneSystem.md`. | feature/weapon-stones |
| 2026-06-11 | Accumulate cleanup — `LoadoutComponent.cpp`'s anon-namespace `AccumulateBonus`/`AccumulateResistance` replaced at all 10 call sites by the `FEquipmentStatBonus::Accumulate` / `FResistanceBonus::Accumulate` struct members (behaviour-identical field-wise add); the duplicate helpers deleted. | chore/legacy-cleanup |
