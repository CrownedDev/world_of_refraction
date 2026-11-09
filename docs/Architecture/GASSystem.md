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
- Debug: `wor.Effects.PrintTurns`, `UWoRTurnEffectTracker::GetDebugString` / `PrintState`, `showdebug abilitysystem`. Spec `WoR.GAS.TurnTracker`.

### Effects (stage 3, B-042b part 2)

- `GAS/WoREffectContext.h`: `FWoREffectIdentity` (type, element, EffectID, SourceKey, gear origin; `IsSameIdentity` = EffectID + SourceKey, like `FActiveSkillEffect`) inside `FWoREffectContext`. `UWoRAbilitySystemGlobals` allocates it for every context (`DefaultGame.ini`).
- Family classes on `UWoRGameplayEffect`: `UWoRGE_Stun` / `Silence` / `HealBlock` (granted `Status.*`), `UWoRGE_Immunity` (the `Immunity.*` tag per spec), `UWoRGE_StatModifier` (no attribute modifier; value in `SetByCaller.Magnitude`, Q-042b), `UWoRGE_DamageOverTime` (grants `Status.DOT`) / `UWoRGE_HealOverTime`, `UWoRGE_HealthTick` (Instant, Health += magnitude). Asset tags `Effect.Family.*`; gear adds `Effect.Origin.Gear`.
- `GAS/WoREffectSpecBuilder.h`: `GetEffectClass`, `Build` (spec + tick spec + turns), `Apply` (replaces the same identity; timed through the tracker, permanent straight to the ASC), `FindByIdentity`, `SumMagnitude` (= `GetTotalStatModifier`), `GetDebugString`.
- The tracker's `ApplySpecForHolderTurns` takes a tick spec, paid at each holder turn start the GE survives (Q-044c).

### Effect mirror and the UseGAS switch (stage 3, B-042c part 3)

- `GAS/WoREffectMirrorComponent.h` (on `ACombatCharacter`): one untracked, tick-less Infinite GE per stored enum-path effect of a ported type (`FWoREffectSpecBuilder::GetEffectClass`), built with `Build(..., bAlreadyStored = true)`. Resyncs on the effect manager's applied / removed / stacks / duration events for its owner by diffing identity, element, gear and stacked value; the enum path still owns lifetime, stacking and ticks.
- `wor.Effects.UseGAS` (default 0, Q-042c): at 1, `USkillEffectManager::HasEffectOfType` (and so `IsStunned` / `IsSilenced`), `GetTotalStatModifier` and `GetTotalStatModifierForElement` read the mirror for ported types on actors that carry one (`UWoREffectMirrorComponent::GetGASReadSource`). Tagged types read their tag; `HasActiveDOT` and `GetEffectTags` go through `HasEffectOfType` too (B-042x), so every Stun / Silence / HealBlock / immunity / DOT check reads the tag. The AI's DOT tick-size read stays on the stored copy until B-042yb.
- Debug: `UWoREffectDebug::GetEffectMismatchString` / `wor.Effects.Compare`.
- A GAS read resyncs the actor's mirror first (turn order and the HP/EP pool read before or beside the mirror's events). The reads count every WoR GE on the ASC: GEs applied any other way (B-042d) would count twice until B-042h makes GAS the store.
- At UseGAS 1 a const effect-manager read can apply or remove GEs (the resync), so ASC listeners run inside it, sometimes while the manager walks an actor's effects; an ASC listener must not change the effect manager. Each read costs a full sync (fine for the PIE pass; B-042h drops it).

### Effect lifetime on GAS (stage 3, B-042h part 3b)

- At UseGAS 1 on an owner with a `UWoRTurnEffectTracker` (`UWoREffectMirrorComponent::OwnsLifetime`), the mirror applies each timed effect's GE through `ApplySpecForSpan` with the stored span; permanent and Immediate effects stay untracked. A refresh in `ApplyEffect` restarts the stored span and the sync pushes it (`SetSpan`); a stack swaps the GE with the copy's span.
- Turn end: the tracker counts first and the stored copies take its count (`GetTrackedTurnsLeft`). Turn start: the tracker ends finished GEs, then `ReleaseEndedCopies` removes their copies (`RemoveEffectByIdentity`); the enum expiry only finds what GAS doesn't hold. A GE GAS ends early stays off (`EndedByGAS`) until then; a same-source re-apply starts it again (`ForgetEnded`).
- B-042y: each mirrored GE keeps its tick spec; at UseGAS 1 the effect manager's DOT / HOT payout calls `UWoREffectMirrorComponent::PayTick` (Q-042y), and the tracker's own tick loop stays unused for them. Debug: `UWoREffectMirrorComponent::GetDebugString` (tracked / turns left / pending releases), `wor.Effects.PrintTurns`, `wor.Effects.Compare`; spec `WoR.GAS.EffectLifetime`.

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
- B-042o: at UseGAS 1 `UStatusBuildupManager::AddStatusBuildup` applies it to the target (the bar is the Buildup attribute; see "Status bar on GAS").

### Status bar on GAS (stage 3, B-042o part 5e)

- At UseGAS 1, for a target whose ASC holds the Buildup attribute (`UStatusBuildupManager::GetBarASC`), the attribute is the bar: hits go through `AddThroughExecution` (the spec's source is the hitter's ASC; a source with none keeps its gathered multiplier), reset / reduce / decay / `ResetForCombat` write it (`SetBar`), the getters read it. `FStatusBarState` keeps a copy and the pending element, physical type, last source and decay count (Q-042o changed by Crown: a Water hit also reads Ice, B-226).
- The immunity gates read tags (`HasEffectOfType` at UseGAS 1), the cap fires the break through `ApplyEffect` (tracker lifetime, B-042h), decay runs after the tracker's turn start. Debug: `GetBarDebugString`, `wor.StatusBars.Print`.

### Tick execution (stage 3, B-042n part 5d)

- `Skills/Effects/TickMath.h`: `WoRTickMath::GetDotTickDamage` (value x the element multiplier, 1 for a share of max HP, rounded) and `GetHotTickHeal` (value x (1 + healing received / 100), rounded) over `FTickMathInputs`. `USkillEffectManager::GetDotTickDamage` and the HealthRestore tick call them.
- `GAS/WoRTickExecution.h`: `UWoRTickExecution`, run by `UWoRGE_HealthTick` / `UWoRGE_HealTick`, over an `FWoRTickRequest` on `FWoREffectContext::TickRequest` (set by `FWoREffectSpecBuilder::Build`, authority only). A DOT reads the holder's element resistance (`UWoRDamageExecution::ReadElementResistance`, shared with hits), a HOT its HealingReceivedBuff total as the enum path reads it (`GetTotalStatModifier`). Output: Health -= damage or += heal.
- B-042y: at UseGAS 1 a DOT / HOT tick is paid by this execution (`PayTick`), and `UWoRAttributeSet_Vitals::PostGameplayEffectExecute` hands the Health change to `ServerTakeDamage` / `ServerHeal` (death path), then sets Health back to the HP; a DOT's `BuildupPerTick` goes through `AddStatusBuildup`, so at UseGAS 1 it runs the buildup execution (B-042o).
- B-042yd: the execution pays every payout type (`GetPayout`): HP drain / self-damage take Health, a percent HP restore heals a rounded share of max HP amplified by healing received, energy restore / drain and the percent EP restore change Energy; the max pools come from the character data at the tick (`ReadPools`, Q-042yc). The Vitals set hands an Energy change to `ServerSpendEnergy` / `ServerGainEnergy` as it does Health. An instant payout (never stored) pays through a tick spec of its own (`PayTick`, Q-042yd). `UWoRGE_HealthTick` is now every non-heal payout tick (the name stays); the heals use `UWoRGE_HealTick`. Spec `WoR.GAS.OtherPayouts`.

### Stat sources (stage 3, B-042p part 5f)

- `Character/StatLayers.h`: `FWoRStatLayers` (StatBase capped alone, Gear, Stone, Transient factors; `Compose()` clamps to `STAT_MODIFIER_MIN/MAX`). `UCharacterDataComponent::GetStatLayers` fills them for SpellDamage, RawDamage, ActionSpeed, SpellSpeed; those four `GetEffective*` getters return `Compose()`.
- `UWoRGE_StatSources` (+ one subclass per stat): Infinite, `Effect.Family.StatSources`, three MultiplyCompound modifiers reading `SetByCaller.Stat.Gear / Stone / Transient`. `UWoRAttributeSet_Stats::PreAttributeChange` clamps the four.
- B-042q: StatusMultiplier, Defense and CritDamage join them (seven composed stats, `UWoRAttributeSet_Stats::GetComposedStats`, the one table the clamp and the sync read). Each stat's clamp comes from `FWoRStatLayers::ForStat` (Defense [0, 0.85], crit damage [x1.0, x2.5], the rest [0, 2]). `wor.Attributes.Compare` also checks each composed base and its one sources effect. The rest: B-042r, B-042s, B-042t, B-042u, B-042v, B-042w.
- B-042r: Luck (clamp [0, `LUCK_GEAR_CEILING`]) and Reflex (unbounded) join them: nine composed stats. The stats of other shapes are B-042s (Q-042r).
- B-042s (Q-042s): derived stats. `UWoRGE_DerivedStat` (`Effect.Family.DerivedStat`) adds clamped terms (`UWoRMMC_ClampedStatTerm`: the source captured live, clamped with `ForStat`) so the value follows its source live: crit chance = (`CRIT_CHANCE_BASE` + Luck x `CRIT_CHANCE_LUCK_BONUS`) x its buff layer (SetByCaller, `GetCritChanceTransientFactor`), clamped [0, 1]; Efficiency = 1 - `EfficiencyReduction` (a composed stat). `SyncDerivedStat` sets the bases and applies each once. The pillars, pools, turn speed and resistance are B-042t.
- B-042t (Q-042t): the pillars. Mind, Body and Spirit use the stat-sources shape over their world base (`GetPillarLayers`, `UWoRGE_StatSources_<Pillar>`: equipment, crystal, pillar buff); the attribute set clamps them to [0, 2] x their base value, as the getter caps the modifier alone. The pools are B-042u; turn speed and resistance are B-042v.
- B-042u (Q-042u): the max pools. MaxHealth / MaxEnergy compose from the layers the last `RecomputeMaxPools` used (`GetMaxHPLayers` / `GetMaxEPLayers`): base = the stat portion, one `UWoRGE_PoolSources` effect adds the gear's flat amount (AddBase) and multiplies the summed stone + buff percent; `UWoRAttributeSet_Vitals` rounds them. A max set another way mirrors with no pool effect.
- B-042v (Q-042v): turn speed. The TurnSpeed attribute is the speed turn order runs the actor at (`UTurnManager::GetEffectiveTurnSpeed`); on GAS the cached base (`GetTurnSpeedLayers`) and one `UWoRGE_StatSources_TurnSpeed` effect (stone, buff), rounded and floored at 1 by the attribute set. The resistance sources are B-042w.
- B-042w (Q-042w): the resistances. Every `Resist_*` attribute and StatusResistBonus has a 0 base; one `UWoRGE_ResistanceSources_*` effect per source (effect, class, gear, boss) adds its bucket by SetByCaller keyed by `DamageType.*` tags (`WoRResistance::GetTypeResistanceSources` / `GetStatusResistBonusSources`). StatusResistance uses the stat-sources shape (`GetGeneralStatusResistanceLayers`, `UWoRGE_StatSources_StatusResistance`), clamped to [0, RESISTANCE_MAX] by the attribute set.
- `UWoRAttributeSyncComponent::SyncComposedStat`: base = StatBase; applies the stat's sources effect once and updates its SetByCaller magnitudes in place when the layers change (re-applied if something removed it; removed at EndPlay). Current value = the getter (Q-042p).

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
  - `wor.Tags.Print`: every enum value and its tag, with a round-trip check.
  - `wor.Tags.Compare`: each combatant's tag queries against the enum queries.
- `UWoRAttributeDebug`:
  - `wor.Attributes.Print`
  - `wor.Attributes.Compare`: each attribute against the getter it mirrors; it names any that drifted.
- Specs: `WoR.Tags`, `WoR.GAS.Attributes`.

## Known limitations / TODOs

- B-042y (Q-042y, B-042yd): at UseGAS 1 every Health- or Energy-changing GE execution on a combatant with character data reaches the HP path, so `FWoREffectSpecBuilder::Apply` with a tick spec would pay on top of the effect manager's pass (no play caller yet; B-042i).
- **Mirror, switching.** At UseGAS 1 the tag read sites (B-042x), the DOT / HOT payouts (B-042y) and the sub-stat and pillar getters (B-042yb, `UWoRAttributeSyncComponent::ReadComposedOnGAS`) and the other getters (B-042yc: resistances, turn speed, Defense, crit, luck, reflex, efficiency, status multiplier) read GAS; the other payouts (B-042yd: energy, drains, self-damage, percent restores; instants too) read GAS; the per-effect reads (B-042ye: the AI's DOT tick and turns, the buff / debuff counts, the character panel's list) read GAS through `USkillEffectManager::GetHeldEffects` / `PreviewDotTickDamage` (mirror: `ReadHeldEffects`, `PreviewDotTick`; Q-042ye).
- MaxBuildup mirrors the fixed bar cap (`CombatConstants::STATUS_EFFECT_THRESHOLD`).
- A stat change with no event (e.g. weather, a crystal breaking mid-turn) reaches the attributes at that character's next turn start. `wor.Attributes.Compare` shows such drift in between.
- Bare test actors (and anything that isn't an `ACombatCharacter`) have no ASC unless one is added, as `WoR.GAS.Attributes` does.
- Per-AI Minimal replication mode waits for co-op.
- B-042yd: a heal paid through GAS (HOT tick, percent HP restore) reports the HP actually gained (it stops at
  MaxHP); the game path's `HealActor` reports the full amplified heal. Near full HP the two `OnHealingDone`
  amounts differ. No spec yet for a HealingReceived buff on a percent restore at UseGAS 1.
- B-042ye: the AI resyncs the mirror per DOT preview and per `GetDebuffCount` (cheap when nothing changed,
  but per candidate x target); `wor.Effects.Compare` doesn't show the held list beside the stored one yet.

- B-318: the held-effects snapshot (`UWoREffectMirrorComponent::HeldSnapshot`) is refreshed by the mirror's syncs; a GE that GAS ends without an effect-manager event (outside the holder-turn start, which `ReleaseEndedCopies` covers) shows on guests only from the next sync. Each entry sends the whole `FActiveSkillEffect` (names, conditions); a slim replicated struct can come later if bandwidth asks.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | B-040: native tags, enum to tag maps, `GetEffectTags`, `wor.Tags.Print` / `wor.Tags.Compare`. | feature/B-040-gas-tags |
| 2026-10-26 | B-041: attribute sets, ASC + `UWoRAttributeSyncComponent` on `ACombatCharacter`, `wor.Attributes.Print` / `wor.Attributes.Compare`; effect clears broadcast after removal. | feature/B-041-gas-attributes |
| 2026-10-27 | B-042 (stage 3, part 1): native `UWoRGE_Stun`; `UWoRTurnEffectTracker` on `ACombatCharacter`, driven by the effect manager's turn hooks; `wor.Effects.PrintTurns`. The rest of stage 3 is B-042b-g. | feature/B-042-gas-turn-tracker |
| 2026-10-27 | B-042b (stage 3, part 2): `FWoREffectContext` + globals class; family GEs; `FWoREffectSpecBuilder`; tracker tick specs. Not routed in play yet (B-042c). | feature/B-042b-gas-effect-builder |
| 2026-10-27 | B-042c (stage 3, part 3): `UWoREffectMirrorComponent`; `wor.Effects.UseGAS` facade reads; `wor.Effects.Compare`; `Build` takes `bAlreadyStored`. Write side moved to B-042h (Q-042c). | feature/B-042c-gas-effect-mirror |
| 2026-10-27 | B-042d (stage 3, part 4, narrowed): `WoRCombatEvents::Send`; `Event.Combat.*` sent from the effect manager. Rest split into B-042i, B-042j, B-042k. | feature/B-042d-gas-combat-events |
| 2026-10-27 | B-042k (stage 3, part 4b): seven role families; every effect type mirrored. | feature/B-042k-gas-remaining-families |
| 2026-10-27 | B-042e (stage 3, part 5, narrowed): `UWoRDamageExecution` + `UWoRGE_Damage`; rest split into B-042l, B-042m, B-042n. | feature/B-042e-gas-damage-execution |
| 2026-10-27 | B-042l (stage 3, part 5b, narrowed): `WoRBuildupMath`, `UWoRBuildupExecution` + `UWoRGE_Buildup`, `ReadSyncedAttribute`; bar / cap / decay split into B-042o. | feature/B-042l-gas-buildup-execution |
| 2026-10-27 | B-042m (stage 3, part 5c, narrowed): Ignore Defense as an amount in the shared damage core the damage execution runs; sources as GEs split into B-042p. | feature/B-042m-ignore-defense-amount |
| 2026-10-27 | B-042p (stage 3, part 5f, narrowed): the four Pattern P stats composed on GAS (`UWoRGE_StatSources`, base = capped stat); other stats split into B-042q. | feature/B-042p-gas-stat-sources |
| 2026-10-27 | B-042q (stage 3, part 5g, narrowed): StatusMultiplier, Defense and crit damage composed on GAS (per-stat clamps); `wor.Attributes.Compare` checks composed bases and sources effects; other stats split into B-042r. | feature/B-042q-gas-stat-sources-rest |
| 2026-10-27 | B-042r (stage 3, part 5h, narrowed): Luck and the Reflex window factor composed on GAS; the stats of other shapes split into B-042s (Q-042r). | feature/B-042r-gas-luck-reflex |
| 2026-10-27 | B-042s (stage 3, part 5i, narrowed): crit chance and Efficiency derived on GAS (`UWoRGE_DerivedStat`, `EfficiencyReduction`); the pillars, pools, turn speed and resistance split into B-042t (Q-042s). | feature/B-042s-gas-derived-stats |
| 2026-10-27 | B-042t (stage 3, part 5j, narrowed): the pillars composed on GAS; the pools split into B-042u, turn speed and resistance into B-042v (Q-042t). | feature/B-042t-gas-last-stats |
| 2026-10-27 | B-042u (stage 3, part 5k): MaxHealth / MaxEnergy composed on GAS from the last recompute's pool layers (Q-042u). | feature/B-042u-gas-pools |
| 2026-10-27 | B-042v (stage 3, part 5l, narrowed): turn speed composed on GAS from turn order's own getter; the resistance sources split into B-042w (Q-042v). | feature/B-042v-gas-turn-speed |
| 2026-10-27 | B-042w (stage 3, part 5m): the resistances composed on GAS from one effect per source; general status resistance on the stat-sources shape (Q-042w). | feature/B-042w-gas-resistance |
| 2026-10-27 | B-042n (stage 3, part 5d, narrowed): `WoRTickMath`, `UWoRTickExecution` on the DOT / HOT tick GEs, `ReadElementResistance`; the per-tick buildup split into B-042o (Q-042n). | feature/B-042n-gas-dot-hot-ticks |
| 2026-10-27 | B-042h (stage 3, part 3b, narrowed): at UseGAS 1 the tracker owns timed effects' lifetime (`ApplySpecForSpan`, `SetSpan`, `ReleaseEndedCopies`, `ForgetEnded`); payouts move with B-042y (Q-042hb). | feature/B-042h-gas-effect-lifetime |
| 2026-10-27 | B-042o (stage 3, part 5e): at UseGAS 1 the status bar is the Buildup attribute; hits through `UWoRBuildupExecution` (Q-042o). | feature/B-042o-gas-status-bar |
| 2026-10-27 | B-042x (stage 3, part 3c): `HasActiveDOT` / `GetEffectTags` read through `HasEffectOfType`, so at UseGAS 1 every tagged-type check reads the holder's tags; AI lethal-burn gate; `WoR.GAS.TagReads`. | feature/B-042x-gas-tag-reads |
| 2026-10-27 | B-042y (stage 3, part 3d): at UseGAS 1 DOT / HOT ticks paid by the mirror's tick spec; the Vitals set hands GAS Health changes to the HP / death path; the tracker reads real HP (Q-042y). | feature/B-042y-gas-getters-hp |
| 2026-10-27 | B-042yb (stage 3, part 3d2): at UseGAS 1 the sub-stat and pillar getters return their attribute after a one-attribute refresh (`ReadComposedOnGAS`, Q-042yb); `ReadSourceValues` composes on the game side. | feature/B-042yb-gas-stat-getters |
| 2026-10-27 | B-226: `UWoRBuildupExecution::ReadAttributes` adds `Resist_Ice` for a Water hit (Q-042o changed). | fix/B-226-water-buildup-ice |
| 2026-11-02 | B-042yc (stage 3, part 3d3): at UseGAS 1 resistances (per type, general status, blanket bonus), turn speed, Defense, crit chance / damage, luck, reflex, efficiency and status multiplier read their attribute (`ReadComposedOnGAS`); max pools and the wear control resistance stay game-side (Q-042yc). Turn order reads GAS TurnSpeed at UseGAS 1 and the entry's cached speed at 0. | feature/B-042yc-gas-getters |
| 2026-11-02 | B-042yd (stage 3, part 3d4): at UseGAS 1 energy restore / drain, HP drain, self-damage and the percent restores are paid by their tick spec (`UWoRTickExecution::GetPayout`); the Vitals set hands Energy changes to the EP path; instants pay through their own tick spec (Q-042yd). | refactor/B-042yd-gas-payouts |
| 2026-11-02 | B-042ye (stage 3, part 3d5): at UseGAS 1 the per-effect reads go through GAS: `GetHeldEffects` (live mirrored GEs, tracker turns; a client keeps the stored list) feeds the character panel, `GetBuffCount` / `GetDebuffCount` and the AI's DOT checks; `PreviewDotTickDamage` sizes a DOT from its tick spec (Q-042ye). Spec `WoR.GAS.EffectReads`. | refactor/B-042ye-gas-effect-reads |
| 2026-11-02 | B-042z (stage 3, part 3e): fires-once records and armed conditionals are kept on each fighter's `UWoRTurnEffectTracker` (D-064, Q-042h), reset at fight start; the defend dispatcher loops over a copy of the armed list (a re-arm mid-loop could reallocate it). | feature/B-042z-fight-records-on-tracker |
| 2026-11-02 | B-042j (stage 3, part 4d, D-092): the absorb family (Barrier / Shield / Ward, `UWoRGE_Absorb` + `WoRAbsorb`) held on `UWoRTurnEffectTracker`; `AbsorbHit` runs after Guard and Defense (Q-044e B), one charge per impact (Q-044f A), uses / pool / refill at the holder's turn start; Reflect sees only what got through (Q-042j); debug `wor.Absorb.Add`. | feature/B-042j-absorb-family |
| 2026-11-03 | B-260 (P2): `UWoRAttributeSyncComponent` HP / EP / buildup events set only Health, Energy and Buildup (`SyncVitals`) and queue one full sync for the next tick (`QueueFullSync`); a max HP / EP change or a Broken Darkness flip syncs in full at once; EndPlay drops a queued sync. Debug: `FullSyncCount`. | refactor/B-260-light-attribute-sync |
| 2026-11-03 | B-293: `WoR.Balance.Sim.GASParity` plays each balance matchup on two seeds with `wor.Effects.UseGAS` 0 then 1 (`FBalanceSimRunner::SetUseGAS`) and expects identical fight records; `wor.Balance.Run ... 1` runs a preset on GAS. Unbuilt (cloud). | feature/B-293-balance-targets |
| 2026-11-03 | B-261 (PerfAudit P5): `GetGASReadSource` syncs the mirror only when `GetSyncFingerprint()` (stored effects via `USkillEffectManager::FindActiveEffects`, live GE count, our GEs' liveness, tracker, `ReadsFromGAS`) differs from the last finished sync's; `ForgetEnded` clears it; `GetSyncCount` and a debug line. Fingerprint over a dirty flag: a read can come between a change and its event. Not caught: a stored change mid-sync with no event | refactor/B-261-mirror-dirty-check |
| 2026-11-03 | B-318 (Q-042ye, Q-318): `UWoREffectMirrorComponent` replicates (`SetIsReplicatedByDefault`); every sync at UseGAS 1 refreshes `HeldSnapshot` (`BuildHeld`: the stored effects as GAS holds them, turns left from the tracker), a replicated property; on a client `ReadHeldEffects` returns it, so the character panel's list, turns left and buff / debuff counts match the server. Spec `WoR.GAS.ClientReads`. Unbuilt (cloud). | feature/B-318-client-gas-reads |
| 2026-11-03 | B-042i (Q-042i-b): no code change; `WoR.GAS.TriggerParity` (`GASTriggerParitySpec.cpp`) pins that a defender trigger's payload (`OnDefenseResolvedHandler`) and a starting effect (`ApplyEquipmentEffects`) reach GAS through `FWoREffectSpecBuilder` in the same `ApplyEffect` call (the mirror's `OnEffectApplied` sync), as one GE of their identity on both paths; a re-fire replaces, a removal ends the GE. The stored copies go with B-042g. | refactor/B-042i-trigger-gas-specs |
| 2026-11-04 | B-042j2 (D-092, Q-042j2): absorbers spend on DoT ticks (`ApplyEffectLogic` DOT case; a touched tick pays through the enum path); what they stop fires `OnDefenseResolved` Block when the defender did not guard (rule 7) and a stopped hit still sends `Event_Combat_Hit` at 0 (rule 8); `EWoRWardScope` Element / Elemental / All; six `Negate*` `ESkillEffectType` values that `USkillEffectManager::ApplyEffect` turns into charges on the holder's tracker (after the fires-once gate; `RemoveAbsorbsFrom` makes a same-source re-cast replace), with their GE family (`UWoRGE_Absorb`), names and text. Reflect as one GE: B-042j3. Reviewer (Sonnet) on the desktop: must-fixes applied (the DoT case ticked the wrong edge; an authored Ward lost its element in a skill cast, now the payload's own or the cast element; the Negate interception ran before the fires-once gate and never recorded the use, and a same-source re-cast piled charges up, now it replaces them, Q-044 A); should-fixes applied (no On Guard on a corpse, the charges-without-duration warning skips a Negate, the Hit event's comment, a GE family and names for the new types); 481 green + validation 0 new after them. Merged into dev. | feature/B-042j2-absorb-part2 |
| 2026-11-04 | B-042j3 (D-092, Q-042j3): a reflect's authored Element is its scope (`SkillEffectClassification::IsReflect`; `BuildRuntimeFromPayload` keeps it, never the cast element; `ReflectShareOfHit` sums by the hit's element; the text names it). Rule 8 for conditionals: an unguarded impact's `OnDefenseResolved(None)` (On Take Damage) is broadcast by `ApplyOneImpact` after the hit, only when it did damage (`FPendingDefenseContext::bTakeDamageOutcomePending` / `PendingAttackEnergyCost`); guards fire at defense time as before. `UWoRTurnEffectTracker::PreviewAbsorb` (spends on copies) and `USkillEffectManager::PreviewDotTickDamage` after the absorbers, read by the AI's lethal checks. no must-fix; should-fixes applied (the deferred outcome reads its flags before the hit and never fires on a corpse, the lumped tail clears it, the Generic and physical reflect text, the AI reads the later DoT ticks bare). Merged into dev. | feature/B-042j3-absorb-part3 |
| 2026-11-09 | B-042f (stage 3 step 5, Q-042f): stones and gear lines enter GAS through `FWoREffectSpecBuilder::BuildStatSources` / `BuildPoolSources` (the attribute sync's sources GEs, numbers unchanged); `wor.Effects.Compare` checks each stat's Stone factor (`UWoRAttributeSyncComponent::GetStoneMismatchString`); item effects already mirrored, pinned by the B-134 baseline's gear cases. | `feature/B-042f-stones-item-effects-gas-cloud` (UNBUILT) |
| 2026-11-09 | B-042g part 1 (Q-042g): `FWoREffectIdentity::PhysicalType` / `EffectValue`; `FWoREffectSpecBuilder::GetEffectsOfType`; `USkillEffectManager::GetEffectsByType` reads the GEs at UseGAS 1 (resistance, buildup, Ignore Defense, reflect); parts 2–6 are B-042g2..g6. | `feature/B-042g-gas-readers-default-cloud` (UNBUILT) |
| 2026-11-09 | B-043a (stage 4 part 1, Q-043a): skill costs as GEs. `UWoRGE_EnergyCost` / `UWoRGE_HPBacklash` (Instant, Additive on Energy / Health, `SetByCaller.Cost.*` = minus the cost from `CalculateActionEnergyCost` / the commit-time backlash; asset tags `Effect.Family.Cost` / `Backlash`); `WoRCosts` (`GAS/WoRActionCosts.h`) pays through them when `wor.Costs.UseGAS` and `wor.Effects.UseGAS` are both 1, else declines; `UActionExecutor::SpendEnergy` (after the Silence check) and `ApplyPendingInfusionHPCost` use it; the Vitals set hands the change to `ServerSpendEnergy` / `ServerTakeDamage`. Debug `wor.Costs.Print` (`UWoRCostDebug`); spec `WoR.GAS.Costs`. B-043 sliced into B-043a–h | `feature/B-043a-costs-gas-cloud` (UNBUILT) |

| 2026-11-09 | B-042f (stage 3 step 5, Q-042f): stones and gear lines enter GAS through `FWoREffectSpecBuilder::BuildStatSources` / `BuildPoolSources` (the attribute sync's sources GEs, numbers unchanged); `wor.Effects.Compare` checks each stat's Stone factor (`UWoRAttributeSyncComponent::GetStoneMismatchString`); item effects already mirrored, pinned by the B-134 baseline's gear cases. | `feature/B-042f-stones-item-effects-gas-cloud` (in dev) |
| 2026-11-09 | B-273b: the damage and buildup executions read by hit kind: `UWoRDamageExecution::ReadHitDamageResistance` (`ReadElementResistance` forwards) and `FWoRBuildupRequest::GetHitKind()` read `Resist_<ElementType()>` plus the row's `AlsoResistedBy` instead of the hard-coded "Water adds Ice", so an Ice hit reads `Resist_Ice` alone (Q-145b). `FDamageMathInputs` / `FWoRBuildupRequest` carry a `DealtTypeOverride`; `MakeRequest` by hit kind (the pair form forwards). | `feature/B-273b-hit-kind-cloud` (UNBUILT) |
