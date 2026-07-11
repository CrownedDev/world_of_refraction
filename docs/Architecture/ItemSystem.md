# Item System

## Overview

The item system covers **crystals** — consumable and equippable magical items
built on a ten-gem / 7-tier matrix (the unified `ECrystalType` enum also carries
the augment-stone family — see `AugmentStoneSystem.md`). The system splits into two
complementary tracks: **item and refined crystals** are identified by `FCrystalId`
(Type + Tier) and stored count-based in `UCrystalInventoryComponent`, with their
behaviour, names, and tier tables living in the `CrystalEffectTable` and
`ItemIdentity` namespaces; **evolution crystals** are authored individually
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
target type live in the `CrystalEffectTable` and `ItemIdentity` namespaces
(inline tables keyed by `FCrystalId`; the namespace was renamed from
`CrystalIdentity`, commit `291ce39d`). Refined and item crystals are fungible
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
  `Kind ∈ {None, Crystal, Evolution, AugmentStone, Fusion}` (`EAttachedItemKind.h`)
  + `CrystalType` / `CrystalTier` (for `Crystal` **and** `AugmentStone` — both carry
  an `FCrystalId` identity), `Evolution` (`UEvolutionItemData*`), or a `FFusionId`
  (two `FCrystalId` halves + a bonus stat) for the `Fusion` Kind.
- `FRuntimeAttachedItem` (runtime, on `FWeaponInventoryEntry::AttachedItem`
  and `FRingInventoryEntry::AttachedItem`): same shape plus per-instance
  durability and a `FGuid` for evolution identity.

The `AugmentStone` Kind is the **augment-stone family** — now the full stat-stone
set (`DamageStone`, `AbilityStone`, `DefenseStone`, `CritStone`, `TurnSpeedStone`,
`StatusStone`, `EfficiencyStone`, `MaxHPStone`, `MaxEPStone`, `SpellDamageStone`,
`ResistanceStone`, `SpellSpeedStone`, `ActionSpeedStone`, `DurabilityStone`,
`LuckStone`, `ReflexStone`) — a second attachment family sharing this slot and the
`FCrystalId` identity with crystals. The `Fusion` Kind is a third family (player-fused
pairs, `FFusionId`). Both are documented separately in `AugmentStoneSystem.md`; this
doc covers the crystal (gem) and evolution tracks.

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
- **No refinement axis.** `bIsRefined` was removed — every evolution crystal
  is slottable as-is (attaches directly onto weapons/rings; no cut/refine
  step). The `CanBeSlotted()` / `CanHaveSpells()` / `IsRefined()` helpers
  survive for Blueprint compatibility but now unconditionally return `true`.
- `Breakability` (`EBreakability`) — who wears/breaks the crystal:
  `Breakable` (default — any class wears it down; evolutions are gear and wear
  in combat), `BDBreakable` (only Broken Darkness / Reality wielders),
  `Unbreakable` (no one). The intrinsic-mechanic override
  `FEvolutionAttachment::ApplyWear(_, bForceWear=true)` bypasses the gate —
  Broken Darkness uses it so per-asset authoring can't silently disable BD's
  wear (see `CrystalWear.md`).
- `MaxDurability`, `CurrentDurability` — every evolution asset tracks
  durability (no longer gated on a refinement flag). `MaxDurability` defaults
  from `Tier` if authored 0 (`PostInitProperties` / `PostLoad`).
- `EvolutionType` (`EEvolutionType`) — Balanced / Mind / Body / Spirit / etc.
- `BaseStatBonus` (`FEquipmentStatBonus`) — evolution stat modifiers. The
  embedded struct's `ClampMin=0` UPROPERTY meta can't be overridden at the
  embedding site, so out-of-range values (crystals permit negatives down to
  `CRYSTAL_BONUS_MIN`) surface as `IsDataValid` warnings. Supersedes the
  deprecated legacy pillar fields (`Mind/Body/SpiritModifierPercent`):
  `PostLoad` copies legacy values into `BaseStatBonus` on load (in-memory;
  persists only on asset re-save). `PostLoad`/`PostInitProperties` are
  declared **outside** `WITH_EDITOR` so the migration runs in packaged builds
  too. The legacy fields are deletion-pending once all crystal assets are
  confirmed re-saved.
- `Effects` (`TArray<FSkillEffect>`) — skill effects granted by the evolution
  crystal (passives + triggered).

Quartz is consumable-only and **cannot exist as a `UEvolutionItemData`
asset**. `IsDataValid` rejects any `UEvolutionItemData` with
`CrystalType == Quartz` (error: `"Quartz crystals cannot be evolution crystals — they are consumable only"`).
Quartz exists only as `FCrystalId{Quartz, Tier}` in the `ItemCrystals` pool.
(The old `PostEditChangeProperty` force-clear of `bIsRefined` on a Quartz
switch went away with the refinement flag.)

### UItemExecutor (`UGameInstanceSubsystem`)

Runs item use in combat. Entry point `UseItem(User, Item, Target)`:

1. Validates user/item; defaults `Target` to `User` if null.
2. Switches on the item's `ItemIdentity::GetItemEffectType(FCrystalId)` to a per-effect handler (`ExecuteDamageEffect`, `ExecuteHealingEffect`, …, `ExecuteStatusClearEffect`). *(Formerly `GetPrimaryEffectType()`; renamed in commit `5c22e6e`.)*
3. If the **target** is a Broken Darkness character, `ApplyBrokenDarknessBonus` applies a now **element-aware three-way** interaction (`fix/bd-item-absorption-element`). The tier-scaled amount is the same — **% of target MaxEP**, F=10% .. S=70% (`CrystalEffectTable::GetBrokenDarknessEnergyPercent(Id) × TargetComp->MaxEP`) — but what happens depends on `ItemIdentity::GetElement(Id)`:
   - **Real element** (Fire/Water/Earth/Wind/Light/Darkness/Lightning/Void) → `BDManager->GrantAbsorptionEnergy(Amount, Element)`: **grants** the energy (overload-aware ceiling `MaxEP + 30%`) **and rotates** the BD's active pool to that element.
   - **Reality** (Iolite) → `BDManager->DrainAndRevertToBase(Amount)`: **drains** that energy + **reverts** the active pool to base Darkness (the cleanse) — see `BrokenDarkness.md`.
   - **None** (Quartz) → **no absorption interaction** (no energy, no rotation). *(Previously every crystal fell into the grant path; Quartz now no-ops.)*
   `OutResult.BrokenDarknessEnergyGained` is the honest `CurrentEP` delta (positive grant / negative drain / 0 no-op). Replaces the prior flat tier values (`BD_ENERGY_*` int constants).
4. Sets `bSuccess` on the `FItemUseResult` and broadcasts `OnItemUsed`.

`UseItemMultiTarget` loops `UseItem` and accumulates results. The executor applies effects
only — it does **not** manage inventory or consume item counts.

`IsAlly(User, Target)` (resolved via `UTurnManager::GetActorTeam`) lets handlers branch
buff-vs-debuff (Amber, Opal) or cleanse-vs-strip (Iolite).

### ECrystalType — the 10 gem crystals (+ the augment-stone family)

`ECrystalType` is a **single unified enum** (`CrystalType.h`): `None` + the ten gem
crystals below (`Garnet … Quartz`) + the **augment-stone family** (`DamageStone`,
`AbilityStone`, `DefenseStone`, `CritStone`, `TurnSpeedStone`, `StatusStone`,
`EfficiencyStone`, `MaxHPStone`, `MaxEPStone`, `SpellDamageStone`, `ResistanceStone`,
`SpellSpeedStone`, `ActionSpeedStone`, `DurabilityStone`, `LuckStone`, `ReflexStone`)
— ~26 named values. The stones share the enum and `FCrystalId` identity but are
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
| Quartz | None (non-elemental) | Status-bar clear + elemental protection |

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
| Sapphire | Last Stand ward window (turns) — `GetLastStandWindow` | 2 | 2 | 3 | 3 | 4 | 4 | 5 |
| Healing Stone | Heal % of MaxHP — `GetHealPercent` | 15 | 20 | 25 | 30 | 35 | 45 | 60 |
| Citrine | EP restore % of MaxEP | 30 | 40 | 50 | 60 | 70 | 85 | 100 |
| Emerald | Bonus-turn delay (global turns) — `GetEmeraldBonusTurnDelay` | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| Amber | Defense buff/debuff % — `GetBuffPercentage` | 6 | 10 | 14 | 18 | 22 | 26 | 30 |
| Opal | Crit buff/debuff % — `GetCritBuffPercent` | 6 | 10 | 14 | 18 | 22 | 26 | 30 |
| Amber/Opal | Duration — `GetCrystalDuration` | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Onyx | EP drain % of MaxEP (F–A one-shot) — `GetSilencePercentage` | 15 | 30 | 30 | 50 | 70 | 70 | 100 |
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
- **Onyx** — F–A spend the drain % above as a **one-shot EP drain** (instant, no lingering
  lock, no per-tier duration). **S-rank** instead applies a binary `Silenced` effect for 1 turn
  (see S-Rank Specials) — there is no `GetSilenceDurationNew` getter.
- **Sapphire** is now defy-death: living target → Last Stand ward (survive at 50% MaxHP if it
  would die in the window); dead target → revive at 30% MaxHP. The 50%/30% values are fixed;
  only the ward window scales by tier. The plain heal moved to **Healing Stone** (`GetHealPercent`).
- **Amber/Opal/Emerald-speed** share one curve, `STAT_CRYSTAL_BUFF_PERCENT` (6→30).
- **Citrine** no longer has an HP cost. **Iolite** no longer grants immunity.

## Description Model

Every crystal needs distinct UI text for identity and effect; evolution
crystals also carry a designer-authored reveal field. These flow through three
independent getters and one authored field — never composed into a single
`Description`. Numerics come from `CrystalEffectTable`; this layer only
formats text on top of the FCrystalId-keyed value tables.

- **`CrystalDescription::GetCrystalText(const FCrystalId &Id)`** — shared
  identity sentence usable by any crystal kind. Gems:
  `"A {tier-descriptor} {name-lowercase} crystal."`, e.g. Garnet S returns
  `"A legendary garnet crystal."`. Stones take their own branch
  (`IsAugmentStoneType`) — a stone is not a crystal:
  `"A {tier-descriptor} {Spaced Name}."` using the spaced display form of
  `ItemIdentity::GetTypeName` (which runs stone enum names through
  `FName::NameToDisplayString`, so `DamageStone` renders `"Damage Stone"`;
  gem names are single words and keep the literal switch). Tier descriptors
  are F=crude, E=common, D=refined, C=quality, B=exceptional, A=masterwork,
  S=legendary (supplied by `GetTierDescriptor`).
- **`CrystalDescription::GetItemEffectText(const FCrystalId &Id)`** —
  mechanical-effect sentence for item / refined consumable crystals.
  Per-`CrystalType` switch with S-tier conditional alternates (Sapphire,
  Emerald, Onyx) and effect-count branches (Iolite); pluralises turn counts
  (`"1 turn"` vs `"N turns"`). Example (Garnet S):
  `"Applies a fire burn dealing 30% of target's max HP per turn for 1 turn."`
  Also consumed by the shop detail panel's `Effect:` line (3j — the full
  mechanical sentence, numbers included, replaces the enum-display
  parenthetical; see [`MerchantShopSystem.md`](./MerchantShopSystem.md)).
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

Every crystal returns `ETargetType::Anyone` from `UEvolutionItemData::GetItemTargetType()` —
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
| 2026-06-11 | `WITH_EDITOR` guard fix — `UEvolutionItemData::PostLoad`/`PostInitProperties` were declared inside `#if WITH_EDITOR` but defined outside it: packaged builds failed to compile, and guarding the definitions instead would have skipped the legacy pillar→`BaseStatBonus` migration in shipping (dropping un-re-saved assets' pillar data). Declarations moved out of the guard — the pillar + durability migrations now run in all build configs. Legacy pillar fields remain deprecated, deletion-pending on confirmed content re-save. | chore/legacy-cleanup |
| 2026-06-16 | Doc-sync: namespace `CrystalIdentity` → `ItemIdentity` in the body (`291ce39d`); `FAttachedItem::Kind` gains `Fusion` (+`FFusionId`); the `ECrystalType` "12 values / 10 gems + 2 stones" framing corrected to the full augment-stone family (~26 values); Overview de-"10-type"-ified. (Historical changelog rows keep their period names.) | feature/realtime-defense |
| 2026-06-21 | `ApplyBrokenDarknessBonus` is now **element-aware** (`fix/bd-item-absorption-element`): the three-way on `ItemIdentity::GetElement(Id)` — real element → `GrantAbsorptionEnergy(Amount, Element)` (grant + rotate the BD's active pool); Reality (Iolite) → `DrainAndRevertToBase(Amount)` (drain + revert to base Darkness, the cleanse); None (Quartz) → no-op (was a flat grant — removed). Updated step 3 of the use flow. **Stale-fact fix:** the crystal table listed `Quartz | Generic` — corrected to `None (non-elemental)` (post the `Generic→None` element-sentinel migration). | fix/bd-item-absorption-element |
| 2026-06-22 | **Sapphire → defy-death** (back-fill; no longer a heal): living target → grants a `LastStand` ward (the `CheckDeath` intercept), dead target → revives (`ServerResurrect`, any tier); routes via `EItemEffectType::DefyDeath`. The instant **heal relocated to the consume-only `HealingStone`** (`EItemEffectType::RestoreHealth` → `ExecuteHealingStoneEffect`; inert if attached — no `StoneTargetStat`). `EItemEffectType::Healing` is now **dead** (superseded by `DefyDeath` + `RestoreHealth`). | item/effect arcs |
| 2026-06-22 | Cleanup: handler `ExecuteHealingEffect` (Sapphire defy-death, a misnomer since the reshape) renamed `ExecuteDefyDeathEffect`; pure rename, no behaviour change. Stale `ECrystalType` inline `=N` enum-position comments removed (they had drifted 1–3 from real values; append-only positions are what matter). | feature/effect-build-unification |
| 2026-06-23 | **Crystal Behaviour table stale-row fixes** (doc-only; values reconciled to live `CrystalEffectTable`): Sapphire row was still the old heal curve — replaced with the Last Stand ward window (`GetLastStandWindow`, 2/2/3/3/4/4/5) and a new Healing Stone row for the relocated heal (`GetHealPercent`, 15–60). Amber (`GetBuffPercentage`) and Opal (`GetCritBuffPercent`) corrected to the shared `STAT_CRYSTAL_BUFF_PERCENT` curve (6/10/14/18/22/26/30) — both were pre-`STAT_CRYSTAL_BUFF_PERCENT` literals. Removed the phantom "Onyx energy-lock duration / `GetSilenceDurationNew`" row (no such getter; F–A is a one-shot drain, S-rank a binary 1-turn `Silenced`). Mechanics docs (`Crystals.md`, `AugmentStones.md`) gained player-facing per-tier value tables. | docs/item-tier-value-tables |
| 2026-07-11 | **`UEvolutionItemData::bIsRefined` REMOVED** — evolution crystals are always slottable (no refinement step). Durability init (`PostInitProperties`/`PostLoad` tier-defaulting) and the `MaxDurability`/`Breakability` edit conditions no longer gate on it; `CanBeSlotted`/`CanHaveSpells`/`IsRefined` retained as always-true BP-compat helpers; the Quartz `PostEditChangeProperty` force-clear went with the flag. Doc also caught up on the pre-existing `bCanBreak` → 3-state `Breakability` swap (code comments swept on-branch). | feature/hub-merchants |
| 2026-07-11 | **Stone display naming + shop effect text (3j).** `ItemIdentity::GetTypeName` renders stone enum names in spaced display form via `FName::NameToDisplayString` (`DamageStone` → `"Damage Stone"`); `CrystalDescription::GetCrystalText` gives stones their own identity sentence (`"A {tier} {Spaced Name}."` — a stone is not "a … crystal"). The shop detail panel's `Effect:` line now uses `GetItemEffectText`'s full mechanical sentence (Crown-accepted numbers on stone text); Sapphire and DamageStone effect texts reworded/trimmed. | feature/hub-merchants |
