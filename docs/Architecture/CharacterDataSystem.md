# Character Data System

## Overview

The Character Data System holds a character's stats and identity, and tracks
their runtime combat state. It is split into two cooperating pieces:

- **`UCharacterData`** — an immutable, design-time `UPrimaryDataAsset`. It
  defines the character's identity (name, class, innate element, portrait),
  AI/control flag, default loadout reference, the stat-point distribution
  (character "DNA"), world-level progression, and cosmetic/defense animation
  references. It also contains all the *stat formulas* (efficiency, spell
  damage, crit, defense, HP/EP pools, etc.) as inline `BlueprintPure` helpers.

- **`UCharacterDataComponent`** — a mutable, replicated `UActorComponent` that
  wraps a `UCharacterData` asset for combat use. It holds the live runtime state
  — current/max HP and EP, alive/dead, and Broken Darkness transformation — and
  is the server-authoritative entry point for damage, healing, energy spend/gain,
  death and resurrection. It also exposes **crystal-aware** stat queries that
  layer equipment and evolution-crystal modifiers on top of the asset formulas.

In combat, the asset is the source of truth for *potential* (stat points and
formula shapes); the component is the source of truth for *current condition*
(HP/EP/alive) and for *effective* stats once equipment is factored in.

## Architecture

### `UCharacterData` (`UPrimaryDataAsset`)

Immutable design-time data. Key groups of fields:

- **Identity**: `Name`, `CharacterClass` (`ECharacterClass`, default `Caster`),
  `InnateElement` (`ESpellElement`, editor-gated to Casters only),
  `Description`, `Portrait`.
- **Control**: `bIsAIControlled`.
- **Default loadout**: `DefaultLoadout` (`ULoadoutData*`; nullptr means build
  from inventory).
- **World stat levels**: `WorldMindLevel`, `WorldBodyLevel`, `WorldSpiritLevel`
  (each clamped 0–7) — progression multipliers.
- **Sub-stats (character DNA)** — 13 `int32` point pools across three pillars:
  - Mind (4): `Efficiency`, `SpellDamage`, `CritChance`, `SpellSpeed`.
  - Body (4): `Defense`, `ActionSpeed`, `RawDamage`, `MaxHealth`.
  - Spirit (5): `MaxEnergy`, `Resistance`, `TurnSpeed`, `Luck`,
    `StatusMultiplier`.
- **Defense animations**: `DodgeLeftMontage`, `DodgeRightMontage`,
  `BlockMontage`, `ParryMontage`, `bUseWeaponParryAnimation`.
- **Cosmetics**: `UnarmedStance`, `ItemUseSelfMontage`, `ItemUseTargetMontage`,
  `RingSwitchMontage` (Resonator-gated), `InfusionDisplay`,
  `EquippedWeatherVariant`.

Class helpers: `IsGeneric/IsCaster/IsResonator`, `CanUseSpells`,
`CanUseAbilities`, `HasInnateElement`, `UsesRings`, `CanDualWield`, `ShouldUseAI`,
and `GetElement()` (Caster returns `InnateElement`, all others return `Generic`).

Stat-budget helpers: `GetTotalPool()` (`StatConstants::INITIAL_STAT_BUDGET` plus
world-level points), `GetTotalSpent()`, `GetPointsRemaining()`,
`IsValidDistribution()`.

Stat formula layers (all inline `BlueprintPure`):

- `GetTotal<Stat>()` accessors return the raw point value of each sub-stat.
- `GetBaseMind/Body/Spirit()` sum the sub-stats per pillar.
- `GetEffectiveMind/Body/Spirit()` scale the base by the matching world level
  (`WORLD_*_SCALING_BONUS`).
- `Calculate*` functions derive combat values from effective pillars:
  `CalculateEfficiencyMultiplier`, `CalculateEfficiencyRingBreakReduction`
  (Resonator-only), `CalculateStatusMultiplier` (Spirit-driven),
  `CalculateSpellDamage`, `CalculateCritChance`, `CalculateSpellSpeed`,
  `CalculateFlatDefense`, `CalculateActionSpeed`, `CalculateAnimationSpeed`,
  `CalculateTurnSpeed`, `CalculateLuck`, `CalculateMaxHealth`,
  `CalculateMaxEnergy`, `CalculateRawDamage`, `CalculateResistance`,
  `CalculateStatusMultiplierFlat`.

`FEvolutionCostResult` (`USTRUCT`) — describes what a character gains/loses on
evolution (`bCanEvolve`, `CostDescription`, `GainDescription`, `Warnings`).
Defined in the header but not consumed within these files.

`CharacterData.cpp` is almost entirely empty (formulas are inline in the
header). Its only logic is `IsDataValid()` (editor-only), which errors on an
empty name and warns when a Caster has no innate element or when no
`DefaultLoadout` is assigned.

### `UCharacterDataComponent` (`UActorComponent`)

Replicated runtime state component. Tick disabled. Constructor seeds
HP/EP/Max to 100 and `bIsAlive = true`.

Replicated fields:

| Field | Replication | Notes |
|-------|-------------|-------|
| `CurrentHP` | `ReplicatedUsing = OnRep_CurrentHP` | Live HP. |
| `CurrentEP` | `ReplicatedUsing = OnRep_CurrentEP` | Live EP. |
| `MaxHP` / `MaxEP` | not replicated | Computed locally via `RecomputeMaxPools()`. |
| `bIsAlive` | `ReplicatedUsing = OnRep_bIsAlive` | Alive/dead flag. |
| `bIsBrokenDarkness` | `SaveGame`, `ReplicatedUsing = OnRep_bIsBrokenDarkness` | Runtime Broken Darkness flag. Must be read via `IsBrokenDarkness()`, never directly. `SaveGame`-tagged for future persistence; currently session-only. |

`CharacterData` (`UCharacterData*`) — the template the component wraps.

Private helpers: `CheckDeath`, `HasServerAuthority`, `CalculateMaxHealth`,
`CalculateMaxEnergy` (both currently stub formulas returning 100), and the four
`OnRep_*` callbacks.

## How It Works

### Initialization (`BeginPlay`)

1. If `CharacterData` is set, the component locates the sibling
   `UInventoryComponent` and `ULoadoutComponent` on the owning actor and
   initializes them **first** (`Inventory->InitializeFromCharacterData`, then
   `Loadout->InitializeFromCharacterData`). This ordering matters: the
   crystal-aware pillar reads need the loadout's slotted evolution crystal to be
   available.
2. `RecomputeMaxPools()` computes `MaxHP` / `MaxEP`, then `CurrentHP` / `CurrentEP`
   are set to full.
3. If the server has authority and the asset's `InnateElement` is
   `BrokenDarkness`, `bIsBrokenDarkness` is set true and `CurrentEP` zeroed — a
   character-created BD starts in the correct state without a transform event.

`InitializeFromTemplate()` and `ResetToMax()` are alternative (re)initialization
entry points; `InitializeFromTemplate()` uses the stub
`CalculateMaxHealth/Energy` (always 100).

### Max pool computation

`RecomputeMaxPools()` (safe to re-call on equipment change) computes:

- `MaxHP` = `MAX_HEALTH_BASE` + `GetCrystalModifiedBody()` ×
  `CharacterData->GetTotalMaxHealth()` × `MAX_HEALTH_PER_POINT`, plus the active
  loadout's `BonusMaxHP`.
- `MaxEP` = `MAX_ENERGY_BASE` + `GetCrystalModifiedSpirit()` ×
  `GetTotalMaxEnergy()` × `MAX_ENERGY_PER_POINT`, plus loadout `BonusMaxEnergy`.

It does **not** clamp `CurrentHP`/`CurrentEP` or broadcast change events — the
caller decides clamp/refill/notify policy.

### Crystal-aware stat layer

A file-local `ApplyCrystalPillarModifier()` helper layers **two** optional
modifier sources on top of an asset pillar value:

1. **Crystal layer** — the primary-weapon-slot evolution crystal's
   `BaseStatBonus.Bonus{Mind,Body,Spirit}ModifierPercent`, applied
   multiplicatively (divided by `STAT_PERCENT_DIVISOR`). Read via
   `ULoadoutComponent::GetActivePrimaryEvolutionCrystal()`.
2. **Equipment layer** — the active loadout's matching
   `FEquipmentStatBonus` percent, applied multiplicatively on top.

`GetCrystalModifiedMind/Body/Spirit()` feed `GetEffectiveMind/Body/Spirit()`
through this helper. The derived `GetCrystalModified*` functions
(`SpellDamage`, `RawDamage`, `CritChance`, `FlatDefense`,
`SpellDamageForHealing`, `EfficiencyMultiplier`) mirror the asset's `Calculate*`
formula shapes but substitute the crystal-modified pillar as input.

`GetEquipmentModifiedLuck()` is crystal-aware Luck: pillar-scaled against
`GetCrystalModifiedSpirit()`, plus the loadout's `BonusLuck`, plus
skill-effect `LuckBuff`/`LuckDebuff` modifiers from `USkillEffectManager`,
clamped to `LUCK_RAW_MAX`.

All crystal-aware getters fall back to the raw asset value (or a sensible
default) if no `LoadoutComponent` or `CharacterData` is present.

### Server-authoritative state mutation

All mutators check `HasServerAuthority()` first (which returns true unconditionally
in `NM_Standalone`/PIE):

- `ServerTakeDamage` — subtracts (floored at 0), broadcasts `OnHPChanged`,
  calls `CheckDeath`.
- `ServerHeal` / `ServerSetHP` — clamp to `[0, MaxHP]`, broadcast.
- `ServerSpendEnergy` — subtract EP, broadcast.
- `ServerGainEnergy` / `ServerSetEP` — **suppressed** for BD characters
  (BD energy is event-driven absorption, not passive regen — see below) and for
  Resonators without a usable EP target (`!HasUsableEPTarget()`); `ServerSetEP`
  allows setting to 0 in those cases.
- `ServerGainBrokenDarknessEnergy` — the BD absorption-gain path. Bypasses the
  `ServerGainEnergy` BD early-out and lets `CurrentEP` exceed `MaxEP` into
  overload (clamped to a caller-supplied `AbsoluteMax`). Since Session 5, BD
  energy is unified onto `CurrentEP` — see `docs/Architecture/BrokenDarkness.md`.
- `ServerResurrect` — restores HP (clamped `[1, MaxHP]`), sets `bIsAlive`,
  broadcasts `OnResurrected` and `OnHPChanged`.

`CheckDeath()` includes a **Revive intercept**: if `USkillEffectManager` reports
the owner has a `Revive` skill effect, HP is restored to 30% of `MaxHP`, the
effect is consumed, and the death broadcast is skipped. Otherwise `bIsAlive` is
set false and `OnDied` broadcasts.

### Broken Darkness state

`IsBrokenDarkness()` is the unified read — true if either the runtime
`bIsBrokenDarkness` flag is set *or* the asset's `InnateElement` is
`BrokenDarkness`. All BD-aware code must use this helper rather than checking
either source directly.

`ServerSetBrokenDarkness(bool)` (server-only) is called by
`BrokenDarknessManager::TriggerTransformation` on a successful break roll; on
transition to BD it zeroes `CurrentEP` and broadcasts `OnEPChanged`.

### Equipment access passthroughs

`GetActiveWeapon()` delegates to the sibling `ULoadoutComponent::GetActiveWeapon()`.
`HasUsableEPTarget()` returns true if the loadout has an active weapon or a
primary slot of type `EPrimarySlotType::Evolution` — used to gate Resonator EP
gain. `DebugToggleWeapon()` (CallInEditor) forwards to
`ULoadoutComponent::ToggleEquipment()`.

### Replication callbacks

`OnRep_CurrentHP` / `OnRep_CurrentEP` re-broadcast the matching change delegate
on clients. `OnRep_bIsAlive` broadcasts `OnResurrected` or `OnDied` depending on
the new state. `OnRep_bIsBrokenDarkness` re-broadcasts `OnEPChanged` so the EP
bar repaints (no client-side state mutation — the server already cleared
`CurrentEP`).

## Integration Points

### Delegates broadcast (`UCharacterDataComponent`)

- `FOnHPChanged OnHPChanged(int32 CurrentHP, int32 MaxHP)`.
- `FOnEPChanged OnEPChanged(int32 CurrentEP, int32 MaxEP)`.
- `FOnDied OnDied(AActor* DeadActor)`.
- `FOnResurrected OnResurrected(AActor* ResurrectedActor)`.

### Subsystems / systems it depends on

- `USkillEffectManager` (`UGameInstanceSubsystem`) — queried in `CheckDeath`
  (`HasEffectOfType`/`RemoveEffectsByType` for the Revive intercept) and in
  `GetEquipmentModifiedLuck` (`GetTotalStatModifier` for Luck buff/debuff).
  Resolved via `GetGameInstance()->GetSubsystem<USkillEffectManager>()`.
- `ULoadoutComponent` — sibling component; used for active weapon, primary slot
  type, `GetActiveStatBonus`, `GetActivePrimaryEvolutionCrystal`, and
  `ToggleEquipment`.
- `UInventoryComponent` — sibling component; initialized from `CharacterData` in
  `BeginPlay`.
- `BrokenDarknessManager` (per code comments) — calls `ServerSetBrokenDarkness`.

### Systems that depend on it

- `ULoadoutComponent` — reads `CharacterData` (class, defense/cosmetic montages,
  innate element) via the sibling `UCharacterDataComponent`.
- Combat systems (damage, healing, status, AI, turn order) — consume the
  `GetCrystalModified*` / `GetEquipmentModifiedLuck` stat queries and the
  `Server*` mutators.
- UI (HP/EP bars, death/resurrection feedback) — bind to the four delegates.
- The asset's `Calculate*` formulas are referenced (with crystal-aware
  substitution) throughout the combat stat pipeline.

## Known Limitations / TODOs

- `UCharacterDataComponent::CalculateMaxHealth()` and `CalculateMaxEnergy()` are
  stubs — both carry `// TODO: Implement actual HP/EP formula` and return a flat
  `100`. The real pool math lives in `RecomputeMaxPools()`; `InitializeFromTemplate()`
  still uses the stubs, so it produces incorrect 100/100 pools if used.
- `bIsBrokenDarkness` is `SaveGame`-tagged but the header notes "no save system
  exists yet, so the flag is session-only as of this commit."
- `MaxHP` / `MaxEP` are not replicated — clients rely on local
  `RecomputeMaxPools()` producing the same result; equipment-driven divergence is
  a latent risk in a networked session.
- A code comment in `CharacterDataComponent.cpp` notes the old
  `UItemData::CalculateModified*` crystal helpers "were removed in the
  BaseStatBonus migration" — crystal pillar percentages are now read directly
  from `Crystal->BaseStatBonus`.
- The `CharacterData` header retains a `Body (3)` comment over the Body
  calculations section, though Body now has 4 sub-stats — a stale comment.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
