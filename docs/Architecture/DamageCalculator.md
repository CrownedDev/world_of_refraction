# Damage Calculator

## Overview

The Damage Calculator is the centralized damage-calculation system for combat. Every damage formula is intended to flow through it for consistency. It is a `UGameInstanceSubsystem` (`UDamageCalculator`) and computes final damage from a base value plus attacker multipliers, grid modifiers, status-effect modifiers, element interactions, critical hits, and defender flat defense. It also provides convenience paths for weapon attacks, status-buildup amounts, and healing.

## Architecture

### `UDamageCalculator` (`UGameInstanceSubsystem`)

The subsystem itself. Lifecycle: `Initialize` logs a startup line; there is no `Deinitialize` override.

Cached cross-subsystem references (both `mutable`, lazy-acquired):
- `CachedSkillEffectManager` (`USkillEffectManager*`) — buff/debuff and passive stat modifiers.
- `CachedCombatGridSubsystem` (`UCombatGridSubsystem*`) — grid-position damage/defense modifiers.

### `FDamageCalculationInput` (`USTRUCT`)

Input struct for `CalculateDamage`. Important fields:
- `BaseDamage` (`int32`) — base damage before modifiers; calculation returns early if `<= 0`.
- `ActionType` (`EActionType`) — `Spell` selects `SpellDamage`; `Ability` / `Attack` / `None` select `RawDamage`.
- `Element` (`ESpellElement`) — elemental type; `Generic` skips element interaction.
- `bCanCrit`, `bWasInfused`, `InfusionLevel`.
- `ActionMods` (`FActionStatModifiers`) — per-action stat modifiers from all active sources (Reality innate/slotted/infused, Evolution slotted/infused, future buffs). The calculator consumes `StatusMultiplier` / `SpellDamage` / `RawDamage` / `CritChance` from it.
- `bIsRawMode`, `HitCount`, `OverrideCritChance` (negative = use default), `bIgnoreDefense`, `bIgnoreResistance`.

Note: `bIsRawMode`, `HitCount`, `InfusionLevel`, `bWasInfused`, and `bIgnoreResistance` are present on the struct but are **not consumed** inside `CalculateDamage` in the current implementation (see Known Limitations).

### `FDamageCalculationResult` (`USTRUCT`)

Output struct. Fields: `FinalDamage`, `DamageBeforeDefense`, `bWasCritical`, `DamageBlockedByDefense`, `ElementMultiplier`, `StatusBuildup`, `EffectiveElement`, plus debug breakdown fields `AttackerDamageMultiplier`, `CritMultiplier`, `DefenderFlatDefense`, and `SelectedSource` (`EInfusionSourceOption`).

### `DamageConstants` namespace

`CRIT_MULTIPLIER = 1.5`, `BASE_CRIT_CHANCE = 0.05`, `MAX_CRIT_CHANCE = 0.60`, `MAX_RESISTANCE = 0.50`, `MIN_DAMAGE = 1`, `WEAKNESS_MULTIPLIER = 1.5`, `RESISTANCE_MULTIPLIER = 0.5`, `NEUTRAL_MULTIPLIER = 1.0`, `POWER_INFUSION_L1_MULT = 1.3`, `POWER_INFUSION_L2_MULT = 1.6`. `ELEMENT_INFUSION_PENALTY` was removed per a locked cost matrix.

## How It Works

### `CalculateDamage(Attacker, Defender, Input)` — main path

1. Initialize `Result`, set `EffectiveElement = Input.Element`. Return early if `BaseDamage <= 0`.
2. **Step 1 — Attacker damage multiplier.** `GetAttackerDamageMultiplier` returns the crystal-aware Spell or Raw damage multiplier (branched on `ActionType`). `ActionMods.ApplyTo` boosts the matching sub-stat (`SpellDamage` or `RawDamage`). Equipment stat bonus is read directly from the attacker's `ULoadoutComponent` (`GetActiveStatBonus`) and folded into `AttackerMult` as a fractional multiplier (`BonusRawDamage * RAW_DAMAGE_PER_POINT` or `BonusSpellDamage * SPELL_DAMAGE_PER_POINT`). `RunningDamage *= AttackerMult`.
3. **Step 1.5 — Grid attacker modifier.** Multiply by `UCombatGridSubsystem::GetDamageModifier(Attacker)`.
4. **Step 2 — Status-effect modifier.** Multiply by `GetStatusEffectDamageModifier(Attacker, Defender)` (see below).
5. **Step 3 — Element interaction.** If `Element != Generic` and a defender exists, multiply by `GetElementInteractionMultiplier(Element, Defender InnateElement)`. Currently always `1.0` (no weakness/resistance system).
6. **Step 4 — Critical hit.** If `bCanCrit`: take `OverrideCritChance` if non-negative, else `GetCriticalChance(Attacker)`; apply `ActionMods.CritChance` (deliberately not re-clamped here). Add a Luck-driven bonus: `(RawLuck / LUCK_RAW_MAX) * LUCK_CRIT_BONUS_MAX`, where `RawLuck` comes from `UCharacterDataComponent::GetEquipmentModifiedLuck`. A `GuaranteedCrit` skill effect forces a crit. On crit, multiply by `CRIT_MULTIPLIER * GetCritDamageMultiplier(Attacker)`.
7. Store `DamageBeforeDefense = RoundToInt(RunningDamage)`.
8. **Step 5 — Flat defense.** Skipped if `Input.bIgnoreDefense` or the attacker has an `IgnoreDefense` skill effect. Otherwise subtract `min(DefenderFlatDefense, RunningDamage)` and record `DamageBlockedByDefense`.
9. **Step 6.5 — Grid defender modifier.** If `GetDefenseModifier(Defender) > 0`, divide `RunningDamage` by it.
10. **Step 7 — Minimum damage.** `FinalDamage = max(MIN_DAMAGE, RoundToInt(RunningDamage))`.

Status buildup is **not** computed inside `CalculateDamage` — the comment notes the caller should handle it separately.

### `CalculateAttackDamage(Attacker, Target, Attack, bIsInfused)` — weapon convenience wrapper

Requires a non-null `Attack` (`UWeaponAttackData`) and resolvable `UCharacterData`. Builds an `FDamageCalculationInput` with `BaseDamage = Attack->BaseDamage` (strict — no fallback; a `0` base deals `0`), `ActionType = Attack`. Applies the attack's `CalculateRequirementPenalty` as a multiplicative reduction. Sets `Element` to the attacker `InnateElement` when infused, else `Generic` (infused attacks still scale by `RawDamage`; element only affects status routing). Sets `bCanCrit = true`, `HitCount = Attack->HitCount`, then delegates to `CalculateDamage`.

### Component calculations

- `GetAttackerDamageMultiplier` — `GetCrystalModifiedSpellDamage` or `GetCrystalModifiedRawDamage` from `UCharacterDataComponent`; `1.0` if missing.
- `GetDefenderFlatDefense` — `GetCrystalModifiedFlatDefense`, plus flat `BonusDefense` from the loadout, then a multiplicative percentage modifier from `DefenseBuff − DefenseDebuff` skill effects.
- `GetCriticalChance` — `GetCrystalModifiedCritChance`, plus `BonusCritChance/100` from the loadout, plus `(CritChanceBuff − CritChanceDebuff + ModifyCritChance)/100` from skill effects; clamped to `[0, MAX_CRIT_CHANCE]`.
- `RollCriticalHit` — `FRand() < (OverrideChance or GetCriticalChance)`.
- `GetElementInteractionMultiplier` — uses `IsWeakTo` / `ResistsElement`, both of which currently return `false`, so this always returns `NEUTRAL_MULTIPLIER` (`1.0`).

### Status / healing helpers

- *`CalculateStatusBuildup` — removed (sweep-3, dead code).* The live status-buildup pipeline is `UStatusBuildupManager::AddStatusBuildup` — it applies the attacker's crystal-aware `StatusMultiplier` + equipment bonus, then (sweep-3) the `StatusMultiplierBuff`/`Debuff` skill-effect deltas, then the defender's element-filtered `Resistance` reduction. See `StatusBuildupSystem.md`.
- `GetBDStackStatusMultiplier` — `1.0` unless the attacker's `UBrokenDarknessManager` is transformed and the spell element matches `GetCurrentAlignment`; then returns `GetStackStatusMultiplier`. This is a **status-buildup** multiplier (1×/1×/2×/4× at stacks 0-3, matching-element only) — not a damage buff. Still consumed by the BD damage path.
- `CalculateHealing(Healer, Target, BaseHealing)` — scales by `GetCrystalModifiedSpellDamageForHealing` (Mind-based) and by `ModifyHealing` passive skill effect (`1 + ModifyHeal/100`).

### Private helpers

- `GetStatusEffectDamageModifier` — attacker side: `(DamageBuff − DamageDebuff)/100` and `ModifyDamageDealt`. Defender side: `ModifyDamageTaken`, clamped at `-90` so reduction never exceeds 90% (positive increase uncapped). Result floored at `0`.
- `GetCritDamageMultiplier` — `1 + max(0, ModifyCritDamage/100)` from skill effects.
- `GetInfusionDamageMultiplier(InfusionLevel)` — static; returns L1/L2 power-infusion multipliers or `1.0`.

## Integration Points

### Delegates broadcast
- None. `UDamageCalculator` declares and broadcasts no delegates.

### Subsystems / components it depends on
- `USkillEffectManager` (`UGameInstanceSubsystem`) — buff/debuff and passive stat modifiers (`GetTotalStatModifier`, `HasEffectOfType`), accessed via lazy-cached `CachedSkillEffectManager`.
- `UCombatGridSubsystem` (`UGameInstanceSubsystem`) — `GetDamageModifier` / `GetDefenseModifier`, via lazy-cached `CachedCombatGridSubsystem`.
- `UCharacterDataComponent` / `UCharacterData` — crystal-aware stat curves (`GetCrystalModifiedRawDamage`, `GetCrystalModifiedSpellDamage`, `GetCrystalModifiedCritChance`, `GetCrystalModifiedFlatDefense`, `GetCrystalModifiedSpellDamageForHealing`, `GetEquipmentModifiedLuck`).
- `ULoadoutComponent` — `GetActiveStatBonus` returning `FEquipmentStatBonus`.
- `UBrokenDarknessManager` (actor component) — BD transform state for status-buildup amplification.
- `UWeaponAttackData` — base damage / hit count / requirement penalty for `CalculateAttackDamage`.
- `CombatConstants` — `RAW_DAMAGE_PER_POINT`, `SPELL_DAMAGE_PER_POINT`, `LUCK_RAW_MAX`, `LUCK_CRIT_BONUS_MAX`.

### Systems that depend on it
Any combat caller invoking `CalculateDamage` / `CalculateAttackDamage` / `CalculateHealing` (e.g. action execution). These call sites are outside the two files reviewed and were not traced here.

## Known Limitations / TODOs

- **Duplicated Step 7.** `CalculateDamage` computes `Result.FinalDamage` twice in identical back-to-back statements (`DamageCalculator.cpp` lines 166–170). Harmless but redundant.
- **No element advantage system.** `IsWeakTo` and `ResistsElement` always return `false`; `GetElementInteractionMultiplier` always returns `1.0`. The `WEAKNESS_MULTIPLIER` / `RESISTANCE_MULTIPLIER` constants and `MAX_RESISTANCE` are presently unused for elemental interaction.
- **Unused input fields.** `FDamageCalculationInput::HitCount`, `InfusionLevel`, `bWasInfused`, `bIsRawMode`, and `bIgnoreResistance` are not consumed by `CalculateDamage`. Multi-hit (`HitCount`) and infusion-level multipliers (`GetInfusionDamageMultiplier`) are not wired into the main path.
- **`FDamageCalculationResult::StatusBuildup`** is never populated by the calculator — it is left at its default and expected to be filled by the caller.
- Comments reference a prior `RawDamageBuff` / `CritChanceBuff` status-effect path for per-instance weapon bonuses; the current code reads equipment bonuses directly via `ULoadoutComponent`. The comment in `CalculateAttackDamage` still describes the older equip-time status-effect approach, which is a potential source of confusion.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-3 — removed dead `CalculateStatusBuildup` (zero callers). Status-buildup amplification (Spirit-driven, equipment bonus, `StatusMultiplierBuff`/`Debuff` aggregation, target-side `Resistance`) now lives entirely on `UStatusBuildupManager::AddStatusBuildup`. `GetBDStackStatusMultiplier` retained — still the BD status-buildup leaf accessor. | feature/integration-gaps-sweep-3 |
