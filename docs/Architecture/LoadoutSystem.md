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
  `Evolution` / `AugmentStone` / `Fusion`).
- `RefinedId` — `FCrystalId` (meaningful when `Kind == Crystal` or
  `Kind == AugmentStone` — both carry an `FCrystalId` identity; a `Fusion` slot
  instead carries an `FFusionId` of two halves + a bonus stat).
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
  list**. The customizable slot count now keys on the **weapon's tier**
  (`SlotsForContainerTier`, see *slot caps scale on container tier* below);
  `LoadoutConstants::MAX_WEAPON_ABILITIES` remains the absolute ceiling. Empty by default.
- `AssignedAugmentStoneAbilities` — `TArray<UAbilityData*>`; a **separate**
  sequential override list for the slots an attached `AbilityStone` grants
  (parallel to `AssignedAbilities`, not merged with it). Its cap is **per-tier**,
  not a flat constant — see *stone-ability slots* below.
- `AssignedSpells` — `TArray<USpellData*>`; sequential override list, valid only
  if the weapon has a crystal/evolution.
- `OverrideAttack` — `USkillDataBase*` (post the attack/ability merge — holds a
  `UAbilityData` with `bIsAttack=true`); per-loadout attack override that, if set,
  replaces the weapon asset's `WeaponAttack`.

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
  capped via `CrystalEffectTable::ResolveSpellSlotCap(Attachment, <fall-through>)`:
  a gem-crystal attachment (or a fusion's gem-half) keys the per-tier
  `GetAttachmentSlotsForTier` curve; otherwise the **weapon's own tier** caps it —
  `SlotsForContainerTier(WeaponEntry.Weapon->Tier)` is passed as the fall-through
  `FlatCeiling` (with `LoadoutConstants::MAX_SPELL_SLOTS` as the no-weapon fallback).
- `ValidateAbilities()` — validates each assigned (non-null) ability and
  **hard-fails** (`return false`, no soft-warning path) on any of:
  - the ability is not owned (`OwnedAbilities.HasAbility` is false);
  - `Ability->RequiredWeaponType` does not equal the weapon's `WeaponType`;
  - `Ability->AllowsWieldMode(weapon's WieldMode)` is false — the ability
    declares a `RequiredWieldMode` (`EWeaponWieldMode`) the equipped weapon
    doesn't satisfy. `Single` requires nothing (any wield mode passes); `Dual`
    accepts only `Dual` weapons; `OffHandShield` accepts only sword-and-shield.

  It also fails if `AssignedAbilities.Num()` exceeds
  `GetCustomizableAbilityCount()`. Set `RequiredWieldMode` to `Dual` or
  `OffHandShield` on an ability whose animation or mechanics only make sense
  with that specific off-hand configuration.
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
    (AbilityStone F=1, E=2, D=3, C=4, B=5, A=6, S=6; **0** when no augment stone is
    attached, so any assigned stone-abilities are then rejected).
  - Authored storage is `FSavedLoadout::PrimaryAugmentStoneAbilities` /
    `SecondaryAugmentStoneAbilities`, copied by `FCombatLoadout::CreateFromSavedLoadout`
    into the loadout entries' `AssignedAugmentStoneAbilities`.

  See `AugmentStoneSystem.md` for the slot table and the
  `GetAttachmentSlotsForTier(FCrystalId)` helper; its sibling
  `SlotsForContainerTier(EItemTier)` applies the same curve to a container's own
  tier (now driving weapon/ring/evolution native slots).
- `InitializeFromWeapon()` — empties `AssignedAbilities` (and
  `AssignedAugmentStoneAbilities`); presets are merged in at query time.

### `FRingLoadoutEntry` (`USTRUCT`)

The ring equivalent — thinner, because the inventory entry already carries the
assigned spells. It wraps a single `FRingInventoryEntry RingEntry`.

Notable behavior:

- `GetLockedSpellCount()` — returns `PresetSpells.Num()` when the ring's
  `bSpellsLocked` is true, else 0. Mirrors `FWeaponLoadoutEntry`'s ability
  locking.
- `GetCustomizableSpellCount()` — the ring's **tier** cap
  (`SlotsForContainerTier(RingEntry.Ring->Tier)`, gem tier when a gem is attached)
  minus locked count; `MAX_RING_SPELLS` is the no-ring fallback.
- `GetAllSpells()` — for locked rings returns presets only; otherwise
  sequential override merge of `RingEntry.GetSpells()` over `PresetSpells`,
  capped on the ring's tier via `ResolveSpellSlotCap(Attachment, SlotsForContainerTier(ring tier))`
  (gem tier overrides; `MAX_RING_SPELLS` the no-ring fallback).
- `GetLockedSpells()` / `GetCustomizableSpells()` — partition accessors.
- `ValidateSpells()` — ownership + element match against `RingEntry.GetElement()`, via
  `ElementHelpers::SpellElementMatchesHost` (a **Generic** spell is a wildcard — it resolves
  to the host element at cast; Reality/BD hosts accept any element; concrete-element spells
  still must match).
- `IsEvolved()` — delegates to `RingEntry.IsEvolved()`; evolved rings cost 2
  loadout slots instead of 1.
- `InitializeFromRing()` — empties `RingEntry.AssignedSpells`.

### Slot caps scale on container tier

Slot counts are driven by one shared curve (`AugmentStoneConstants::ATTACHMENT_SLOTS`
= `{1,2,3,4,5,6,6}`, F→S) via `CrystalEffectTable::SlotsForContainerTier(EItemTier)`.
Each container keys the curve on **its own tier**:

- **Weapon abilities** — `FWeaponLoadoutEntry::GetCustomizableAbilityCount()` keys on
  the weapon tier (defined in the `.cpp`); `ValidateAbilities` inherits it.
- **Weapon native spells** / **ring spells** — `SlotsForContainerTier(container tier)`
  is passed as the **fall-through `FlatCeiling`** to the unchanged `ResolveSpellSlotCap`,
  so a gem/fusion-gem attachment still keys the **gem's** tier and the container tier
  applies only when there is no gem.
- **Evolution spells** — capped in three `LoadoutComponent.cpp` sites
  (`GetValidationErrors`, `GetValidationFindings`, the `EvolutionOverflow` trim), each
  keyed on `Loadout.PrimaryEvolution.Item->Tier`. `ResolveSpellSlotCap` is **not** on the
  evolution path (evolution has no attachment crystal).

The flat `MAX_WEAPON_ABILITIES` / `MAX_SPELL_SLOTS` / `MAX_RING_SPELLS` /
`MAX_EVOLUTION_SPELLS` constants remain defined as absolute ceilings / no-container
fallbacks — no longer the live cap. `UInventoryDebug::LogActiveLoadout` prints a
per-container `Slots [tier]: used/cap` readout.

### Caster innate spell budget

Caster innate spells (and the Broken Darkness pools — see `BrokenDarkness.md`) use a
**weighted budget** distinct from the equipment slot-count model. Constants + helpers live
in `Loadout/SpellPoolConstants.h`. **Two independent limits, both must pass:**

- **Count** — each innate school holds ≤ `SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL` (6)
  spells (tier-blind). (Per-school count was previously unenforced; the old flat
  `MAX_INNATE_SPELLS_TOTAL = 24` is retired.)
- **Weight** — Σ `SpellSlotEffectiveCost(tier, discount)` ≤ `INNATE_SPELL_BUDGET` (24).
  A spell costs its tier (F1…S7); the **per-pillar mastery discount** (`SpellSlotDiscount`,
  reading the character's raw world Mind/Body/Spirit ≥ 4/7/5 → −1 each, floored at 1)
  lowers costs. BD doubles the budget to `BD_SPELL_BUDGET` (48), shared across pools.

**Validation split:** the count cap is enforced at **both** the asset gate
(`FSavedLoadout::GetValidationErrors`) and the runtime gate
(`ULoadoutComponent::GetValidationErrors` + `CollectInvalidSlotFindings`). The weight check
is **runtime-only** — the asset path has no character to compute the discount (null
`CharData` → discount 0, weight still enforced). `ValidateBDSpellLoadout` carries the same
split via its `(int32 Discount, bool bCheckWeight)` parameters.

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
  - **BD (single active pool).** For a `IsBrokenDarkness()` caster, `GetAvailableSpells`
    returns **only the single active pool** — `InnateSpells` when `GetActivePool() ==
    Darkness`, else the matching `BDSpellPools` entry. The non-BD path (full
    `GetAllSpells`) is unchanged. (Was: Darkness pool always + every absorbed pool.) See
    `BrokenDarkness.md`.
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
  - **Per-point tier scaling (tier-power arc, 2026-06-21).** Each contributing item's
    **12 substat** fields are multiplied by `TierPowerScaling::GetTierPowerMultiplier(itemTier)`
    (weapon/ring/evolution `->Tier`) **before** the field-wise sum — so a higher-tier item's
    points are worth more, and this is the **only** place per-item tier survives (the downstream
    conversion sites read the already-summed, tierless `Combined`). **Both signs** scale by
    magnitude (a cursed S −8 roll → ~−38). Substats accumulate as **float, rounded once** into
    the int fields, so the 12 consumers are unchanged. **Excluded:** `BonusMaxHP`/`BonusMaxEnergy`
    (pools — own 3.0/3.5 gear rate in `RecomputeMaxPools`; scaling here too would double-dip) and
    the 3 pillar-percent fields. Pairs with the **flat ~20** substat budget (`FIXED_SUBSTAT_BUDGET`)
    now rolled at every tier. See `TierPowerScaling.md`.
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

- `FOnActiveWeaponChanged OnActiveWeaponChanged(UWeaponData *NewWeapon)` (B-138): fired only when `GetActiveWeapon()` really changes. Every loadout mutator that can change it announces on each return path (a scope guard calls `NotifyActiveWeaponChanged`); the component also re-checks on the inventory's `OnInventoryChanged` (loads and gear removal edit `SavedLoadouts` directly). Code that edits `SavedLoadouts` itself calls `NotifyActiveWeaponChanged()` after. Listener: `UWeaponMeshComponent`.
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
| 2026-06-21 | Generic Spell Inheritance arc. `GetAvailableSpells` BD path now returns the **single active pool** (`GetActivePool()` — Darkness/`InnateSpells` or the matching `BDSpellPools` entry), gated on `IsBrokenDarkness()`; non-BD path unchanged (was: Darkness pool always + every absorbed pool). Element-match validation (`FWeaponLoadoutEntry`/`FRingLoadoutEntry::ValidateSpells`, the `LoadoutComponent` findings, `FCombatLoadout::ValidateBDSpellLoadout`) routes through `ElementHelpers::SpellElementMatchesHost` — **Generic** spells are wildcards (resolve to the host element at cast). See `BrokenDarkness.md`, `InfusionSystem.md`. | feature/generic-spell-inherit |
| 2026-06-07 | Weapon-stone alignment — `FEquippedCrystalSlot.Kind` corrected to `{None, Crystal, Evolution, WeaponStone}` (`RefinedId` carries `WeaponStone` identity too); documented the per-tier stone-ability slots (`AssignedWeaponStoneAbilities`, `GetWeaponStoneAbilities`, `ValidateWeaponStoneAbilities` capped by `GetAttachmentSlotsForTier`) and the `FSavedLoadout`/`FCombatLoadout` plumbing. See new `AugmentStoneSystem.md`. *(The `WeaponStone*` names in this row were renamed to `AugmentStone*` on 2026-06-08, commit `e9d22103` etc.; the doc body uses the current names.)* | feature/weapon-stones |
| 2026-06-16 | Doc-sync: `FEquippedCrystalSlot.Kind` gains `Fusion` (+`FFusionId`); `FWeaponLoadoutEntry::GetAllSpells` cap re-pointed to `CrystalEffectTable::ResolveSpellSlotCap` (tier gem-crystal/fusion-gem-half spell slots, `0e920df7`), not a flat `MAX_SPELL_SLOTS`; flagged the stale `WeaponStone*` names in the 2026-06-07 changelog row. | feature/realtime-defense |
| 2026-06-11 | Accumulate cleanup — `LoadoutComponent.cpp`'s anon-namespace `AccumulateBonus`/`AccumulateResistance` replaced at all 10 call sites by the `FEquipmentStatBonus::Accumulate` / `FResistanceBonus::Accumulate` struct members (behaviour-identical field-wise add); the duplicate helpers deleted. | chore/legacy-cleanup |
| 2026-06-17 | Attack/ability merge — `FWeaponLoadoutEntry::OverrideAttack` (and `UWeaponData::WeaponAttack`) widened to `USkillDataBase*` (hold a `UAbilityData` with `bIsAttack=true`). **Slotting gate added:** `FAbilityCollection::LearnAbility` rejects `IsAttack()` assets (basic attacks never enter the learned pool), with belts in `GetAbilitiesForWeaponType` and `ValidateAbilities`/`ValidateAugmentStoneAbilities` — a basic attack can't be slotted as an ability (the functional payoff of `bIsAttack`). `UWeaponAttackData` deleted. | feature/realtime-defense |
| 2026-06-20 | Equipment slot tier scaling — slot caps now key on each container's own tier via the shared curve `{1,2,3,4,5,6,6}` (`CrystalEffectTable::SlotsForContainerTier`): weapon abilities (`GetCustomizableAbilityCount`, moved to `.cpp`) on weapon tier; weapon native + ring spells pass `SlotsForContainerTier(tier)` as the `ResolveSpellSlotCap` fall-through `FlatCeiling` (gem-keying preserved); evolution spells keyed at the three `LoadoutComponent.cpp` cap sites. Flat `MAX_*` constants retained as absolute ceilings/fallbacks. New *Slot caps scale on container tier* section; cap-line corrections throughout; per-container slot readout added to `InventoryDebug`. | feature/equipment-slot-tier-scaling |
| 2026-06-20 | Caster innate / BD spell budget — weighted-budget model in new `Loadout/SpellPoolConstants.h` (cost curve F1…S7, `INNATE_SPELL_BUDGET=24` / `BD_SPELL_BUDGET=48`, per-pillar discount Mind4/Body7/Spirit5, `MAX_EQUIPPED_SLOT_POOL=6`, helpers). Per-school count ≤6 enforced at both asset + runtime gates; weight budget runtime-only (needs the character's discount; null `CharData`→0, still enforced). `ValidateBDSpellLoadout` gained `(Discount, bCheckWeight)`. Retired `MAX_INNATE_SPELLS_PER_SCHOOL`/`_TOTAL` + `MAX_BD_POOL_SPELLS`. New *Caster innate spell budget* section. | feature/innate-bd-spell-budget |
| 2026-06-21 | Tier-power gear — `GetActiveStatBonus` now applies a **per-item tier multiplier** to each contributing item's 12 substat fields at aggregation (both signs; float-accumulate, round once; pools + pillar-percents excluded) — the only place per-item tier survives before the tierless sum. Pairs with the **flat ~20** substat budget (`FIXED_SUBSTAT_BUDGET`) now rolled at every tier (was F6→S45). See `TierPowerScaling.md`. | feature/tier-power-scaling |
| 2026-07-03 | Wield-mode gate — `ValidateAbilities` / `ValidateAugmentStoneAbilities` and the `LoadoutComponent` finding now call `Ability->AllowsWieldMode(weapon WieldMode)` (replaces the `bRequiresDualWeapon` bool). `RequiredWieldMode=Single` allows any wield mode; `Dual` / `OffHandShield` require an exact match. | feature/hub-merchants |
| 2026-10-26 | B-022 — `GetActiveLoadout()` returns `const FCombatLoadout&` into `SavedLoadouts` (function-static empty loadout when there is none); callers that bind a const reference stop copying, the ~20 by-value callers are unchanged. Don't hold the reference across anything that adds/removes saved loadouts. New BlueprintPure `GetCharacterClass()` = the owner's CharacterData class (field only without CharacterData); all C++ reads use it. The `CharacterClass` UPROPERTY stays, synced, flagged OBSOLETE (B-153 removes it); a Blueprint write to it is ignored while the owner has CharacterData. `EnsureDefaultLoadout` at BeginPlay now seeds with the CharacterData class (no practical change: `InitializeForClass` gives the same empty loadout for every class). Spec `WoR.Loadout.Getters`. | refactor/B-022-loadout-getters |
| 2026-10-26 | B-024 — the ring/weapon managers folded in. New `GetActiveRingElement()` (BlueprintPure) and `SwitchToNextUsableRing()` (BlueprintCallable); the crystal-break ring auto-switch is `HandleCrystalBroken`, called by `UCrystalManager::NotifyCrystalBroken` before it broadcasts `OnCrystalBroken` (fixed order: listeners see the new ring). Every C++ caller of `URingManager` / `UWeaponManager` (ActionExecutor, DurabilityHeaderWidget) reads the actor's loadout component; the two subsystems are OBSOLETE forwarders kept for Blueprints until B-154 removes them. Spec `WoR.Loadout.RingWeaponQueries`. | refactor/B-024-fold-weapon-ring-managers |
| 2026-10-26 | B-026 — `UInventoryComponent` weapon/ring socket pairs (`AttachEvolutionTo*`, `AttachCrystalTo*`, `AttachFusionTo*`, `RemoveEvolutionFrom*`, `RemoveCrystalFrom*`) forward to five private member templates on the entry type (`AttachEvolutionToGear` etc.); ring-only rules (gems only, elemental fusions only) are `if constexpr (IsRingGear<TEntry>)` blocks. Public/Blueprint API, guard order, logs and broadcasts unchanged; removal wear is the named `EVOLUTION_REMOVAL_WEAR_FRACTION`. Spec `WoR.Inventory.GearAttach` (with the test-only `UInventoryChangeRecorder`). | refactor/B-026-gear-attach-template |
| 2026-10-26 | B-028 — one loadout rule set (D-073 = A): `FLoadoutRules::CollectFindings` (`Loadout/LoadoutRules.h`) on `FCombatLoadout` + `FLoadoutRuleContext` (class, BD by character, discount, optional ownership/castability). Pre-fight adapter `ULoadoutComponent::BuildRuleContext` / `CollectInvalidSlotFindings(Index, OwnershipSource)`; `GetValidationErrors` = the findings' reasons. Editor adapter `FSavedLoadout::GetValidationErrors(bBD)` = `CollectAuthoredShapeErrors` + the rules on `CreateFromSavedLoadout` (discount 0, D-080); `UCharacterData` / `UInventoryComponent` pass the character's BD flag. `FInvalidSlotFinding`/`ELoadoutSlotType` moved to `LoadoutRules.h` (+`AbilityOverflow`, `ItemSlot`, report-only). Old bodies kept as `*Legacy` (OBSOLETE) for `wor.Loadout.CompareRules` until PIE; equip-time refusal is B-159; tier caps B-155 | refactor/B-028-one-loadout-validator |
| 2026-10-26 | B-155 — utility crystal tier caps (D-072, D-077): `FLoadoutRules` `CheckUtilityCrystalTierCaps` reports item slots over S 1 / A+B 2 / C+D 3 / E+F 4 gem crystals (per crystal, bands combined; stones and attachments ignored) as a non-clearable `ItemSlot` finding. Constants `LoadoutConstants::MAX_UTILITY_CRYSTALS_*`. Report-only until B-159 | feature/B-155-utility-crystal-caps |
| 2026-10-26 | B-159 — D-081 = A. `ULoadoutComponent::TryAssignWeaponAbility/WeaponSpell/RingSpell/InnateSpell` refuse a pick that adds a rule break (before/after reason diff, `TryChangeLoadout`). `DropInvalidParts(Index)` (the old `ClearInvalidSlots` body, any loadout) now also trims `AbilityOverflow`, out-of-range `ItemSlot` quantities and the new `UtilityCrystalTierCap` (`FLoadoutRules::TrimUtilityCrystalsToTierCaps`); `DropInvalidPartsFromAllLoadouts` runs at the end of `UCharacterDataComponent::BeginPlay` (after the world stats and BD flag are seeded); `OnPartsDropped(LoadoutIndex, Messages)` announces drops, naming the removed abilities / crystals. Picks are refused into an unused primary and when already assigned; empty (dropped) ability entries don't count toward the cap and are filled first. Debug: `wor.Loadout.TryAbility <name>`. Spec `WoR.Loadout.Picks`. Gear-change drops wait on B-085 (B-171) | feature/B-159-refuse-or-drop |
| 2026-10-26 | B-161 — D-080 = D. `FSavedLoadout::GetValidationErrors` / `UInventoryData::GetValidationErrors` take a `SpellDiscount` (default 0 = no character); `UCharacterData::GetAuthoredSpellDiscount()` (authored World levels) is passed by `UCharacterData::IsDataValid` and the `InitializeFromInventoryAsset` log | feature/B-161-editor-discount-authored-stats |
| 2026-10-26 | B-175 (ET-19): one free weapon or ring switch per turn. `ULoadoutComponent::TrySwitchWeaponThisTurn` / `TrySwitchRingThisTurn` wrap `ToggleEquipment` / `SetActiveRingIndex` and refuse once used (`GetTurnSwitchRefusedReason`, LOCTEXT); a switch that changes nothing does not count. `ACombatOrchestrator::ProceedWithTurnStart` calls `ResetTurnSwitch` (normal and bonus turns, Q-175). The command menu greys a used switch with the reason. `ToggleEquipment` stays BlueprintCallable and unguarded (ring break auto-switch, debug). Spec `WoR.Loadout.TurnSwitch` | feature/B-175-one-switch-per-turn |
| 2026-10-26 | B-147 (D-039): spec case: a saved evolution at its tier's spell count passes and one over is refused (`SlotsForContainerTier`). The flat-6 check remains only in `GetValidationErrorsLegacy` (B-172 retires it). | fix/B-147-rulings-in-code |
| 2026-10-26 | Q-163 = B: `UActionExecutor::GetAvailableInfusionSources` skips an active or primary ring whose crystal broke (no element to push), matching the broken weapon crystal; its F-tier spells still cast. Tests in `WoR.Combat.BrokenSources`. | fix/Q-163-broken-ring-infusion |
| 2026-10-26 | Q-163 follow-up: the AI (`DecideSpellInfusionSource`) keeps only sources both allowed for the spell and on offer; the command menu pins a spell's infusion at 0 when its source is not on offer (a broken ring or weapon crystal). Q-163b raised (Caster via Innate, Generic element). | fix/Q-163-broken-ring-infusion |
| 2026-10-26 | Q-161 = D: `FLoadoutRuleContext::bCheckSpellBudget` (default true) lets the rules skip the spell weight budgets; `UInventoryData::GetStandaloneValidation` makes them warnings when an inventory asset is validated on its own (`IsDataValid`). The character's own check is unchanged (D-080). | fix/Q-161-inventory-budget-warning |
| 2026-10-26 | Q-159 = A: `DropInvalidParts` returns the utility crystals it drops (duplicate type, quantity over 3, over a tier band's cap) to the crystal inventory; with a full inventory they are logged as lost until home storage exists (D-097). | fix/Q-159-return-dropped-crystals |
| 2026-10-26 | Q-163b = B: `ULoadoutComponent::IsSpellFromBrokenRing`; `UActionExecutor::GetAllowedInfusionSourcesForSpell` gives such a spell no source and `ResolveSpellCastElement` a Generic one None, so it casts plain. | fix/Q-163b-broken-ring-spells-plain |
| 2026-10-26 | Q-147 = B: an evolution attached to a weapon or ring holds spells by its own instance tier (`ResolveSpellSlotCap` -> `SlotsForContainerTier(Evolution.Tier)`), like a primary-slot one (D-039); before, it took the holder's cap. Gems and fusions unchanged. Spec case in `WoR.Loadout.Rules`. | fix/Q-147-attached-evolution-spells-by-tier |
| 2026-10-26 | B-196 (spell cap): `FWeaponLoadoutEntry` / `FRingLoadoutEntry` split `GetAllSpells` into `GetAllSpellsUncapped`, `GetSpellSlotCap`, `GetSpellsOverCap`; `FLoadoutRules` reports each assigned spell over the holder's cap (clearable) on the primary weapon, primary ring and Resonator rings; spec cases in `WoR.Loadout.Rules`. | fix/B-196-loadout-spell-cap |
| 2026-10-26 | B-204 (D-083, Q-204): `GetBrokenSourceReason` checks the primary, then the secondary weapon (the first that holds the spell decides, `FindSpellCatalystHolder`'s order), not only the weapon in hand; an intact ring that holds the spell wins first (`ResolveSpellSource`'s order). The AI (`GetUsableSpells`) and `ValidateAction` follow. B-197: a broken weapon's loadout-level `AssignedSpells` grey too; `FRingLoadoutEntry::GetAllSpells(bAsIfIntact)`; `ResolveSpellSource` returns `RingCrystal` for a broken ring's F-tier spells. | fix/B-204-broken-secondary-weapon |
| 2026-10-26 | B-099 (D-027): a Generic with an evolution primary keeps its secondary weapon on every path. `HasSecondaryEquipment` no longer returns false for it; `GetCurrentStance` shows the secondary weapon's stance (an evolution has none; a ring primary still shows unarmed); `HasRingInSecondary` is always false (the secondary holds weapons only; no callers, kept until PIE). Already agreeing: `GetActiveWeaponLoadout`, `FCombatCapabilities`, conversion, rules. Not changed: `TrySwitchWeaponThisTurn` doesn't check for two weapons (the UI hides the button); the secondary weapon still gives spells (`FCombatLoadout.cpp:209`, B-098). Spec `WoR.Loadout.EvolutionSecondary`. | fix/B-099-evolution-keeps-secondary |
| 2026-10-26 | B-081 (D-008): `ToggleEquipment` and `SetActiveRingIndex` gather the old gear's effects first, then `SwapGearEffects` (see SkillEffectSystem.md). | feature/B-081-gear-effects-follow-swap |
| 2026-10-26 | B-082: `GetStoneRingAttachment`, `GetAttachedStonePercent(Stat)`, `GetAttachedStonePercentForType` (weapon plus counting ring); see AugmentStoneSystem.md. | feature/B-082-ring-fusion-stone |
| 2026-10-27 | B-138: `OnActiveWeaponChanged` delegate and `NotifyActiveWeaponChanged()`; 13 mutators announce through `FActiveWeaponChangeScope`; the component binds the inventory's `OnInventoryChanged`. | refactor/B-138-weapon-mesh-delegate |
| 2026-10-27 | B-085 (Q-085): the owned weapon / ring / evolution is the truth; loadout slots are caches keyed by `PersistentID` / `PrimaryEvolutionInstance`. `UInventoryComponent::LinkLoadoutsToOwned` (both init paths) links unresolved authored slots to an owned entry of the same asset; `RefreshLoadoutsFromOwned` / `RefreshEvolutionInLoadouts` after every socket op, level-up, downgrade and evolution removal; `WriteLoadoutToOwned` at every fight end (`CombatOrchestrator`), on `SetActiveLoadoutIndex` and after `ApplyCarriedGear` (the active loadout leads mid-fight); `DetachEvolutionFromOwnedGear` on evolution removal; debug `GetOwnedGearSyncString` in `wor.PrintInventory`. Spec `WoR.Inventory.OwnedGearSync`. | refactor/B-085-owned-gear-truth |
