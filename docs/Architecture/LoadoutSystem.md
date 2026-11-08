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

- B-320 (D-116, Q-320): only the ACTIVE loadout's empty primary is refilled; a spare loadout may stay empty. The refusal checks the dismantling member's own loadouts: a piece another member wears as primary isn't guarded (Q-320b follow-up). A refill after `DropInvalidParts` isn't named in the "Removed:" toast, and a pool-driven strip (`HandlePoolGearChanged` -> `RefreshLoadoutsFromOwned`) broadcasts no loadout change.
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
- The B-302 slot mirror (`FCombatLoadout::Slots`) is rebuilt only for the
  active loadout, and only when the loadout component announces a gear change.
  `InventoryComponent` edits that don't announce (run-preset swap,
  `LinkLoadoutsToOwned` / `StripUnownedGear`, loadout inflation) leave it stale
  until the next announcement. Nothing reads it yet; B-304 heals it on read.
- B-303: a Resonator with an adapter in the *primary* slot (blocked for saved
  loadouts, not for runtime edits) has it counted in the budget finding, but
  `ClearInvalidSlots` drops only `RingLoadout` adapters, so the finding could
  survive the clear. Not reachable from today's screens.
- B-310: carried run wear with a GUID is matched by GUID only. If an item is
  relinked to a new GUID mid-run, that slot starts the next fight with fresh
  wear (old saves without GUIDs still match by asset, in slot order).
- B-304: the worn-elsewhere skip reads the pool's wearer without checking the
  wearer is in the party (TODO in `IsWornByAnother`), so a stale wearer key
  hides a piece; and a wearer change doesn't announce a gear change, so the
  weapon mesh can show the old pick until the next announcement.
- B-304: the active-gear getters are `const` but bring the slot mirror in step
  (a cache) on every read: game thread only.

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
| 2026-10-27 | B-171 (D-081, Q-171): `ULoadoutComponent::HandleGearSocketChanged` (bound to the inventory's new `OnGearSocketChanged`) drops, in each loadout using that gear, only what a socket can break (weapon / ring spells, weapon abilities, the overflows) via `DropInvalidParts(Index, OnlySlotTypes)`, announcing through `OnPartsDropped`; skipped while combat is active. Spec `WoR.Loadout.GearChangeDrops`. | feature/B-171-gear-change-drops |
| 2026-10-27 | B-091 (D-020, Q-091): one shared armoury for the party. `UInventoryComponent::DrawSharedGear` (party members, at spawn) banks starting gear into `UPoolSubsystem` once, takes the whole armoury and re-links the presets; `ClaimActiveSharedGear` / `ULoadoutComponent::ClaimSharedGear` (spawn, loadout switch, `PrepareForBattle`) keep one wearer per piece; `PushGearToPool` / `HandlePoolGearChanged` keep every member's copy in step. Spec `WoR.Pool.SharedGear`. | feature/B-091-shared-gear-pool |
| 2026-11-02 | B-258 (P1): the read paths take `GetActiveLoadout()` by const reference (21 of 22 sites); the copies that stay are where the loadout changes while used (`CrystalManager` before-wear, `ActionExecutor`). | refactor/B-258-loadout-by-ref |
| 2026-11-02 | B-285: spec `WoR.Loadout.Budget` pins the Resonator budget 5 and slot costs (CL-10..12) and the innate budget 24 / 48 with tier costs and pillar cuts (CL-34 / 35). No code change. | chore/B-285-loadout-budget-spec |
| 2026-11-02 | B-301 (D-110 step 1): `FGearInstance` (+ `EGearKind` Weapon / Adapter, append-only) is one owned-gear shape for a weapon or an adapter, with converters to and from `FWeaponInventoryEntry` / `FRingInventoryEntry`. Nothing reads it yet (B-302 mirrors slots onto it). Spec `WoR.Loadout.GearInstance`. | refactor/B-301-gear-instance |
| 2026-11-02 | B-302 (D-110 step 2, D-111): `FLoadoutSlot` (the owned item by GUID is the truth, the gear copy a derived cache) and `FCombatLoadout::Slots` / `ActiveWeaponSlot` / `ActiveAdapterSlot`, rebuilt from the old fields by `RebuildSlotsFromLegacy(Class)` after every loadout change; the mirror holds what the fight reads for the class. Nothing reads it yet (B-303 moves the rules onto it). Debug `wor.Loadout.CompareSlots` (`ULoadoutRulesDebug::GetCompareSlotsString`). Spec `WoR.Loadout.Slots`. | refactor/B-302-loadout-slots |
| 2026-11-03 | B-303 (D-110 step 3): `LoadoutClassTable` (Refractor 3 / 1 item; Generic 6 / 2 items, 2 weapons, 1 adapter (Q-303); Resonator 5 / 1 weapon / any adapters) and one `FLoadoutRules::SlotCost` over the unified slots; `CollectClassLimitFindings` replaces the Resonator-only budget block, and `ClearInvalidSlots`' adapter drop uses the same `NonAdapterCost`. Same results, except the budget charges the gear the fight uses (Q-303b). Spec `WoR.Loadout.ClassLimits`. | refactor/B-303-class-table-cost |
| 2026-11-03 | B-310 (D-111 bug, Q-310): carried run wear (`FCarriedGearState`, `FSavedCarriedAttachment::GearID`) and crystal break / destroy / repair (`FindAttachedItemForSlot`, `FEquippedCrystalSlot::GearID`) follow the item's GUID, so two identical adapters keep their own wear; a GUID that names nothing finds nothing. Asset lookups prefer the copy in use (Q-310). Old saves match by asset. Spec `WoR.Party.RunWearByGuid`. | fix/B-310-run-wear-by-guid |
| 2026-11-03 | B-304 (D-110 step 4, Q-304): `GetActiveSlot(Kind)`; the active-weapon / adapter getters forward to it. The mirror heals itself on read (`EnsureSlotsInStep`, slots know their old entry). An empty or stale active slot falls back to the first usable slot of its kind (a Resonator's only inside its adapter loadout); a piece another member wears is skipped (B-221b). A Generic's secondary weapon gets the spell checks. Wear lands on the piece the getters call in use. Supersedes B-243. Spec `WoR.Loadout.ActiveSlot`. | refactor/B-304-active-slot |
| 2026-11-03 | B-320 (D-116, Q-320): always one valid primary. `UInventoryComponent::LoadoutHasPrimary` (Weapon or None type, as the fight reads), `IsPrimaryInAnyLoadout`, `RefillActivePrimary` (first usable owned weapon, else an adapter for a non-Resonator; skips worn-by-other and in-loadout gear; fresh slot picks). Refilled after a strip (`RemoveFromSavedLoadouts`), on the pool bind and after `DropInvalidParts`. `UEconomyService` refuses to dismantle any loadout's primary (weapon, adapter, evolution; `RemovePrimaryEvolution` always refuses now). `ValidateLoadout` logs an empty primary. | fix/B-320-always-a-primary |
| 2026-11-03 | B-305 (D-110): one switch. `TrySwitchActiveSlot(EGearKind)` is the turn's free switch for a weapon (the other weapon) or an adapter (the next one, wrapping over the valid adapters); `SwitchToNextUsableSlot(EGearKind)` the automatic one (adapters only). `TrySwitchWeaponThisTurn`, `TrySwitchRingThisTurn` and `SwitchToNextUsableRing` forward; the command menu calls `TrySwitchActiveSlot` for both buttons. No behaviour change. | refactor/B-305-one-switch-v2 |
| 2026-11-03 | B-306 (D-110 step 6, Q-244 A): `ForEachContributingGear` visits the gear that counts, for every class: the adapter in use, then the weapon in hand (live entries via `GetActiveSlot`; an entry with no asset is skipped). `GetActiveStatBonus`, `GetActiveResistanceBonus`, `GetActiveEffects`, `GetActiveEffectsGathered`, `GetActiveConditionalEffectsGathered` and the gear-effects debug print read it; the evolution seat is still each reader's. Changes: a primary weapon whose slot type is None (fresh / auto-filled loadouts) now counts, every class; an evolution-seat Generic's secondary weapon gives its stats, resistances and effects; a shared piece another member wears no longer counts (B-221b). Debug print labels are `ActiveAdapter` / `ActiveWeapon`. Supersedes B-244. Spec `WoR.Loadout.GearVisitor`. | refactor/B-306-gear-visitor-v2 |
| 2026-11-03 | B-311 (D-111, Q-311): lore vs custom loadouts. `FCombatLoadout::bLore` marks loadouts built from the character asset (and the Default seeds); they are rebuilt each spawn, never kept, can't be deleted. Every other loadout is custom: `DuplicateLoadout` always makes one (the way to keep changes to a lore loadout), at most `PoolConstants::MAX_CUSTOM_LOADOUTS` (5) per character (replaces `MaxSavedLoadouts` for create / copy; that property is flagged obsolete). `UInventoryComponent::PushCustomLoadoutsToPool` (EndPlay, and before every save) stores them in `UPoolSubsystem` via `FCombatLoadout::ToSavedLoadout` (pieces by owned GUID); `DrawSharedGear` appends them after the lore ones (not in a run). Spec `WoR.Loadout.CustomLoadouts`. | feature/B-311-custom-loadouts-saved |
| 2026-11-03 | B-313 (D-111, D-117, Q-313), combat side: a party member's fight items are its own pouch. `ULoadoutComponent::UsesPouchItems`, `GetUsableItems` (the pouch's gems, sorted, each capped by its D-072 band's room this fight via `FLoadoutRules::GetUtilityBandIndex` / `GetUtilityBandCap`; stones not listed), `CanUseCarriedItem` / `UseCarriedItem` (one item from the pouch only; `OnItemUsed` with SlotIndex `INDEX_NONE`), per-fight `BattleBandUses`. `ExecuteItem`, the Items submenu (index into `GetUsableItems`, `FText` label) and `FCombatCapabilities` read them. `PrepareForBattle` skips auto-equip for a pouch member (Q-155); `CreateAndConfigureLoadout` debits nothing for one. A run-only recruit's authored preset items are banked into its pouch once. `ItemSlots`, `UseItem`, `ApplyAutoEquip`, `BattleItemsTaken` and the crystal-inventory mirror flagged OBSOLETE (B-313); enemies keep the slot path; B-313b moves the rest. Spec `WoR.Pool.PouchItemUse`. Written unbuilt in a cloud session. | refactor/B-313-pouch-per-member |
| 2026-11-03 | B-314 (D-111, D-097, Q-314), store side: the run layer. `UPoolSubsystem` keeps an `FPoolRunMark` per gear ID while a run is open (run-only / on the run, plus the home attachment, its spells and an evolution's durability), beside the entry so a member's pushed copy can't wipe it. `BeginRunLayer` / `TakeOnRun` / `TakeHomeGearOnRun` / `MarkRunOnly` / `EndRunLayer` (`Private/Pool/PoolRunLayer.cpp`); at run end home pieces go back to their home attachment and wear, run-only gear is removed and its tier kept in `UpgradeMemory` per definition (highest kept); `ApplyUpgradeMemory` raises the next copy found. `UTrialRunSubsystem::EnterTrial` opens it and takes every home piece (until B-116), `ExitTrial` closes it before the exit save, `RestoreRun` opens a fresh one (the layer is saved in B-314b). `UInventoryComponent::PushGearToPool` marks a piece new during a run run-only and raises it first. `wor.Run.PrintKept`. Spec `WoR.Pool.RunLayer`. Written unbuilt in a cloud session. | refactor/B-314-run-layer |
| 2026-11-03 | B-314 review fixes: `EndRunLayer` also compares `GetCurrentDurability` (`FRuntimeAttachedItem::operator==` is identity only), so worn gems really go back; `DiscardRunLayer`, and `BeginRunLayer` discards a still-open layer (`EnterTrial` ends it first, `RestoreRun` discards); `RemoveGear` calls `NoteRunGearRemoved` (a run-only piece removed mid-run is remembered, Q-314c); `UEconomyService` refuses to dismantle or convert a home piece on the run (Q-314b); the memory is applied only when `MarkRunOnly` succeeds. | refactor/B-314-run-layer |
| 2026-11-03 | B-347 (D-137, Q-347): the run loadout is free. `UPoolSubsystem::TakeHomeGearOnRun(WearerKeys)` takes the pieces the given party members wear (their active loadouts, within the D-110 budget) and an evolution attached to one, not every home piece; `UTrialRunSubsystem` opens the layer with the local party's keys (`OpenRunLayerWithWornLoadouts`). `PushGearToPool` takes a home piece onto the run the first time it changes mid-run, so it resets too. `UTrialRunSubsystem::StartRunGearAndGold` is the testable half of `EnterTrial` (layer + run Gold, no travel). `EconomyRefusesOnRun` widened: while a run is open every home piece is held (Q-314b note). Reads the wearers record until D-122. Written unbuilt in a cloud session. | feature/B-347-free-run-loadout |
| 2026-11-04 | B-313 built and tested on the desktop; merged into dev. | refactor/B-313-pouch-per-member |
| 2026-11-04 | B-314 built and tested on the desktop; merged into dev. | refactor/B-314-run-layer |
| 2026-11-04 | B-347 built and tested on the desktop; merged into dev. | feature/B-347-free-run-loadout |
| 2026-11-04 | B-354 (D-143, Q-354): one S-tier crystal per battle loadout (`MAX_EQUIPPED_S_TIER_CRYSTALS`, `FLoadoutRules::CountEquippedSTierCrystals`, finding `EquippedSTierCrystal`, cleared through `TrimSTierItemCrystals`); D-072 bands (`MAX_UTILITY_CRYSTALS_*`, `TrimUtilityCrystalsToTierCaps`, `GetUtilityBand*`) and B-313's `BattleBandUses` flagged OBSOLETE; pouch uses uncapped. Written unbuilt in a cloud session. | feature/B-354-tier-caps-retire |
| 2026-11-04 | B-354 built and tested on the desktop; reviewer fixes: `CountEquippedSTierCrystals` / `TrimSTierItemCrystals` take the class and skip the slots it doesn't fight with (as `CollectFindings` does); `GetCarriedBandRoom` flagged OBSOLETE. Merged into dev. | feature/B-354-tier-caps-retire |
| 2026-11-04 | B-356 (D-146, Q-356): the per-school innate cap (`FLoadoutRules`, `ULoadoutComponent`, `FSavedLoadout`), `FSpellCollection::GetSpellsBySchool`, `FCombatLoadout::GetInnateSpellCountForSchool` and the pool school filter read `USpellData::GetSchool`, so a retired (Conjuration-tagged) spell counts and lists as Enhancement. | feature/B-356-three-schools |
| 2026-11-04 | B-356c (D-146 corrected by Crown, Q-356): the three schools are Conjuration (damage), Enhancement, Restoration. `SpellSchools::ToCurrent` maps Destruction onto Conjuration and Conjuration onto itself (Destruction OBSOLETE, append-only); `USpellData::School` and the pool school filter default to Conjuration; the per-school cap, filters and `GetSchool` readers are unchanged. Data validation: no loadout overflows a school any more (Q-356b's seven baseline lines deleted). Built and tested on the desktop. | feature/B-356c-conjuration-damage-school |
| 2026-11-04 | B-356b: `FSpellCollection::GetSpellsBySchool` / `GetSpellsByElementAndSchool` and the pool school filter normalise their argument with `SpellSchools::ToCurrent` (the retired Destruction finds the Conjuration spells); `InventoryDebug` lists the shown school (retired flagged) and counts innate spells per shown school. | feature/B-356b-three-schools-part2 |
| 2026-11-04 | B-351 (D-141, D-145, Q-351): gear entries get a `StoneSocket` beside the element socket (`GearSockets.h`); stones attach there on weapons and adapters; stone getters read both; weapons get one skill-slot list (`SkillSlotList.h`, `SkillPicks`, replace / restore, Ability Stone slots), live per weapon once edited; old lists OBSOLETE; save v13 (12 was B-348's on the dev merge) wipes older saves. UNBUILT (cloud). | feature/B-351-two-sockets |
| 2026-11-04 | B-351 built and tested on the desktop (save v13; 12 went to B-348). Review fixes: `CountAttachedSTier` counts the element and the stone socket; `InventoryGearSync::CopyOwned` keeps an active list's `SkillPicks` through a socket refresh; `FWeaponLoadoutEntry::GetSkillSlotString`; `GetSkillSlotCount` reads `GetStoneAttachment()`; `wor.Loadout.PrintSkillSlots` / `ReplaceSkill` / `RestoreSkill` (LoadoutRulesDebug) until the gear screen (B-351b). Q-351b. Merged into dev. | feature/B-351-two-sockets |
| 2026-11-07 | B-351b part 1 (Q-351c): `RemoveCrystalFromGear` clears the stone socket too (speed recalc reads both); `AttachCrystalToGear` refuses a stone over an authored element-socket stone; `FCarriedAttachment` / `FSavedCarriedAttachment::StoneSocket` (save v22) through `ForEachAttachmentSlot`, the snapshot / apply and the run save. Part 2 (B-351c) keeps the rest of the row. Unbuilt (cloud pre-write). | feature/B-351b-stone-socket-carry-cloud |
| 2026-11-04 | B-313b (Q-313, Q-313b): a pouch member's crystal reads (`UCrystalInventoryComponent::GetCount` / `GetCountForTier` / `GetTotalCount` / `GetStackCount`, new `GetHeldStacks`) come from its pouch, never the mirror (still filled for Blueprint, OBSOLETE, goes with B-317); `FLoadoutRuleContext::bItemsInPouch` (set by `BuildRuleContext` from `UsesPouchItems`): `CheckItems` skips a pouch member's inert item slots, its attached S crystals still count (`CountEquippedSTierCrystals` `bCountItemSlots`). Enemies' own item list and the `ItemSlots` deletion moved to B-317. no must-fix; should-fixes were the hand-off docs, and the authored-preset validation keeping the item rules is noted in Q-313b. Merged into dev. | feature/B-313b-pouch-rest |
| 2026-11-04 | B-314b (Q-314): the run layer in the save (v15). `FSavedRunMark` (gear ID, run-only / on-run, home attachment with the evolution as a ref, home spells as refs, home durability); `FSavedRun::bHasRunLayer` / `RunMarks` captured by `USaveSubsystem::CaptureRun` from the pool's open layer; `USaveSubsystem::RestoreRun` hands them to `UPoolSubsystem::RestoreRunLayer`, which replaces the fresh layer `UTrialRunSubsystem::RestoreRun` opened and drops a mark whose gear is gone; `FSavedPool::UpgradeMemory` (`CapturePool` / `RestorePool`). Found items are never marked run-only (kept, D-097); B-238 left for Crown. no must-fix; should-fixes applied (a retired fusion home state reads empty, warnings when restore drops marks or a saved run has no layer); B-238 left for Crown. Merged into dev. | feature/B-314b-run-layer-save |
| 2026-11-04 | B-316b (Q-316, Q-316b): learned skills per character. `UPoolSubsystem` `FPoolSkills` by character key (`BankSkills` / `SetSkills` / `KnowsSpell` / `KnowsAbility` / `MoveSpell` / `MoveAbility`, `OnPoolSkillsChanged`, `PoolSkills.cpp`, `wor.Pool.PrintSkills`); `UInventoryComponent::DrawSharedGear` banks the asset's skills once and loads the store's after, a learn / forget pushes, a give reloads the spawned member; `WoRGive::TryGiveSkill` / `TryRequestSkill`; the give placeholder lists skills; `UEconomyService::PurchaseFor` buys skills for the receiver (`PurchaseImpl`); save v16 (`FSavedCharacterSkills`). A run-only recruit keeps its asset's skills. No must-fix left: PurchaseImpl no longer falls back to the payer, a restore reloads spawned members, a skill give waits for the fight to end. Merged into dev. | feature/B-316b-skills-per-character |
| 2026-11-04 | B-347b (Q-347): a run's armoury is what it brought. `UPoolSubsystem::IsInRunView` (off a run every piece; during one only run-marked pieces) filters `DrawSharedGear`, `IsMineInStore` (so `OwnsWeaponAsset` / `OwnsRingAsset` and loadout validation), `HandlePoolGearChanged` and `CompareWithStore`; a run-only recruit's whole-armoury draw too. `PushGearToPool` neither takes on the run nor removes a left-home piece it doesn't hold. `FCombatLoadout::HeldHomeLoadout` (not saved): a custom loadout drawn during a run naming a left-home piece is written back as stored, also after `EndRunLayer` and through a mid-run delete. `WoRGive` chest put / take refused during a run. Specs `WoR.Pool.RunLayer`, `WoR.Pool.Chest`. | feature/B-347b-run-armoury |
