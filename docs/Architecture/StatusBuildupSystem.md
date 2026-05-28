# Status Buildup System

## Overview

The Status Buildup System owns the per-actor "status bar" in combat. Buildup accrues from attacks (spell, ability, and weapon hits); when an actor's bar reaches the cap, an appropriate skill effect (Stun, Silenced, DOT, etc.) fires. It is implemented as a `UGameInstanceSubsystem` (`UStatusBuildupManager`).

The system was split out of `USkillEffectManager` in 2026-05. `USkillEffectManager` retains effect tracking (durations, stacks, removal, immediate/triggered application); `UStatusBuildupManager` owns only the bar. The two communicate via a cached pointer:

```
Buildup bar fills → UStatusBuildupManager::TriggerSkillEffectFromBuildup
                  → USkillEffectManager::ApplyTriggeredSkillEffect (lands the effect)
```

## Architecture

### `UStatusBuildupManager` (`UGameInstanceSubsystem`)

Lifecycle: `Initialize` empties `StatusBarStates` and logs; `Deinitialize` empties state and clears `EffectManagerRef`.

State:
- `StatusBarStates` — `TMap<TWeakObjectPtr<AActor>, FStatusBarState>`, one entry per actor with an active bar.
- `EffectManagerRef` (`USkillEffectManager*`) — lazy-acquired cached cross-subsystem reference.

### `FStatusBarState` (private struct)

Per-actor bar state:
- `CurrentBuildup` (`float`) — current accumulated buildup.
- `PendingElement` (`ESpellElement`) — element of the most recent hit (used for UI bar tinting).
- `PendingPhysicalType` (`EPhysicalDamageType`) — physical damage type of the most recent hit.
- `LastSource` (`TWeakObjectPtr<AActor>`) — actor that last added buildup.
- `TurnsSinceLastHit` (`int32`) — decay counter, reset to 0 on every hit.

### `FOnStatusBuildupChanged` delegate

`DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams` with `(AActor* Target, float CurrentBuildup, float MaxBuildup, ESpellElement PendingElement)`. Exposed as the `BlueprintAssignable` property `OnStatusBuildupChanged`. The `PendingElement` param lets UI re-tint the bar per hit.

## How It Works

### Adding buildup — `AddStatusBuildup(Source, Target, Amount, Element, PhysicalType)`

1. Return `false` if `Target` is null or `Amount <= 0`.
2. Resolve the trigger this hit would fire via `BarCapTriggerResolver::ResolveTrigger(Element, PhysicalType)` (Element wins over PhysicalType when non-Generic).
3. **Immunity gating** (buildup never accrues for immune targets — returns `false`, short-circuiting amplification/resistance/bar update):
   - `GrantAllStatusImmunity` — blocks any buildup.
   - Per-trigger immunity — `GrantStunImmunity` when resolved trigger is `Stun`, `GrantSilenceImmunity` when `Silenced`, `GrantDOTImmunity` when `DOT`.
   - Per-element immunity — `GetElementImmunityType(Element)` maps the incoming element to its `GrantXxxImmunity` flag; if the target holds it, buildup is absorbed regardless of resolved trigger.
4. Get or create the `FStatusBarState` via `FindOrAdd`.
5. **Attacker amplification.** If `Source` has a `UCharacterDataComponent` + `CharacterData`: `Amount *= 1 + (ModifiedSpirit × TotalPoints × STATUS_MULTIPLIER_PER_POINT) + (BonusPoints × STATUS_MULTIPLIER_PER_POINT)`, where `ModifiedSpirit` is `GetEvolutionModifiedSpirit`, `TotalPoints` is `CharacterData->GetTotalStatusMultiplier`, and `BonusPoints` is `BonusStatusMultiplier` from the source's `ULoadoutComponent`. This inlines the `UCharacterData::CalculateStatusMultiplier` formula shape (kept in sync manually).
5b. **Skill-effect StatusMultiplier aggregation (sweep-3).** Via `USkillEffectManager::GetTotalStatModifier` for both `StatusMultiplierBuff` and `StatusMultiplierDebuff`, then `Amount *= max(0, 1 + (SmBuff − SmDebuff) / 100)`. Element-agnostic — `StatusMultiplier` is caster-output amplification, not per-element. Mirrors the damage-side `DamageBuff`/`Debuff` math in `DamageCalculator::GetStatusEffectDamageModifier` (`:521-523`). Sits between attacker-stat amplification and target resistance — same layering as damage.
6. **Target resistance reduction.** If `Target` has a `UCharacterDataComponent` + `CharacterData`: start with `CharacterData->CalculateResistance()`, add `BonusResistance × RESISTANCE_PER_POINT` from the target loadout, add `GetTotalElementResistance(Target, Element)`, add `ModifyStatusResist/100` from skill effects, clamp to `[0, RESISTANCE_MAX]`, then `Amount *= (1 - Resistance)`.
7. Update bar state — most recent hit wins on `PendingElement` / `PendingPhysicalType`; set `LastSource`; reset `TurnsSinceLastHit` to 0.
8. Add `Amount` to `CurrentBuildup`; broadcast `OnStatusBuildupChanged` (with `MaxBuildup = STATUS_EFFECT_THRESHOLD`).
9. **Cap check.** If `CurrentBuildup >= STATUS_EFFECT_THRESHOLD`:
   - If the resolved trigger is not `None`: call `TriggerSkillEffectFromBuildup`, then `ResetStatusBar`, return `true`.
   - If the resolved trigger is `None`: the bar is held at/above cap (logged Verbose) and waits for a hit with a real trigger — prevents a "phantom cap" where the bar fills and empties with nothing visible. Returns `false`.

### Resistance query — `GetTotalElementResistance(Target, Element)`

Pulls `ResistanceBuff` and `ResistanceDebuff` effects from `USkillEffectManager::GetEffectsByType`. Sums `GetStackedValue()` only for effects whose `Element` matches the queried element (mismatched elements contribute zero). Returns `(BuffSum − DebuffSum) / STAT_PERCENT_DIVISOR`. Although the effects are stored in `USkillEffectManager`, this query lives here because element resistance is a buildup-side concept (it reduces buildup, not damage).

### Reducing buildup — `ReduceStatusBuildup(Target, Fraction)` and `ReduceStatusBuildupByAmount(Target, Amount)`

Two reduction surfaces with different shapes:

- `ReduceStatusBuildup(Target, Fraction)` — fraction-based (0–1) reduction; `CurrentBuildup *= (1 - Fraction)`. Used by Quartz items.
- `ReduceStatusBuildupByAmount(Target, Amount)` *(sweep-4)* — flat absolute subtraction; `CurrentBuildup = max(0, CurrentBuildup - Amount)`. **No** amplification or resistance modulation — pure subtraction clamped at 0. This is the apply-path for the sweep-4 `StatusDecrease` effect type (parallel to `AddStatusBuildup` being the apply-path for `StatusIncrease`).

Both broadcast `OnStatusBuildupChanged` with the current `PendingElement` so UI keeps its tint, and both are no-ops when the target has no active bar state.

### Decay — `ProcessStatusBarDecay(Target)`

Called at turn start. No-op if no state or `CurrentBuildup <= 0`. Increments `TurnsSinceLastHit`. If it reaches `STATUS_DECAY_FULL_RESET_TURNS`, the bar is fully reset. Otherwise `CurrentBuildup *= (1 - STATUS_DECAY_RATE)` and `OnStatusBuildupChanged` is broadcast (element passed through so UI keeps the current tint).

### Reset — `ResetStatusBar(Target)`

Zeroes `CurrentBuildup`, sets `PendingElement` to `Generic`, `PendingPhysicalType` to `None`, clears `LastSource`, resets `TurnsSinceLastHit`, and broadcasts `OnStatusBuildupChanged` with zeroed values.

### Triggering — `TriggerSkillEffectFromBuildup(Source, Target, StatusType, Element)`

No-op if `Target` null or `StatusType` is `None`. Resolves `USkillEffectManager`; if unavailable, logs a Warning and returns. Otherwise calls `EffectMgr->ApplyTriggeredSkillEffect(Source, Target, StatusType, Element)`.

### Queries

- `GetStatusBarPercent` — `CurrentBuildup / STATUS_EFFECT_THRESHOLD`, clamped `[0,1]`.
- `GetStatusBarBuildup` — raw `CurrentBuildup` (0 if no state).
- `GetBuildupToTrigger` — `max(0, STATUS_EFFECT_THRESHOLD − CurrentBuildup)`.
- `GetPendingTrigger` — resolved live via `BarCapTriggerResolver::ResolveTrigger` from `PendingElement` + `PendingPhysicalType` (no cached trigger state).
- `GetPendingElement` — `PendingElement` from state (`Generic` if none).

## Integration Points

### Delegates broadcast
- `OnStatusBuildupChanged` (`FOnStatusBuildupChanged`) — fired by `AddStatusBuildup`, `ResetStatusBar`, and `ProcessStatusBarDecay`. Consumed by UI for bar fill and tint.

### Subsystems / components / utilities it depends on
- `USkillEffectManager` (`UGameInstanceSubsystem`) — via lazy-cached `EffectManagerRef` (`GetEffectManager`, `const_cast` pattern matching `UActionExecutor::GetSkillEffectManager`). Uses `HasEffectOfType`, `GetEffectsByType`, `GetTotalStatModifier`, `ApplyTriggeredSkillEffect`.
- `BarCapTriggerResolver` — static `ResolveTrigger(Element, PhysicalType)` mapping (Lightning/Impact → Stun, Darkness → Silenced, Fire/Slash → DOT, etc.).
- `UCharacterDataComponent` / `UCharacterData` — `GetEvolutionModifiedSpirit`, `GetTotalStatusMultiplier`, `CalculateResistance`.
- `ULoadoutComponent` — `GetActiveStatBonus` returning `FEquipmentStatBonus` (`BonusStatusMultiplier`, `BonusResistance`).
- `FActiveSkillEffect` — `Element` field and `GetStackedValue()` used in resistance aggregation.
- `CombatConstants` — `STATUS_EFFECT_THRESHOLD`, `STATUS_MULTIPLIER_PER_POINT`, `RESISTANCE_PER_POINT`, `RESISTANCE_MAX`, `STAT_PERCENT_DIVISOR`, `STATUS_DECAY_RATE`, `STATUS_DECAY_FULL_RESET_TURNS`.

### Systems that depend on it
- Combat callers that add buildup on hit (the damage/action pipeline) invoke `AddStatusBuildup`. Those call sites are outside the two files reviewed and were not traced here.
- The turn system invokes `ProcessStatusBarDecay` at turn start.
- UI widgets bind `OnStatusBuildupChanged` for bar display.

## Known Limitations / TODOs

- No `// TODO`, `// FIXME`, `// HACK`, or deprecated markers are present in either file.
- **Manually-synced formula.** The attacker amplification in `AddStatusBuildup` inlines the `UCharacterData::CalculateStatusMultiplier` shape. The comment explicitly warns that if that asset-side formula changes, this copy must be updated too — a known maintenance hazard.
- **Held-cap state.** When the bar reaches cap but the resolved trigger is `None` (e.g. a `Generic`-element, `None`-physical-type hit), the bar stays at/above cap indefinitely until a hit with a real trigger consumes it. This is intentional ("phantom cap" prevention) but means `CurrentBuildup` can exceed `STATUS_EFFECT_THRESHOLD`; `GetStatusBarPercent` clamps the displayed value.
- The buildup amount itself is computed by the caller and passed in; this manager only amplifies/resists/accumulates it. (`UDamageCalculator::CalculateStatusBuildup` was removed in sweep-3 — see `DamageCalculator.md`.)

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-3 — added `StatusMultiplierBuff`/`Debuff` skill-effect aggregation as step 5b in `AddStatusBuildup`, sitting between attacker-stat amplification and target resistance. Sweep-4 — added `ReduceStatusBuildupByAmount` (flat subtraction; apply-path for the new `StatusDecrease` effect type). | feature/integration-gaps-sweep-3, feature/integration-gaps-sweep-4 |
