# Gameplay Ability System (GAS) layer

## Overview

The game is moving stats, effects and skills onto Epic's Gameplay Ability System in four stages (`docs/Design/GASMigration.md`). Each stage keeps the game working. This doc covers what is built so far: **stage 1, tags** (B-040), and **stage 2, attributes** (B-041).

So far GAS **mirrors** the existing systems. The enum effects in `USkillEffectManager` and the stat getters on `UCharacterDataComponent` / `UDamageCalculator` are still the source of truth (Q-040, Q-041). The tags and attributes are proven equal to them by debug compares. Stage 3 (B-042) makes GameplayEffects write the attributes and tags, and the getters switch over then.

## Architecture

### Tags: `Tags/WoRGameplayTags.h` (`namespace WoRTags`)

- Native tags (`UE_DEFINE_GAMEPLAY_TAG`):
  - `Element.*` (11 values, including Generic = inherit and None = non-elemental)
  - `Class.*`
  - `Status.{Stun, Silence, HealBlock, DOT}`
  - `Immunity.{All, Stun, Silence, DOT, Element.<element>}`
  - These are owned-state tags. `Effect.Family.*` asset tags come with the GameplayEffects (Q-040b).
- Maps:
  - `ElementToTag` / `TagToElement`
  - `ClassToTag` / `TagToClass`
  - `EffectTypeToTag`, plus `TaggedEffectTypes()`. Stat modifiers have no tag yet.
- `USkillEffectManager::GetEffectTags(Actor)` builds an actor's Status/Immunity tags from its active effects.

### Attributes: `GAS/WoRAttributeSets.h`

- `UWoRAttributeSet_Vitals`:
  - Health, MaxHealth, Energy, MaxEnergy, Buildup, MaxBuildup.
  - No clamping: EP may overload above max for Broken Darkness.
- `UWoRAttributeSet_Stats`:
  - Mind, Body, Spirit (gear-modified).
  - The twelve composed sub-stats: Efficiency, SpellDamage, StatusMultiplier, CritDamage, SpellSpeed, Defense, ActionSpeed, RawDamage, Resistance, TurnSpeed (buffed), Luck, Reflex (window factor).
  - CritChance.
- All attributes are replicated with rep-notifies.

### Ability system component on `ACombatCharacter`

- `ACombatCharacter` implements `IAbilitySystemInterface` and owns a `UAbilitySystemComponent` with Mixed replication. Battle pawns are spawned per fight, so the pawn owns its ASC (GASMigration §1).
- Both attribute sets are default subobjects of `ACombatCharacter`, so the ASC finds the same replicated sets on every machine.
- `UWoRAttributeSyncComponent` is declared last, after every component it reads. Only the authority writes. On BeginPlay it:
  - adds the sets if the ASC lacks them (`PrepareAbilitySystem`; a fallback for actors that add an ASC at runtime, as the spec does),
  - sets the actor info,
  - mirrors `ReadSourceValues(Actor)`, the one list of attribute/getter pairs.
- It re-syncs on:
  - HP or EP change (`UCharacterDataComponent`)
  - an effect applied, removed or restacked on the owner (`USkillEffectManager`)
  - a buildup change (`UStatusBuildupManager`)
  - a loadout change
  - the owner's own turn start (`UTurnManager`), a safety net for changes that have no event.
- It unbinds the subsystems on EndPlay.

### Effects (stage 3, B-042 part 1)

- `GAS/WoRGameplayEffects.h`: native GameplayEffect classes, one per family. So far `UWoRGE_Stun` (Infinite, a `UTargetTagsGameplayEffectComponent` made in the constructor and added to `GEComponents`, grants `Status.Stun`). Nothing applies it in play yet.
- `GAS/WoRTurnEffectTracker.h`: `UWoRTurnEffectTracker`, a default subobject of `ACombatCharacter`. `ApplyForHolderTurns(Class, Turns, Instigator)` applies an Infinite GE through the owner's ASC and keeps an `FTurnSpan` per handle. `OnHolderTurnStart` removes finished GEs by handle (before payouts), `OnHolderTurnEnd` counts the turn. A GE removed elsewhere leaves the map through `OnAnyGameplayEffectRemovedDelegate`. `USkillEffectManager::ProcessStartOfTurnEffects` / `ProcessEndOfTurnEffects` call the hooks.
- Debug: `wor.DumpTurnEffects`, `UWoRTurnEffectTracker::GetDebugString` / `PrintState`, `showdebug abilitysystem`. Spec `WoR.GAS.TurnTracker`.

### Effects (stage 3, B-042b part 2)

- `GAS/WoREffectContext.h`: `FWoREffectIdentity` (type, element, EffectID, SourceKey, gear origin; `IsSameIdentity` = EffectID + SourceKey, like `FActiveSkillEffect`) inside `FWoREffectContext`. `UWoRAbilitySystemGlobals` allocates it for every context (`DefaultGame.ini`).
- Family classes on `UWoRGameplayEffect`: `UWoRGE_Stun` / `Silence` / `HealBlock` (granted `Status.*`), `UWoRGE_Immunity` (the `Immunity.*` tag per spec), `UWoRGE_StatModifier` (no attribute modifier; value in `SetByCaller.Magnitude`, Q-042b), `UWoRGE_DamageOverTime` (grants `Status.DOT`) / `UWoRGE_HealOverTime`, `UWoRGE_HealthTick` (Instant, Health += magnitude). Asset tags `Effect.Family.*`; gear adds `Effect.Origin.Gear`.
- `GAS/WoREffectSpecBuilder.h`: `GetEffectClass`, `Build` (spec + tick spec + turns), `Apply` (replaces the same identity; timed through the tracker, permanent straight to the ASC), `FindByIdentity`, `SumMagnitude` (= `GetTotalStatModifier`), `GetDebugString`.
- The tracker's `ApplySpecForHolderTurns` takes a tick spec, paid at each holder turn start the GE survives (Q-044c).

### Effect mirror and the UseGAS switch (stage 3, B-042c part 3)

- `GAS/WoREffectMirrorComponent.h` (on `ACombatCharacter`): one untracked, tick-less Infinite GE per stored enum-path effect of a ported type (`FWoREffectSpecBuilder::GetEffectClass`), built with `Build(..., bAlreadyStored = true)`. Resyncs on the effect manager's applied / removed / stacks / duration events for its owner by diffing identity, element, gear and stacked value; the enum path still owns lifetime, stacking and ticks.
- `wor.Effects.UseGAS` (default 0, Q-042c): at 1, `USkillEffectManager::HasEffectOfType` (and so `IsStunned` / `IsSilenced`), `GetTotalStatModifier` and `GetTotalStatModifierForElement` read the mirror for ported types on actors that carry one (`UWoREffectMirrorComponent::GetGASReadSource`). Tagged types read their tag.
- Debug: `UWoREffectDebug::GetEffectMismatchString` / `wor.CompareEffects`.
- A GAS read resyncs the actor's mirror first (turn order and the HP/EP pool read before or beside the mirror's events). The reads count every WoR GE on the ASC: GEs applied any other way (B-042d) would count twice until B-042h makes GAS the store.
- At UseGAS 1 a const effect-manager read can apply or remove GEs (the resync), so ASC listeners run inside it, sometimes while the manager walks an actor's effects; an ASC listener must not change the effect manager. Each read costs a full sync (fine for the PIE pass; B-042h drops it).

### Combat GameplayEvents (stage 3, B-042d part 4)

- `GAS/WoRCombatEvents.h`: `WoRCombatEvents::Send(Target, Event, Instigator, Magnitude)` hands an `FGameplayEventData` to the target's ASC (`HandleGameplayEvent`); no ASC, no event, no error. `wor.Events.Log 1` logs each.
- Sent by `USkillEffectManager` beside its dispatcher (Q-044j A): `Event.Combat.Hit` (target; attacker; damage), `Event.Combat.DefenseResolved` (defender; attacker; attack base cost), `Event.Combat.TurnStart` / `TurnEnd` (the actor). Nothing listens yet; B-043's passive abilities will.

### Every effect type has a family (stage 3, B-042k part 4b)

- `UWoRGE_Reflect`, `UWoRGE_LastStand`, `UWoRGE_OnHit`, `UWoRGE_AttackModifier`, `UWoRGE_Control`, `UWoRGE_Periodic`, `UWoRGE_Action`: Infinite, `Effect.Family.*` asset tag, no modifier or granted tag (Q-042k). `GetEffectClass` returns a class for every type but None, so the mirror copies every stored effect; the game code they run stays on the enum path until B-042h. At UseGAS 1 every read of these types now resyncs the mirror too, including hot paths (crit, Ignore Defense, Lifesteal, Absorb, Double Hit, AI scoring): fine for the PIE pass.

### Damage execution (stage 3, B-042e part 5)

- `GAS/WoRDamageExecution.h`: `UWoRDamageExecution` runs `WoRDamageMath::Compute` (B-219) over an `FWoRDamageRequest` (`UDamageCalculator::GatherDamageMathInputs` + override / ignore-resistance flags) carried on `FWoREffectContext::DamageRequest` (authority only, not net serialized). `ReadAttributes` replaces the attacker's CritChance (unless overridden) and CritDamage, and the defender's Defense and `Resist_<type>` (+ Ice for Water), from the ASCs. Output: Health -= the final damage. `UWoRGE_Damage` (Instant) carries it; `MakeRequest` / `MakeDamageSpec` build one hit; `GetRequestString` (Q-042e).
- Not applied in play: `UDamageCalculator` stays the damage path until B-042h.

### Buildup execution (stage 3, B-042l part 5b)

- `Skills/Effects/BuildupMath.h`: `WoRBuildupMath::Compute` over `FBuildupMathInputs` (amount, source StatusMultiplier, element-keyed multiplier, Broken Darkness stack factor, the three resistance layers, clamped). `UStatusBuildupManager::GatherBuildupMathInputs` fills it; `AddStatusBuildup` runs gather + Compute between the immunity gates and the bar.
- `GAS/WoRBuildupExecution.h`: `UWoRBuildupExecution` runs the core over an `FWoRBuildupRequest` on `FWoREffectContext::BuildupRequest` (authority only). `ReadAttributes` takes the source's StatusMultiplier (with a source, not baked in) and the target's StatusResistance + `Resist_<element type>` (+ physical) + StatusResistBonus. Output: Buildup += the result. `UWoRGE_Buildup` (Instant); `MakeRequest` / `MakeBuildupSpec` / `GetRequestString` (Q-042l).
- `UWoRAttributeSyncComponent::ReadSyncedAttribute`: the read gate both executions share (set present, owner has the sync and character data).
- Not applied in play; the bar, cap and decay move with B-042o.

## Integration points

- Delegates it listens to:
  - `UCharacterDataComponent::OnHPChanged`, `OnEPChanged`
  - `USkillEffectManager::OnEffectApplied`, `OnEffectRemoved`, `OnEffectStacksChanged`
  - `UStatusBuildupManager::OnStatusBuildupChanged`
  - `ULoadoutComponent::OnLoadoutChanged`
  - `UTurnManager::OnTurnStarted`
- `USkillEffectManager::RemoveAllEffects` / `ClearAllEffects` empty the store **before** broadcasting `OnEffectRemoved`, like every single-effect removal. Listeners re-read the target's effects.

## Debug

- `UWoRTagDebug`:
  - `wor.DumpTags`: every enum value and its tag, with a round-trip check.
  - `wor.CompareTags`: each combatant's tag queries against the enum queries.
- `UWoRAttributeDebug`:
  - `wor.DumpAttributes`
  - `wor.CompareAttributes`: each attribute against the getter it mirrors; it names any that drifted.
- Specs: `WoR.Tags`, `WoR.GAS.Attributes`.

## Known limitations / TODOs

- **Mirror only.** Nothing gameplay-facing reads the tags or attributes yet. The read sites (Q-040) and the getters and HP/EP writes (Q-041) switch in stage 3.
- MaxBuildup mirrors the fixed bar cap (`CombatConstants::STATUS_EFFECT_THRESHOLD`).
- A stat change with no event (e.g. weather, a crystal breaking mid-turn) reaches the attributes at that character's next turn start. `wor.CompareAttributes` shows such drift in between.
- Bare test actors (and anything that isn't an `ACombatCharacter`) have no ASC unless one is added, as `WoR.GAS.Attributes` does.
- Per-AI Minimal replication mode waits for co-op.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | B-040: native tags, enum to tag maps, `GetEffectTags`, `wor.DumpTags` / `wor.CompareTags`. | feature/B-040-gas-tags |
| 2026-10-26 | B-041: attribute sets, ASC + `UWoRAttributeSyncComponent` on `ACombatCharacter`, `wor.DumpAttributes` / `wor.CompareAttributes`; effect clears broadcast after removal. | feature/B-041-gas-attributes |
| 2026-10-27 | B-042 (stage 3, part 1): native `UWoRGE_Stun`; `UWoRTurnEffectTracker` on `ACombatCharacter`, driven by the effect manager's turn hooks; `wor.DumpTurnEffects`. The rest of stage 3 is B-042b-g. | feature/B-042-gas-turn-tracker |
| 2026-10-27 | B-042b (stage 3, part 2): `FWoREffectContext` + globals class; family GEs; `FWoREffectSpecBuilder`; tracker tick specs. Not routed in play yet (B-042c). | feature/B-042b-gas-effect-builder |
| 2026-10-27 | B-042c (stage 3, part 3): `UWoREffectMirrorComponent`; `wor.Effects.UseGAS` facade reads; `wor.CompareEffects`; `Build` takes `bAlreadyStored`. Write side moved to B-042h (Q-042c). | feature/B-042c-gas-effect-mirror |
| 2026-10-27 | B-042d (stage 3, part 4, narrowed): `WoRCombatEvents::Send`; `Event.Combat.*` sent from the effect manager. Rest split into B-042i, B-042j, B-042k. | feature/B-042d-gas-combat-events |
| 2026-10-27 | B-042k (stage 3, part 4b): seven role families; every effect type mirrored. | feature/B-042k-gas-remaining-families |
| 2026-10-27 | B-042e (stage 3, part 5, narrowed): `UWoRDamageExecution` + `UWoRGE_Damage`; rest split into B-042l, B-042m, B-042n. | feature/B-042e-gas-damage-execution |
| 2026-10-27 | B-042l (stage 3, part 5b, narrowed): `WoRBuildupMath`, `UWoRBuildupExecution` + `UWoRGE_Buildup`, `ReadSyncedAttribute`; bar / cap / decay split into B-042o. | feature/B-042l-gas-buildup-execution |
