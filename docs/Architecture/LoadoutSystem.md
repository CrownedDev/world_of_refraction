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
| `GetCharacterClass()` | `ECharacterClass` | Determines loadout rules: the owner's `CharacterData` class, `Generic` without one (a getter, not a field, since B-153). |
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
- **Stone-ability slots: removed (D-175, B-437).** No stone adds ability slots. The separate
  stone list is gone from the weapon entry, the saved loadout, the save file and the combat
  menu; `GetSkillSlotCount()` is the tier's slots plus the attached crystal's, 12 at most.

  See `AugmentStoneSystem.md` for the slot table and the
  `GetAttachmentSlotsForTier(FCrystalId)` helper; its sibling
  `SlotsForContainerTier(EItemTier)` applies the same curve to a container's own
  tier (now driving weapon/ring/evolution native slots).
- `InitializeFromWeapon()` — empties `AssignedAbilities`; presets are merged in at query time.

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
`bUseWeaponParryAnimation` (OBSOLETE since B-392: the guard is a per-character setting). It also exposes `InitializeForClass`, `Clear`,
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
   The class is read from `CharacterData` through `GetCharacterClass()`. If the
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
- **Guard (B-392, D-170)** — `GetGuardStyle` (`EGuardStyle` Weapon / Unarmed: no weapon in hand is
  Unarmed; Unarmed holds only for the weapon put away, by its GUID), `SetGuardStyle` (a stamped party
  pawn writes through to `FPartyMember::GuardStyle` / `GuardWeaponId`, saved v32) and `GetGuardMontage`,
  the one resolver `UDefenseSystem::PlayDefenseAnimation` plays on every Guard: the weapon's
  `ParryMontage`, else the character's `BlockMontage`. Looks only. Debug: `wor.Guard.Print` /
  `wor.Guard.Set` (`Debug/Combat/GuardDebug`). Co-op (B-392b): on a guest, `SetGuardStyle` and
  `SetParryElementPriority` also send `AWoRPlayerState::ServerSetGuard` / `ServerSetParryPriority`; the host
  applies them to its copy of the fighter when that player commands it (`CommandsFighter`).
  Known (review): the guest writes its own copy first; if the host refuses (e.g. Unarmed with no weapon in
  hand there) it only logs, so the two can differ until the guest sets it again. TODO: a Client read-back when
  that shows up in play.
- **Parry colour priority (B-393, D-170)** — `GetParryElementPriority` / `SetParryElementPriority`
  (`EParryElementPriority` Weapon / Magic, default Weapon; a stamped party pawn writes through to
  `FPartyMember::ParryElementPriority`, saved v33): which element colours a landed parry's effect when
  the weapon guard has both a weapon crystal and the character's own magic. Read by `ParryVisuals`
  (`Combat/Presentation/`). Debug: `wor.Guard.Print` (the parry pick), `wor.Guard.SetParryPriority`.
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
| 2026-10-27 | B-085 (Q-085): the owned weapon / ring / evolution is the truth; loadout slots are caches keyed by `PersistentID` / `PrimaryEvolutionInstance`. `UInventoryComponent::LinkLoadoutsToOwned` (both init paths) links unresolved authored slots to an owned entry of the same asset; `RefreshLoadoutsFromOwned` / `RefreshEvolutionInLoadouts` after every socket op, level-up, downgrade and evolution removal; `WriteLoadoutToOwned` at every fight end (`CombatOrchestrator`), on `SetActiveLoadoutIndex` and after `ApplyCarriedGear` (the active loadout leads mid-fight); `DetachEvolutionFromOwnedGear` on evolution removal; debug `GetOwnedGearSyncString` in `wor.Inventory.Print`. Spec `WoR.Inventory.OwnedGearSync`. | refactor/B-085-owned-gear-truth |
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
| 2026-11-07 | B-374 (D-163, Q-374): six spell schools. `ESpellSchool` appends Manipulation and Impairment; Destruction is the damage school again (`SpellSchools::ToCurrent` is identity, nothing retired). The per-school innate cap (6) counts each school apart, so a Destruction spell no longer fills Conjuration. `EPieMenuSpellSchool` mirrors the six; `UCombatCommandMenuSubsystem::BuildSchoolButtons` is public static and lists held schools in enum order. `USpellDataDebug::ProposeSchool` / `IsSchoolMismatch` propose a school from the main effect. Unbuilt (cloud pre-write). | feature/B-374-six-schools-cloud |
| 2026-11-04 | B-351 (D-141, D-145, Q-351): gear entries get a `StoneSocket` beside the element socket (`GearSockets.h`); stones attach there on weapons and adapters; stone getters read both; weapons get one skill-slot list (`SkillSlotList.h`, `SkillPicks`, replace / restore, Ability Stone slots), live per weapon once edited; old lists OBSOLETE; save v13 (12 was B-348's on the dev merge) wipes older saves. UNBUILT (cloud). | feature/B-351-two-sockets |
| 2026-11-04 | B-351 built and tested on the desktop (save v13; 12 went to B-348). Review fixes: `CountAttachedSTier` counts the element and the stone socket; `InventoryGearSync::CopyOwned` keeps an active list's `SkillPicks` through a socket refresh; `FWeaponLoadoutEntry::GetSkillSlotString`; `GetSkillSlotCount` reads `GetStoneAttachment()`; `wor.Loadout.PrintSkillSlots` / `ReplaceSkill` / `RestoreSkill` (LoadoutRulesDebug) until the gear screen (B-351b). Q-351b. Merged into dev. | feature/B-351-two-sockets |
| 2026-11-07 | B-351b part 1 (Q-351c): `RemoveCrystalFromGear` clears the stone socket too (speed recalc reads both); `AttachCrystalToGear` refuses a stone over an authored element-socket stone; `FCarriedAttachment` / `FSavedCarriedAttachment::StoneSocket` (save v22) through `ForEachAttachmentSlot`, the snapshot / apply and the run save. Part 2 (B-351c) keeps the rest of the row. Unbuilt (cloud pre-write). | feature/B-351b-stone-socket-carry-cloud |
| 2026-11-08 | B-351c part 1 (Q-351d): `UInventoryDebug::LogActiveLoadout` counts abilities from the one list (`GetSkillSlotAbilities`, cap `GetSkillSlotCount`) and prints both sockets and `GetSkillSlotString`; new `wor.Loadout.Print`; SkillSlotListSpec: a stone slot's pick reaches `FCombatLoadout::GetAllAbilities`. `GetEquippedCrystals` stays on the element socket (break / repair / element source only). The crystal spell list is B-351d. | `feature/B-351c-one-list-readers-cloud` (in dev) |
| 2026-11-07 | B-357 (D-147, Q-357): `LoadoutConstants::EVOLUTION_SLOT_COST` 2, the one evolution cost; `WEAPON_EVOLUTION_SLOT_COST` / `RING_EVOLUTION_SLOT_COST` are the gear base plus it (evolved adapter 2 → 3); `EVOLUTION_PRIMARY_SLOT_COST` an OBSOLETE alias. The resolve path and the gear rule are B-357b. Unbuilt (cloud pre-write). | feature/B-357-evolution-one-cost-cloud |
| 2026-11-04 | B-313b (Q-313, Q-313b): a pouch member's crystal reads (`UCrystalInventoryComponent::GetCount` / `GetCountForTier` / `GetTotalCount` / `GetStackCount`, new `GetHeldStacks`) come from its pouch, never the mirror (still filled for Blueprint, OBSOLETE, goes with B-317); `FLoadoutRuleContext::bItemsInPouch` (set by `BuildRuleContext` from `UsesPouchItems`): `CheckItems` skips a pouch member's inert item slots, its attached S crystals still count (`CountEquippedSTierCrystals` `bCountItemSlots`). Enemies' own item list and the `ItemSlots` deletion moved to B-317. no must-fix; should-fixes were the hand-off docs, and the authored-preset validation keeping the item rules is noted in Q-313b. Merged into dev. | feature/B-313b-pouch-rest |
| 2026-11-04 | B-314b (Q-314): the run layer in the save (v15). `FSavedRunMark` (gear ID, run-only / on-run, home attachment with the evolution as a ref, home spells as refs, home durability); `FSavedRun::bHasRunLayer` / `RunMarks` captured by `USaveSubsystem::CaptureRun` from the pool's open layer; `USaveSubsystem::RestoreRun` hands them to `UPoolSubsystem::RestoreRunLayer`, which replaces the fresh layer `UTrialRunSubsystem::RestoreRun` opened and drops a mark whose gear is gone; `FSavedPool::UpgradeMemory` (`CapturePool` / `RestorePool`). Found items are never marked run-only (kept, D-097); B-238 left for Crown. no must-fix; should-fixes applied (a retired fusion home state reads empty, warnings when restore drops marks or a saved run has no layer); B-238 left for Crown. Merged into dev. | feature/B-314b-run-layer-save |
| 2026-11-04 | B-316b (Q-316, Q-316b): learned skills per character. `UPoolSubsystem` `FPoolSkills` by character key (`BankSkills` / `SetSkills` / `KnowsSpell` / `KnowsAbility` / `MoveSpell` / `MoveAbility`, `OnPoolSkillsChanged`, `PoolSkills.cpp`, `wor.Pool.PrintSkills`); `UInventoryComponent::DrawSharedGear` banks the asset's skills once and loads the store's after, a learn / forget pushes, a give reloads the spawned member; `WoRGive::TryGiveSkill` / `TryRequestSkill`; the give placeholder lists skills; `UEconomyService::PurchaseFor` buys skills for the receiver (`PurchaseImpl`); save v16 (`FSavedCharacterSkills`). A run-only recruit keeps its asset's skills. No must-fix left: PurchaseImpl no longer falls back to the payer, a restore reloads spawned members, a skill give waits for the fight to end. Merged into dev. | feature/B-316b-skills-per-character |
| 2026-11-04 | B-347b (Q-347): a run's armoury is what it brought. `UPoolSubsystem::IsInRunView` (off a run every piece; during one only run-marked pieces) filters `DrawSharedGear`, `IsMineInStore` (so `OwnsWeaponAsset` / `OwnsRingAsset` and loadout validation), `HandlePoolGearChanged` and `CompareWithStore`; a run-only recruit's whole-armoury draw too. `PushGearToPool` neither takes on the run nor removes a left-home piece it doesn't hold. `FCombatLoadout::HeldHomeLoadout` (not saved): a custom loadout drawn during a run naming a left-home piece is written back as stored, also after `EndRunLayer` and through a mid-run delete. `WoRGive` chest put / take refused during a run. Specs `WoR.Pool.RunLayer`, `WoR.Pool.Chest`. | feature/B-347b-run-armoury |
| 2026-11-08 | B-351d part 1 (D-145): a crystal's one spell list. `SpellPicks` / `bSpellSlotsActive` on `FWeaponInventoryEntry` / `FRingInventoryEntry` (cleared by `RemoveCrystal`), `GetSpellSlots` / `ReplaceSpell` / `RestoreSpell` / `GetSpellSlotString` on both loadout entries (`SkillSlotList` templates), `GetAllSpellsUncapped` returns the list once active; carried by `FGearInstance`, saved in `FSavedGearDetail` (no version bump), kept by `CopyOwned` and written back by `WriteLoadoutToOwned`; `wor.Loadout.PrintSpellSlots` / `ReplaceSpell` / `RestoreSpell`. Deletion and the gear screen: B-351e. | feature/B-351d-crystal-spell-lists-cloud (in dev) |
| 2026-11-08 | B-357b part 1 (D-147, Q-357b): `ULoadoutComponent::GatherAttachedEvolutions` is the one resolve path (the character's slot, then the gear that counts with an evolution in its socket, each with its `EEvolutionPlace`); `WoREvolutionPlacement::GivesPassives` (gear only at charge L2); `GetActiveEffects` / `GetActiveEffectsGathered` / `GetActiveConditionalEffectsGathered` read evolutions through it at charge 0 (same effects as before); `wor.Loadout.PrintEvolutions`. The rest is B-357c. | feature/B-357b-evolution-resolve-path-cloud (in dev) |
| 2026-11-08 | B-357c part 1 (D-147): `WoREvolutionPlacement::GivesStats` (character slot only); `BuildPillarLayers` and `GetActiveResistanceBonus` sum evolutions through `GatherAttachedEvolutions`, so a weapon's or adapter's evolution adds no pillar percent or resistance. The L2 action: B-357d. | feature/B-357c-gear-evolutions-passives-only-cloud (in dev) |
| 2026-11-08 | B-357 (Crown changed Q-357): costs as frame + attachment (`WEAPON_FRAME_COST` 1, `ADAPTER_FRAME_COST` 0, `CRYSTAL_ATTACHMENT_COST` 1, `EVOLUTION_SLOT_COST` 2); the evolved adapter is 2 again (B-357's 3 undone); Generic budget 6 → 5. | feature/B-357-evolution-one-cost-cloud (in dev) |
| 2026-11-08 | B-357 (Crown's final costs): `EVOLUTION_ALONE_SLOT_COST` 3 (`EVOLUTION_PRIMARY_SLOT_COST` 2 → 3); an evolution on gear stays +2; no one-evolution rule. | feature/B-357-evolution-one-cost-cloud (in dev) |
| 2026-11-08 | B-380 (D-164, Q-380): apparel: `UApparelData` (HP and Defence lines), `FApparelEntry` (its own crystal socket, a refined crystal only), `FCombatLoadout::Apparel` (up to 2, outside the budget and the Slots mirror), its lines in `GetActiveStatBonus`; `wor.Loadout.PrintAll` (renamed at merge: `wor.Loadout.Print` is B-351c's), `wor.Loadout.WearApparel`. Owned store and save: B-380b. | `feature/B-380-apparel-cloud` (in dev) |
| 2026-11-08 | B-380 (Crown, D-164): an apparel crystal adds its element's resistance (`FApparelEntry::GetResistance`); apparel effects count in the three effect gatherers. | `feature/B-380-apparel-cloud` (in dev) |
| 2026-11-08 | B-380 (Crown, D-164): an evolution can sit on apparel (`FApparelEntry::GetEvolution`): its passives in the effect gatherers, its element and resistance like a crystal. | `feature/B-380-apparel-cloud` (in dev) |
| 2026-11-08 | B-380b: owned apparel in the pool (`UPoolSubsystem::OwnedApparel`, `AddApparelToPool`), saved in the one gear list with its socket and roll; `wor.Loadout.WearApparel` banks it. Loadout slots saved: B-380c. | `feature/B-380b-apparel-store-save-cloud` (in dev) |
| 2026-11-08 | B-380c: loadout apparel saved by owned id (`FSavedLoadout`, `FSavedCustomLoadout`, save v29) and resolved from the store; `UPoolSubsystem::AttachToApparel` → `HandlePoolGearChanged` → `ApparelRules::RefreshFromStore` → `ULoadoutComponent::NotifyApparelChanged`; `wor.Loadout.SocketApparel`. | `feature/B-380c-apparel-slots-saved-cloud` (in dev) |
| 2026-11-08 | B-380d: `UPoolSubsystem::AttachToApparel` returns the swapped-out crystal to storage and checks an evolution (owned, the piece owner's, not in another socket: `IsEvolutionSocketedElsewhere`). Lore-loadout apparel is B-380e. | feature/B-380d-apparel-lore-socket-cloud (unbuilt) |
| 2026-11-08 | B-386 (Q-386): apparel's Amp lines count: `UApparelData::GetStatLines` / `FApparelEntry::GetStatLines` add them; `GetActiveStatBonus` sums them raw with the rest (B-382). | feature/B-386-apparel-amp-cloud (unbuilt) |
| 2026-11-09 | B-357d (D-147, Q-357b A, Q-357d): an action charged to L2 wears the lasting passives of the gear evolutions in the items that power it, for that action only: `WoREvolutionPlacement::PowersAction` / `FilterForAction`, `EEvolutionPlace::Apparel` (apparel joins `GatherAttachedEvolutions`), `ULoadoutComponent::GetGearEvolutionPassivesForAction`, `UActionExecutor::PutOnGearEvolutionPassives` / `TakeOffGearEvolutionPassives` (through `SwapEquipmentEffects`). The remaining old evolution readers: B-357e. | feature/B-357d-gear-evolution-l2-passives |
| 2026-11-09 | B-357e (D-147): `ULoadoutComponent::GetCharacterSlotEvolution` (the Place Character entry of `GatherAttachedEvolutions`); every remaining reader of the character slot's evolution reads it (infusion source, wear, Reality check, seat stats and Amp, broken Breakthrough, capabilities, quest conditions, combat tier, debug prints); the slot's writes stay on `FCombatLoadout::PrimaryEvolution`. `GetActivePrimaryEvolutionCrystal` / `Attachment` OBSOLETE (no C++ reader). `FSkillEffect::IsLastingPassive` shared by `SwapEquipmentEffects` and `GetGearEvolutionPassivesForAction`. No play change. | refactor/B-357e-evolution-readers (in dev) |
| 2026-11-09 | B-392 (D-170, Q-392): two guards. `EGuardStyle`; `ULoadoutComponent::GetGuardStyle` / `SetGuardStyle` / `GetGuardMontage` / `ResolveGuardStyle`; the setting on `FPartyMember` (saved, `FSavedPartyMember`, save v32); every Guard plays `GetGuardMontage`; `FWeaponLoadoutEntry::bUseWeaponParryAnimation` OBSOLETE; `wor.Guard.Print` / `wor.Guard.Set`; spec `WoR.Defense.TwoGuards`. | feature/B-392-two-guards (in dev, built 2026-11-12) |
| 2026-11-09 | B-393 (D-170 corrected, Q-393): the parry colour priority. `EParryElementPriority`; `ULoadoutComponent::GetParryElementPriority` / `SetParryElementPriority`; on `FPartyMember` (saved, `FSavedPartyMember`, save v33); `UPartySessionSubsystem::SetMemberParryPriority`; `wor.Guard.Print` adds the parry pick, `wor.Guard.SetParryPriority`. | feature/B-393-parry-vfx (in dev, built 2026-11-12) |
| 2026-11-12 | B-392b (Crown, Q5 = B): a co-op guest's guard and parry colour priority reach the host: `AWoRPlayerState::ServerSetGuard` / `ServerSetParryPriority` (Reliable; enum checked; refused unless `CommandsFighter`: the member id stamped on the pawn, else its character) call the setters on the host's pawn, which write its party record; the guest's setters send them when `NM_Client`. D-103 TODOs cleared. Spec `WoR.Defense.TwoGuards` (co-op). | `fix/B-392b-guest-guard-sync` |
| 2026-11-12 | B-098 (D-027, Q-098): weapons give no spells. `Loadout/WeaponSpellRules` (`wor.Loadout.WeaponSpells` 0; `FScopedWeaponSpells` for the old path's specs; `wor.Loadout.ListWeaponSpells` survey) gates `FWeaponLoadoutEntry::GetAllSpells` / `GetSpellSlots` / `ReplaceSpell`, `FCombatCapabilities::BuildFrom` (Resonate Weapon spells) and `GetWeaponResonateSpells`. Merge (Q-098, Crown): a weapon's spells are no longer saved or loaded (`SaveSubsystem` gear detail; adapters unchanged; save v37), and `FWeaponLoadoutEntry::GetSkillSlotCount` adds the attached gem / evolution's spell-slot count as ability slots (broken or not; the old path off). Element limit on those slots: B-410; deleting the switch and old code: B-411 (Sweep row 48). Specs `WoR.Loadout.WeaponsNoSpells`, `WoR.Save.GearDetail`. | `feature/B-098-weapons-no-spells-cloud` (in dev) |
| 2026-11-13 | D-175 (Crown): the Ability Stone is retired; a weapon's ability slots are its tier plus the attached crystal's extra slots (B-098 / B-410), at most 12. The stone-ability list goes in B-437. Docs only, branch docs/retire-ability-stone. |
| 2026-11-14 | B-437 (D-175): the stone-ability list is deleted: `FWeaponLoadoutEntry::AssignedAugmentStoneAbilities`, `GetAugmentStoneAbilities`, `ValidateAugmentStoneAbilities`, `FLoadoutSlot`'s copy, `FSavedLoadout::PrimaryAugmentStoneAbilities` / `SecondaryAugmentStoneAbilities`, the saved `PrimaryStoneAbilities` / `SecondaryStoneAbilities` (save v38, older saves wiped, Q-437), `MAX_AUGMENTSTONE_ABILITIES`, the stone-ability overflow finding, and `FCombatCapabilities`' stone abilities with the Resonate Weapon ability branch. `SkillSlotList::SlotCount` goes: `GetSkillSlotCount` is the tier's slots plus the crystal's (12 at most). Spec: `WoR.Loadout.SkillSlotList` (no stone kind adds a slot; S weapon + S gem = 12). | feature/B-437-remove-ability-stone |
| 2026-11-14 | B-439b (Q-439 point 2, D-176): a quit run takes back the items found in it. The run layer keeps `RunStartItems` (each item kind's count over every pouch and storage, taken in `BeginRunLayer`; `GetRunStartItems`, `RestoreRunStartItems`); `UPoolSubsystem::TakeRunFoundItems` removes what is held above it (pouches first, then storage) and `UTrialRunSubsystem::ExitTrial` calls it before `EndRunLayer` when the run was abandoned. Starting items banked mid-run are added to the start counts (`BankStartingItems`, D-097). Saved with the run (`FSavedRun::bHasRunStartItems`, `RunStartItems`; no version bump: a save without them takes what the resumed run holds as its start). `wor.Run.PrintKept` lists the finds. Specs `WoR.Pool.RunLayer` (2 cases), `WoR.Save.RunLayer` | fix/B-439b-quit-takes-pouch-finds |
| 2026-11-14 | B-172 (D-204: Crown waived the PIE gate of B-028): the legacy validators are deleted (`ULoadoutComponent::GetValidationErrorsLegacy`, `CollectInvalidSlotFindingsLegacy`, `FSavedLoadout::GetValidationErrorsLegacy`) with `wor.Loadout.CompareRules`; `FLoadoutRules` is the only check. `wor.Loadout.PrintFindings` prints each character's live findings; `ULoadoutRulesDebug::CompareLoadoutRules` (BlueprintCallable) is kept under its old name, printing the same, until the editor shows no Blueprint calls it. | chore/B-172-delete-legacy-validators |
| 2026-11-14 | B-309 (D-204): `PrimarySlotCost` (`LoadoutRules.cpp`, OBSOLETE since B-303) is deleted; `FLoadoutRules::SlotCost` over the unified slots is the budget. The old loadout fields are not deletable yet (the slots are still a mirror of them, and the save and the screens write them): B-309b. `UActionExecutor::DoWeaponStatsApply` waits for the editor's Blueprint check. | chore/B-309-old-loadout-path |
| 2026-11-14 | B-153 (D-204; no Blueprint reads it, Find in Blueprints): the OBSOLETE `ULoadoutComponent::CharacterClass` UPROPERTY is removed with its sync in `InitializeFromCharacterData`; `GetCharacterClass()` is the only class read (the owner's CharacterData, `Generic` without one); `WoR.Loadout.Getters` drops its synced-field check. | `feature/B-153-loadout-class-field-delete-cloud` (UNBUILT; after Crown's PIE of B-022) |
| 2026-11-14 | B-154 (D-204; no Blueprint use, Find in Blueprints): `URingManager` and `UWeaponManager` (the OBSOLETE B-024 forwarders) are deleted, with `UActionExecutor::GetRingManager` / `GetWeaponManager` and their cached refs; ring and weapon queries are only on `ULoadoutComponent`. Full suite green on the desktop. | chore/B-154-ring-weapon-managers |
| 2026-11-14 | B-317 (Q-313b, D-204), first part: `ULoadoutComponent::BattleItemsTaken` / `GetBattleItemsTaken` and `ResetBattleState`'s hand-back are deleted (no actor that auto-equips shares a pouch since B-313, so nothing was ever recorded); `LoadoutReturnDroppedCrystals` returns nothing for a pouch member, as before. `ItemSlots`, `UseItem` and `ApplyAutoEquip` stay for enemies until their own item list lands (B-317c). | `chore/B-317-store-old-copies` |
| 2026-11-14 | B-238b (D-097, Q-238b): a run that ends any way but a quit moves the items found in it from the pouches to home storage: `UPoolSubsystem::StoreRunFoundItems` (`Pool/PoolRunLayer.cpp`, the same per-kind count as `TakeRunFoundItems`), called by `UTrialRunSubsystem::ExitTrial` before `EndRunLayer`; `wor.Run.PrintKept` says so. | `feature/B-238b-run-finds-to-storage` |
| 2026-11-14 | B-309 / B-172 leftovers (Find in Blueprints found neither node): `UActionExecutor::DoWeaponStatsApply` deleted; `ULoadoutRulesDebug::CompareLoadoutRules` renamed `PrintLoadoutFindings` (`wor.Loadout.PrintFindings` calls it). | `chore/B-309-B-172-blueprint-node-leftovers` |
| 2026-11-14 | B-472 (D-206): loadout costs: the gear is free, only what is in it costs. `LoadoutConstants` D-206 block (`LOADOUT_GEAR_COST` 0, `LOADOUT_CRYSTAL_COST` 1, `LOADOUT_STONE_COST` 1, `LOADOUT_EVOLUTION_COST` 2, `EVOLUTION_ALONE_SLOT_COST` 2, `REFRACTOR_LOADOUT_BUDGET` 2, `MAX_GENERIC_LOADOUT_ITEMS` 3, `MAX_RESONATOR_ADAPTERS` 5); the B-357 frame costs now feed only the pouch capacity. `FLoadoutRules::SlotCost` charges both sockets through the new `AttachmentCost`. Class table: Refractor 2 / 1 item, Generic 5 / 3 items, Resonator 5 / 5 adapters. New finding type `ELoadoutSlotType::PrimaryStone`: a Refractor over 2 drops its stone (`DropInvalidParts` returns it, Q-159 = A, after writing the owned item); a Resonator's sixth adapter is a clearable RingOverflow, and the RingOverflow clear truncates by `SlotCost` and the cap. The loadout screen's cost line names only what is inside ("Free" for bare gear). Specs `WoR.Loadout.ClassLimits`, `WoR.Loadout.Budget`, `WoR.UI.LoadoutScreen`. | `feature/B-472-loadout-costs-cloud` (UNBUILT) |
| 2026-11-14 | B-477 (D-209 to D-211, Q-477), first part: Generic weapon sets. `EWeaponHandling` (SingleOnly default / DualOnly / SingleOrDual) and soft `DualWieldStance` on `UWeaponData`; `FWeaponLoadoutEntry::OffHandEntry` / `HasOffHand`; `Loadout/WeaponSetRules` (`CheckPair`, `CanStandAlone`, `GetPairTierSlots` = average rounded down, `GetSetTierSlots`, `GetSetBuiltIns`, `GetSetWieldMode`, `GetSetStance`); `GetSkillSlotCount` / `GetSkillSlots` read the set. `FSavedLoadout::PrimaryOffHandWeapon` / `SecondaryOffHandWeapon` (+ instances) through `CreateFromSavedLoadout` / `ToSavedLoadout`; save v39 (`FSavedCustomLoadout::PrimaryOffHand` / `SecondaryOffHand`). `FLoadoutRules::CheckWeapon` refuses a bad pair, a pair off a Generic and a DualOnly weapon alone (new `ELoadoutSlotType::OffHandWeapon`, cleared to the main hand), and checks abilities against the set's wield mode. `ULoadoutComponent::IsWeaponSetPaired`: the off hand's stats and resistances add, the pair's stance is `GetSetStance`. Owned-gear sync visits both off hands (`ForEachGearSlot`, `StripFromLoadout`). Debug `wor.Loadout.PrintWeaponSets`; spec `WoR.Loadout.WeaponSets`. Left for B-477b: the off hand's mesh, the AI's sets. | `feature/B-477-weapon-sets-cloud` (UNBUILT) |
| 2026-11-14 | B-480 (D-210, Q-480): entries. A Generic's adapter entry beside two weapons rides `FCombatLoadout::RingLoadout` (the slot mirror visits it for a Generic; `ComputeActiveSlotIndices` makes it the adapter in use when there is no adapter primary; `CreateFromSavedLoadout` builds it; `FSavedLoadout::EquippedRings` shows for a Generic). `FLoadoutRules`: ring checks and the S-tier count cover a Generic's adapter entry; `CheckEvolutionPlacement` (new `ELoadoutSlotType::GearEvolution`, `LoadoutGearEvolutionIndex`): no evolution on a Resonator's weapon or a Generic's adapter, cleared by taking it off; a Generic's extra adapter or over-budget drops its adapter entry first (RingOverflow, which now counts a kept adapter primary). Two stones: `AttachCrystalToGear` puts a stone in the crystal socket once the stone socket is full; the loadout screen offers it there. Spec `WoR.Loadout.Entries`; `WoR.Inventory.TwoSockets` updated. | `feature/B-480-entries-sockets-cloud` (UNBUILT) |
| 2026-11-14 | B-474 (D-207): the active weapon follows the skill. `FSavedSettings::bSwitchToSkillWeapon` (default on, no bump; `USaveSubsystem::IsSwitchToSkillWeaponOn` / `SetSwitchToSkillWeapon`, `wor.Settings.SwitchToSkillWeapon`). `ULoadoutComponent::GetOtherWeaponSkills` / `SwitchToWeaponOfSkill` (through `TrySwitchWeaponThisTurn`, ET-19); `UActionExecutor::ExecuteActionAsync` calls it for an Ability action before validating; `ACombatPlayerController`'s pick check accepts the other weapon's skills (co-op). `FCombatCapabilities::OtherWeaponAbilities` / `OtherWeaponName`; `UCombatCommandMenuSubsystem::BuildAbilitySubmenu` adds them "(weapon)" while the setting is on. Switch Weapon moves to L3 (keyboard 6). The loadout screen's L3 / 6 sets a weapon row active (`WoRLoadoutScreen::SetRowActive`). Debug `wor.Loadout.PrintOtherWeaponSkills`; spec `WoR.Loadout.WeaponFollowsSkill`. | `feature/B-474-weapon-follows-skill-cloud` (UNBUILT) |
| 2026-11-14 | B-410 (Q-098): abilities carry an `Element` (default Generic, no asset change); the slots a weapon's gem adds take only None, Generic or the gem's element (`FWeaponLoadoutEntry::IsGemSkillSlot` / `CanHoldSkill`, refused in `ReplaceSkill` with a reason on the loadout screen); validation finding `ELoadoutSlotType::WeaponGemSkill` restores the slot; spec `WoR.Loadout.GemSlotElement` | `feature/B-410-gem-slot-element-cloud` |
| 2026-11-15 | B-503p / B-503q: `GetCurrentStance` reads `GetActiveWeaponLoadout()`; `FCombatLoadout::HasPrimaryWeapon` is the active rule's Weapon-or-None test and feeds `GetAllAbilities`, `GetAllSpells`, `GetAllWeaponAttacks` and the spell source / catalyst lookups; `GetPrimary/Secondary/ActiveSlotSpells` OBSOLETE. Spec `WoR.Loadout.ActiveWeaponRule` | `feature/B-503pqr-active-entry-alignment` |
| 2026-11-15 | B-309b part 1: `ULoadoutComponent::TrySwitchWeaponThisTurn` / `TrySwitchRingThisTurn` deleted (callers use `TrySwitchActiveSlot`); `HandleCrystalBroken` calls `SwitchToNextUsableSlot(Adapter)`; `SwitchToNextUsableRing` OBSOLETE (Blueprint node, B-309f); `InfusionSourceOptionHelpers::WeaponStatsApply` deleted (no caller). The rest of B-309b is split into B-309c (save on the slots), B-309d (writers), B-309e (readers), B-309f (old fields go). | `refactor/B-309b-switch-forwarders-cloud` (cloud, unbuilt) |
| 2026-11-15 | B-503n (Q-503n): `wor.Debug.TestGear [fill] [refractor\|resonator\|bd\|reality]` (`Debug/Inventory/TestGearDebug.*`): test gear through `AddWeapon` / `AddRing` / `CarryOrStore`; `fill` uses the v2 screen's own path (`BuildMainHandChoices` / `BuildAdapterChoices` + `TryChangeGear` + `ApplyDraft`, then `BuildSocketChoices` + `ApplySocketChoice`); variants set `CharacterClass` / `InnateElement` in memory, put back on the session-ending world cleanup. Spec `WoR.Debug.TestGear` | `feature/B-503n-test-gear-grant` |
| 2026-11-15 | B-503s2 (D-218): the weapon in hand lives on the record, `FPartyMember::ActiveWeaponId` (saved on `FSavedPartyMember`, no version bump): `ULoadoutComponent::RecordWeaponInHand` writes it from `NotifyActiveWeaponChanged` (stamped pawns, authority only); `UCharacterDataComponent::BeginPlay` reads it before the draw and puts it back after `DrawSharedGear`; `StampPartyMember` stamps a late pawn (hub: `AWoRHubController::OnPossess`; trial: `ATrialGameMode` stamps at spawn). Co-op guests: host-only (B-230). Two pawns stamped with one member: last write wins (B-360b). Spec `WoR.Field.WheelWeaponCarry` | `fix/B-503s2-wheel-weapon-reaches-fight` |
| 2026-11-15 | B-505 (D-221 stage 1, part 1): the worn-gear record. `FWornGear` / `FWheelPick` (`Loadout/WornGear.h`); equipment by owned GUID in fixed `WornGearLayout` places for the transition (primary, secondary, then the adapter loadout), `ActiveGearID` the weapon drawn. `FCombatLoadout::ToWornGear` / `CreateFromWornGear` (through `CreateFromSavedLoadout`, pieces from the drawn copies only). `UPoolSubsystem::GetWornGear` / `SetWornGear` / `SetWheelPick`, `OnWornGearChanged`; `FLoadoutSlot::OffHandID`. Debug `wor.Loadout.PrintWorn [member]`. Nothing reads it yet (B-506). Spec `WoR.Loadout.WornGear` | `feature/B-505-worn-gear-record` |
| 2026-11-15 | B-506 (D-221 stage 1, part 2): every map wears the record. `UInventoryComponent::DrawSharedGear` builds a stamped member's active loadout from it (`WearWornGear`; the lore loadout lends only its name and item slots) and seeds it once from the lore loadout; `WriteWornGear` from `ULoadoutComponent::NotifyActiveWeaponChanged` / `NotifyApparelChanged` (every gear change, CS-8) and the stamp. `StampPartyMember` wears the record through `TryChangeGear` before stamping (a refusal leaves the pawn unstamped); B-503s2's put-back only for a member with no record. Unstamped pawns never read or write it. B-347b's hold for gear left at home. The wheel pick is written to the record (`UPartySessionSubsystem` setters) and pulled on invite (`PullWheelPicksFromPool`). Specs `WoR.Loadout.WornGear.Maps`, `WoR.Field.WheelWeaponCarry` | `feature/B-506-maps-wear-the-record` |
| 2026-11-15 | B-507 (D-221 stage 1, part 3; save v40): the worn-gear record is saved (`FSavedWornGear` / `FSavedWornEntry` in `FSavedPool::WornGear`; `UPoolSubsystem::RestoreWornGear`); `FSavedPartyMember` loses `ActiveWeaponId` and the field-pick fields; `USaveSubsystem::RestorePool` ends with `UPartySessionSubsystem::PullWheelPicksFromPool`, so the restored party takes its picks from the records. Spec `WoR.Save`; stage 1 PIE sheet `docs/PIE/B-507.md` | `feature/B-507-save-worn-gear` |
| 2026-11-15 | B-508 (D-221 stage 1b, part 1; D-219; save v41): the trial start keeps the run party's worn-gear records (`UPoolSubsystem::SnapshotRunStartGear`, `FSavedRun::RunStartWornGear`); every ending goes through `UPoolSubsystem::CloseRun(EPoolRunEnding)` (Kept: extract or boss win; Lost: out of wipes; Quit: a quit or a closed game), from `UTrialRunSubsystem::FinishRun` and the save's closed-run fallback; a quit puts the records back after the run layer closes, wheel picks included, and the party pulls its picks. `SaveWornGearRecord` / `RestoreWornGearRecords` shared by the pool and the run. Spec `WoR.Run.EndLoadout` | `feature/B-508-run-start-snapshot` |
| 2026-11-15 | B-509 (D-221 stage 1b, part 2; D-219): `UPoolSubsystem::CloseRun` keeps the run's gear on every ending but a quit: `EndRunLayer(bKeepUpgrades, bKeepRunSockets, KeepAsHubGear)` leaves each home piece's socket and an adapter's spells as the run left them (repaired to full), except a found evolution that leaves; on an extract or a boss win every found piece named in a worn-gear record (and a found evolution in a kept or home socket) stays as hub gear (`IsHubGear`, `FSavedPool::HubGear`, never sold or dismantled: `UEconomyService`, the inventory and vendor refusals). An item a home socket gave up in the run is added to the run-start counts so it isn't taken back as a find (shortcut: an item swapped out and destroyed, CL-28b, also counts, so one find of it can stay). Spec `WoR.Run.EndGear` | `feature/B-509-kept-endings-hub-gear` |
| 2026-11-15 | B-510 (D-221 stage 1b, part 3; D-219): a lost run costs tier. `GearTierLoss::RANKS_LOST_ON_LOST_RUN = 3`; `UPoolSubsystem::ApplyLostRunTierDrop(Key)` lowers each piece in the record (equipment and off hands, the evolution, apparel; not socketed items), never below F; `CloseRun` applies it once to the run party (the run-start copy's keys) on Lost and Quit, before anything goes back. Rules ER-65f / ER-65g. Spec `WoR.Run.TierDrop`; stage 1b PIE sheet `docs/PIE/B-510.md` | `feature/B-510-lost-run-tier-drop` |
| 2026-11-15 | B-510b: run-ending debug. `wor.Trial.EndRun <win|extract|wipes|quit>` ends the open run through the real path (`MarkRunBossBeaten` + `ExitTrialNextTick`, `LeaveRunEarly`, `RecordWipe` until out of wipes, `AbandonRun`; each reaches `FinishRun` -> `CloseRun`); `wor.Trial.GrantFoundWeapon [asset path]` gives the played character a run-only weapon through `AddWeapon`. `Debug/Trial/RunEndDebug` | `chore/B-510b-run-end-debug` |
| 2026-11-15 | B-511 (D-221 stage 1c, part 1): the class default kit. `UClassDefaultLoadoutData` (`Loadout/ClassDefaultLoadoutData.h`: per `ECharacterClass`, the pieces in the `WornGearLayout` places with off hand and skill picks, the ACTIVE place, apparel), found by `UTrialRunSubsystem::FindClassDefaults` from the config path `ClassDefaultLoadouts` (`DefaultGame.ini`, `/Game/Data/Loadout/DA_Loadout_ClassDefaults`; a missing asset or class warns, and the trial keeps what is worn). `UPoolSubsystem::MintClassDefaultKit` mints each character's kit once as its own (sockets empty; a place whose piece is still the same asset is reused, a changed or removed place's piece leaves the store); `IsClassDefault`; saved in `FSavedPool::ClassDefaultKits` (no bump). Kit pieces are refused for sale and dismantling (`EconomyRefusesOnRun`, `InventoryVerbs`, `VendorServices`), giving and the chest (`GearGive`). Debug `wor.Trial.PrintClassDefaults`, `PrintDefaults` on the asset. Spec `WoR.Pool.ClassDefaultKit` | `feature/B-511-class-default-kit` |
| 2026-11-15 | B-512 (D-221 stage 1c, part 2): a trial starts on the class kit. `UTrialRunSubsystem::StartRunGearAndGold` calls `UPoolSubsystem::ResetClassDefaultKit` per member with a kit before `BeginRunLayer` (mint, then each piece back to the asset's tier with the same ID, element and stone sockets emptied into the owner's pouch or storage, the kit's skill picks; apparel sockets cleared), so freed crystals count as held at the start, not finds; after `SnapshotRunStartGear`, `WearClassDefaultKit` writes the record (the kit's places, `ActiveGearID` its ACTIVE place, the wheel pick cleared; `EvolutionID`, innate spells and BD pools kept) and each piece is taken on the run; members with no kit go through `TakeHomeGearOnRun` as before. `RestoreRun` applies no kit (a resumed run keeps its saved gear). `SetClassDefaultLoadouts` lets specs set the kits. Spec `WoR.Trial.ClassDefaults`; PIE `PIE/B-512.md` (with the `DA_Loadout_ClassDefaults` editor step) | `feature/B-512-trial-starts-on-kit` |
| 2026-11-15 | B-516 (D-224, asset overhaul stage 2): every adapter can be one asset (`DA_Ring_Adapter`), so a cast's ring is the entry, not the asset. `ULoadoutComponent::FindSpellCatalystAttachment` gives the crystal of the ring entry whose list holds the spell (the one in use first; a weapon as `FindAttachedItemByHolder`), used by `ResolveActionTier`. Ring infusion wear still goes on the ring in use (B-310). The v2 Equipment screen's add window sockets a crystal into a new entry's piece (`LoadoutV2DraftSocket::FindDraftRow` reads it off a preview loadout; `BuildSocketChoices(World, ..., Preview)`); the detail reads the owned piece's `Tier`; `WoRGameNames::NumberRepeatedNames` numbers same-named entries on the Equipment list and the quick strip. Spec `WoR.Loadout.TwoSameRings`; PIE `PIE/B-516.md` | `feature/B-516-empty-adapter` |
| 2026-11-16 | B-517 (D-224, skills track S1 part 1; save v42): the picks live on the worn entry. `FLoadoutSlot::AssignedAbilities` / `SpellOverrides` hold the entry's skill and spell picks slot by slot (they mirrored the obsolete loadout override lists); `FCombatLoadout::CreateFromWornGear` puts them on the built loadout's copies (`FCombatLoadout::ApplyEntryPicks`; spell picks need a spell source) and ignores the owned piece's; `FWornGear::SameAs` compares both; `FSavedWornEntry::Abilities` / `Spells` save them one ref per slot. A custom loadout keeps its own through the pool and the save: `FSavedLoadout::EntryPicks` (filled by `ToSavedLoadout`, applied by `CreateFromSavedLoadout` through `FCombatLoadout::ApplyEntryPicks`) and `FSavedCustomLoadout::EntryPicks`. `UInventoryComponent::WriteLoadoutToOwned` writes sockets only; a class kit piece is minted with no picks and `WearClassDefaultKit` puts `FClassDefaultPiece::SkillPicks` on the worn entry. The piece's pick fields (`FWeaponInventoryEntry` / `FRingInventoryEntry` / `FGearInstance` / `FSavedGearDetail`) are OBSOLETE on an owned piece (stage 6); on a loadout's copy they stay the runtime list. `wor.Loadout.PrintWorn` lists each entry's picks. Known limits: a piece swapped into an entry arrives with no picks (it brought its own before); edits while the record is held (gear left at home in a run) aren't kept, as for the rest of that record. PIE `PIE/B-517.md` | `feature/B-517-picks-on-worn-entry` |
| 2026-11-16 | B-516b (D-223 "Two pieces with the same name"): the tier tells same-named pieces apart first. `WoRGameNames::NumberRepeatedNames(Names, Tiers, bTierLetter)` numbers only a repeated name AND tier; `FLoadoutV2Entry::Tier` (a weapon entry's main hand, an adapter's own until stage 2b) is the Equipment row's tier badge (the letter after the name with no badge texture); the quick strip's `FFieldSlice::Tier` adds the tier letter where one name has two tiers. Spec `WoR.Loadout.TwoSameRings`; PIE `PIE/B-516.md` checks 11-13 | `feature/B-516b-tier-tells-names-apart` |
| 2026-11-16 | B-518 (D-224, skills track S1 part 2): no built-in skills on gear. `WeaponSetRules::GetSetBuiltIns` returns none (OBSOLETE); a weapon's skill and spell slots and a ring's spell slots resolve from the worn entry's picks alone (`SkillSlotList::Resolve` with no built-ins); the `bSkillSlotsActive` / `bSpellSlotsActive` old-merge fallbacks went from `FWeaponLoadoutEntry`, `FRingLoadoutEntry` and `FCombatCapabilities`, so every list is the one list; a locked (conjured) piece is an ordinary one (Q7). `GetBrokenSourceReason` reads a ring's picks only. The Equipment screen offers only known skills (`LoadoutSkillsModel`), drops the Built-in detail line and the compare's built-ins (`LoadoutV2Model`). Q-518: `FCombatLoadout::FillEmptyPicksFromKnown` (WornGear.cpp), run by `InitializeFromInventoryAsset` for enemy-origin characters, fills an empty weapon or ring from the known skills up to its slots (TODO: stage 5). `PresetAbilities`, `bAbilitiesLocked`, `PresetSpells`, `bSpellsLocked`, `DefaultSpells` marked OBSOLETE (stage 6 deletes them). The B-196 spell-cap check can no longer fire. | feature/B-518-no-builtin-skills |
| 2026-11-16 | B-520 (D-224, skills track S2): skills name what they need. `FSkillGearRequirement` (`SkillGearRequirement.h`) on `USkillDataBase::GearRequirement`, read by the virtual `GetGearRequirement` (Kind None falls back to an ability's `RequiredWeaponType` / a spell's `RequiredEvolutionCrystal`). `SkillRequirementRules::IsMet(Skill, FSkillGearHost)` checks it against the piece the skill sits in (`FromWeapon` / `FromAdapter` / `FromEvolutionSeat`); read by `FLoadoutRules` (weapon abilities, adapter spells), `FWeaponLoadoutEntry::ValidateAbilities`, `WoRLoadoutSkills::BuildSkillChoices` (via `FLoadoutSkillTab::Host`) and `FCombatCapabilities::BuildFrom` (unmet weapon picks dropped, unmet spells in `BlockedSpellReasons`). Debug: `GetRequirementString`. Q-520 | `feature/B-520-skill-gear-requirement` |
| 2026-11-16 | B-522 (D-223, asset overhaul stage 4): `UInventoryData::StartingGear` (`FClassDefaultLoadout`) and `StartingEvolution`; `FClassDefaultPiece::Crystal` (starting gear only); `FClassDefaultLoadout::StartingSkills` (field only, S3). `UPoolSubsystem::WearStartingGear` (`PoolClassDefaults.cpp`) mints the pieces once, sockets each crystal at full durability, mints the evolution into the seat and sets the worn record; the worn fill is shared with `WearClassDefaultKit` (`FillWornPieces`). `UInventoryComponent::DrawSharedGear` calls it before the draw when the character has StartingGear and no record | `feature/B-522-starting-gear` |
| 2026-11-16 | B-523 (D-224 (2), skills track S3a): a run's skill list. A trial start keeps each kit member's hub skill list aside (`UPoolSubsystem::BeginRunSkills`, `RunStartSkills`, saved in `FSavedPool::RunStartSkills`) and gives it a run list: the innate, Broken Darkness and own-evolution spells, the kit's picks, and the kit's `StartingSkills` (in the list, not a slot). Purchases, chests and teaches learn into it as before. `EndRunLayer` puts the hub lists back (`PutBackRunStartSkills`); settling the slotted ones is B-525. Spec `WoR.Run.Skills` | `feature/B-523-run-skill-list` |
| 2026-11-16 | B-524 (D-224 (2), skills track S3b): chests (`TrialLoot::GrantTo`), merchants (`UEconomyService::PurchaseFor`) and Mentor Token teaches already learn into the pool's list, which is the run list while a run is open; proven by a new `WoR.Pool.Skills` case, no code changes | `feature/B-524-run-skill-sources` |
| 2026-11-16 | B-525 (D-224, D-219; skills track S3c): `UPoolSubsystem::SettleRunSkills`, called by `CloseRun` before `EndRunLayer` puts the hub lists back. Slotted (a pick in a worn entry's slot at the end) found skills join the hub list on a finish, an extract or out of wipes; on a quit they are lost; out of wipes and on a quit the hub copy of every slotted skill drops `GearTierLoss::RANKS_LOST_ON_LOST_RUN`. Innate, BD and evolution spells are not settled (Q-525). Spec `WoR.Run.Skills` | `feature/B-525-settle-run-skills` |
| 2026-11-16 | B-526 (D-224 question 12, skills track S3d): proven, no code: a skill's own-tier power reads the user's learned copy (B-519 part 5), so a copy dropped by a lost run (B-525) hits softer. Spec `WoR.Combat.SkillCopyTier` | `feature/B-526-skill-copy-tier` |
| 2026-11-16 | B-527 (D-226): a ring's spell slots come from its socket's tier (`FRingLoadoutEntry::GetSpellSlotCap` reads `CrystalEffectTable::GetSocketSkillTier`; an empty ring its own tier); skill power, wear and Combat Tier keep the built average (B-519). The Equipment detail's averaged "Skill tier" line becomes "Adapter tier" / "Weapon tier" and "Crystal tier" (`LoadoutV2Model.cpp` `AddPieceAndCrystalTiers`). `WoRGameNames::GetAdapterName` shows an adapter's own `Name` (not empty, the class default or "Adapter Ring"), else names it by its socket (D-223). Q-527. Specs `WoR.Loadout.AdapterTier`, `WoR.Loadout.Rules`, `WoR.GameNames`. | `feature/B-527-adapter-slots-names-shop` |
| 2026-11-16 | B-529: one starting-gear seed, `UPoolSubsystem::SeedStartingGear` (no worn record yet and the template authors some), used by a pawn's first draw (`DrawSharedGear`) and a trial start (`StartRunGearAndGold`, before the kit): a member who reaches a trial with no record gets its starting gear first, so the run-start record is that and the kit (per run, D-221) never becomes its first record. Spec `WoR.Party.HubLeader` | fix/B-529-hub-party-leader |
| 2026-11-16 | B-540 (D-231 point 5): one crystal type per piece. `UEquipmentDataBase::AcceptedCrystalType` (None: any) read by `GearSockets::AcceptsCrystal`; `UInventoryComponent::AttachCrystalToGear` refuses another gem before any debit; `FLoadoutScreenRow::AcceptedCrystal` and a `BuildSocketChoices` refusal; `IsDataValid` checks the setting names a gem and matches the authored crystal | `feature/B-540-crystal-requirement` |
| 2026-11-16 | B-549 (D-228, one-player stage 2): the class table on `UClassDefaultLoadoutData`: `Classes` (`TMap<ECharacterClass, FClassNumbers>`; `FClassNumbers` = `Base`, `ActionSpeed`, a Refractor's `Elements`; `FClassElementNumbers` = element, BD flag, world levels, portrait, innate spells, BD pools), `FindNumbers` (a Caster by element and BD, other classes their base), `FillFrom` / the `FillFromSources` button (one row per source character, a repeat replaces), `GetClassTableString` / `PrintClassTable`. Not read by gameplay yet (stage 3). Spec `WoR.Loadout.ClassTable` | `feature/B-549-class-table-stage2` |
| 2026-11-17 | B-556 (D-228, stage 4): `UClassDefaultLoadoutData::PickWeapons`, `PickAdapter`, `MakePickedGear` (the picked pieces over a kit or alone), `GetPickedCrystals`, `ClassPickRules::PICK_ADAPTER_COUNT`; `PrintDefaults` lists the pick tables. `UTrialRunSubsystem::StartRunGearAndGold` gives a member with picks its class kit with the picks laid over it. Spec `WoR.Loadout.ClassPicks`. | `feature/B-556-picks-set-gear` |
| 2026-11-18 | Fix (Generic trial-entry crash): `UClassDefaultLoadoutData::MakePickedGear` took a reference to the first weapon place, then grew the list for the second (a kit of one piece moved), and wrote the picks into freed memory; the heap damage surfaced later in unrelated allocations (GAS, TEDS, a URL string) as a write to `UWeaponData`'s vtable. The second place is taken first now. Spec case in `WoR.Loadout.ClassPicks` (one-piece kit, two picks). | `fix/generic-trial-entry-crash` |
| 2026-11-24 | B-309d piece 1 (the slots can be the truth): `FLoadoutSlot::Place` (the `WornGearLayout` place, set by `RebuildSlotsFromLegacy`), `OffHand` (the off hand's cache), `MergeAbilities` / `WeaponSpells` / `bUseWeaponParryAnimation` (the old entry's own lists, until B-309f), `ToWeaponLoadoutEntry` / `ToRingLoadoutEntry`; `FCombatLoadout::RebuildLegacyFromSlots` rebuilds the old fields from the slots by place with no loss (an empty adapter place kept; a filled place sets its type field, an empty one keeps it; a non-Generic's secondary and a Caster's adapter loadout, never mirrored, are cleared). Nothing calls it yet: the writers move in B-309d2..d4. Spec `WoR.Loadout.SlotsTruth`. | `refactor/B-309d-slots-truth-base` |
| 2026-11-24 | B-309d2 (the screens write the slots): `FCombatLoadout::EditSlots(Class, Edit)` mirrors the old fields fully (`RebuildSlotsFromLegacy`: an in-place edit of an old entry is not seen by `EnsureSlotsInStep`), runs the edit on the slots and the non-slot fields (evolution primary, `bShowPrimary`, the type fields), then `RebuildLegacyFromSlots`; `FindSlotAt` / `PutSlotAt` (place order, replacing) / `ClearSlotAt` by `WornGearLayout` place. The gear screen's `PutGearChoice` (takes the class now) and `TakeOffGear`'s second-weapon move-up, and loadout v2's `ApplyDraft`, `ReplaceEntry`, `TakeOut`, `RemoveAllGear`, write through it. An old entry the class's mirror never holds (a weapon under a Ring or Evolution type, a Caster's adapter loadout, a non-Generic's second weapon) is kept through an edit, as the old writers kept it; the edits that dropped one before (a primary cleared, Remove all's second weapon) clear it themselves. Left: socket and skill edits through `FindRowGear` (B-309d2b), the inventory writers (B-309d3). Spec `WoR.Loadout.ScreensWriteSlots`. | `refactor/B-309d2-screens-write-slots` |
| 2026-11-24 | B-309d2b (the socket and skill edits write the slots): `WoRLoadoutScreen::EditRowGear(Loadout, Class, Row, Edit)` maps the row to its `WornGearLayout` place, and inside `EditSlots` copies that slot's weapon or adapter entry (`ToWeaponLoadoutEntry` / `ToRingLoadoutEntry`), runs the edit and puts it back (`PutSlotAt`); with no slot there (a weapon under a Ring or Evolution primary, a non-Generic's second weapon, a Caster's adapter loadout) it edits the old entry through `FindRowGear`, which `EditSlots` keeps. `PutSocketChoice` (the socket preview) and `WoRLoadoutSkills::PutSkill` take the class and go through it; innate spells are not slots and are unchanged. The owned-piece socket writers (`ApplySocketChoice`, `TakeOffSocket`, `RestoreSocket`) are `UInventoryComponent`'s, B-309d3. Spec `WoR.Loadout.ScreensWriteSlots` (7). | `refactor/B-309d2b-socket-skill-edits-write-slots` |
| 2026-11-24 | B-309d3 (the inventory and loadout component write the slots): `FCombatLoadout::EditGearEntries(Class, Edit(Place, Weapon, Adapter))` walks every weapon and adapter entry inside `EditSlots`: a slot's entry is edited and put back in its place (an emptied one leaves it), an old entry no slot holds is edited where it is. `UInventoryComponent::StripFromLoadout(Loadout, ID, Class = Generic)` strips the slots (an off hand alone; the adapter places after a removed one move up, as `RemoveAt` did) and keeps the old-field strip for what no slot holds and for the type fields and `ActiveRingIndex`. The gear sync (`LinkLoadoutsToOwned`, `RefreshLoadoutsFromOwned`, `AdoptPresetOnlyGear`, `StripUnownedGear`, `MoveLoadoutSlots`) uses `InventoryGearSync::EditGearSlots`; the run preset, `RefillActivePrimary` and `ULoadoutComponent::AutoPopulateLoadout` put the primary or adapter slot (`PutSlotAt`); `CreateLoadout`'s picks and the invalid-slot clears edit by place (`LoadoutSlotWrites::EditWeaponAt` / `EditAdapterAt`). `EditSlots` also keeps an entry its type would mirror but no slot held (picks with no gear). Spec `WoR.Loadout.InventoryWritesSlots` (4). | `refactor/B-309d3-inventory-writes-slots` |
