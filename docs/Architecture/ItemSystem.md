# Item System

## Overview

The item system covers **crystals** — consumable and equippable magical items
built on a 10-type / 7-tier matrix. The system splits into two complementary
tracks: **item and refined crystals** are identified by `FCrystalId` (Type +
Tier) and stored count-based in `UCrystalInventoryComponent`, with their
behaviour, names, and tier tables living in the `CrystalEffectTable` and
`CrystalIdentity` namespaces; **evolution crystals** are authored individually
as `UEvolutionItemData` primary data assets and stored instance-based in
`UEvolutionInventoryComponent`. The `UItemExecutor` subsystem runs item-crystal
use in turn-based combat. Each crystal maps to one of the nine elements (plus
Generic) and to a single combat effect — damage, healing, buffs, status
effects, etc. Tiers F→S scale the effect's magnitude.

> This document reflects the **shipped** item system on `main` after the
> `feature/item-system-redesign` work (Phases 1–3). Crystal values are
> percentage-based and the redesign is fully implemented.

## Architecture

### Storage model

The crystal-bearing surface splits into two complementary tracks, each with
its own asset shape and runtime container.

**Item & refined crystals (FCrystalId-based, no asset).** Each item/refined
crystal is fully identified by an `FCrystalId` (Type + Tier). Authoring is
count-based: `TMap<FCrystalId, int32>` on `UInventoryData::ItemCrystals` and
`RefinedCrystals`, with the per-tier cap enforced by `IsDataValid`. Runtime
storage lives on `UCrystalInventoryComponent`, which mirrors the two
count-based pools. Percentage tables, display names, primary effect type, and
target type live in the `CrystalEffectTable` and `CrystalIdentity` namespaces
(inline tables keyed by `FCrystalId`). Refined and item crystals are fungible
within their pool — two refined Garnet (F) crystals are interchangeable.

**Evolution crystals (`UEvolutionItemData` asset).** Each evolution crystal is
its own `UPrimaryDataAsset`, authored individually with custom `Effects`,
`BaseStatBonus`, `EvolutionType`, and a designer-authored
`RevealedDescription`. Per-asset identity matters — trade APIs need the
underlying `FGuid`, so the runtime container (`UEvolutionInventoryComponent`)
stores instances, not counts.

**Attachment slot (`FAttachedItem` / `FRuntimeAttachedItem`).** The attachment
point on equipment is a discriminated union, not a pointer:
- `FAttachedItem` (design-time, on `UEquipmentDataBase::AttachedItem`):
  `Kind ∈ {None, Crystal, Evolution, AugmentStone}` + `CrystalType` /
  `CrystalTier` (for `Crystal` **and** `AugmentStone` — both carry an `FCrystalId`
  identity) or `Evolution` (`UEvolutionItemData*`, for `Evolution`).
- `FRuntimeAttachedItem` (runtime, on `FWeaponInventoryEntry::AttachedItem`
  and `FRingInventoryEntry::AttachedItem`): same shape plus per-instance
  durability and a `FGuid` for evolution identity.

The `AugmentStone` Kind is the **augment-stone family** (`DamageStone` /
`AbilityStone`) — a second attachment family sharing this slot and the
`FCrystalId` identity with crystals. It is documented separately in
`AugmentStoneSystem.md`; this doc covers the crystal (gem) and evolution tracks.

Routing into these tracks happens structurally at inventory build time:
`UInventoryComponent::InitializeFromInventoryAsset` reads the three ownership
lists on `UInventoryData` and dispatches each by field — `ItemCrystals` and
`RefinedCrystals` to `UCrystalInventoryComponent` by count,
`EvolutionEquipment` to `UEvolutionInventoryComponent` by instance. There is
no runtime flag-based routing; the dispatch is determined by which
`UInventoryData` field the entry was authored into.

### UEvolutionItemData (`UPrimaryDataAsset`)

Immutable design-time definition of an evolution crystal. Key fields:

- `CrystalType` (`ECrystalType`) — element, drives `GetAssociatedElement()`.
- `Tier` (`EItemTier`) — scales magnitudes; drives durability defaults.
- `ItemName` (`FString`) — designer-authored evolution name.
- `Description`, `RevealedDescription` (`FString`) — see Description Model.
- `bIsRefined` — `false` = unrefined (in inventory); `true` = slotted on a
  weapon/ring.
- `bCanBreak` — opt-in durability wear. Default `false`: the crystal is
  permanent and its displayed durability is cosmetic. Refined evolution
  crystals only break when this is explicitly authored true. The intrinsic-
  mechanic override `FEvolutionAttachment::ApplyWear(_, bForceWear=true)`
  bypasses this gate — Broken Darkness uses it so per-asset opt-in can't
  silently disable BD's wear (see `CrystalWear.md`). Default behavior for
  every other caller is unchanged.
- `MaxDurability`, `CurrentDurability` — durability tracking when `bCanBreak`.
  Defaults from `Tier` if author leaves `MaxDurability == 0`.
- `EvolutionType` (`EEvolutionType`) — Balanced / Mind / Body / Spirit / etc.
- `BaseStatBonus` (`FEquipmentStatBonus`) — evolution stat modifiers. The
  embedded struct's `ClampMin=0` UPROPERTY meta can't be overridden at the
  embedding site, so out-of-range values (crystals permit negatives down to
  `CRYSTAL_BONUS_MIN`) surface as `IsDataValid` warnings.
- `Effects` (`TArray<FSkillEffect>`) — skill effects granted by the evolution
  crystal (passives + triggered).

Quartz is consumable-only and **cannot exist as a `UEvolutionItemData`
asset**. `IsDataValid` rejects any `UEvolutionItemData` with
`CrystalType == Quartz` (error: `"Quartz crystals cannot be evolution crystals — they are consumable only"`).
`PostEditChangeProperty` additionally force-clears `bIsRefined` if the
designer switches `CrystalType` to Quartz on an existing asset. Quartz exists
only as `FCrystalId{Quartz, Tier}` in the `ItemCrystals` pool.

### UItemExecutor (`UGameInstanceSubsystem`)

Runs item use in combat. Entry point `UseItem(User, Item, Target)`:

1. Validates user/item; defaults `Target` to `User` if null.
2. Switches on the item's `ItemIdentity::GetItemEffectType(FCrystalId)` to a per-effect handler (`ExecuteDamageEffect`, `ExecuteHealingEffect`, …, `ExecuteStatusClearEffect`). *(Formerly `GetPrimaryEffectType()`; renamed in commit `5c22e6e`.)*
3. If the **target** is a Broken Darkness character, applies the BD energy absorption — `ApplyBrokenDarknessBonus` grants energy scaled as **% of target MaxEP** *(sweep-1)*, tier-keyed F=10% .. S=70% via `CrystalEffectTable::GetBrokenDarknessEnergyPercent(Id) × TargetComp->MaxEP`. Routes through `BDManager->GrantAbsorptionEnergy` so the overload-aware ceiling (`MaxEP + 30%`) applies. Replaces the prior flat tier values (`BD_ENERGY_*` int constants).
4. Sets `bSuccess` on the `FItemUseResult` and broadcasts `OnItemUsed`.

`UseItemMultiTarget` loops `UseItem` and accumulates results. The executor applies effects
only — it does **not** manage inventory or consume item counts.

`IsAlly(User, Target)` (resolved via `UTurnManager::GetActorTeam`) lets handlers branch
buff-vs-debuff (Amber, Opal) or cleanse-vs-strip (Iolite).

### ECrystalType — the 10 gem crystals (+ 2 stones)

`ECrystalType` is a **single unified enum** of 12 values: the ten gem crystals
below (`Garnet … Quartz`) plus `DamageStone` and `AbilityStone`, the
augment-stone family. The stones share the enum and `FCrystalId` identity but are
**not** consumable crystals — see `AugmentStoneSystem.md`. The table below is the
ten gems.

| Crystal | Element | Effect role |
|---|---|---|
| Garnet | Fire | Fire DOT |
| Sapphire | Water | Healing / revive |
| Citrine | Lightning | Energy restore |
| Emerald | Wind | Delayed bonus turn (tier-scaled delay; S = immediate) |
| Amber | Earth | Defense buff / debuff |
| Opal | Light | Crit buff / debuff |
| Onyx | Darkness | Silence (energy-lock / binary) |
| Amethyst | Void | Gamble (random buff/debuff) |
| Iolite | Reality | Cleanse / strip |
| Quartz | Generic | Status-bar clear + elemental protection |

`ECrystalType::Quartz` is the last enum value — `ItemDataDebug` loops iterate up to it.

### EItemEffectType

`Damage, Healing, EnergyRestore, BuffRawDamage, BuffDefense, GrantBonusTurn, BuffCrit, Silence,
Cleanse, Gamble, StatusClear, Repair, None`. `ItemIdentity::GetItemEffectType()` maps each
`CrystalType` to one of these. `StatusClear` (Quartz) replaced the removed `Transform` value
— a redirect maps old `Transform` data to `StatusClear`. `DamageStone` maps to
`BuffRawDamage`; **`AbilityStone` maps to `None`** (attach-only — `UseItem` finds no handler
and rejects it as a non-usable consumable).

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
| Emerald | Bonus-turn delay (global turns) — `GetEmeraldBonusTurnDelay` | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| Amber | Defense buff/debuff % | 15 | 20 | 25 | 30 | 35 | 40 | 50 |
| Opal | Crit buff/debuff % | 5 | 8 | 10 | 12 | 15 | 18 | 25 |
| Amber/Opal | Duration — `GetCrystalDuration` | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
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

## Description Model

Every crystal needs distinct UI text for identity and effect; evolution
crystals also carry a designer-authored reveal field. These flow through three
independent getters and one authored field — never composed into a single
`Description`. Numerics come from `CrystalEffectTable`; this layer only
formats text on top of the FCrystalId-keyed value tables.

- **`CrystalDescription::GetCrystalText(const FCrystalId &Id)`** — shared
  identity sentence usable by any crystal kind. Format:
  `"A {tier-descriptor} {name-lowercase} crystal."`, e.g. Garnet S returns
  `"A legendary garnet crystal."`. Tier descriptors are F=crude, E=common,
  D=refined, C=quality, B=exceptional, A=masterwork, S=legendary (supplied by
  `GetTierDescriptor`).
- **`CrystalDescription::GetItemEffectText(const FCrystalId &Id)`** —
  mechanical-effect sentence for item / refined consumable crystals.
  Per-`CrystalType` switch with S-tier conditional alternates (Sapphire,
  Emerald, Onyx) and effect-count branches (Iolite); pluralises turn counts
  (`"1 turn"` vs `"N turns"`). Example (Garnet S):
  `"Applies a fire burn dealing 30% of target's max HP per turn for 1 turn."`
- **`UEvolutionItemData::GetEvolutionEffectText()`** — evolution-effect
  sentence composed from the asset's `BaseStatBonus` + `Effects`. Parallel to
  `GetItemEffectText`; also returns a self-contained sentence ending in `"."`.
  Editor-only (`#if WITH_EDITOR`).
- **`UEvolutionItemData::RevealedDescription`** (FString, designer-authored)
  — in-world reveal flavour shown when the crystal is revealed (slotting,
  Mind unlocking). The reveal mechanic is designed but not yet wired up.

`UEvolutionItemData::GenerateDescription()` produces the `Description` field
contents by calling `CrystalDescription::GetCrystalText` — so the asset's
`Description` is just the identity sentence. The `PostEditChangeProperty`
description-regen path regenerates it on first fill or Type/Tier change.
Effect text is queried separately at display time, never composed into
`Description`.

## Targeting Rules

Every crystal returns `ETargetType::SingleAnyone` from `UEvolutionItemData::GetItemTargetType()` —
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
- **Emerald S** — the granted bonus turn fires **immediately** (tier delay 0) via `UTurnManager::RequestExtraTurn`; lower tiers schedule the *same* bonus turn **delayed** (F=6 … A=1 global turns) via `ScheduleBonusTurn`. All tiers grant a (delayed/immediate) **bonus turn**, not a turn-speed buff. Using Emerald **forfeits the user's current turn** (the item use *is* the turn-ending action). Self-target = tempo (act again sooner); enemy-target = **force their turn** — their DoTs tick *and* they act, the gamble.
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

The command menu's `Item` selection reads `UEvolutionItemData::GetItemTargetType()` to open the
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
  `UItemExecutor::GetCharacterData`, `UEvolutionItemData::GetLightningBuildupPercent`,
  `ItemConstants::GENERIC_RESISTANCE_*` / `GENERIC_DURATION_*`, `FItemUseResult::GenericResistanceApplied`.
  The second tier of pre-redesign value getters and their `Display*` editor mirrors has
  been removed (see Changelog, 2026-05-18); `ItemDataDebug` now reads only the
  percentage-based getters.
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
| 2026-05-18 | Tier 2 dead-code cleanup — removed 11 pre-redesign value getters and their `Display*` editor mirrors from `UItemData`; `ItemDataDebug` validation/logging/tier tables modernised to the percentage-based getters; `CombatOrchestratorTestActor` item log updated | chore/tier2-dead-code |
| 2026-05-26 | Two-track storage split — item/refined crystals migrated from `UItemData` assets to `FCrystalId` + `CrystalEffectTable` / `CrystalIdentity` tables; 23 obsolete crystal assets deleted; legacy item-crystal authoring fields removed; description model split into `GetCrystalText` (shared) / `GetItemEffectText` (item) / `GetEvolutionEffectText` (evolution) | feature/crystal-evolution-refactor |
| 2026-05-26 | `UItemData` renamed to `UEvolutionItemData` (evolution-only); item-effect surface decoupled from the asset; `bImmuneToBreaking` flipped to `bCanBreak` (opt-in breaking, default false); dead asset-pointer slotting tower removed | feature/crystal-evolution-refactor |
| 2026-05-26 | Final field removal — `bIsEvolutionCrystal` deleted (`UEvolutionItemData` is now structurally evolution-only); 5 dead category helpers removed; `UInventoryComponent::AddItem`/`AddItemInternal` flag-routed dispatch deleted (modern routing via `UInventoryData` field dispatch only); `UEquipmentDataBase::SlottedCrystal` removed with its `PostLoad` migration | feature/crystal-evolution-refactor |
| 2026-05-27 | `bCanBreak` description note — overridable via `FEvolutionAttachment::ApplyWear(_, bForceWear=true)` for intrinsic mechanics (Broken Darkness). | feature/crystal-wear-substat-modifier |
| 2026-05-28 | Sweep-1 — BD crystal absorption energy rescaled to **% of target MaxEP** (F=10% .. S=70%, `ItemConstants::BD_ENERGY_PERCENT_*`). `CrystalEffectTable::GetBrokenDarknessEnergyBonus` renamed to `GetBrokenDarknessEnergyPercent`; `ItemExecutor::ApplyBrokenDarknessBonus` resolves `MaxEP × Percent` at the call site. Scales correctly with target's energy pool instead of granting a flat lump. | refactor/bd-crystal-absorption-percent |
| 2026-06-07 | Weapon-stone alignment — corrected the attachment-slot block (`Kind ∈ {None, Crystal, Evolution, WeaponStone}`, `CrystalType`/`CrystalTier` shared by `Crystal`+`WeaponStone`); reframed `ECrystalType` as 10 gems + 2 stones in one unified enum; `GetPrimaryEffectType` → `ItemIdentity::GetItemEffectType`; `EItemEffectType` gains `None` (`AbilityStone`). Weapon-stone family documented in new `AugmentStoneSystem.md`. | feature/weapon-stones |
