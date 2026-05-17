# Item System

## Overview

The item system covers **crystals** — consumable and equippable magical items built on a
10-type / 7-tier matrix. Crystals are authored as `UItemData` primary data assets and
executed in turn-based combat by the `UItemExecutor` subsystem. Each crystal maps to one
of the nine elements (plus Generic) and to a single combat effect — damage, healing,
buffs, status effects, etc. Tiers F→S scale the effect's magnitude.

> This document reflects the item system as of the `feature/item-system-redesign` branch.
> Crystal **values** are still the original flat tables in shipped code; the
> percentage-based redesign is specced but **not yet implemented** (see Crystal Behaviour).

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
tables inside `UItemData`'s `Get*()` functions (`GetDamageValue`, `GetEnergyValue`,
`GetBuffPercentage`, `GetSilenceDuration`, …). Quartz is **consumable-only**: `IsDataValid`
and `PostEditChangeProperty` reject / clear `bIsRefined` and `bIsEvolutionCrystal` on Quartz.

### UItemExecutor (`UGameInstanceSubsystem`)

Runs item use in combat. Entry point `UseItem(User, Item, Target)`:

1. Validates user/item; defaults `Target` to `User` if null (self-target items).
2. Switches on `Item->GetPrimaryEffectType()` to a per-effect handler (`ExecuteDamageEffect`, `ExecuteHealingEffect`, …).
3. Always applies the Generic-class elemental-resistance bonus and the Broken-Darkness energy bonus.
4. Sets `bSuccess` on the `FItemUseResult` and broadcasts `OnItemUsed`.

`UseItemMultiTarget` loops `UseItem` and accumulates results. The executor applies effects
only — it does **not** manage inventory or consume item counts.

### ECrystalType — the 10 crystals

| Crystal | Element | Effect role |
|---|---|---|
| Garnet | Fire | Damage |
| Sapphire | Water | Healing |
| Citrine | Lightning | Energy restore |
| Emerald | Wind | Speed buff |
| Amber | Earth | Defense buff |
| Opal | Light | Crit buff / info reveal |
| Onyx | Darkness | Silence |
| Amethyst | Void | Gamble (random buff/debuff) |
| Iolite | Reality | Cleanse |
| Quartz | Generic | Status clear (consumable-only) |

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

> **Phase 2 of `feature/item-system-redesign` will replace the current flat values with the
> percentage-based design below.** Until then the flat tables are authoritative in code.

Tier order in all tables: **F / E / D / C / B / A / S**.

### Current Implementation (flat values)

| Crystal | Metric | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| Garnet | Damage | 60 | 75 | 95 | 120 | 150 | 180 | 220 |
| Sapphire | Heal | 60 | 75 | 95 | 120 | 150 | 180 | 220 |
| Citrine | Energy restore | 20 | 25 | 35 | 45 | 60 | 80 | 100 |
| Citrine | HP cost | 10 | 10 | 10 | 15 | 15 | 20 | 25 |
| Emerald | Speed buff % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| Amber | Defense buff % | 15 | 20 | 25 | 30 | 35 | 40 | 50 |
| Opal | Crit buff % | 5 | 8 | 10 | 12 | 15 | 18 | 20 |
| Emerald/Amber/Opal | Buff duration | 3 | 3 | 4 | 4 | 5 | 5 | 6 |
| Onyx | Silence % | 15 | 30 | 30 | 50 | 70 | 70 | 100 |
| Onyx | Silence duration | 1 | 1 | 2 | 2 | 2 | 3 | 1 |
| Iolite | Debuffs removed | 1 | 1 | 2 | 2 | 3 | 3 | 0* |
| Iolite | Immunity duration | 1 | 2 | 1 | 2 | 2 | 3 | 0 |

- Garnet S adds a burn DOT: 15 damage/turn for 3 turns (flat, S-tier only).
- \* Iolite S `GetDebuffsToRemove()` returns 0 (intended "remove all") — currently broken, see Known Limitations.
- Amethyst has no flat value table — gamble magnitude reads `GetBuffPercentage()`, which returns 0 for Amethyst (see Known Limitations).
- Quartz had a flat transform-threshold table (200→750); the transform system and its getter were removed on `feature/item-system-redesign`.

### Designed Values — Pending Phase 2 (percentage based)

| Crystal | Metric | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| Garnet | DOT damage %/turn | 5 | 7 | 9 | 12 | 16 | 20 | 30 |
| Garnet | DOT duration | 3 | 3 | 3 | 2 | 2 | 2 | 1 |
| Sapphire | Heal % of max HP | 15 | 20 | 25 | 30 | 35 | 45 | 60 |
| Citrine | EP restore % of max EP | 30 | 40 | 50 | 60 | 70 | 85 | 100 |
| Citrine | Lightning buildup % | 15 | 20 | 30 | 40 | 55 | 70 | 70 |
| Emerald | Speed buff % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| Emerald | Duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Amber | Defense buff/debuff % | 15 | 20 | 25 | 30 | 35 | 40 | 50 |
| Amber | Duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Opal | Crit buff/debuff % | 5 | 8 | 10 | 12 | 15 | 18 | 25 |
| Opal | Duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Onyx | Silence % | 15 | 30 | 30 | 50 | 70 | 70 | 100 |
| Onyx | Duration | 3 | 3 | 2 | 2 | 2 | 1 | 1 |
| Amethyst | Buff/debuff % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| Amethyst | Duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Amethyst | Buff chance % | 10 | 20 | 30 | 40 | 50 | 60 | 70 |
| Iolite | Debuffs/buffs removed | 1 | 1 | 2 | 2 | 3 | 3 | 99 |
| Iolite | Immunity duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| Quartz | Status bar cleared % | 25 | 35 | 45 | 55 | 65 | 80 | 100 |
| Quartz | Resistance duration | 4 | 4 | 3 | 3 | 3 | 2 | 2 |

Designed mechanic changes vs current: Citrine drops its HP cost; Garnet becomes a pure DOT;
Quartz becomes an enemy status-bar cleaner; Amethyst gains an explicit buff-chance roll;
Iolite S uses `99` as the remove-all sentinel (fixing the current bug).

## Targeting Rules

| Crystal | Target |
|---|---|
| Garnet | Enemy |
| Sapphire | Ally or self |
| Citrine | Self only — handler uses `User`, ignores `Target` |
| Emerald | Ally or self |
| Amber | Ally or self |
| Opal | Ally or self (S-rank reveal reads the enemy) |
| Onyx | Enemy |
| Amethyst | Self — gamble applies to `User` |
| Iolite | Ally or self |
| Quartz | Enemy (designed — clears the target's status bar) |

`UseItem` defaults a null `Target` to `User`, so self-target crystals work without an
explicit target.

## S-Rank Specials

### Implemented

- **Garnet S** — only tier with a secondary effect: burn DOT (15/turn × 3 turns).
- **Opal S** — flags enemy HP + stat reveal (`GetRevealsHP` / `GetRevealsStats`); the reveal
  broadcast itself is an unimplemented `// TODO` in `ExecuteCritBuffEffect`.
- **Onyx S** — 100% silence (full energy lock vs partial lock at lower tiers); duration drops to 1.
- **Iolite S** — intended to remove all debuffs (currently broken — see Known Limitations).

### Designed (Phase 2)

- **Sapphire S** — revive a dead target at 30% HP.
- **Emerald S** — grant an extra turn instead of the speed buff.
- **Iolite S** — debuffs-removed becomes `99` (remove-all sentinel), fixing the bug below.

## Integration Points

### → SkillEffectManager (`UGameInstanceSubsystem`)

`UItemExecutor::GetSkillEffectManager()` lazily fetches and caches the subsystem. The buff,
debuff, silence, gamble, cleanse, Generic-resistance and Garnet-burn handlers each build an
`FActiveSkillEffect` and call `SkillEffectManager->ApplyEffect(Target, Effect, Source,
SourceName, SourceTeam)`. Cleanse additionally calls `RemoveAllDebuffs`, `RemoveEffectByID`
and `GetDebuffCount`.

### → StatusBuildupManager (`UGameInstanceSubsystem`)

**Not currently connected.** `UItemExecutor` holds no reference to `StatusBuildupManager`.
The Phase 2 designed mechanics — Citrine's lightning buildup and Quartz's status-bar clear
— will require this integration; it does not exist yet.

### → CharacterDataComponent (`UActorComponent`)

`GetCharacterDataComponent(Actor)` resolves via `FindComponentByClass`. Damage/heal/energy
handlers call `ServerTakeDamage`, `ServerHeal`, `ServerGainEnergy` and read `CurrentHP` /
`CurrentEP`. `GetCharacterData()->InnateElement` drives the Generic-class resistance bonus
and the Broken-Darkness energy bonus applied to every item use.

### OnItemUsed delegate

`DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, AActor* User, UItemData* Item,
const FItemUseResult& Result)` — broadcast at the end of every successful `UseItem`. UI and
listeners bind to it for feedback. `OnGambleResult` is a second delegate for Amethyst
outcomes. `FOnQuartzTransformed` was removed with the transform system.

## Known Limitations / TODOs

### Phase 2 — pending implementation

- **`ExecuteStatusClearEffect` does not exist.** Quartz returns `EItemEffectType::StatusClear`
  from `GetPrimaryEffectType()`, but `UseItem`'s switch has no handler — using a Quartz item
  hits `default` and returns `"Unknown item effect type"`.
- Percentage-based crystal values (see Crystal Behaviour) replace the flat tables, including
  new getters (`GetDOTDamagePercent`, `GetHealPercent`, `GetEPRestorePercent`,
  `GetLightningBuildupPercent`, `GetBuffChancePercent`, `GetStatusClearPercent`, …).
- Citrine lightning buildup + Quartz status-bar clear require `StatusBuildupManager` wiring.
- Sapphire S revive and Emerald S extra-turn specials.

### Existing bugs / placeholders

- **Amethyst gamble magnitude is 0** — `ExecuteGambleEffect` reads `GetBuffPercentage()`,
  which has no Amethyst case and returns 0; every gamble applies a 0-magnitude effect.
- **Iolite S removes 0 debuffs** — `GetDebuffsToRemove()` returns 0 for S-tier (meant as
  "all"), but `ExecuteCleanseEffect` only triggers remove-all on `>= 99`. Phase 2 sets it to 99.
- **Onyx silence** uses `ESkillEffectType::EnergyDrain` as a placeholder — no real silence type.
- **Opal S reveal** is a `// TODO` log line; no reveal event is broadcast to UI.
- **Cleanse immunity** is applied as a 100% `ResistanceBuff` pseudo-immunity (no dedicated
  debuff-immunity effect type exists).
- Self-targeted buff items with duration 1 expire at the end of the casting turn (effect
  durations tick down at the end of the affected actor's own turn), giving no benefit —
  designers must tune self-buff durations to ≥ 2.

### Phase 3 — further out

- **Item inventory / consumption.** `UItemExecutor` applies effects but never decrements
  item counts or checks ownership — inventory wiring is unbuilt.
- Full Quartz `StatusClear` mechanic, including `StatusBuildupManager` integration.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Quartz transform system removed, StatusClear added | feature/item-system-redesign |
| 2026-05-17 | ReduceDamageTaken/IncreaseDamageTaken split | feature/item-system-redesign |
| 2026-05-17 | IsDebuff() fixed for 8 misclassified types | feature/item-system-redesign |
| 2026-05-17 | Quartz consumable-only restriction added | feature/item-system-redesign |
