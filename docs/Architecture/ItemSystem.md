# Item System

## Overview

The item system covers **crystals** — consumable and equippable magical items built on a
10-type / 7-tier matrix. Crystals are authored as `UItemData` primary data assets and
executed in turn-based combat by the `UItemExecutor` subsystem. Each crystal maps to one
of the nine elements (plus Generic) and to a single combat effect — damage, healing,
buffs, status effects, etc. Tiers F→S scale the effect's magnitude.

> This document reflects the **shipped** item system on `main` after the
> `feature/item-system-redesign` work (Phases 1–3). Crystal values are
> percentage-based and the redesign is fully implemented.

## Architecture

### UItemData (`UPrimaryDataAsset`)

Immutable design-time definition of a crystal. Key fields:

- `CrystalType` (`ECrystalType`) — selects element + effect path; drives every value getter.
- `Tier` (`EItemTier`) — scales all magnitudes; gates S-rank specials.
- `bIsRefined` — `false` = consumable; `true` = slottable onto a weapon/ring (durability, spells).
- `bIsEvolutionCrystal` — `true` = grants an evolution when slotted; enables `EvolutionType`, `BaseStatBonus`, `Effects`.
- `MaxDurability`, `bImmuneToBreaking` — durability tracking (refined crystals only).
- `BaseStatBonus` (`FEquipmentStatBonus`), `Effects` (`TArray<FSkillEffect>`) — evolution-crystal traits.
- `Display*` fields — `VisibleAnywhere` editor mirrors, recomputed in `PostEditChangeProperty`.

Per-crystal combat numbers are **not stored as fields** — they are hardcoded tier `switch`
tables inside `UItemData`'s `Get*()` functions (`GetDOTDamagePercent`, `GetHealPercent`,
`GetEPRestorePercent`, `GetElementalBuildupPercent`, `GetCrystalDuration`, …). Quartz is
**consumable-only**: `IsDataValid` and `PostEditChangeProperty` reject / clear `bIsRefined`
and `bIsEvolutionCrystal` on Quartz.

### UItemExecutor (`UGameInstanceSubsystem`)

Runs item use in combat. Entry point `UseItem(User, Item, Target)`:

1. Validates user/item; defaults `Target` to `User` if null.
2. Switches on `Item->GetPrimaryEffectType()` to a per-effect handler (`ExecuteDamageEffect`, `ExecuteHealingEffect`, …, `ExecuteStatusClearEffect`).
3. If the **target** is a Broken Darkness character, applies the BD energy absorption.
4. Sets `bSuccess` on the `FItemUseResult` and broadcasts `OnItemUsed`.

`UseItemMultiTarget` loops `UseItem` and accumulates results. The executor applies effects
only — it does **not** manage inventory or consume item counts.

`IsAlly(User, Target)` (resolved via `UTurnManager::GetActorTeam`) lets handlers branch
buff-vs-debuff (Amber, Opal) or cleanse-vs-strip (Iolite).

### ECrystalType — the 10 crystals

| Crystal | Element | Effect role |
|---|---|---|
| Garnet | Fire | Fire DOT |
| Sapphire | Water | Healing / revive |
| Citrine | Lightning | Energy restore |
| Emerald | Wind | Turn-speed buff / extra turn |
| Amber | Earth | Defense buff / debuff |
| Opal | Light | Crit buff / debuff |
| Onyx | Darkness | Silence (energy-lock / binary) |
| Amethyst | Void | Gamble (random buff/debuff) |
| Iolite | Reality | Cleanse / strip |
| Quartz | Generic | Status-bar clear + elemental protection |

`ECrystalType::Quartz` is the last enum value — `ItemDataDebug` loops iterate up to it.

### EItemEffectType

`Damage, Healing, EnergyRestore, BuffDamage, BuffDefense, BuffSpeed, BuffCrit, Silence,
Cleanse, Gamble, StatusClear, Repair`. `GetPrimaryEffectType()` maps each `CrystalType` to
one of these. `StatusClear` (Quartz) replaced the removed `Transform` value — a redirect
maps old `Transform` data to `StatusClear`.

### EItemTier

Seven tiers: `F_Tier, E_Tier, D_Tier, C_Tier, B_Tier, A_Tier, S_Tier`. Tier indexes the
value tables and gates S-rank specials.

## Crystal Behaviour

All crystal values are **percentage-based** (the redesign is shipped). Tier order in every
table: **F / E / D / C / B / A / S**.

| Crystal | Metric | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| Garnet | DOT damage %/turn (of target MaxHP) | 5 | 7 | 9 | 12 | 16 | 20 | 30 |
| Garnet | DOT duration (turns) | 3 | 3 | 3 | 2 | 2 | 2 | 1 |
| Sapphire | Heal % of MaxHP | 15 | 20 | 25 | 30 | 35 | 45 | 60 |
| Citrine | EP restore % of MaxEP | 30 | 40 | 50 | 60 | 70 | 85 | 100 |
| Emerald | Turn-speed buff % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| Amber | Defense buff/debuff % | 15 | 20 | 25 | 30 | 35 | 40 | 50 |
| Opal | Crit buff/debuff % | 5 | 8 | 10 | 12 | 15 | 18 | 25 |
| Emerald/Amber/Opal | Duration — `GetCrystalDuration` | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Onyx | Silence % (energy-lock) | 15 | 30 | 30 | 50 | 70 | 70 | 100 |
| Onyx | Energy-lock duration, F–A — `GetSilenceDurationNew` | 3 | 3 | 2 | 2 | 2 | 1 | — |
| Amethyst | Buff/debuff magnitude % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| Amethyst | Buff chance % | 10 | 20 | 30 | 40 | 50 | 60 | 70 |
| Amethyst | Duration — `GetGambleDuration` | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Iolite | Effects removed (99 = all) | 1 | 1 | 2 | 2 | 3 | 3 | 99 |
| Quartz | Status bar cleared % | 25 | 35 | 45 | 55 | 65 | 80 | 100 |
| Quartz | Resistance duration — `GetResistanceDuration` | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Garnet/Citrine/Onyx/Amethyst | Elemental buildup % — `GetElementalBuildupPercent` | 10 | 15 | 20 | 30 | 40 | 50 | 60 |

Notes:
- **Status buildup** — Garnet (Fire), Citrine (Lightning), Onyx (Darkness) and Amethyst
  (Void) each add status-bar buildup on use via the shared `GetElementalBuildupPercent`
  table. Buildup amount = `BarMax × percent / 100` (Garnet uses target MaxHP as the base).
- **Onyx S-rank** does not use the energy-lock duration table — it applies a binary
  `Silenced` effect for 1 turn (see S-Rank Specials).
- **Citrine** no longer has an HP cost. **Iolite** no longer grants immunity.

## Targeting Rules

Every crystal returns `ETargetType::SingleAnyone` from `UItemData::GetItemTargetType()` —
any living combatant (ally or enemy) is a legal target, for tactical flexibility. The
command menu's `Item` case reads `GetItemTargetType()` directly; the old Quartz `Self`
special-case is gone.

The `SingleAnyone` target picker groups its buttons into two labelled sections —
**Allies** and **Enemies** — using `EPieMenuCategory::SectionHeader` rows
(`MakeSectionHeaderButton`). Section-header rows are force-disabled and non-clickable.

Handlers that care about ally-vs-enemy resolve it at execution time via
`UItemExecutor::IsAlly` (Amber → buff/debuff, Opal → buff/debuff, Iolite → cleanse/strip).

## S-Rank Specials

Final implemented S-rank behaviour for all 10 crystals:

- **Garnet S** — DOT at 30%/turn for **1 turn** (a hard single-turn burst vs the long low-tier burn).
- **Sapphire S** — revives a **dead** target at 30% MaxHP via `ServerResurrect`; heals a living target for 60% MaxHP.
- **Citrine S** — restores 100% of target MaxEP; adds 60% Lightning buildup (shared table).
- **Emerald S** — grants the target an **immediate extra turn** via `UTurnManager::RequestExtraTurn`, instead of the turn-speed buff.
- **Amber S** — 50% defense buff (ally) or debuff (enemy).
- **Opal S** — 25% crit buff (ally) or debuff (enemy). *(A stat-reveal special is designed but unimplemented — see TODOs.)*
- **Onyx S** — applies the binary `ESkillEffectType::Silenced` effect (full spell lockout) for 1 turn, instead of the F–A percentage energy-lock.
- **Amethyst S** — 70% buff chance, 40% magnitude.
- **Iolite S** — removes **all** debuffs (ally) or **all** buffs (enemy) — the `99` sentinel.
- **Quartz S** — clears 100% of the status bar and grants full elemental **immunity**
  (`GrantXxxImmunity`) for the pending element, instead of the F–A `ResistanceBuff`.

## Integration Points

### → SkillEffectManager (`UGameInstanceSubsystem`)

`UItemExecutor::GetSkillEffectManager()` lazily fetches and caches the subsystem. Buff/
debuff/silence/gamble handlers build an `FActiveSkillEffect` and call
`ApplyEffect(Target, Effect, Source, SourceName, SourceTeam)`. Iolite uses
`GetActiveEffects` + `IsBuff()/IsDebuff()` + `RemoveEffectByID`. Quartz uses
`CreateBuff` with `ResistanceBuff` / `GrantXxxImmunity`.

### → StatusBuildupManager (`UGameInstanceSubsystem`)

Now connected. `UItemExecutor` reaches it via `GetGameInstance()->GetSubsystem<…>()`:
- `AddStatusBuildup(Source, Target, Amount, Element, PhysicalType)` — Garnet (Fire),
  Citrine (Lightning), Onyx (Darkness), Amethyst (Void).
- **`ReduceStatusBuildup(Target, Fraction)`** — *new* function added for Quartz; reduces
  the bar by a 0–1 fraction.
- `GetStatusBarBuildup` / `GetBuildupToTrigger` — used to derive `BarMax` for buildup sizing.
- `GetPendingElement` — Quartz reads it to choose which element to resist/immunise.

**Elemental-buildup pattern** — the status-building crystals share an inline pattern (not
a single helper function): compute `BarMax = GetStatusBarBuildup + GetBuildupToTrigger`,
then `AddStatusBuildup(…, BarMax × GetElementalBuildupPercent()/100, <element>, None)`.

### → CharacterDataComponent (`UActorComponent`)

`GetCharacterDataComponent(Actor)` resolves via `FindComponentByClass`. Handlers call
`ServerTakeDamage`, `ServerHeal`, `ServerGainEnergy`, `ServerResurrect`, and read
`CurrentHP/CurrentEP`, `MaxHP/MaxEP`, `bIsAlive`. Class checks use
`CharacterData->CharacterClass` (Generic) and `IsBrokenDarkness()` (catches
runtime-transformed BD).

### → TurnManager (`UGameInstanceSubsystem`)

`RequestExtraTurn` (Emerald S) and `GetActorTeam` (the `IsAlly` helper).

### → CombatCommandMenuSubsystem (`UGameInstanceSubsystem`)

The command menu's `Item` selection reads `UItemData::GetItemTargetType()` to open the
target picker, and renders `EPieMenuCategory::SectionHeader` rows in the `SingleAnyone`
picker.

### Delegates

`OnItemUsed(User, Item, Result)` broadcasts at the end of every successful `UseItem`.
`OnGambleResult` is a second delegate for Amethyst outcomes. `FOnQuartzTransformed` was
removed with the transform system.

## Known Limitations / TODOs

- **BP styling for `SectionHeader` buttons (pinned).** The `EPieMenuCategory::SectionHeader`
  rows are functional (force-disabled, non-clickable) but need a distinct Blueprint visual
  style so they read as headers rather than dimmed buttons.
- **Opal S-rank stat reveal unimplemented.** `ExecuteCritBuffEffect` carries a
  `// TODO: S-rank stat reveal — implement in UI pass`; no reveal event is broadcast.
- **Dead-code cleanup pending.** The redesign orphaned several getters/helpers. Genuinely
  dead (zero callers): `UItemExecutor::ApplySecondaryEffect`, `UItemExecutor::IsGenericCharacter`,
  `UItemExecutor::GetCharacterData`, `UItemData::GetLightningBuildupPercent`,
  `ItemConstants::GENERIC_RESISTANCE_*` / `GENERIC_DURATION_*`, `FItemUseResult::GenericResistanceApplied`.
  A second tier of pre-redesign getters (`GetDamageValue`, `GetEnergyValue`, `GetSelfDamage`,
  `GetBuffDuration`, `GetDebuffsToRemove`, `GetGrantsImmunity`, `GetImmunityDuration`,
  `HasSecondaryEffect`, `GetSecondaryDamagePerTurn`, `GetSecondaryDuration`,
  `GetSilenceDuration`) has no gameplay callers but is still referenced by the editor
  `Display*` mirrors and `ItemDataDebug` — removing them needs those scaffolds cleaned too.
- **Item inventory / consumption.** `UItemExecutor` applies effects but never decrements
  item counts or checks ownership — inventory wiring is unbuilt.
- Self-targeted buff items with duration 1 expire at the end of the casting turn (effect
  durations tick at the end of the affected actor's own turn) — designers must tune
  self-buff durations to ≥ 2.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Phase 1 — `ModifyDamageTaken` split into `ReduceDamageTaken` / `IncreaseDamageTaken` | feature/item-system-redesign |
| 2026-05-17 | Phase 1 — `IsDebuff()` fixed for 8 misclassified effect types | feature/item-system-redesign |
| 2026-05-17 | Quartz transform system removed, `StatusClear` added | feature/item-system-redesign |
| 2026-05-17 | Quartz consumable-only restriction added | feature/item-system-redesign |
| 2026-05-17 | Garnet redesign — percentage-based fire DOT | feature/item-system-redesign |
| 2026-05-17 | Phase 2 — bulk crystal redesign (Sapphire–Quartz); percentage values; `ExecuteStatusClearEffect`; `ReduceStatusBuildup`; shared elemental buildup | feature/item-system-redesign |
| 2026-05-17 | Phase 3 — class-detection fixes; `ApplyGenericBonus` removed; BD absorption fires on target; `GetItemTargetType` + `SingleAnyone` targeting with section headers | feature/item-system-redesign |
