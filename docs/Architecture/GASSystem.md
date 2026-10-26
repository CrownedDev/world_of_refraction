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
