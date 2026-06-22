# Skill Effect System

## Overview

The Skill Effect System is the central authority for buff/debuff/DOT tracking during turn-based combat in *World of Refraction*. It owns every runtime status effect on every combatant — damage-over-time ticks, stat modifiers, gate effects (Stun/Silence/HealBlock), passive-layer modifiers, immunities, equipment bonuses, on-hit triggers, and (this branch) **conditional effects that fire on defense outcomes at impact**.

Its core type is `USkillEffectManager`, a `UGameInstanceSubsystem`. Per the project architecture rules, immutable design-time effect definitions live in `UPrimaryDataAsset`-style assets (now `UEffectDefinition` bundles of `FSkillEffect` structs), while mutable runtime state is held by the manager as `FActiveSkillEffect` instances.

Key design principles:
- Durations tick on the **affected actor's turn**, not a global turn counter.
- Stacking rules are configurable per effect.
- An effect's **identity is its authoring definition** — the same `UEffectDefinition` referenced from two sources resolves to the same `EffectID` and *merges* on a target rather than double-applying.
- Conditional effects integrate with `ESkillTrigger`; defense-outcome triggers dispatch from `UDefenseSystem::OnDefenseResolved`.
- The system is server-authoritative for multiplayer (apply/heal/damage all route through `Server*` functions on `UCharacterDataComponent`).

> **2026-06 reshape.** This branch (`feature/dynamic-skill-effects`) replaced the legacy flat `FSkillEffect` (two fixed conditions + one payload + scalar fields) with a **dynamic** shape (`Conditions[]` + `Payloads[]`), moved authoring into **reference-only `UEffectDefinition` bundles**, introduced **definition-identity merge**, and added the **defender / conditional trigger** path. The legacy flat fields, `MigrateLegacyToNew`, and the migration parity guard were deleted — the dynamic shape is the sole end state. See the [Changelog](#changelog).

## Architecture

### `USkillEffectManager` (`UGameInstanceSubsystem`)

Central service. Owns all effect state and exposes the application/removal/query API.

Important internal fields:
- `TMap<TWeakObjectPtr<AActor>, TArray<FActiveSkillEffect>> ActiveEffects` — all active effects, keyed per actor. Not a `UPROPERTY` (`TArray` nested in a `TMap` is not reflection-supported).
- `TSet<int32> FiredOnceThisMatch` — `EffectID`s that have fired this combat (backs `bFiresOncePerMatch`); cleared by `ResetForNewCombat`.
- `TMap<TWeakObjectPtr<AActor>, TArray<FGatheredEffect>> ArmedConditionals` — gear **conditional** effects armed per actor at combat start; read by the defender-trigger handler. Cleared by `ResetForNewCombat`.
- `int32 NextInstanceID` — counter for `GenerateInstanceID()`.

Public-facing API groups (all `UFUNCTION(BlueprintCallable)` unless noted):
- **Application** — `ApplyEffect`, `ApplyEffects`, `ApplyInfusionDOT`, `ApplyEquipmentEffects`, `ApplyPhysicalDamageEffect`, `ApplyWeaponInfusionDOT`.
- **Removal** — `RemoveEffectByID`, `RemoveEffectsByName`, `RemoveEffectsByType`, `RemoveAllBuffs`, `RemoveAllDebuffs`, `RemoveAllDOTs`, `RemoveAllEffects`, `ClearAllEffects`, `RemoveEffectsBySource`.
- **Combat lifecycle** — `ResetForNewCombat` (clears fires-once + armed conditionals), `ArmConditionalEffects(Actor, Conditionals)` (stores a per-actor armed set at combat start).
- **Turn processing** — `ProcessStartOfTurnEffects`, `ProcessEndOfTurnEffects`, `ProcessTriggerEffects`.
- **Queries** — `GetActiveEffects`, `GetEffectsByType`, `HasEffectByID`, `HasEffectOfType`, `GetTotalStatModifier`, `GetEffectCount`, `GetBuffCount`, `GetDebuffCount`.
- **Status checks** — `IsStunned`, `IsSilenced`, `HasActiveDOT`, `IsImmuneToEffectType`.
- **Event handlers (`UFUNCTION`)** — `OnDamageDealtHandler` (bound to `UActionExecutor::OnDamageDealt`), `OnDefenseResolvedHandler` (bound to `UDefenseSystem::OnDefenseResolved`).
- **Debug** — `DebugPrintEffects`, `DebugPrintAllEffects` (both `CallInEditor`), `GetEffectsSummary`, `WOR_StartingEffects` (`Exec`).
- **Buildup bridge (non-`UFUNCTION`)** — `ApplyTriggeredSkillEffect`, the single buildup → effect entry point: `UStatusBuildupManager::TriggerSkillEffectFromBuildup` calls it when a target's buildup bar caps. Public so the separate buildup subsystem can call across the boundary.

Notable internal helpers: `ProcessEffectsWithTiming`, `TickDurations`, `ApplyEffectLogic` (the per-type effect resolver), `IsTriggerConditionMet` / `IsSingleTriggerMet` (condition evaluation), `ResolveSubjectActors` (subject → actor set), `FindEffectByID`, `ResetTurnFlags`, `IsSpeedEffect`, `NotifySpeedChanged`.

`EEffectApplicationResult` reports how `ApplyEffect` resolved: `Applied`, `StackAdded`, `DurationRefreshed`, `Rejected`.

### The authoring type layer

Effects are authored as a tree: a **`UEffectDefinition`** bundle holds **`FSkillEffect`**s; each `FSkillEffect` holds a **`Conditions[]`** group (`FSkillCondition`) and a **`Payloads[]`** list (`FSkillEffectPayload`). Sources (weapons/rings/evolutions/skills) don't author effects inline — they **reference** definitions.

#### `UEffectDefinition` (`UPrimaryDataAsset`)

A named, authored effect **bundle**. Fields:
- `FText DisplayName` — UI / shop / authoring label.
- `TArray<FSkillEffect> Effects` — the bundled effects (new shape). An item referencing this bundle gets *every* effect in the list.
- `int32 Price` — shop price (ready for the later shop phase; harmless now).

`IsDataValid` (editor) is the **sole stable-ID guard**: a bundle's effects index `0..N-1` in this def's `DefID*100` window, so it caps **≤10 effects per definition** (`EFFECT_ID_SUBBAND_MAX + 1`) and **≤9 payloads per effect** (`LoadoutConstants::MAX_PAYLOADS`). Exceeding either is a validation error — that is the only place packed IDs could collide. `LogEffect` (`CallInEditor`) dumps each effect's `Conditions[]`/`Payloads[]`.

Sources reference bundles via `TArray<TObjectPtr<UEffectDefinition>> ReferencedEffects` (on `UEquipmentDataBase`, `UEvolutionItemData`, `USkillDataBase`). Inline `Effects[]` was **deleted** from all owners — bundles are the sole source.

#### `FSkillEffect` (`USTRUCT(BlueprintType)`)

One authored effect. Dynamic shape:
- `FString EffectName` — display name.
- `TArray<FSkillCondition> Conditions` — the condition group (gate). **Empty == `Always`** (unconditional). Single source of truth for gating.
- `TArray<FSkillEffectPayload> Payloads` — one effect may carry several payloads; each payload is one applied effect.
- Stacking (per-effect, shared across payloads): `bStackable`, `MaxStacks` (1–99), `bFiresOncePerMatch`.

Classifiers read the arrays directly: `IsValid` (any payload typed), `IsBuff`/`IsDebuff` (any payload classifies so, via `SkillEffectClassification`), `IsRestore`, `IsDrain` (a restore payload with `DrainPercent>0` **and** an owner-side `OnHit` condition), `IsInstant` (all payloads `Duration==0`), `IsConditional` (non-empty group), `HasTargetCondition` (any target-side entry), `IsConditionalEffect` (any target-side entry **or** any non-`Always` trigger — its complement is a *starting* effect). `GetDescription`/`DescribePayload` render the shape for debug/UI. `PostSerialize` → `SyncThresholdFlags` keeps each condition's `bUsesThreshold` live.

#### `FSkillCondition` (`USTRUCT(BlueprintType)`)

One entry in a condition group:
- `ESkillTrigger Trigger` — when satisfied (default `Always`).
- `float Threshold` (0–100) — for threshold triggers; editor-gated by `bUsesThreshold`.
- `ECondCombine Combine` — `And` / `Or`, joining this entry with the **previous** entry in the group.
- `ECondSubject Subject` — which actor(s) this evaluates against (default `Self`).

#### `FSkillEffectPayload` (`USTRUCT(BlueprintType)`)

The "what happens" half:
- `ESkillEffectType EffectType` (default `None`).
- `float Magnitude` (decimal percent, e.g. `0.2` = 20%) **or** `int32 Value` (flat) — author one, not both.
- `int32 Duration` (turns; 0 = instant).
- `ETargetType Target` (Self/Ally/Enemy/Anyone) + `ETargetCount TargetCount` (Single/Double/All) — **per-payload** targeting.
- `float DrainPercent` (0–1) — for drain payloads.

#### `ECondCombine` / `ECondSubject` (`UENUM(BlueprintType)`)

- `ECondCombine` — `And`, `Or`.
- `ECondSubject` — `Self`, `SelfTeam` (any ally), `Target`, `TargetTeam` (any enemy). `Self`/`SelfTeam` are the **owner's** side; `Target`/`TargetTeam` are the owner's **target's** side. **Team scopes fire if ANY member meets the condition.** This is the universal "whose state" axis for *all* condition types (it replaced the old `bTargetSide` bool).

#### `FGatheredEffect` (`USTRUCT`)

The carrier produced by the gather accessors: `{ int32 DefID, int32 BundleIndex, FSkillEffect Effect }`. `DefID` is the owning `UEffectDefinition`'s `GetUniqueID()` (its `*100` ID window); `BundleIndex` is the effect's position within the def's `Effects[]`. These two feed `PackEffectID` so the same definition yields the same `EffectID` regardless of which source referenced it.

### `ESkillEffectType` (`UENUM(BlueprintType)`)

The unified, **element-agnostic** status type enum (unchanged this branch). Effects are generic mechanics; the element (`ESpellElement`) supplies the display name (`DOT` + `Fire` = "Burn"). Categories: core status-bar triggers, pillar/sub-stat buff-debuff pairs, utility (`HealthRestore`, `EnergyRestore`, …), special combat (`SelfDamage`), debuff removal (`CleanseSelf`/`CleanseAllies`, `RemoveSpeedDebuff`, …), bar-cap gate effects (`Stun`, `HealBlock`, `Silenced`, `RandomSkill`), the Phase 2 passive layer (`Modify*`, `Restore*Percent`, `Drain*`, reflect types, `Grant*Immunity`, `Apply*ToTarget`, `ExtraAction`, `GuaranteedCrit`, `IgnoreDefense`, `DoubleHit`, `LastStand` (formerly `Revive`, now live — see below)), and the sweep-4 gauge manipulators (`StatusIncrease`/`StatusDecrease`). New values are **appended only** to preserve `.uasset` enum-by-value stamping.

### `ESkillTrigger` (`UENUM(BlueprintType)`)

Trigger conditions. This branch **appended** the defense-outcome triggers: `OnParry`, `OnPerfectParry`, `OnPerfectBlock`, `OnPerfectDodge` (`OnBlock`/`OnDodge` already existed), plus `OnTakeDamage`. **Superset semantics** — a perfect outcome fires both its perfect trigger **and** the base (an effect authored `OnParry` fires on *any* parry; `OnPerfectParry` only on a perfect one). There is deliberately **no "miss" trigger**. Mapping lives in `SkillTriggerUtils::DefenseOutcomeToTriggers`.

### `FActiveSkillEffect` (`USTRUCT(BlueprintType)`)

The **runtime instance** applied to an actor. Holds identity (`EffectName`, `EffectID`), timing (`ProcessTiming` of type `ESkillEffectTiming`; legacy `TriggerCondition`/… fields default-construct — the runtime carries `Conditions[]` instead), duration (`RemainingTurns`, `InitialDuration`, `bPermanent`), effect data (`EffectType`, `EffectValue`, `Element`), the carried condition group (`Conditions`), stacking (`bCanStack`, `CurrentStacks`, `MaxStacks`, `bRefreshDurationOnReapply`, `bFiresOncePerMatch`), source tracking (`SourceActor`, `SourceAbilityName`, `SourceTeamIndex`), and runtime flags (`bProcessedThisTurn`, `bPendingRemoval`, `bTriggerActive`).

Static factories:
- `CreateAllFromSkillEffect(SourceName, SourceID, Source, EffectIndex)` — builds **one runtime per payload** of an authored `FSkillEffect`. Each is packed `EffectIdentity::PackEffectID(SourceID, EffectIndex, SubIndex=payloadIndex)`, carries `Source.Conditions`, the stacking flags, and is promoted to `OnTrigger` timing when any owner-side condition is non-trivial. Used by equipment/evolution starting effects (`ApplyEquipmentEffects`).
- `CreateFromSpellEffect(...)` — builds **one** runtime for a single payload; auto-detects timing (DOT → `EndOfOwnTurn`, restores → `StartOfOwnTurn`). The caller carries `Conditions[]`/stacking. Used by the cast apply loop *and* the defender-trigger fire.
- `CreateFromSkillEffect` — deprecated single-payload wrapper (returns the first runtime of `CreateAllFromSkillEffect`); kept only for not-yet-repointed callers.
- `CreateFromInfusion` / `CreateFromWeaponInfusion` / `CreateFromPhysicalDamageType` — Space-B synthetic factories (`base*10 + offset` IDs, a deliberately separate ID space from the def-identity Space-A).
- `CreateBuff`, `CreateDOT`, `CreatePersistent` — quick constructors.

Helpers: `IsBuff`/`IsDebuff` (delegate to the single-source `SkillEffectClassification`), `IsDOT`, `GetStackedValue` (`EffectValue * CurrentStacks`), `CanAddStack`, `GetDurationString`, `GetStackString`. `operator==` / `GetTypeHash` key on `EffectID`.

## How it works

### Definition identity & the ID scheme

`EffectIdentity::PackEffectID(SourceID, EffectIndex, SubIndex) = SourceID*100 + EffectIndex + SubIndex*10` (`EffectIdentity.h`) is the single owner of the **Space-A** packing scheme: a per-source 100-slot window with the effect index in the ones place and the payload index in the tens place.

- **Equipment / evolution / cast** all pack `PackEffectID(DefID, BundleIndex, PayloadIndex)`.
- Because `DefID` is the `UEffectDefinition`'s identity, the **same bundle referenced by two sources** (weapon + ring, spell + weapon) produces the **same `EffectID`** for the same effect+payload. On a given target that means `ApplyEffect`'s `FindEffectByID` finds the existing instance and **merges** — refresh duration if non-stacking, add a stack if `bStackable` (the effect's own flag governs) — instead of stacking a duplicate. Identity is **per-Target**.
- This dissolved the old equipment **actor-window aggregate** collision (where effects were keyed on `Actor->GetUniqueID()*100 + i`).
- Caps (≤10 effects, ≤9 payloads) are enforced in `UEffectDefinition::IsDataValid` — the only collision surface.
- **Space B** (`CreateFromInfusion`/`WeaponInfusion`/`PhysicalDamageType`, `base*10 + offset`) is a separate ID space. The cast physical-DOT path now packs by def-identity (Space-A `PayloadEffectID`), discarding the factory's `WeaponID*10+7` re-pack that previously double-packed and risked a Space-B collision.
- *id-overflow NOTE:* `SourceID*100` overflows `int32` only above ~21.47M; `SourceID` is `GetUniqueID()` (live UObject-slot index, far below that). Documented, unreachable — not live debt.

### Gather → apply flow

1. **Gather.** Sources expose def-identity accessors returning `TArray<FGatheredEffect>`:
   - Equipment/evolution: `GetStartingEffectsGathered()` (non-conditional subset) and `GetConditionalEffectsGathered()`.
   - Skills: `GetAllEffectsGathered()`.
   - `ULoadoutComponent` aggregates per character class: `GetActiveEffectsGathered(Actor)` (starting) and `GetActiveConditionalEffectsGathered(Actor)` (conditional), mirroring `GetActiveStatBonus` coverage + the innate primary-slot evolution (an evolution *attached* to a weapon/ring contributes nothing).
2. **Apply (starting, combat start).** `ApplyEquipmentEffects(Target, Effects)` runs each `FGatheredEffect` through `CreateAllFromSkillEffect(EffectName, DefID, Effect, BundleIndex)` and `ApplyEffect`s every produced payload — def-identity packed, so duplicates from another source merge.
3. **Apply (cast).** `UActionExecutor::ApplySkillEffects` loops per effect → per payload, resolves targets per payload via `GetEffectTargets`, packs `PayloadEffectID`, builds one runtime via `CreateFromSpellEffect`, carries `Conditions`/stacking, and `ApplyEffect`s.
4. **Apply (defender triggers).** See [Conditional / defender triggers](#conditional--defender-triggers) below.

### Effect application — `ApplyEffect`

1. Reject a null target; check `IsImmuneToEffectType`.
2. **Fires-once gate** — if `bFiresOncePerMatch` and `EffectID ∈ FiredOnceThisMatch`, reject *before* any stacking/refresh.
3. Stamp source attribution; set `InitialDuration` from `RemainingTurns`.
4. `FindEffectByID` on the target's array:
   - found + `bCanStack` + `CanAddStack()` → `CurrentStacks++`, optional refresh, broadcast `OnEffectStacksChanged`, return `StackAdded`.
   - found + `bRefreshDurationOnReapply` → reset `RemainingTurns`, broadcast `OnEffectDurationChanged`, return `DurationRefreshed`.
   - found + neither → `Rejected`.
5. Otherwise append. `Immediate` timing runs `ApplyEffectLogic` now (and marks `bPendingRemoval` unless permanent). Broadcast `OnEffectApplied`; speed effects call `NotifySpeedChanged`. On a fresh apply, record `bFiresOncePerMatch` into `FiredOnceThisMatch` (exactly once — the gate above blocks re-entry).

### Condition evaluation

`SkillTriggerUtils` classifies triggers: `IsThresholdTrigger` (HP/EP %), `IsDefenseOutcomeTrigger` (the 7 impact-driven values), `DefenseOutcomeToTriggers` (outcome → trigger set, superset rule).

`EvaluateConditionGroup(Conditions, Participates, IsMet)` (`FSkillCondition.h`) is the shared two-predicate AND/OR fold reused by every eval site:
- `Participates(C)` false skips the entry entirely (a `continue` — distinct from `IsMet` returning false, which still folds into the AND/OR sets).
- Result: all participating `And` entries met **and** (no participating `Or` entries, or ≥1 met). Empty / all-skipped == true.
- Each call site supplies its own predicates: source-state (cast), action-result (`ActionExecutor`), or defense-outcome (defender path).

`ResolveSubjectActors(Subject, Owner, Target)` maps an `ECondSubject` to the actor(s) it evaluates against: `Self`→{Owner}, `Target`→{Target}, `SelfTeam`/`TargetTeam`→that side's `UTurnManager::GetTeamMembers`. State checks **ANY-fold** over the set (a one-element set reduces to the prior single-actor check). `IsTriggerConditionMet` prefers the `Conditions[]` group when present, falling back to the legacy two-field logic only for synthetic Space-B effects that carry no group.

### Conditional / defender triggers

Conditional effects are a **gear** concept (skills fire all their effects on cast). The flow:

1. **Arm.** At combat start, `ACombatOrchestrator` calls `SkillEffectManager->ResetForNewCombat()` then, per actor, `ArmConditionalEffects(Actor, Loadout->GetActiveConditionalEffectsGathered(Actor))`. The armed sets sit inert in `ArmedConditionals` until an outcome fires them.
2. **Dispatch.** `OnDefenseResolvedHandler` is bound to `UDefenseSystem::OnDefenseResolved(Defender, Attacker, DefenseType, bPerfect, ImpactIndex)`. It maps the outcome to its trigger set via `DefenseOutcomeToTriggers`, then evaluates from **both perspectives** — `{Owner=Defender, Target=Attacker}` ("I parried") and `{Owner=Attacker, Target=Defender}` ("my target dodged me").
3. **Guard.** For each armed effect, a **defense-trigger guard** skips any group that contains no `IsDefenseOutcomeTrigger` condition — so a pure-threshold conditional never leaks into the impact path.
4. **Evaluate.** `EvaluateConditionGroup` runs with: `Participates = IsOwnerSide(Subject) || Target != nullptr`; `IsMet` dispatches — a defense-outcome condition requires `OutcomeTriggers.Contains(Trigger)` **and** that `ResolveSubjectActors(Subject, Owner, Target)` contains the actual `Defender` (so the outcome's perspective resolves correctly per owner); other (threshold) conditions ANY-fold `IsSingleTriggerMet` over the subject's actors.
5. **Fire (per payload).** On a match, for each payload: resolve application targets via `UActionExecutor::GetEffectTargets(Owner, {Target}, P.Target, P.TargetCount, OwnerTeam, …)`, pack `PayloadEffectID = PackEffectID(DefID, BundleIndex, PayloadIndex)`, build **one** runtime via `CreateFromSpellEffect`, carry the authored stacking/fires-once flags, and `ApplyEffect`. Building one runtime per payload (not `CreateAllFromSkillEffect`, which expands *all* payloads) avoids an N² double-apply inside the per-payload loop.
   - The defense-outcome conditions are the **gate** (already passed) and are deliberately **not** copied onto the runtime, so the consequence ticks on its natural timing (DOT end-of-turn, etc.) rather than being promoted to an inert `OnTrigger` effect.
   - Element stays `Generic` (no cast context — parity with equipment effects).
   - Def-identity is preserved: a defender-fired effect from the same bundle as a cast/gear effect merges and shares the fires-once gate.

### Turn processing

- **`ProcessStartOfTurnEffects`** — `ResetTurnFlags`, process `StartOfOwnTurn` then `Persistent` timings, then `ProcessTriggerEffects(OnTurnStart)`.
- **`ProcessEndOfTurnEffects`** — process `EndOfOwnTurn` timings (DOTs, drain), `ProcessTriggerEffects(OnTurnEnd)`, `TickDurations`, then sweep `bPendingRemoval`.
- **`ProcessEffectsWithTiming`** — per matching, not-yet-processed effect: `ApplyEffectLogic`, set flag, broadcast `OnEffectTriggered`.
- **`TickDurations`** — decrement non-permanent/non-immediate effects, broadcast `OnEffectDurationChanged`, remove at 0 (broadcast `OnEffectRemoved`); speed expiry triggers one `NotifySpeedChanged`.
- **`ProcessTriggerEffects`** — for `OnTrigger`-timed effects whose `TriggerCondition` matches the event, evaluate `IsTriggerConditionMet`; on a rising edge set `bTriggerActive` and run `ApplyEffectLogic`.

### Effect resolution — `ApplyEffectLogic`

A large `switch` on `EffectType` using `GetStackedValue()` (unchanged this branch):
- `DOT` — full tick damage via `ServerTakeDamage`; **lethal** (leave-≥1-HP clamp removed, `a9bb3e8c`). Optional per-tick status buildup (`BuildupPerTick`) routes through `UStatusBuildupManager`.
- `HealthRestore` / `EnergyRestore` / `EnergyDrain` — `ServerHeal` / `ServerGainEnergy` / `ServerSpendEnergy`.
- All stat-modifier types — **passive no-ops**; queried via `GetTotalStatModifier`.
- `RemoveSpeedDebuff` / `RemoveDamageDebuff` / `RemoveDefenseDebuff` — `RemoveEffectsByType`.
- `SelfDamage`, `DrainHP` (lethal), `DrainEnergy` — direct mutations.
- Gate effects (`Stun`, `HealBlock`, `Silenced`) — no-op markers; enforcement lives elsewhere (`ActionExecutor::ValidateAction`, `ServerHeal`, EP-cost gates).
- Passive-layer (`Modify*`, reflect, `Lifesteal`, `AbsorbDamage`, `Grant*Immunity`, `Apply*ToTarget`) — passive markers consumed by the damage pipeline / buildup manager / on-hit handler.
- `RestoreHPPercent` / `RestoreEnergyPercent` — percent of `MaxHP`/`MaxEP`.
- `CleanseSelf` / `CleanseAllies` — `RemoveAllDebuffs` on self / each ally.
- `ExtraAction` — `UTurnManager::RequestExtraTurn`.
- `RandomSkill`, `GuaranteedCrit`, `IgnoreDefense`, `DoubleHit` — log-only stubs (see Known Limitations). `LastStand` (formerly `Revive`) is **live** — death-denial via `USkillEffectManager::ConsumeLastStandCharge` (best-of-N, restores HP%, consumes a charge).
- `StatusIncrease` / `StatusDecrease` — instant gauge build/drain via `UStatusBuildupManager` (`AddStatusBuildup` / `ReduceStatusBuildupByAmount`). Element/value handling per the tables below.

#### `StatusIncrease` / `StatusDecrease` element & value

`Effect.Element` for these two is set by `ActionExecutor::ApplySkillEffects` from the resolved cast element (four-branch: Spell/Ability × infused/uninfused; abilities/attacks uninfused → `Generic`). Every other effect type stays `Generic` in the loop. Runtime `Value`: gauge manipulators + DOT pass authored `Value`/`P.Value` through; every other type uses the `Magnitude × 100` percentage shape.

### On-hit effects — `OnDamageDealtHandler`

Bound to `UActionExecutor::OnDamageDealt`. When the attacker carries the effect: `Lifesteal` (heal % of damage), `ApplyBurnToTarget`/`ApplyChillToTarget` (Fire/Water DOT on the victim, default 10, 3 turns), `ApplyStunToTarget` (1-turn Stun on a **critical** hit only).

### Buildup-triggered effects

`ApplyTriggeredSkillEffect` is the bar-cap entry point: it builds a full-power `FActiveSkillEffect` per `StatusType` using bar-cap magnitudes and applies it. `BurstDamage` and bar-cap `DOT` are lethal (1-HP clamp removed, `a9bb3e8c`).

## Integration Points

### Delegates broadcast (all `BlueprintAssignable`)

- `OnEffectApplied`, `OnEffectRemoved`, `OnEffectTriggered` — `AActor* Target`, `const FActiveSkillEffect& Effect`.
- `OnEffectStacksChanged` — adds `int32 NewStacks`.
- `OnEffectDurationChanged` — adds `int32 RemainingTurns`.

### Subsystems / components it depends on

- `UActionExecutor` — forced init dependency; binds to its `OnDamageDealt`; **reuses its (now public) `GetEffectTargets`** for per-payload target resolution on the defender-trigger fire.
- `UDefenseSystem` — forced init dependency; binds to its `OnDefenseResolved` to dispatch defender triggers at impact.
- `UTurnManager` — team queries (`GetActorTeam`/`GetTeamMembers`) for subject resolution + `CleanseAllies`; `NotifySpeedChanged`; `RequestExtraTurn`.
- `UCharacterDataComponent` — HP/EP state and all `Server*` mutations.
- `SkillEffectDisplayNames` — element-aware display names.

### Systems that depend on it

- `CombatOrchestrator` — drives `ProcessStart/EndOfTurnEffects`; at combat start calls `ResetForNewCombat` + `ArmConditionalEffects`; applies starting effects via `ApplyEquipmentEffects`.
- `UActionExecutor` — calls `ApplyEffect` / `ApplySkillEffects` when spells/abilities hit.
- `UStatusBuildupManager` — calls `ApplyTriggeredSkillEffect` on bar cap; queries `HasEffectOfType` for immunity gating.
- `UAIDecisionManager` — queries effect state for survival/cleanse decisions.
- `ULoadoutComponent` — supplies gathered effects (`GetActiveEffectsGathered` / `GetActiveConditionalEffectsGathered`).
- UI — binds events; uses `GetEffectsSummary`.

### File map (this branch's additions)

- `Public/Skills/Effects/EffectDefinition.h` + `Private/.../EffectDefinition.cpp` — `UEffectDefinition` bundle + `IsDataValid` caps.
- `Public/Skills/Effects/EffectIdentity.h` — `PackEffectID` + parity `static_assert`s.
- `Public/Skills/Effects/FSkillCondition.h` — `FSkillCondition`, `IsOwnerSide`/`IsTargetSide`, `EvaluateConditionGroup`.
- `Public/Skills/Effects/FSkillEffectPayload.h` — `FSkillEffectPayload`.
- `Public/Skills/Effects/FGatheredEffect.h` — `FGatheredEffect`.
- `Public/Skills/Effects/ECondCombine.h`, `ECondSubject.h` — the condition enums.
- `Public/Skills/Effects/SkillTriggerUtils.h` — `IsThresholdTrigger`, `IsDefenseOutcomeTrigger`, `DefenseOutcomeToTriggers`.
- `Public/Skills/Effects/SkillEffectDebug.h` + `.cpp` — effect-shape debug dumps.

## Known Limitations / TODOs

- **`RandomSkill` unimplemented** — `ApplyEffectLogic` logs a warning; full implementation pending.
- **`Apply*ToTarget` Phase B hook** — the working path today is the manager's own `OnDamageDealtHandler`; the broader `ActionExecutor` OnHit/OnCrit broadcast is still pending.
- **`GuaranteedCrit` / `IgnoreDefense` / `DoubleHit`** — `ApplyEffectLogic` cases are log-only. (`LastStand`, formerly `Revive`, is **live** — see `USkillEffectManager::ConsumeLastStandCharge`.)
- **Per-element immunities** — `Grant<Element>Immunity` is enforced only in `UStatusBuildupManager::AddStatusBuildup` (which has `Element` in scope); `ApplyEffect`'s immunity check covers only `Stun`/`Silenced`/`DOT`.
- **Defender-trigger element is `Generic`** — defense-fired effects have no cast context, so DOT/status payloads fired this way don't inherit an element (acceptable for the current gear authoring).
- **Attacker-perspective is `Target`-subject only** — "react to how my target defended" works via `ECondSubject::Target`/`TargetTeam`; there is no separate attacker-side trigger and no accuracy-miss trigger (flagged future axes).
- **Cast-time target-state conditions** — "cast fires only if target below X%" is not supported at the cast site (the cast loop is action-result-driven; target-side *state* eval is the impact path's job).
- **id-overflow NOTE** — documented, unreachable (see ID scheme).

## Starting effects (gear, combat start)

A **starting effect** is a gear-authored `FSkillEffect` with no trigger condition — the complement of `FSkillEffect::IsConditionalEffect()`. At combat start, `ACombatOrchestrator` applies each combatant's `ULoadoutComponent::GetActiveEffectsGathered` (the starting subset) via `ApplyEquipmentEffects`. They land through the normal `ApplyEffect` path as ordinary clearable effects (wiped by `ClearAllEffects` at combat end). **Conditional** gear effects are *not* auto-applied — they are armed (`ArmConditionalEffects`) and await their defense-outcome triggers. No mid-combat re-application: gear swapped after combat start neither adds nor removes effects.

Coverage mirrors `GetActiveStatBonus` per class plus the innate (primary-slot) evolution; an evolution *attached* to a weapon/ring contributes no effects. Debug: `WOR_StartingEffects` (`Exec`) prints the per-source PRE list with a coverage cross-check and the POST contents.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-4 — `StatusIncrease`/`StatusDecrease` gauge manipulators; four-branch resolved cast element; absolute `Value` for these two | feature/integration-gaps-sweep-4 |
| 2026-06-11 | Starting effects: combat-start gear application filtered to non-conditional effects; coverage mirrors `GetActiveStatBonus` + innate evolution | feature/starting-effects |
| 2026-06-16 | Doc-sync: transient sub-stat effect types; single-source `SkillEffectClassification`; `RandomDebuff` classification | feature/realtime-defense |
| 2026-06-20 | **Dynamic effect reshape** — `FSkillEffect` → `Conditions[]` (`FSkillCondition`, per-entry AND/OR via `ECondCombine`, subject via `ECondSubject`) + `Payloads[]` (`FSkillEffectPayload`); added `EvaluateConditionGroup`, stable `EffectID`s, authored `bStackable`/`MaxStacks`/`bFiresOncePerMatch`; legacy flat fields + `MigrateLegacyToNew` + parity guard deleted. | feature/dynamic-skill-effects |
| 2026-06-20 | **Reference-only authoring** — effects authored once in `UEffectDefinition` bundles, referenced via `ReferencedEffects`; inline `Effects[]` deleted from all owners; gather via `Get*EffectsGathered()` → `FGatheredEffect`. | feature/dynamic-skill-effects |
| 2026-06-20 | **Definition-identity merge** — `EffectIdentity::PackEffectID(DefID, BundleIndex, PayloadIndex)`; same bundle from two sources → same `EffectID` → merges via `FindEffectByID`; per-Target; equipment actor-window collision dissolved; caps ≤10 effects / ≤9 payloads in `UEffectDefinition::IsDataValid`. | feature/dynamic-skill-effects |
| 2026-06-20 | **Cleanups** — cast physical-DOT packs by def-identity (was double-packed `WeaponID*10+7`); dead window helpers removed; id-overflow downgraded to a documented NOTE. | feature/dynamic-skill-effects |
| 2026-06-20 | **Defender / conditional triggers** — appended defense-outcome `ESkillTrigger`s (superset rule); `bTargetSide` → `ECondSubject`; `ResolveSubjectActors` + team ANY-fold; gear conditionals armed at combat start (`ArmedConditionals`) and fired by `OnDefenseResolvedHandler` (binds `UDefenseSystem::OnDefenseResolved`); defense-trigger guard; per-payload fire via `GetEffectTargets` + `CreateFromSpellEffect` + `ApplyEffect` (def-identity). | feature/dynamic-skill-effects |
