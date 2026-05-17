# Skill Effect System

## Overview

The Skill Effect System is the central authority for buff/debuff/DOT tracking during turn-based combat in *World of Refraction*. It owns every runtime status effect on every combatant — damage-over-time ticks, stat modifiers, gate effects (Stun/Silence/HealBlock), passive-layer modifiers, immunities, equipment bonuses, and on-hit triggers.

Its core type is `USkillEffectManager`, a `UGameInstanceSubsystem`. Per the project architecture rules, immutable design-time effect definitions live in `UPrimaryDataAsset`-style assets as `FSkillEffect` structs, while mutable runtime state is held by the manager as `FActiveSkillEffect` instances.

Key design principles (from the class header):
- Durations tick on the **affected actor's turn**, not a global turn counter.
- Stacking rules are configurable per effect.
- Conditional effects integrate with `ESkillTrigger`.
- The system is intended to be server-authoritative for multiplayer (apply/heal/damage all route through `Server*` functions on `UCharacterDataComponent`).

## Architecture

### `USkillEffectManager` (`UGameInstanceSubsystem`)

Central service. Owns all effect state and exposes the application/removal/query API.

Important internal fields:
- `TMap<TWeakObjectPtr<AActor>, TArray<FActiveSkillEffect>> ActiveEffects` — all active effects, keyed per actor. Not a `UPROPERTY` because `TArray` nested in a `TMap` is not supported by reflection.
- `int32 NextInstanceID` — counter for `GenerateInstanceID()`, used to distinguish same-type effects.

Public-facing API groups (all `UFUNCTION(BlueprintCallable)` unless noted):
- **Application** — `ApplyEffect`, `ApplyEffects`, `ApplyInfusionDOT`, `ApplyEvolutionEffects`, `ApplyEquipmentEffects`, `ApplyWeaponBonuses`, `ApplyRingBonuses`, `ApplyPhysicalDamageEffect`, `ApplyWeaponInfusionDOT`.
- **Removal** — `RemoveEffectByID`, `RemoveEffectsByName`, `RemoveEffectsByType`, `RemoveAllBuffs`, `RemoveAllDebuffs`, `RemoveAllDOTs`, `RemoveAllEffects`, `ClearAllEffects`, `RemoveEffectsBySource`, `RemoveEquipmentEffects`, `RemoveWeaponBonuses`, `RemoveRingBonuses`.
- **Turn processing** — `ProcessStartOfTurnEffects`, `ProcessEndOfTurnEffects`, `ProcessTriggerEffects`.
- **Queries** — `GetActiveEffects`, `GetEffectsByType`, `HasEffectByID`, `HasEffectOfType`, `GetTotalStatModifier`, `GetEffectCount`, `GetBuffCount`, `GetDebuffCount`.
- **Status checks** — `IsStunned`, `IsSilenced`, `HasActiveDOT`, `IsImmuneToEffectType`.
- **Debug** — `DebugPrintEffects`, `DebugPrintAllEffects` (both `CallInEditor`), `GetEffectsSummary`.
- **Buildup bridge (non-`UFUNCTION`)** — `ApplyImmediateSkillEffect` and `ApplyTriggeredSkillEffect`. The latter is the single buildup → effect entry point: `UStatusBuildupManager::TriggerSkillEffectFromBuildup` calls it when a target's buildup bar caps. It is intentionally public so the separate buildup subsystem can call across the boundary.

Notable internal helpers: `ProcessEffectsWithTiming`, `TickDurations`, `ApplyEffectLogic` (the per-type effect resolver), `IsTriggerConditionMet` / `IsSingleTriggerMet` (compound trigger evaluation), `FindEffectByID`, `ResetTurnFlags`, `IsSpeedEffect`, `NotifySpeedChanged`.

`EEffectApplicationResult` enum reports how `ApplyEffect` resolved: `Applied`, `StackAdded`, `DurationRefreshed`, `Rejected`.

### `ESkillEffectType` (`UENUM(BlueprintType)`)

The unified, **element-agnostic** status type enum. Effects are generic mechanics; the element (`ESpellElement`) supplies the display name (e.g. `DOT` + `Fire` = "Burn"). Categories: core status-bar triggers (`DOT`, `SpeedDebuff`, `SkipTurn`, `BurstDamage`, …), pillar stat modifiers (`MindBuff`/`BodyBuff`/`SpiritBuff` + debuffs), Mind/Body/Spirit sub-stat buff/debuffs, legacy generic buffs, utility (`Heal`, `EnergyRestore`, …), special combat (`RetaliationDamage`, `SelfDamage`), debuff removal (`Cleanse`, `RemoveSpeedDebuff`, …), bar-cap gate effects (`Stun`, `HealBlock`, `Silenced`, `RandomSkill`), and the Phase 2 passive layer (`Modify*`, `Restore*Percent`, `Drain*`, reflect types, `Grant*Immunity`, `Apply*ToTarget`, `ExtraAction`, `GuaranteedCrit`, `IgnoreDefense`, `DoubleHit`, `Revive`). New values are **appended only** to preserve `.uasset` enum-by-value stamping.

### `FSkillEffect` (`USTRUCT(BlueprintType)`)

The **authored / design-time** definition of a single effect on an ability, spell, weapon, ring, or evolution crystal. Fields: `EffectType`, `Magnitude` (decimal percent), `Value` (flat amount — authors use one or the other, not both), `Duration` (0 = instant), `Target` (`ETargetType`), `EffectName`, plus a trigger block: source-side `Condition`/`ConditionThreshold`, optional `SecondaryCondition`/`SecondaryThreshold` joined by `bRequireBothConditions` (AND/OR), `TargetCondition`/`TargetThreshold`, and `DrainPercent` for OnHit restore effects. The `b*UsesThreshold` flags are auto-synced by `PostSerialize` (and by the owning UObject's `PostEditChangeChainProperty`) — not edited manually. Helpers: `IsBuff`, `IsDebuff`, `IsRestore`, `IsDrain`, `IsInstant`, `IsConditional`, `IsAlwaysActive`, `GetDescription`, etc.

### `FActiveSkillEffect` (`USTRUCT(BlueprintType)`)

The **runtime instance** applied to an actor. Holds identity (`EffectName`, `EffectID`, `Description`), timing (`ProcessTiming` of type `ESkillEffectTiming`, plus `TriggerCondition`/`SecondaryTriggerCondition`/`TargetTriggerCondition` with thresholds and `bRequireBothTriggers`, `bTriggerActive`), duration (`RemainingTurns`, `InitialDuration`, `bPermanent`), effect data (`EffectType`, `EffectValue`, `Element`), stacking (`bCanStack`, `CurrentStacks`, `MaxStacks`, `bRefreshDurationOnReapply`), source tracking (`SourceActor`, `SourceAbilityName`, `SourceTeamIndex`), and runtime flags (`bProcessedThisTurn`, `bPendingRemoval`).

Static factory methods build instances from upstream data:
- `CreateFromSpellEffect` — from spell data; auto-detects timing (DOT → `EndOfOwnTurn`, restores → `StartOfOwnTurn`) and promotes to `OnTrigger` when a non-`Always` source condition is supplied.
- `CreateFromSkillEffect` — from an authored `FSkillEffect`; used for evolution-crystal **and** equipment effects (shared type after the Phase 3 merge). `EffectID` is packed as `SourceID*100 + EffectIndex`.
- `CreateFromInfusion` / `CreateFromWeaponInfusion` — infusion DOTs (`AbilityID*10 + 5` / `+8` ID offsets).
- `CreateFromWeaponBonuses` / `CreateFromRingBonuses` — permanent stat bonuses (weapon IDs at `WeaponID*100 + 1..6`; ring IDs at `RingID*100 + 50 + 1..6` to avoid collision).
- `CreateFromPhysicalDamageType` — Slash → Bleed DOT, Pierce → Armor Break (`DefenseDebuff`), Impact → `Stun`.
- `CreateBuff`, `CreateDOT`, `CreatePersistent` — quick constructors.

Helpers: `IsBuff`, `IsDebuff`, `IsDOT`, `DealsDamage`, `GetStackedValue` (`EffectValue * CurrentStacks`), `CanAddStack`, `GetDurationString`, `GetStackString`. `operator==` compares by `EffectID`; `GetTypeHash` hashes `EffectID`.

## How It Works

### Effect application

1. A caller (`UActionExecutor`, equipment system, evolution system, or `UStatusBuildupManager`) invokes an `Apply*` function. Most route through `ApplyEffect`.
2. `ApplyEffect` rejects a null target, then checks `IsImmuneToEffectType`. `GrantAllStatusImmunity` short-circuits everything; `Stun`/`Silenced`/`DOT` are gated by their matching `Grant*Immunity`. (Per-element immunities are checked elsewhere — see Known Limitations.)
3. Source attribution is stamped onto the effect (`SourceActor`, `SourceAbilityName`, `SourceTeamIndex`), and `InitialDuration` is set from `RemainingTurns`.
4. The actor's effect array is fetched via `FindOrAdd`. `FindEffectByID` looks for an existing effect with the same `EffectID`:
   - If found and `bCanStack` + `CanAddStack()`: `CurrentStacks++`, optionally refresh duration, broadcast `OnEffectStacksChanged`, return `StackAdded`.
   - If found and `bRefreshDurationOnReapply`: reset `RemainingTurns`, broadcast `OnEffectDurationChanged`, return `DurationRefreshed`.
   - If found and neither applies: return `Rejected`.
5. Otherwise the effect is appended. If `ProcessTiming == Immediate`, `ApplyEffectLogic` runs immediately and (unless permanent) the effect is marked `bPendingRemoval`. `OnEffectApplied` is broadcast; speed effects also call `NotifySpeedChanged`.

### Turn processing

- **`ProcessStartOfTurnEffects(Actor)`**: resets per-turn flags (`ResetTurnFlags`), processes `StartOfOwnTurn` then `Persistent` timings via `ProcessEffectsWithTiming`, then runs `ProcessTriggerEffects` for `OnTurnStart`.
- **`ProcessEndOfTurnEffects(Actor)`**: processes `EndOfOwnTurn` timings (DOTs, energy drain), runs `ProcessTriggerEffects` for `OnTurnEnd`, then `TickDurations`, then sweeps out any effect with `bPendingRemoval`.
- **`ProcessEffectsWithTiming`**: for each effect matching the timing and not yet `bProcessedThisTurn`, calls `ApplyEffectLogic`, sets the flag, broadcasts `OnEffectTriggered`.
- **`TickDurations`**: decrements `RemainingTurns` for non-permanent, non-immediate effects, broadcasts `OnEffectDurationChanged`, and removes effects at 0 turns (broadcasting `OnEffectRemoved`). Speed-effect expiry triggers a single `NotifySpeedChanged`.
- **`ProcessTriggerEffects`**: for `OnTrigger`-timed effects whose `TriggerCondition` matches the event, evaluates `IsTriggerConditionMet` (handles compound primary + secondary triggers via `IsSingleTriggerMet`, which resolves HP/EP threshold checks; event-based triggers are considered met when the event fires). On a rising edge it sets `bTriggerActive` and runs `ApplyEffectLogic`.

### Effect resolution — `ApplyEffectLogic`

A large `switch` on `EffectType`, using `GetStackedValue()`:
- `DOT` — damage clamped so it can never kill (leaves ≥1 HP), via `CharComp->ServerTakeDamage`.
- `HealthRestore` / `EnergyRestore` / `EnergyDrain` — route through `ServerHeal` / `ServerGainEnergy` / `ServerSpendEnergy`.
- All stat-modifier types — **passive no-ops**; other systems query them via `GetTotalStatModifier`.
- `RemoveSpeedDebuff` / `RemoveDamageDebuff` / `RemoveDefenseDebuff` — call `RemoveEffectsByType`.
- `SelfDamage` — `ServerTakeDamage` (no kill clamp).
- Gate effects (`Stun`, `HealBlock`, `Silenced`) — no-op markers; enforcement lives elsewhere (`ActionExecutor::ValidateAction`, `CharacterDataComponent::ServerHeal`, EP-cost gates).
- Passive-layer (`Modify*`, reflect types, `Lifesteal`, `AbsorbDamage`, `Grant*Immunity`, `Apply*ToTarget`) — passive markers consumed by the damage pipeline / buildup manager / on-hit handler.
- `RestoreHPPercent` / `RestoreEnergyPercent` — `Value` is a percent of `MaxHP`/`MaxEP`.
- `DrainHP` (lethal, no clamp) / `DrainEnergy` (flat).
- `CleanseSelf` — `RemoveAllDebuffs(self)`; `CleanseAllies` — `RemoveAllDebuffs` on each ally via `UTurnManager::GetTeamMembers`.
- `ExtraAction` — calls `UTurnManager::RequestExtraTurn`.
- `RandomSkill`, `GuaranteedCrit`, `IgnoreDefense`, `DoubleHit`, `Revive` — currently log-only stubs (see Known Limitations).

### On-hit effects — `OnDamageDealtHandler`

Bound to `UActionExecutor::OnDamageDealt` at `Initialize`. When the attacker carries the relevant effect:
- `Lifesteal` — heals the attacker for a percent of damage dealt.
- `ApplyBurnToTarget` / `ApplyChillToTarget` — applies a Fire/Water `DOT` to the victim (magnitude from the effect, default 10, 3 turns).
- `ApplyStunToTarget` — on a **critical hit only**, applies a 1-turn `Stun` to the victim.

### Buildup-triggered effects

`ApplyTriggeredSkillEffect` is the bar-cap entry point: it builds a full-power `FActiveSkillEffect` per `StatusType` using bar-cap-design magnitudes (e.g. `DOT` = 8% of target MaxHP/tick, `BurstDamage` = 25% MaxHP one-shot clamped non-lethal, `Stun`/`Silenced`/`HealBlock`/`RandomSkill` gates) and applies it. `ApplyImmediateSkillEffect` is the weaker immediate-status variant.

## Integration Points

### Delegates broadcast (all `BlueprintAssignable`)

- `OnEffectApplied` (`FOnEffectApplied`) — `AActor* Target`, `const FActiveSkillEffect& Effect`.
- `OnEffectRemoved` (`FOnEffectRemoved`) — same signature.
- `OnEffectTriggered` (`FOnEffectTriggered`) — fired when an effect processes.
- `OnEffectStacksChanged` (`FOnEffectStacksChanged`) — adds `int32 NewStacks`.
- `OnEffectDurationChanged` (`FOnEffectDurationChanged`) — adds `int32 RemainingTurns`.

### Subsystems / components it depends on

- `UActionExecutor` — forced as an init dependency (`Collection.InitializeDependency`); the manager binds to its `OnDamageDealt` delegate.
- `UTurnManager` — `NotifySpeedChanged` calls `OnActorSpeedChanged`; `ExtraAction` calls `RequestExtraTurn`; `CleanseAllies` uses `GetActorTeam` / `GetTeamMembers`.
- `UCharacterDataComponent` — read for HP/EP state and all damage/heal/energy mutations (`ServerTakeDamage`, `ServerHeal`, `ServerGainEnergy`, `ServerSpendEnergy`).
- `SkillEffectDisplayNames` — element-aware display-name generation.

### Systems that depend on it

- `CombatOrchestrator` — drives `ProcessStartOfTurnEffects` / `ProcessEndOfTurnEffects`.
- `UActionExecutor` — calls `ApplyEffect` when spells/abilities hit.
- `UStatusBuildupManager` — calls `ApplyTriggeredSkillEffect` on bar cap, and queries `HasEffectOfType` for immunity gating.
- `UAIDecisionManager` — queries `GetActiveEffects`, `HasActiveDOT`, `GetDebuffCount` for survival/cleanse/status decisions.
- Equipment, ring, and evolution systems — call the corresponding `Apply*` / `Remove*` functions.
- UI — binds to the events for visual feedback; uses `GetEffectsSummary`.

## Known Limitations / TODOs

- **`RandomSkill` unimplemented** — `ApplyEffectLogic` logs a warning; full implementation (`LoadoutComponent` random pick + `ActionExecutor` forced-action injection) is flagged pending "Session Y/Z".
- **`Apply*ToTarget` Phase B hook pending** — `ApplyBurnToTarget`/`ApplyChillToTarget`/`ApplyStunToTarget` are described in `ApplyEffectLogic` as consumed by an `ActionExecutor` OnHit/OnCrit broadcast that is a "Phase B" pending missing-hooks pass; the working path today is the manager's own `OnDamageDealtHandler`.
- **`GuaranteedCrit` / `IgnoreDefense` / `DoubleHit` / `Revive`** — `ApplyEffectLogic` cases are log-only; bespoke handlers in other subsystems are implied but not present here.
- **Per-element immunities not handled in `IsImmuneToEffectType`** — `GrantFire/Water/...Immunity` are only enforced in `UStatusBuildupManager::AddStatusBuildup`, which has the incoming `Element` in scope. `ApplyEffect`'s immunity check only covers `Stun`/`Silenced`/`DOT` trigger-type matches.
- **DOT magnitude as percent** — `ApplyTriggeredSkillEffect`'s `DOT` case carries a `// TODO`: long-term, DOT processing should interpret `Value` as a percent of MaxHP directly; for now it resolves to flat damage at apply time for compatibility with `ApplyEffectLogic`'s int-damage handler.
- **`RandomDebuff` in `ApplyTriggeredSkillEffect`** — marked superseded by `RandomSkill` in the new bar-cap design; left wired until "Session Y" so legacy trigger mappings still resolve.
- **`ApplyEvolutionEffects` / `ApplyEquipmentEffects` ID collision risk** — IDs are packed `SourceID*100 + index`; the header explicitly warns callers to pass non-colliding `SourceID` ranges. Removal scans a fixed 100-slot window.
- **No cached subsystem pointers** — consistent with the project rule; subsystems are fetched per call.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
