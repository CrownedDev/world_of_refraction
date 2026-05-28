# AI System

## Overview

The AI System drives non-player combatants through their turns and through defensive reactions during turn-based combat. It chooses a turn action (Attack / Spell / Ability / Item / Defend), selects targets, decides infusion levels, and — separately — reacts inside the defense window with a Block/Parry/Dodge.

Its single class is `UAIDecisionManager`, a `UGameInstanceSubsystem`. All decisions are routed through the standard combat action pipeline (`FAction` submitted to `ACombatOrchestrator`), so AI turns and player turns share the same execution path. Decision quality scales with `EAIDifficulty` (`Easy`, `Medium`, `Hard`, `Expert`).

## Architecture

### `UAIDecisionManager` (`UGameInstanceSubsystem`)

Combat-registration state:
- `ACombatOrchestrator* CurrentCombat` — the active combat, set via `SetCombatOrchestrator` / cleared via `ClearCombatOrchestrator`.
- `AActor* PendingActor` — the actor currently waiting on a thinking-delay timer.
- `FTimerHandle ThinkingTimerHandle` — the turn-decision delay timer.
- `UDefenseSystem* DefenseSystemRef` — cached lazily (may not exist at `Initialize`).
- `TMap<AActor*, FTimerHandle> DefenseTimerHandles` — per-actor defense reaction timers.

Public API (`UFUNCTION(BlueprintCallable)`):
- `SetCombatOrchestrator` / `ClearCombatOrchestrator` — combat registration.
- `RequestDecision(AActor*)` — request a turn decision (applies a thinking delay then submits).
- `ScheduleDefenseDecision(Defender, AttackSize, BaseDamage, WindowDuration)` — called by `UDefenseSystem` when a defense window opens.
- `GetCurrentDifficulty()` — `BlueprintPure`, reads difficulty from the orchestrator.

Internal subsystem accessors: `GetSkillEffectManager()`, `GetActionExecutor()`.

#### Decision-logic functions (private)

- `ExecuteDecision` — timer callback; validates turn ownership, builds the action, submits it.
- `BuildAction` — top-level dispatch: Easy = random; Medium+ = `BuildAction_Smart`.
- `ChooseActionType` — random action-type pick (used by Easy).
- `BuildAction_Smart` — three-branch decision: survival → cleanse → offensive.
- `TrySurvivalBranch`, `TryCleanseBranch`, `BuildOffensiveAction` — the branches.
- Thinking delay: `GetThinkingDelayRange`, `CalculateThinkingDelay`.

#### Target scoring & estimation

- `ScoreTarget`, `SelectBestTarget`, `EstimateBestDamage`.
- `EstimateSpellDamage`, `EstimateAbilityDamage` — execution-accurate estimates that route through `UDamageCalculator` with `FActionStatModifiers` computed by `UActionExecutor`.
- `EstimateStatusScore` (spell and ability overloads) — scores a skill's status-buildup payload.
- `CanAffordSpell`, `CanAffordAbility` — EP-affordability checks via `UActionExecutor::CalculateActionEnergyCost`.
- `CalculateThreatLevel`, `CanKillTarget`.
- HP/EP helpers: `GetHPPercent`, `GetCurrentHP`, `GetMaxHP`, `GetCurrentEP`, `GetMaxEP`, `GetCharacterData`.
- `HasDangerousDebuff` — detects stun (`SkipTurn`) or a lethal DOT.

#### Loadout detection helpers

`FindHealingSpell` (Restoration school + `Heal`/`HealthRestore` effect), `FindCleanseSpell` (`Cleanse` effect), `FindHealingItem` (Sapphire crystal), `FindCleanseItem` (Iolite crystal), `FindEnergyItem` (Citrine crystal).

#### Defense logic

`ChooseDefenseType`, `GetDefenseAttemptChance`, `GetDefenseAccuracy`, `CalculateDefenseReactionDelay`.

#### Status-bar & infusion

`IsStatusBarNearTrigger`, `WouldTriggerStatusBar`, `IsValuableStatus`, `DecideSpellInfusionLevel`, `DecideAbilityInfusionLevel`.

### `AIDecisionConstants.h` — `namespace AIConstants`

All tunable values are `constexpr` in this namespace:
- **Target scoring** — `KILL_POTENTIAL_SCORE` (2000), `HP_MISSING_WEIGHT` (500), `THREAT_WEIGHT` (2.0).
- **Status buildup** — `STATUS_SCORE_WEIGHT` (0.3), tier scores `STATUS_SCORE_TRIGGER` (50), `STATUS_SCORE_CONTRIBUTE` (12), `STATUS_SCORE_REDUNDANT` (5); threat multipliers `RAW_DAMAGE_THREAT_MULT` (2.0), `STATUS_MULTIPLIER_THREAT_MULT` (1.5), `SPELL_POWER_THREAT_MULT` (2.0).
- **Thinking delays** — per-difficulty min/max ranges (`EASY_THINK_*` 2.0–3.5s … `EXPERT_THINK_*` 0.2–0.3s).
- **Defense rates** — per-difficulty `*_DEFENSE_ATTEMPT` (0.40–0.95) and `*_DEFENSE_ACCURACY` (0.50–0.98).
- **Survival thresholds** — `SURVIVAL_HP_THRESHOLD` (0.25), `ENERGY_CONSERVATION_THRESHOLD` (0.50), `ENERGY_ABUNDANT_THRESHOLD` (0.70).

## How It Works

### Turn decision flow

1. **`RequestDecision(AIActor)`** — stores `PendingActor`, computes a difficulty-based thinking delay via `CalculateThinkingDelay` (random within the difficulty's range), and schedules `ExecuteDecision` on `ThinkingTimerHandle`.
2. **`ExecuteDecision`** — captures the actor locally and clears `PendingActor` **before** submitting (submission can synchronously advance the turn). It validates that the actor is still the orchestrator's current actor — if the turn moved on while thinking, the decision is dropped. Otherwise it calls `BuildAction` and `CurrentCombat->SubmitAction`.
3. **`BuildAction`** — fetches `ULoadoutComponent` and `UCharacterDataComponent`, gets living enemies via `CurrentCombat->GetLivingEnemies`. With no enemies it defends. **Easy** difficulty picks a random action type and random target with simplified data population. **Medium/Hard/Expert** delegate to `BuildAction_Smart`.

### Smart decision branches (`BuildAction_Smart`)

Evaluated in priority order; the first that returns an action wins:

1. **Survival (`TrySurvivalBranch`)** — if HP% ≤ a difficulty-scaled threshold (0.4 for Hard/Expert, 0.25 otherwise): try a healing spell (affordability checked via `UActionExecutor::CalculateActionEnergyCost`), then a healing item (Sapphire), else Defend. Secondarily, if energy% is below `ENERGY_CONSERVATION_THRESHOLD`, use an energy item (Citrine).
2. **Cleanse (`TryCleanseBranch`)** — if `HasDangerousDebuff` is true, use a cleanse spell (affordability-checked) or a cleanse item (Iolite).
3. **Offensive (`BuildOffensiveAction`)** — see below.

### Offensive action (`BuildOffensiveAction`)

1. `SelectBestTarget` scores every living enemy (Easy = random target). `ScoreTarget` sums: `KILL_POTENTIAL_SCORE` if `CanKillTarget`, a missing-HP bonus (`(1 - HP%) * HP_MISSING_WEIGHT`), and `CalculateThreatLevel * THREAT_WEIGHT`. Threat itself weights crystal-modified raw damage, total status multiplier, and crystal-modified spell damage.
2. Available action types are gathered (Attack/Spell/Ability) and each scored:
   - **Attack** — best `FinalDamage` across all weapon attacks via `UDamageCalculator::CalculateAttackDamage`.
   - **Spell** — best `EstimateSpellDamage + EstimateStatusScore * STATUS_SCORE_WEIGHT` among **affordable** Destruction-school spells.
   - **Ability** — same combined score among affordable abilities.
3. The highest-scoring action type is chosen; the concrete spell/ability/attack is then re-selected by the same combined-score metric. If no affordable spell/ability exists, the action falls through to Defend.
4. For spells/abilities, `DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel` picks an infusion level (0/1/2); if the infused cost is unaffordable it drops to L0.

### Damage / status estimation

`EstimateSpellDamage` / `EstimateAbilityDamage` build a full `FAction`, call `UActionExecutor::ComputeActionStatModifiers` to fold in Reality/Evolution sources, then run `UDamageCalculator::CalculateDamage` with `bCanCrit=false` and fold expected crit back in (`1 + CritChance * (CRIT_MULTIPLIER - 1)`). L2 infusion multiplies by `InfusionConstants::CHARGE_L2_DAMAGE_MULT`. If subsystems are unavailable they fall back to the raw asset damage value.

`EstimateStatusScore` returns `STATUS_SCORE_REDUNDANT` if the target already has a dangerous debuff, `STATUS_SCORE_TRIGGER` if the hit would trigger the bar (`WouldTriggerStatusBar`) or the bar is near triggering (`IsStatusBarNearTrigger`), else `STATUS_SCORE_CONTRIBUTE`. Zero buildup scores 0.

### Infusion decisions

`DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel`: Easy never infuses. Otherwise — if L0 already kills, return 0; if L2 kills and energy is above `ENERGY_CONSERVATION_THRESHOLD`, return 2; Medium+ may return 1 when an L1 status buildup would trigger the bar (and L0 wouldn't) on a valuable status, and Hard+ may return 1 when the bar is >70% full and energy is above `ENERGY_ABUNDANT_THRESHOLD`. Low energy forces 0.

### Defense flow (`ScheduleDefenseDecision`)

1. Lazy-loads `UDefenseSystem`. Rolls against `GetDefenseAttemptChance(Difficulty)`; on failure the AI does not defend.
2. `ChooseDefenseType` picks a defense: a lethal hit (`BaseDamage >= CurrentHP`) that is dodgeable always returns `Dodge`. Otherwise — Easy always Blocks; Medium Blocks or Dodges (no Parry); Hard/Expert prefer Dodge, then Parry (70% chance Expert, 40% otherwise), then Block.
3. `CalculateDefenseReactionDelay` picks a delay as a fraction of the window (Easy late 70–90%, Expert early 10–30%) and schedules a per-actor timer.
4. When the timer fires it rolls against `GetDefenseAccuracy(Difficulty)`; on good timing it submits the input via `UDefenseSystem::SubmitDefenseInput` (Dodge picks a random `EDefenseDirection`); on a mistime nothing is submitted. The defense timer is then removed.

## Integration Points

### Delegates broadcast

`UAIDecisionManager` does **not** declare or broadcast any delegates. It is a consumer that drives other systems via direct calls.

### Subsystems / components it depends on

- `ACombatOrchestrator` — `GetCurrentActor`, `GetLivingEnemies`, `GetCombatDifficulty`, `SubmitAction`.
- `UActionExecutor` — `ComputeActionStatModifiers`, `CalculateActionEnergyCost` (energy-cost and stat-modifier source of truth).
- `UDamageCalculator` — `CalculateDamage`, `CalculateAttackDamage`, `GetCriticalChance`.
- `UDefenseSystem` — `CanDodgeAttack`, `SubmitDefenseInput`.
- `USkillEffectManager` — `GetActiveEffects`, `HasActiveDOT`, `GetDebuffCount` (for `HasDangerousDebuff` / `IsValuableStatus`).
- `UStatusBuildupManager` — `GetStatusBarPercent`, `GetBuildupToTrigger`, `GetPendingTrigger`.
- `ULoadoutComponent` — available spells/abilities/attacks and usable items.
- `UCharacterDataComponent` / `UCharacterData` — HP/EP, crystal-modified stats, status multiplier.
- `FTimerManager` (world) — thinking and defense reaction timers.

### Systems that depend on it

- `ACombatOrchestrator` (or combat setup) — registers itself via `SetCombatOrchestrator` and calls `RequestDecision` on AI turns.
- `UDefenseSystem` — calls `ScheduleDefenseDecision` when a defense window opens for an AI defender.

## Known Limitations / TODOs

- *(resolved sweep-2)* `Action.SpellSource` resolution — `BuildOffensiveAction`, `CanAffordSpell`, `EstimateSpellDamage`, `TrySurvivalBranch` (heal), and `TryCleanseBranch` now call `ULoadoutComponent::ResolveSpellSource(Spell)` at all six AI cost/damage-evaluation sites (`AIDecisionManager.cpp:691, 862, 1008, 1021, 1095, 1417`). AI casts now route through the correct cost model (`Innate` → full EP, `Evolution` → wear-as-cost with BD carve-outs, `RingCrystal`/`WeaponCrystal` → 0 EP). Probe-then-OutAction pairs in the survival/cleanse branches resolve source once and reuse so affordability and submission stay consistent.
- **Offensive spell filter is Destruction-only** — `BuildOffensiveAction` and its scoring loop skip any spell whose `School != ESpellSchool::Destruction`, so non-Destruction offensive spells are never used offensively.
- **Easy-difficulty action data is simplified** — the Easy branch picks random spells/abilities with no affordability or target-quality checks; it can pick actions the actor cannot pay for.
- **`IsValuableStatus` / pending-trigger default** — when `UStatusBuildupManager::GetPendingTrigger` returns `None`, infusion logic defaults the assumed status to `DOT`; the comment notes abilities apply physical status rather than a specific type, so the assumption is approximate.
- **No cached subsystem pointers** — consistent with the project rule (`GetSkillEffectManager` / `GetActionExecutor` fetch per call); `DefenseSystemRef` is the one cached reference and is re-fetched lazily if null.
- **L1 buildup multipliers are inline literals** — `DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel` use a hardcoded `* 1.5f` for L1 status buildup rather than a named `InfusionConstants` value.
- No `// FIXME` or `// HACK` markers were found in the source.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-2 — AI now resolves spell source via `ULoadoutComponent::ResolveSpellSource(Spell)` at all six previously-hardcoded `ESpellSource::Innate` sites in `AIDecisionManager`. Casts route through the correct cost model. | feature/integration-gaps-sweep-2 |
