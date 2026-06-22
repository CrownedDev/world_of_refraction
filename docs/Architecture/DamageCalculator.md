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
- `StatScaling` (`TArray<FStatScaling>`) — the skill's authored Souls-style stat-scaling entries (stat + grade), consumed in Step 1 as an additive tier sum on top of the baseline multiplier. Empty = no scaling term (flat base, prior behavior). Replaces the removed `bOverrideStatScaling` hybrid toggle — cross-stat scaling is now authored via these tiers, not a Raw↔Spell flag swap. Full model in `ScalingSystem.md`.
- `Element` (`ESpellElement`) — elemental type; `Generic` skips element interaction.
- `bCanCrit`, `bWasInfused`, `InfusionLevel`.
- `ActionMods` (`FActionStatModifiers`) — per-action stat modifiers from all active sources (Reality innate/slotted/infused, Evolution slotted/infused, future buffs). The calculator consumes `StatusMultiplier` / `SpellDamage` / `RawDamage` / `CritChance` from it.
- `bIsRawMode`, `HitCount`, `OverrideCritChance` (negative = use default), `bIgnoreDefense`, `bIgnoreResistance`.

Note: `bIsRawMode`, `HitCount`, `InfusionLevel`, `bWasInfused`, and `bIgnoreResistance` are present on the struct but are **not consumed** inside `CalculateDamage` in the current implementation (see Known Limitations).

### `FDamageCalculationResult` (`USTRUCT`)

Output struct. Fields: `FinalDamage`, `DamageBeforeDefense`, `bWasCritical`, `DamageBlockedByDefense` (`int32`, now the **HP removed by the % reduction** — cluster 4), `ElementMultiplier`, `StatusBuildup`, `EffectiveElement`, plus debug breakdown fields `AttackerDamageMultiplier`, `CritMultiplier`, `DefenderFlatDefense` (`float` now — the **defense reduction fraction** `[0, 0.5]`, cluster 4; name unchanged, TODO rename), and `SelectedSource` (`EInfusionSourceOption`).

### `DamageConstants` namespace

`BASE_CRIT_CHANCE = 0.05`, `MAX_RESISTANCE = 0.50`, `MIN_DAMAGE = 1`. **Retired:** `POWER_INFUSION_L1/L2_MULT` (1.3/1.6 — fed only the orphaned `GetInfusionDamageMultiplier`; both deleted in the tier-power arc, live infusion damage uses `ActionExecutor::GetChargeDamageMultiplier` 1.15/1.30), `CRIT_MULTIPLIER` (the fixed ×1.5 — crit damage is now the variable `GetCritDamageMultiplier`: `CRIT_DMG_BASE` 1.0 + CritDamage stat + gear; cluster 5e-D) and `ELEMENT_INFUSION_PENALTY` (locked cost matrix). The former `MAX_CRIT_CHANCE 0.60` ceiling is gone — crit chance is now Luck-sourced and clamped to `[0, 1.0]`. (`WEAKNESS_/RESISTANCE_/NEUTRAL_MULTIPLIER` are no longer in this namespace — the element-interaction system is still unbuilt, always `1.0`.)

## How It Works

### `CalculateDamage(Attacker, Defender, Input)` — main path

1. Initialize `Result`, set `EffectiveElement = Input.Element`. Return early if `BaseDamage <= 0`.
2. **Step 1 — Attacker damage multiplier.** The baseline stat axis comes **directly from `Input.ActionType`** (`Spell` → `SpellDamage`, else `RawDamage`) — there is **no stat-swap toggle** (the old `bOverrideStatScaling` / derived-`ScalingType` Raw↔Spell swap was retired; cross-stat scaling is now authored per-skill via `StatScaling` tiers). `GetAttackerDamageMultiplier` returns the crystal-aware Spell or Raw damage multiplier for that axis, and `ActionMods.ApplyTo` boosts the matching sub-stat. Equipment stat bonus is read directly from the attacker's `ULoadoutComponent` (`GetActiveStatBonus`) and folded into `AttackerMult` as a fractional multiplier (`BonusRawDamage * RAW_DAMAGE_PER_POINT` or `BonusSpellDamage * SPELL_DAMAGE_PER_POINT`). **Then the authored scaling-tier sum is added:** for each `FStatScaling` entry in `Input.StatScaling`, `AttackerMult += GetScalingTierCoefficient(Entry.Tier) × GetScalingFraction(Entry.Stat, GetEffectiveStatForScaling(Attacker, Entry.Stat))` — an empty array contributes zero, leaving `AttackerMult` unchanged (prior behavior). Finally `RunningDamage *= AttackerMult`. Full model in `ScalingSystem.md`.
3. **Step 1.25 — Attached augment-stone raw-damage multiplier.** Physical actions only (`ActionType != Spell`). Live-resolves the attacker's active weapon attachment (`Loadout->GetActiveWeaponLoadout()->WeaponEntry.GetAttachedItem()`); if it `IsAugmentStone()`, multiplies `RunningDamage *= (1 + GetDamageStoneBasePercent(Crystal.Id)/100)`. These are **whole-number percentages applied as a direct multiplier** — *not* per-point fractions (`RAW_DAMAGE_PER_POINT` does **not** apply here). `DamageStone` tiers: F=3, E=5, D=7, C=9, B=11, A=13, S=15 (%). See *the BonusRawDamage trap* in Known Limitations and `AugmentStoneSystem.md`.
4. **Step 1.5 — Grid attacker modifier.** Multiply by `UCombatGridSubsystem::GetDamageModifier(Attacker)`.
4. **Step 2 — Status-effect modifier.** Multiply by `GetStatusEffectDamageModifier(Attacker, Defender)` (see below).
5. **Step 3 — Element interaction.** Always `1.0` (`ElementMultiplier`, no weakness/resistance system). The damage element is carried (`EffectiveElement`) for downstream status/absorption routing, not used as a multiplier here. The element reaching the calculator is already the **resolved** cast element (a Generic spell is resolved to its source element at the cast boundary — see `InfusionSystem.md` *Generic Spell Resolution*); `None` is the non-elemental sentinel (was `Generic` pre-migration).
6. **Step 4 — Critical hit.** If `bCanCrit`: take `OverrideCritChance` if non-negative, else `GetCriticalChance(Attacker)`; apply the per-action boost via `ActionMods.ApplyTo(CritChance, ESubStat::Luck)` — routed on the **Luck axis** because crit chance is now Luck-driven (cluster 5e-C2; deliberately not re-clamped here). **No separate Luck crit-bonus is added** — Luck *is* the crit chance (`GetCriticalChance` → `GetEvolutionModifiedCritChance` → `GetLuckModifiedChance`); the old standalone `(RawLuck / LUCK_RAW_MAX) * LUCK_CRIT_BONUS_MAX` block was **removed** (5e-C2, `DamageCalculator.cpp:200-202`), as re-applying it would double-count Luck. A `GuaranteedCrit` skill effect forces a crit. On crit, multiply by `GetCritDamageMultiplier(Attacker)` — the variable crit-damage multiplier (`CRIT_DMG_BASE` ×1.0 + CritDamage stat → ×1.5 + gear → ×2.0); the old fixed `CRIT_MULTIPLIER` ×1.5 was removed in cluster 5e-D.
7. Store `DamageBeforeDefense = RoundToInt(RunningDamage)`.
8. **Step 5 — Defense (% reduction, cluster 4).** Skipped if `Input.bIgnoreDefense` or the attacker has an `IgnoreDefense` skill effect. Otherwise `RunningDamage *= (1 − DefenderDefenseReduction)` where the reduction is `[0, 0.5]`, and record `DamageBlockedByDefense = preReductionRound − postReductionRound` (the HP removed). *(Was a flat-int subtraction before cluster 4; the per-point had become `RoundToInt`-floored to 0, so Defense was a silent no-op until this conversion.)*
9. **Step 6.5 — Grid defender modifier.** If `GetDefenseModifier(Defender) > 0`, divide `RunningDamage` by it.
10. **Step 7 — Minimum damage.** `FinalDamage = max(MIN_DAMAGE, RoundToInt(RunningDamage))`.

Status buildup is **not** computed inside `CalculateDamage` — the comment notes the caller should handle it separately.

**Assembly-layer multipliers (applied by the caller, not here).** Charge/infusion (`GetChargeDamageMultiplier`), tier-gap (`GetTierGapDamageMultiplier`), and **tier-power** (`TierPowerScaling::GetTierPowerMultiplier` — the action's **own** authored tier, F..S curve ×1.00…×4.80, effects excluded) are applied by `ActionExecutor` when assembling the action's damage / status / cost — *outside* `CalculateDamage`. They stack multiplicatively on the base this subsystem returns. Gear bonuses read via `GetActiveStatBonus` are **tier-weighted at aggregation** (see `LoadoutSystem.md`). Full model: `TierPowerScaling.md` / `Mechanics/TierPower.md`.

### `CalculateAttackDamage(Attacker, Target, Attack, bIsInfused)` — RETIRED (no live callers)

> **Dead code as of RequirementGapScaling Cluster 5 (2026-06-22).** It builds a bare `FDamageCalculationInput` and delegates to `CalculateDamage`, but applies **none** of the assembly-layer multipliers the real attack path applies (req-gap / tier-gap / tier-power / expected-crit), so it diverged from execution. AI attack scoring now routes through `UAIDecisionManager::EstimateAbilityDamage` (attacks are `UAbilityData`), the execution-accurate path. Its old `× (1 − requirement penalty)` reduction was also removed — requirement scaling is per-pillar now (see `Mechanics/RequirementGap.md`). Kept with a retirement note in source pending confirmation it's unreferenced, then deleted.

### Component calculations

- `GetAttackerDamageMultiplier` — `GetEvolutionModifiedSpellDamage` or `GetEvolutionModifiedRawDamage` from `UCharacterDataComponent`; `1.0` if missing.
- `GetDefenderFlatDefense` — now returns a **reduction fraction** `[0, 0.5]` (cluster 4; name unchanged, TODO rename to `GetDefenderDefenseReduction`): `GetEvolutionModifiedFlatDefense` (the crystal-aware stat fraction), then the attached `DefenseStone` % and `DefenseBuff − DefenseDebuff` skill modifiers compose **multiplicatively** onto it, final `Clamp(.., 0, UNIVERSAL_STAT_CAP 0.5)`. The flat `BonusDefense` gear field is **deferred** (no % meaning yet — TODO).
- `GetCriticalChance` — crit chance is now **Luck-sourced** (cluster 5e): `GetEvolutionModifiedCritChance` = `GetLuckModifiedChance(BASE_CRIT_CHANCE 0.05, CRIT_CHANCE_LUCK_BONUS 0.45)`, plus the kept `(CritChanceBuff − CritChanceDebuff + ModifyCritChance)/100` skill modifiers; clamped to `[0, 1.0]`. The old loadout `BonusCritChance` term is gone — that field was renamed `BonusCritDamage` and now feeds crit **damage**, not chance. See `StatComposition.md` §6.
- `RollCriticalHit` — `FRand() < (OverrideChance or GetCriticalChance)`.
- `GetElementInteractionMultiplier` — uses `IsWeakTo` / `ResistsElement`, both of which currently return `false`, so this always returns `NEUTRAL_MULTIPLIER` (`1.0`).

### Status / healing helpers

- *`CalculateStatusBuildup` — removed (sweep-3, dead code).* The live status-buildup pipeline is `UStatusBuildupManager::AddStatusBuildup` — it applies the attacker's crystal-aware `StatusMultiplier` + equipment bonus, then (sweep-3) the `StatusMultiplierBuff`/`Debuff` skill-effect deltas, then the defender's element-filtered `Resistance` reduction. See `StatusBuildupSystem.md`.
- *`GetBDStackStatusMultiplier` — REMOVED (`feature/fix-bd-stack-multiplier`; see `DamageCalculator.h:230-235`).* It lost its only caller when `CalculateStatusBuildup` was deleted. The element-gated BD absorption-stack **status-buildup** multiplier (1×/1×/2×/4× at stacks 0-3, matching-alignment only) now lives on the manager as `UBrokenDarknessManager::GetElementStackStatusMultiplier(Element)` and is consumed by `UStatusBuildupManager::AddStatusBuildup` step 5c. See `BrokenDarkness.md`.
- `CalculateHealing(Healer, Target, BaseHealing)` — scales by `GetEffectiveSpellDamage` (full composed spell power: innate + equipment + stone + transient, `[0,2]`-clamped) and by `ModifyHealing` passive skill effect (`1 + ModifyHeal/100`).

### Private helpers

- `GetStatusEffectDamageModifier` — attacker side: `(DamageBuff − DamageDebuff)/100` and `ModifyDamageDealt`. Defender side: `ModifyDamageTaken`, clamped at `-90` so reduction never exceeds 90% (positive increase uncapped). Result floored at `0`.
- `GetCritDamageMultiplier` (`DamageCalculator.cpp:578`) — returns the **full** crit-damage multiplier, not just the skill-effect term. `CRIT_DMG_BASE` ×1.0 + the CritDamage-stat ramp (`GetEvolutionModifiedMind × CritDamage points × CRIT_DAMAGE_PER_POINT`, capped ALONE at `CRIT_DAMAGE_STAT_CAP` ×1.5), then gear (`BonusCritDamage`) + attached `CritStone` (`GetAttachedStonePercent(.., CritDamage)`) + transient `(ModifyCritDamage − CritDamageDebuff)` all **multiply** past, final `Clamp(.., CRIT_DMG_BASE 1.0, CRIT_DAMAGE_GEAR_CEILING 2.0)`. An un-invested crit with no gear is exactly ×1.0. See `StatComposition.md` §6/§8.
- *`GetInfusionDamageMultiplier` — **removed** (tier-power arc): orphaned dead code (zero callers); live infusion damage rides `ActionExecutor::GetChargeDamageMultiplier` (1.15/1.30).*

## Integration Points

### Delegates broadcast
- None. `UDamageCalculator` declares and broadcasts no delegates.

### Subsystems / components it depends on
- `USkillEffectManager` (`UGameInstanceSubsystem`) — buff/debuff and passive stat modifiers (`GetTotalStatModifier`, `HasEffectOfType`), accessed via lazy-cached `CachedSkillEffectManager`.
- `UCombatGridSubsystem` (`UGameInstanceSubsystem`) — `GetDamageModifier` / `GetDefenseModifier`, via lazy-cached `CachedCombatGridSubsystem`.
- `UCharacterDataComponent` / `UCharacterData` — crystal-aware stat curves (`GetEvolutionModifiedRawDamage`, `GetEvolutionModifiedSpellDamage`, `GetEvolutionModifiedCritChance`, `GetEvolutionModifiedFlatDefense`, `GetEquipmentModifiedLuck`).
- `ULoadoutComponent` — `GetActiveStatBonus` returning `FEquipmentStatBonus`.
- `UBrokenDarknessManager` (actor component) — BD transform state for status-buildup amplification.
- Attack assets (`UAbilityData` with `bIsAttack=true` — `UWeaponAttackData` was merged away) — base damage / hit count. (The former `CalculateAttackDamage` requirement-penalty path is retired; requirement scaling is per-pillar via `ComputeActionStatModifiers` — see `Mechanics/RequirementGap.md`.)
- `CombatConstants` — `RAW_DAMAGE_PER_POINT`, `SPELL_DAMAGE_PER_POINT`, `LUCK_RAW_MAX`, `CRIT_DMG_BASE`, `CRIT_DAMAGE_STAT_CAP`, `CRIT_DAMAGE_GEAR_CEILING`, `CRIT_DAMAGE_PER_POINT`. (`LUCK_CRIT_BONUS_MAX` was retired in 5e-C3 — see Step 4.)

### Systems that depend on it
Any combat caller invoking `CalculateDamage` / `CalculateAttackDamage` / `CalculateHealing` (e.g. action execution). These call sites are outside the two files reviewed and were not traced here.

## Known Limitations / TODOs

- **⚠️ The `BonusRawDamage` trap — do not re-route stone damage through it.**
  Stone raw-damage uses the whole-percent channel (Step 1.25,
  `RunningDamage *= 1 + pct/100`) and must **never** be migrated onto
  `FEquipmentStatBonus::BonusRawDamage`. That field is folded as
  `BonusRawDamage × RAW_DAMAGE_PER_POINT` with `RAW_DAMAGE_PER_POINT = 0.0008`
  (0.08%/pt) and a **±21 clamp**, so its entire range is `21 × 0.0008 ≈ **1.7%**`
  — it physically cannot express a stone's up-to-15% bonus. This is a
  **ruled-out refactor**, recorded so it isn't re-attempted. Full reasoning in
  `AugmentStoneSystem.md`.
- **Duplicated Step 7.** `CalculateDamage` computes `Result.FinalDamage` twice in identical back-to-back statements (`DamageCalculator.cpp` lines 166–170). Harmless but redundant.
- **No element advantage system.** `IsWeakTo` and `ResistsElement` always return `false`; `GetElementInteractionMultiplier` always returns `1.0`. The `WEAKNESS_MULTIPLIER` / `RESISTANCE_MULTIPLIER` constants and `MAX_RESISTANCE` are presently unused for elemental interaction.
- **Unused input fields.** `FDamageCalculationInput::HitCount`, `InfusionLevel`, `bWasInfused`, `bIsRawMode`, and `bIgnoreResistance` are not consumed by `CalculateDamage`. Multi-hit (`HitCount`) is not wired into the main path. *(Infusion damage is applied upstream at the `ActionExecutor` assembly layer via `GetChargeDamageMultiplier`, not here; the old `GetInfusionDamageMultiplier` was deleted in the tier-power arc.)*
- **`FDamageCalculationResult::StatusBuildup`** is never populated by the calculator — it is left at its default and expected to be filled by the caller.
- Comments reference a prior `RawDamageBuff` / `CritChanceBuff` status-effect path for per-instance weapon bonuses; the current code reads equipment bonuses directly via `ULoadoutComponent`. The comment in `CalculateAttackDamage` still describes the older equip-time status-effect approach, which is a potential source of confusion.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-3 — removed dead `CalculateStatusBuildup` (zero callers). Status-buildup amplification (Spirit-driven, equipment bonus, `StatusMultiplierBuff`/`Debuff` aggregation, target-side `Resistance`) now lives entirely on `UStatusBuildupManager::AddStatusBuildup`. `GetBDStackStatusMultiplier` retained — still the BD status-buildup leaf accessor. | feature/integration-gaps-sweep-3 |
| 2026-06-07 | Documented **Step 1.25** — the attached weapon-stone whole-percent raw-damage multiplier (`DamageStone` F=3..S=15%, physical-only, direct multiplier). Added the **`BonusRawDamage` trap** to Known Limitations (ruled-out refactor; ±21 × 0.0008 ≈ 1.7% can't express a stone). See new `AugmentStoneSystem.md`. | feature/weapon-stones |
| 2026-06-16 | Stat-redesign sync — **Step 5 Defense** now a capped **% reduction** (`RunningDamage ×= (1 − reduction)`, `[0, 0.5]`) instead of flat-int subtraction (cluster 4; `GetDefenderFlatDefense` returns a fraction, `BonusDefense` deferred). **Crit** updated: fixed `CRIT_MULTIPLIER` retired (5e-D — crit damage is the variable `GetCritDamageMultiplier`), crit **chance** is Luck-sourced and clamped `[0, 1.0]` (`MAX_CRIT_CHANCE` gone). `FDamageResult.DefenderFlatDefense` is now a `float` fraction; `DamageBlockedByDefense` is the HP removed. See `StatComposition.md`. | feature/realtime-defense |
| 2026-06-16 | Doc-sync: §Step 4 — removed the stale standalone Luck crit-bonus (`(RawLuck/LUCK_RAW_MAX)×LUCK_CRIT_BONUS_MAX`, deleted 5e-C2, `DamageCalculator.cpp:200-202`); crit-chance boost routes on the Luck axis. §Private helpers — `GetCritDamageMultiplier` corrected to the **full** base+stat+gear+transient multiplier (`:578`, clamp `[1.0, 2.0]`), and `GetBDStackStatusMultiplier` marked **removed** (now `UBrokenDarknessManager::GetElementStackStatusMultiplier`, consumed by `AddStatusBuildup` step 5c). Depends-on list: dropped retired `LUCK_CRIT_BONUS_MAX`, added the crit-damage constants. | feature/realtime-defense |
| 2026-06-17 | Attack/ability merge — `CalculateAttackDamage` takes `USkillDataBase*` (a `UAbilityData` with `bIsAttack=true`); builds the input with `ActionType = Ability` (`EActionType::Attack` collapsed into `Ability` — both scaled `RawDamage`, so numerically unchanged). `UWeaponAttackData` deleted. | feature/realtime-defense |
| 2026-06-17 | Documented the **hybrid stat toggle** (`bOverrideStatScaling`) — the consume point lives here in **Step 1** (the derived `ScalingType` Raw↔Spell swap, `DamageCalculator.cpp:50`), previously undocumented. Added it to the `FDamageCalculationInput` field list as a **consumed** field (stat-only). The upstream sourcing was split into two honest flags (physical = skill-root, attack-wide; spell = per-cast off `FSkillCastEntry`, threaded via the B1 resolved-table stash); the calculator's consume point is **unchanged** — both still feed `Input.bOverrideStatScaling`. | feature/realtime-defense |
| 2026-06-18 | **Unified scaling-tiers arc** — the hybrid stat toggle (`bOverrideStatScaling`) and the `ScalingType` Raw↔Spell swap were **retired**. Step 1 now takes the stat axis directly from `Input.ActionType` and adds an authored tier sum over `Input.StatScaling` (`Σ GetScalingTierCoefficient × GetScalingFraction`; empty array = prior behavior). `FDamageCalculationInput.bOverrideStatScaling` removed, `StatScaling` added. Full system documented in the new `ScalingSystem.md`. | feature/unified-scaling-tiers | feature/realtime-defense |
| 2026-06-21 | **Tier-power arc sync** — `GetInfusionDamageMultiplier` + `POWER_INFUSION_L1/L2_MULT` were **dead code, deleted** (live infusion uses `ActionExecutor::GetChargeDamageMultiplier`); corrected the `DamageConstants`, Private-helpers, and Known-Limitations references. Added the **assembly-layer multipliers** note (charge × tier-gap × tier-power applied in `ActionExecutor`, outside `CalculateDamage`; gear bonuses tier-weighted at `GetActiveStatBonus`). See `TierPowerScaling.md`. | feature/tier-power-scaling |
