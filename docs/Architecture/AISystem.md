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
- `GetCurrentDifficulty()` — `BlueprintPure`, reads difficulty from the orchestrator.

AI defense is **not** Blueprint-exposed: the per-impact entry `TrySynthesizeImpactDefense(...)` is a plain C++ method invoked by `UActionExecutor::ResolveImpactDefense` (see Defense flow below).

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
- `GetLethalDoTPerTick` — per-tick value of a **one-tick-lethal** DOT on a target (Emerald enemy-gate), else 0; `GetRescueExposureTurns` — target-team turn-slots before the target's next turn via `UTurnManager::PreviewTurnOrder` (Emerald exposure window).

#### Loadout detection helpers

`FindHealingSpell` (Restoration school + `Heal`/`HealthRestore` effect), `FindCleanseSpell` (`Cleanse` effect), `FindHealingItem` (Sapphire crystal), `FindCleanseItem` (Iolite crystal), `FindEnergyItem` (Citrine crystal), `FindBonusTurnItem` (Emerald — `GrantBonusTurn` item).

#### Defense logic

`ChooseDefenseType`, `GetDefenseAttemptChance`, `CalculateDefenseDelta`, and the per-impact entry `TrySynthesizeImpactDefense`.

#### Status-bar & infusion

`IsStatusBarNearTrigger`, `WouldTriggerStatusBar`, `IsValuableStatus`, `DecideSpellInfusionLevel`, `DecideAbilityInfusionLevel`.

**Infusion source + charge (6-5; Medium+ only — Easy never infuses).** The AI doesn't just pick a charge *level*; it picks a *source* and guards the cost:
- **Source selection** — spells via `DecideSpellInfusionSource` → `UActionExecutor::GetAllowedInfusionSourcesForSpell` (the 1:1 origin binding; first allowed, Evolution preferred for the BD/Reality two-source case; empty → don't infuse). Abilities (not origin-bound) via `DecideAbilityInfusionSource` heuristic: Caster → `Innate`, Resonator → `ActiveRing`, else first available crystal source, else `Raw`.
- **HP-affordability guard** — `ClampInfusionLevelForHP` gates the HP-paying sources (`Raw`, `Innate`-on-spell, `Evolution`) through `UInfusionCostHelper::WouldKill` on the exact pre-Efficiency infused EP, dropping L2 → L1 → L0 until survivable (prefers a weaker infusion over self-death). Crystal sources skip it (they pay durability, not HP).
- **Prediction parity** — `EstimateSpellDamage`/`EstimateAbilityDamage` and the level deciders call the real charge getters (`GetChargeDamageMultiplier`/`GetChargeStatusMultiplier` + `ResolveInfusionMode`), so the AI's mode-aware, stat-scaled estimates match execution. The old hand-rolled flat constants (`CHARGE_L2_DAMAGE_MULT`, `SPELL_L1_BUILDUP_MULT`) were removed; the double-apply in the deciders was fixed (estimator called at the target level, single application point).

See `InfusionSystem.md` for the full cost/effect/binding model.

### `AIDecisionConstants.h` — `namespace AIConstants`

All tunable values are `constexpr` in this namespace:
- **Target scoring** — `KILL_POTENTIAL_SCORE` (2000), `HP_MISSING_WEIGHT` (500), `THREAT_WEIGHT` (2.0).
- **Status buildup** — `STATUS_SCORE_WEIGHT` (0.3), tier scores `STATUS_SCORE_TRIGGER` (50), `STATUS_SCORE_CONTRIBUTE` (12), `STATUS_SCORE_REDUNDANT` (5); threat multipliers `RAW_DAMAGE_THREAT_MULT` (2.0), `STATUS_MULTIPLIER_THREAT_MULT` (1.5), `SPELL_POWER_THREAT_MULT` (2.0).
- **Thinking delays** — per-difficulty min/max ranges (`EASY_THINK_*` 2.0–3.5s … `EXPERT_THINK_*` 0.2–0.3s).
- **Defense rates** — per-difficulty `*_DEFENSE_ATTEMPT` (0.40–0.95) for the per-impact attempt roll, and `*_DELTA_BAND_MULT` (Easy 3.0 → Expert 0.35) governing aim-within-the-authored-band.
- **Survival thresholds** — `SURVIVAL_HP_THRESHOLD` (0.25), `ENERGY_CONSERVATION_THRESHOLD` (0.50), `ENERGY_ABUNDANT_THRESHOLD` (0.70).
- **Emerald valuation** — `KILL_SECURE_FACTOR` (1.0), `FREE_ACTION_FACTOR` (1.0), `EMERALD_EXPOSURE_LOOKAHEAD` (10), `STARVE_MARGIN` (1.3), `DELAY_DECAY` (0.15), `ESTIMATED_EP_REGEN_PER_TURN` (0 — self-target dormant; no passive EP regen exists).

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

### Emerald (bonus-turn item) valuation (`BuildOffensiveAction`, Medium+)

After `BestScore` (the best **affordable** action this turn, in HP-damage units) and `BestTarget` are both known — and only there, since both are needed — `BuildOffensiveAction` evaluates whether to instead spend the turn on an **Emerald** bonus-turn item. Easy never reaches here. `FindBonusTurnItem` locates a usable `GrantBonusTurn` (Emerald) item; `CrystalEffectTable::GetEmeraldBonusTurnDelay` gives its tier delay (F=6 … S=0). All Emerald scores are in the **same HP-damage units** as `BestScore`, so they slot directly into the `> BestScore` comparison — `KILL_POTENTIAL_SCORE` (target-selection scale) is deliberately **not** reused.

**Enemy-target (the gamble) — secure a one-tick-lethal DoT kill before the target's team can rescue it.** Gated on `GetLethalDoTPerTick(BestTarget) > 0` (a DoT whose **single next tick** alone ≥ target HP — the *one-tick-lethal* test, distinct from `HasDangerousDebuff`'s accumulated-lethal check, because Emerald forces only one extra turn = one extra tick) **AND** `!CanKillTarget(AIActor, BestTarget, BestScore)` (the AI can't already kill it this turn). Score: `TargetCurrentHP × KILL_SECURE_FACTOR + ThreatPerTurnHP × ExposureTurns − ThreatPerTurnHP × FREE_ACTION_FACTOR`, where `ThreatPerTurnHP = EstimateBestDamage(BestTarget, AIActor)` (the target's own best damage against the AI, natively in HP units) and `ExposureTurns = GetRescueExposureTurns(AIActor, BestTarget)` (target-team turn-slots before the target's next appearance in `UTurnManager::PreviewTurnOrder` — the window their team has to heal/cleanse the pending kill). If the score beats `BestScore`, emit an `Item` action (Emerald) at `BestTarget`. Only `BestTarget` is considered — a one-tick-lethal target is near-dead, so `HP_MISSING_WEIGHT` makes it the selected target in practice.

**Self-target (tempo) — DORMANT.** The intended trigger is EP-starvation: hold a high-value action the AI can't afford this turn (`UnaffordableBest`, tracked alongside `BestScore` in the scoring loop, with its cost) for a bonus turn after EP regenerates. Gate: `CurrentEP + ESTIMATED_EP_REGEN_PER_TURN × DelayTurns ≥ heldCost`, then `UnaffordableBest × (1/(1+DELAY_DECAY×DelayTurns)) − BestScore > BestScore`. **The combat model has no passive per-turn EP regen**, so `ESTIMATED_EP_REGEN_PER_TURN = 0` keeps this path inert — the machinery is wired and correct but never fires until a real EP-regen mechanic exists.

**Future — activating self-target.** The gate is `CurrentEP + ESTIMATED_EP_REGEN_PER_TURN × DelayTurns ≥ heldCost` (the AI must be able to afford the held action by the time the bonus turn fires). To activate, set `ESTIMATED_EP_REGEN_PER_TURN` > 0 — modelling the EP the caster reclaims per turn — **only once a real passive per-turn EP-regen mechanic lands**; otherwise the projection would be fiction. Tracked in `docs/TODO.md`.

### Damage / status estimation

`EstimateSpellDamage` / `EstimateAbilityDamage` build a full `FAction`, call `UActionExecutor::ComputeActionStatModifiers` to fold in Reality/Evolution sources, then run `UDamageCalculator::CalculateDamage` with `bCanCrit=false` and fold expected crit back in (`1 + CritChance * (CritMult - 1)`, where `CritMult = UDamageCalculator::GetCritDamageMultiplier(Attacker)` — the attacker's *actual* x1.0–x2.0 crit-damage multiplier; `AIDecisionManager.cpp:723,787`. The old fixed `CRIT_MULTIPLIER` constant was retired). L2 infusion multiplies by `InfusionConstants::CHARGE_L2_DAMAGE_MULT`. If subsystems are unavailable they fall back to the raw asset damage value.

`EstimateStatusScore` returns `STATUS_SCORE_REDUNDANT` if the target already has a dangerous debuff, `STATUS_SCORE_TRIGGER` if the hit would trigger the bar (`WouldTriggerStatusBar`) or the bar is near triggering (`IsStatusBarNearTrigger`), else `STATUS_SCORE_CONTRIBUTE`. Zero buildup scores 0.

### Infusion decisions

`DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel`: Easy never infuses. Otherwise — if L0 already kills, return 0; if L2 kills and energy is above `ENERGY_CONSERVATION_THRESHOLD`, return 2; Medium+ may return 1 when an L1 status buildup would trigger the bar (and L0 wouldn't) on a valuable status, and Hard+ may return 1 when the bar is >70% full and energy is above `ENERGY_ABUNDANT_THRESHOLD`. Low energy forces 0.

### Defense flow (`TrySynthesizeImpactDefense`)

AI defense is synthesized **per impact** — there is no window-level timer or scheduled decision. `UActionExecutor::ResolveImpactDefense` calls `TrySynthesizeImpactDefense(Defender, Attacker, AttackType, BaseDamage, AttackSize, ImpactDifficulty, ImpactTime)` once per impact for an AI defender; the same chokepoint covers melee, projectile, and AOE impacts.

1. **Attempt roll (per impact).** Rolls against `GetDefenseAttemptChance(Difficulty)`; on failure the AI eats that impact. Each impact is rolled independently — no single per-window decision survives across impacts.
2. `ChooseDefenseType` picks a defense. Dodge is always viable — the attack-size gate was removed, so the AI dodges on timing alone exactly like the player (`bCanDodge` is unconditionally true). A lethal hit (`BaseDamage >= CurrentHP`) always returns `Dodge`. Otherwise — Easy always Blocks; Medium Blocks or Dodges (no Parry); Hard/Expert prefer Dodge, then Parry (70% chance Expert, 40% otherwise), then Block. Block/Parry remain the fallbacks, so the AI is never stranded without a defense.
3. **Aim within the band.** `CalculateDefenseDelta(Difficulty, ChosenType, ImpactDifficulty, …)` computes how far from the impact the synthesized press lands. The band tier comes from the **impact** (keyed on the chosen press type) and is fed straight to `DefenseDifficultyMultiplier` — mirroring the matcher's `TypeTier` exactly (same `Inherit`/`None` → `Easy` ×1.0 terminal), so the AI aims at the same band the matcher judges. AI skill governs **only aim-within-band** via `*_DELTA_BAND_MULT` (Easy ~3× outside the band → usually whiffs early; Expert 0.35 → deep in the perfect band).
4. **Backdated submit.** The press is submitted at `InputTime = ImpactTime − Delta` via `UDefenseSystem::SubmitDefenseInput` (Dodge picks a random `EDefenseDirection`). It is then judged by the **same `MatchAndConsumeInput`** the player's input flows through — there is no separate AI accuracy roll. The AI never "schedules"; it backdates a synthesized press that the shared matcher accepts or rejects purely on timing.

> **Per-impact difficulty window (not authored by the AI).** The *acceptance* window the submitted input must land inside is not fixed — `UDefenseSystem` independently scales it per impact **and per defense type** by the attack's authored `EDefenseDifficulty` (`Inherit`/`Easy` ×1.0 / `Medium` / `Hard` / `Impossible`, via `DefenseDifficultyMultiplier`, `DefenseSystem.cpp:328-364`). On a harder impact the band the AI's reaction timing must hit is much tighter (Impossible floors at `IMPOSSIBLE_WINDOW_FLOOR`, below the normal `MINIMUM_DEFENSE_WINDOW`). The AI neither authors nor reads the tier; it reacts against whatever window `DefenseSystem` enforces.

## Integration Points

### Delegates broadcast

`UAIDecisionManager` does **not** declare or broadcast any delegates. It is a consumer that drives other systems via direct calls.

### Subsystems / components it depends on

- `ACombatOrchestrator` — `GetCurrentActor`, `GetLivingEnemies`, `GetCombatDifficulty`, `SubmitAction`.
- `UActionExecutor` — `ComputeActionStatModifiers`, `CalculateActionEnergyCost` (energy-cost and stat-modifier source of truth).
- `UDamageCalculator` — `CalculateDamage`, `CalculateAttackDamage`, `GetCriticalChance`.
- `UDefenseSystem` — `SubmitDefenseInput`. (The size-based `CanDodgeAttack` dependency was removed — dodge is timing-only.)
- `USkillEffectManager` — `GetActiveEffects`, `HasActiveDOT`, `GetDebuffCount` (for `HasDangerousDebuff` / `IsValuableStatus`).
- `UStatusBuildupManager` — `GetStatusBarPercent`, `GetBuildupToTrigger`, `GetPendingTrigger`.
- `ULoadoutComponent` — available spells/abilities/attacks and usable items.
- `UCharacterDataComponent` / `UCharacterData` — HP/EP, crystal-modified stats, status multiplier.
- `FTimerManager` (world) — thinking-delay timers. (Defense is timerless — AI defense is synthesized per impact and backdated, not scheduled.)

### Systems that depend on it

- `ACombatOrchestrator` (or combat setup) — registers itself via `SetCombatOrchestrator` and calls `RequestDecision` on AI turns.
- `UActionExecutor` — calls `TrySynthesizeImpactDefense` per impact (in `ResolveImpactDefense`) for AI defenders; the synthesized, backdated press is judged by the shared `UDefenseSystem` matcher.

## Known Limitations / TODOs

- *(resolved sweep-2)* `Action.SpellSource` resolution — `BuildOffensiveAction`, `CanAffordSpell`, `EstimateSpellDamage`, `TrySurvivalBranch` (heal), and `TryCleanseBranch` now call `ULoadoutComponent::ResolveSpellSource(Spell)` at all six AI cost/damage-evaluation sites (`AIDecisionManager.cpp:691, 862, 1008, 1021, 1095, 1417`). AI casts now route through the correct cost model (`Innate` → full EP, `Evolution` → wear-as-cost with BD carve-outs, `RingCrystal`/`WeaponCrystal` → 0 EP). Probe-then-OutAction pairs in the survival/cleanse branches resolve source once and reuse so affordability and submission stay consistent.
- **Offensive spell filter is Destruction-only** — `BuildOffensiveAction` and its scoring loop skip any spell whose `School != ESpellSchool::Destruction`, so non-Destruction offensive spells are never used offensively.
- **Easy-difficulty action data is simplified** — the Easy branch picks random spells/abilities with no affordability or target-quality checks; it can pick actions the actor cannot pay for.
- **`IsValuableStatus` / pending-trigger default** — when `UStatusBuildupManager::GetPendingTrigger` returns `None`, infusion logic defaults the assumed status to `DOT`; the comment notes abilities apply physical status rather than a specific type, so the assumption is approximate.
- **No cached subsystem pointers** — consistent with the project rule (`GetSkillEffectManager` / `GetActionExecutor` fetch per call); `DefenseSystemRef` is the one cached reference and is re-fetched lazily if null.
- **AI scores flat at L0** — candidates are ranked uninfused, then a source + charge level is chosen for the winner; the AI does not model (source × level) as distinct candidates. Deeper per-mode candidate fidelity is deferred (see `InfusionSystem.md`).
- No `// FIXME` or `// HACK` markers were found in the source.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-2 — AI now resolves spell source via `ULoadoutComponent::ResolveSpellSource(Spell)` at all six previously-hardcoded `ESpellSource::Innate` sites in `AIDecisionManager`. Casts route through the correct cost model. | feature/integration-gaps-sweep-2 |
| 2026-06-09 | Emerald (bonus-turn item) enemy-target valuation added to `BuildOffensiveAction` (Medium+): one-tick-lethal-DoT gate (`GetLethalDoTPerTick`) + not-already-killable, scored in HP units (`KILL_SECURE` + threat×exposure − threat×freeAction; threat = `EstimateBestDamage`, exposure = `GetRescueExposureTurns`/`PreviewTurnOrder`); `FindBonusTurnItem`. Self-target valuation wired but DORMANT (`ESTIMATED_EP_REGEN_PER_TURN=0` — no passive EP regen). | feature/weapon-stones |
| 2026-06-16 | Dodge is timing-only (Option B). The AI's attack-size dodge gate (`bCanDodge = CanDodgeAttack(Defender, AttackSize)`) was neutralized to `bCanDodge = true` so the AI dodges on timing like the player; `ChooseDefenseType` may freely pick Dodge (Block/Parry stay the fallbacks). `UDefenseSystem::CanDodgeAttack`/`GetDodgeThreshold`/`BaseDodgeThreshold` were deleted (fully dead after both the player and AI size-gates were removed). | feature/realtime-defense |
| 2026-06-16 | Doc-sync: documented the per-impact `EDefenseDifficulty` axis (`Easy`/`Medium`/`Hard`/`Impossible`) that `UDefenseSystem` applies to the defender's acceptance window per defense type (`DefenseDifficultyMultiplier`), which the AI reacts against but does not author. Fixed the crit-fold formula in §Damage/status estimation — the retired fixed `CRIT_MULTIPLIER` constant is now `UDamageCalculator::GetCritDamageMultiplier`. | feature/realtime-defense |
| 2026-06-18 | Infusion rework (6-5): documented AI source selection (`DecideSpellInfusionSource` via the 1:1 spell-origin binding; `DecideAbilityInfusionSource` heuristic), the `ClampInfusionLevelForHP` HP-affordability guard (`WouldKill`, drops level over self-death), and the mirror reconcile — estimators/deciders now call the real charge getters (mode-aware, stat-scaled), the double-apply fixed, and the legacy flat constants removed. Replaced the now-false "inline `* 1.5f` literals" limitation with the "AI scores flat at L0" deferral. | feature/realtime-defense | 
