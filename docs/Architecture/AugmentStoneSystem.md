# Augment Stone System

## Overview

Augment stones are a second family of equipment **attachments**, sitting alongside
crystals and evolution items in the same attachment slot. Where a gem crystal
grants an element and spells, a augment stone grants a **mechanical bonus** to the
weapon it is fused into:

- **`DamageStone`** — a flat whole-percent raw-damage increase. Doubles as a
  consumable (a temporary self-buff).
- **`AbilityStone`** — grants a tier-scaled number of **ability slots** on the
  weapon. Attach-only (it has no consumable effect).

The defining design constraint is that stones and gems share **one identity
type** (`ECrystalType` / `FCrystalId`) and **one attachment slot**
(`FAttachedItem` / `FRuntimeAttachedItem`, discriminated by `EAttachedItemKind`).
The enum is deliberately *not* split into "gems" and "stones" — the planned
FusionStone (see *Design / Phase 2*; formerly *MasteryStone* in earlier design)
fuses **any two attachables**, so both fusion inputs need a common identity type.

> This document covers the **shipped** augment-stone work and a body of **design
> rationale** for it. **Much of what the *Design / Phase 2* sections below frame as
> "not-yet-built" has since SHIPPED** (`feature/realtime-defense` and later): the full
> **stat-stone family** (`DefenseStone`, `CritStone`, `TurnSpeedStone`, `StatusStone`,
> `EfficiencyStone`, `MaxHP/MaxEPStone`, `SpellDamageStone`, `ResistanceStone`,
> `SpellSpeedStone`, `ActionSpeedStone`, `DurabilityStone`, `LuckStone`, `ReflexStone`),
> and **FusionStone** identity + slot + production wear (`FFusionId`,
> `EAttachedItemKind::Fusion`, `FFusionAttachment` — see `CrystalWear.md`). Treat the
> *Design / Phase 2* prose as the original design record; the **status note at that
> fence** lists what landed. Still genuinely unbuilt: the speed-stone→defense-window
> link and the Resistance spell-shave hook.
>
> Related docs: `ItemSystem.md` (crystals + `FCrystalId`), `WeaponSystem.md`
> (`UEquipmentDataBase` / `UWeaponData` / `URingData`), `LoadoutSystem.md`
> (ability slots, crystal queries), `DamageCalculator.md` (the damage pipeline).

---

## Architecture (implemented)

### Unified attachment identity — `ECrystalType` / `FCrystalId`

`ECrystalType` (`Equipment/Crystals/CrystalType.h`) holds the gem crystals **and**
the whole augment-stone family in one enum: `None` (0, hidden) + the ten gems
`Garnet … Quartz` + the stone family (`DamageStone`, `AbilityStone`, `DefenseStone`,
`CritStone`, `TurnSpeedStone`, `StatusStone`, `EfficiencyStone`, `MaxHPStone`,
`MaxEPStone`, `SpellDamageStone`, `ResistanceStone`, `SpellSpeedStone`,
`ActionSpeedStone`, `DurabilityStone`, `LuckStone`, `ReflexStone`) — ~26 named values.
Every value is **append-only**: the header marks each as the serialized `.uasset` /
SaveGame identity, so values must not be reordered or inserted above existing ones.

`FCrystalId` is the `{ECrystalType Type, EItemTier Tier}` pair that uniformly
identifies **gems and stones alike**. Stones carry a tier exactly as gems do —
a `DamageStone (B)` is `FCrystalId{DamageStone, B_Tier}` — and every consumer
(`ItemIdentity`, `CrystalEffectTable`, the runtime attachment) reads
`Crystal.Id` without caring whether the identity is a gem or a stone.

### `EAttachedItemKind` — the slot discriminator

`Equipment/EAttachedItemKind.h`:

| Value | Serialized | Meaning |
|---|---|---|
| `None` | 0 | empty slot |
| `Crystal` | 1 | gem crystal (element + spells) |
| `Evolution` | 2 | evolution item asset |
| `AugmentStone` | 3 | augment stone (appended after `Evolution`; serialized value stable) |
| `Fusion` | 4 | player-fused pair — exposes `FFusionId` (two `FCrystalId` halves + a bonus stat) instead of a single `FCrystalId` |

A slot is exactly one `Kind`. The **serialized order is `None, Crystal, Evolution,
AugmentStone, Fusion`** — each is appended to keep on-disk `Kind` values stable
(`AugmentStone` = `3`, `Fusion` = `4`).

### `FAttachedItem` (design-time) vs `FRuntimeAttachedItem` (runtime)

The attachment point is a **discriminated union**, authored design-time on the
equipment asset and inflated to a runtime struct on the owned inventory entry.

**`FAttachedItem`** (`Equipment/FAttachedItem.h`, on
`UEquipmentDataBase::AttachedItem`):

| Field | Type | Visible when |
|---|---|---|
| `Kind` | `EAttachedItemKind` | always |
| `CrystalType` | `ECrystalType` | `Kind == Crystal \|\| AugmentStone` |
| `CrystalTier` | `EItemTier` | `Kind == Crystal \|\| AugmentStone` |
| `Evolution` | `UEvolutionItemData*` | `Kind == Evolution` |

`CrystalType` / `CrystalTier` are shared by gems and stones — a augment stone's
identity is `FCrystalId{CrystalType (= DamageStone/AbilityStone), CrystalTier}`.
The `CrystalType` dropdown is filtered by `Kind` (see *Kind/type filter* below).

**`FRuntimeAttachedItem`** (`Equipment/FRuntimeAttachedItem.h`, on
`FWeaponInventoryEntry::AttachedItem` / `FRingInventoryEntry::AttachedItem`):

| Field | Type | Notes |
|---|---|---|
| `Kind` | `EAttachedItemKind` | `SaveGame` |
| `Crystal` | `FCrystalAttachment` | identity (`Crystal.Id`) + per-instance durability — carries **both** `Crystal` and `AugmentStone` Kinds |
| `Evolution` | `FEvolutionAttachment` | evolution branch |

Predicates: `IsEmpty()`, `IsCrystal()`, `IsEvolution()`, `IsAugmentStone()`.
`FromAttachedItem(const FAttachedItem&)` is the bridge factory. A augment stone
stores its `FCrystalId` in `Crystal.Id` so downstream code reads stone identity
through the same `Crystal` branch a gem uses.

> **Flagged code-comment drift (not fixed here — code only, doc task):** the
> header comments in `FRuntimeAttachedItem.h` (the type summary and the
> `FromAttachedItem` doc-comment, ~lines 6–10 and 99–114) still say
> "Refined" / `Source.RefinedType` / `Source.RefinedTier`. The fields are
> actually `Crystal` / `CrystalType` / `CrystalTier`, and `Kind` no longer has a
> `Refined` value. The comments predate the Crystal/AugmentStone rename and are
> stale; the *behaviour* is correct.

### The AugmentStone family

#### `DamageStone` — raw-damage percent

`CrystalEffectTable::GetDamageStoneBasePercent(FCrystalId)` returns a
whole-number percent, `0` for any non-`DamageStone`:

| Tier | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| Raw-damage % | 3 | 5 | 7 | 9 | 11 | 13 | 15 |

This single table feeds **both** the attached path (a direct damage multiplier)
and the consumable path (a temporary buff) — see *How It Works*.

#### `AbilityStone` — ability slots

`CrystalEffectTable::GetAttachmentSlotsForTier(FCrystalId)` returns the slot
count a **crystal** attachment grants, gating on crystal type (`AbilityStone` for
abilities, gems for spells). Its sibling `CrystalEffectTable::SlotsForContainerTier(EItemTier)`
reads the **same** curve keyed on a **container's own tier** with no type gate —
now the source of weapon ability slots and weapon/ring/evolution native (no-gem)
spell slots. The one shared curve:

| Tier | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| Slots | 1 | 2 | 3 | 4 | 5 | 6 | 6 |

`DamageStone`, gems, evolution, and `None` all return `0` from the `FCrystalId`
overload. The progression is linear F→A, then S holds at 6 (A and S share 6 — S
leads on power/quality, not slot count).

`AbilityStone` has **no consumable effect**: `ItemIdentity::GetItemEffectType`
maps it to `EItemEffectType::None`, so `UItemExecutor::UseItem` finds no handler
and falls to its "not a usable consumable" default. It is attach-only.

### AbilityStone ability plumbing (loadout)

The slots an `AbilityStone` grants are a **separate ability list** from the
weapon's own `PresetAbilities`, threaded through the loadout layer:

- `FWeaponLoadoutEntry::AssignedAugmentStoneAbilities` — the per-loadout override
  list for stone-granted slots (parallel to `AssignedAbilities`).
- `FWeaponLoadoutEntry::GetAugmentStoneAbilities()` — sequential override merge,
  same shape as `GetAllAbilities()`.
- `FWeaponLoadoutEntry::ValidateAugmentStoneAbilities(OwnedAbilities)` — validates
  ownership and caps the count at `GetAttachmentSlotsForTier(Attachment.Crystal.Id)`.
  With no augment stone attached the slot limit is `0`, so any assigned
  stone-abilities are rejected.
- `FSavedLoadout::PrimaryAugmentStoneAbilities` / `SecondaryAugmentStoneAbilities`
  — the authored storage; `FCombatLoadout::CreateFromSavedLoadout` copies them
  into `PrimaryWeapon` / `SecondaryWeapon.AssignedAugmentStoneAbilities`.

### Kind/type dropdown filter + `IsDataValid` backstop

The single `CrystalType` field serves both `Crystal` and `AugmentStone` Kinds, so
its raw dropdown would offer all 12 enum values for either Kind. Two mechanisms
constrain it (full engine trace recorded in the `feature/weapon-stones` survey):

1. **Editor grey-out — `meta=(GetRestrictedEnumValues="GetRestrictedCrystalTypes")`**
   on `FAttachedItem::CrystalType`. `UEquipmentDataBase::GetRestrictedCrystalTypes()`
   runs on the owning asset (UE resolves `GetRestrictedEnumValues` against the
   outer `UObject`, not the struct — which is why it can read the sibling
   `AttachedItem.Kind`, exactly like `IsAugmentStoneAttached()` does for its
   `EditCondition`). It returns the **wrong-Kind** value names (short form, e.g.
   `"Garnet"` / `"DamageStone"`) to restrict: stones greyed when `Kind==Crystal`,
   gems greyed when `Kind==AugmentStone`. Restricted values are **greyed and
   non-selectable, not hidden** — true hiding would need an `IDetailCustomization`
   editor module, which the project declines. The list is built from the
   `CrystalTypeHelpers` predicates via enum reflection, so it cannot drift.
2. **Hard backstop — `UEquipmentDataBase::IsDataValid`.** Editor-independent
   enforcement: `Kind==Crystal` with a non-gem `CrystalType` → error;
   `Kind==AugmentStone` with a non-stone `CrystalType` → error. Lives on the base
   so both `UWeaponData` and `URingData` inherit it (both call
   `Super::IsDataValid`). `URingData::IsDataValid` keeps its complementary hard
   rejection of *any* `AugmentStone` on a ring.

The gem/stone split is centralised in `CrystalTypeHelpers`:

```cpp
inline bool IsAugmentStoneType(ECrystalType T)  // DamageStone || AbilityStone
inline bool IsGemType(ECrystalType T)           // != None && !IsAugmentStoneType (Garnet..Quartz)
```

`None` is neither. These predicates are the single source of truth for both the
grey-out filter and the validation backstop.

---

## How It Works (implemented)

### DamageStone — attached (the whole-percent damage channel)

In `UDamageCalculator::CalculateDamage` (`Combat/Damage/DamageCalculator.cpp`),
**Step 1.25** runs immediately after the Step-1 attacker multiplier:

```cpp
// Step 1.25: Attached augment-stone raw-damage multiplier (physical actions only)
RunningDamage *= (1.0f + DamageStonePercent / 100.0f);
```

It live-resolves the attacker's active weapon attachment
(`Loadout->GetActiveWeaponLoadout()->WeaponEntry.GetAttachedItem()`), gates on
`Attachment.IsAugmentStone()` and `ActionType != Spell` (physical only, matching
the equipment-bonus gate), and applies the tier base% as a **direct multiplier**.
These are whole-number percentages, **not** per-point fractions — see the
*BonusRawDamage trap* below.

### DamageStone — consumable

The same tier table also drives a consumable. `UItemExecutor` (the
`DamageStone` use path) applies a **self-buff** on the user via
`USkillEffectManager`: an `ESkillEffectType::RawDamageBuff` of magnitude
`GetDamageStoneBasePercent(Id)` for `CombatConstants::DAMAGESTONE_CONSUMABLE_DURATION`
(**3 turns**, flat across tiers). It is consumed physical-only by
`GetStatusEffectDamageModifier`. So `DamageStone` is **both** an attached
multiplier (Step 1.25) **and** a temporary consumable buff — the same percent
either way.

### AbilityStone — slot grant

An attached `AbilityStone` raises the active weapon's stone-ability slot limit to
`GetAttachmentSlotsForTier(Crystal.Id)`. The loadout merge
(`GetAugmentStoneAbilities`) and validation (`ValidateAugmentStoneAbilities`) fill
and police those slots from `AssignedAugmentStoneAbilities`.

### Stones are non-elemental

`UEquipmentDataBase::GetCrystalElement()` switches `Crystal` / `Evolution` /
`None` only; a `AugmentStone` attachment falls through to the `Generic` default.
The runtime `FRuntimeAttachedItem::GetElement()` resolves a stone's identity
through `ItemIdentity::GetElement`, which has no case for `DamageStone` /
`AbilityStone` and likewise returns `Generic`. **Intended:** stones grant
mechanics, not an element. (Flagged because the code expresses it as a `default:`
fall-through rather than an explicit `AugmentStone` case — correct, but easy to
misread as an omission.)

---

## Critical reasoning — the *why* (must-preserve)

> ### ⚠️ The `BonusRawDamage` trap — DO NOT re-attempt
>
> Stone raw-damage **must stay on the whole-percent channel** (Step 1.25,
> `RunningDamage *= 1 + pct/100`). It must **never** be migrated onto
> `FEquipmentStatBonus::BonusRawDamage`.
>
> `BonusRawDamage` is folded into the attacker multiplier as
> `BonusRawDamage × RAW_DAMAGE_PER_POINT`, where
> `RAW_DAMAGE_PER_POINT = 0.0008` (0.08% per point) and the field is **clamped
> to ±21** (`FEquipmentStatBonus` substat clamp). Its entire expressible range
> is therefore `21 × 0.0008 = 0.0168 ≈ **1.7%** max`. A stone needs up to **15%**
> (`DamageStone S`). The field **physically cannot represent** a stone's bonus —
> routing stone damage through it would silently cap every stone at ~1.7%.
>
> This is recorded as a *ruled-out refactor*, not an open question. The
> whole-percent channel exists specifically because `BonusRawDamage` can't do
> this job.

**Why the enum stays unified (no gem/stone split).** The planned FusionStone
fuses **any two attachables** — including a stone fused with a gem crystal. Both
fusion inputs must share one identity type, so `ECrystalType` / `FCrystalId`
stays unified and the gem/stone distinction is expressed by predicates
(`IsGemType` / `IsAugmentStoneType`), not by separate types.

**Why `AbilityStone` is attach-only with effect `None`.** Ability slots are a
**persistent** property of the equipped weapon. A consumable is a one-shot,
temporary effect — there is no coherent "use" semantics for a slot grant, so
`AbilityStone` maps to `EItemEffectType::None` and `UseItem` has no path for it.

**Cap decisions (rationale, carried forward for the stat-stone family below).**
These are the reasons certain stats are safe (or unsafe) to push past their
nominal caps when a future stone/FusionStone targets them:

- **Crit — uncapped (done, commit `7afd0d09`).** The crit roll is `FRand() <
  chance`; a `chance > 1.0` simply always crits, which is harmless. The design
  caps were removed: `GetCriticalChance` and the two crit-curve getters now clamp
  to `[base, 1.0]`, and the `MAX_CRIT_CHANCE` (0.60) / `CRIT_CHANCE_MAX` (0.40)
  constants were deleted. The `1.0` upper bound is kept **deliberately** — the AI
  expected-value scorer reads crit as `1 + CritChance·(CritMult−1)` (where `CritMult`
  is the variable `UDamageCalculator::GetCritDamageMultiplier`; the fixed
  `CRIT_MULTIPLIER` was retired, cluster 5e), which assumes `CritChance ≤ 1.0`;
  unbounded crit would corrupt AI action scoring.
- **Resistance — bound at `1.0` for correctness only (done, commit `7afd0d09`).**
  A resistance `> 1.0` flips the buildup term `Amount *= (1 − Resistance)`
  negative and **heals** the gauge. The design cap was removed by raising
  `CombatConstants::RESISTANCE_MAX` `0.40 → 1.0`; it is the hard correctness
  ceiling. *(Update: the later cluster-A2 model adds an intermediate `UNIVERSAL_STAT_CAP`
  (0.5) on the crystal-Spirit "General" portion before the 1.0 ceiling — see
  `ResistanceSystem.md`.)* (The separate dead `DamageConstants::MAX_RESISTANCE
  = 0.50` is unrelated element-interaction leftover, still pending deletion.)
- **Luck — now the crit chance itself.** Luck's `LUCK_RAW_MAX` is a **normalisation
  basis** (the stat normalises to 1.0 at max), not a ceiling on luck. Since cluster
  5e, Luck *is* the crit chance via `GetLuckModifiedChance(BASE_CRIT_CHANCE,
  CRIT_CHANCE_LUCK_BONUS)` — the old standalone `LUCK_CRIT_BONUS_MAX` crit-bonus curve
  was **retired**. Nothing to lift.

---

## Integration Points

- **`DamageCalculator`** — Step 1.25 reads the active weapon attachment and
  applies the `DamageStone` multiplier (physical actions only).
- **`ItemExecutor`** — the directional stone-consumable handlers (`ExecuteRawDamageBuffEffect` for DamageStone, `ExecuteDefenseBuffEffect` for DefenseStone + Amber).
- **`LoadoutSystem`** — `FWeaponLoadoutEntry` stone-ability slots; `FSavedLoadout`
  / `FCombatLoadout` plumbing; `GetEquippedCrystals` surfaces `AugmentStone` slots.
- **`WeaponSystem`** — `UEquipmentDataBase` owns `AttachedItem`,
  `IsAugmentStoneAttached()`, `GetRestrictedCrystalTypes()`, the `IsDataValid`
  backstop, and the `DefaultAbilities` field seeded onto stone-granted slots.
- **`CrystalEffectTable` / `CrystalTypeHelpers` / `ItemIdentity`** — the value
  tables and the gem/stone predicates.

---

## Implemented this cycle — DefenseStone (first dual-form stat-stone)

The **dual-form stat-stone mechanism is now shipped**, proven end-to-end on
**DefenseStone** across four clusters on `feature/weapon-stones`:

| Cluster | What | Commit |
|---|---|---|
| C1 | `ECrystalType::DefenseStone` + add-a-stone checklist (predicate, `BuffDefense` effect type, type name, BD-energy guard, description) — treated as a augment stone, grants nothing yet | `28fb8818` |
| C2 | generic getter trio — `GetStoneBasePercent(type,tier)` (shared `3-15` curve, DamageStone + DefenseStone), `StoneTargetStat` (DamageStone→RawDamage, DefenseStone→Defense, else `ESubStat::None`), `GetAttachedStonePercent(att,stat)`; `ESubStat::None` appended; `GetDamageStoneBasePercent` → byte-identical wrapper | `715e54d8` |
| C3 | **attached** Defense hook — `GetDefenderFlatDefense ×(1 + GetAttachedStonePercent(defenderAtt, Defense)/STAT_PERCENT_DIVISOR)`; permanent, defender's own live-resolved attachment; inert for non-DefenseStone | `db4f4832` |
| C4 | **directional consumable** — `ExecuteDefenseBuffEffect` generalised (branch `IsAugmentStoneType`: stone → `GetStoneBasePercent` + flat duration; Amber byte-identical); ally `DefenseBuff` / enemy `DefenseDebuff`. **Plus DamageStone consumable self→directional** (`RawDamageBuff`/`RawDamageDebuff`) — shipped-behaviour change | `9ffc2681` |

This is the **reusable template** for the rest of the stat-stone family (design
below): a per-read-site `GetAttachedStonePercent` hook for the attached form + a
directional `IsAlly` consumable. The next stat-stone is mostly the C1 checklist +
`StoneTargetStat`/`GetStoneBasePercent` rows + its own read-site hook.

**Also shipped this cycle (the multiplicative crystal/stone model — framework #2 & #5):**
- **Crit additive → multiplicative** across the board — `GetCriticalChance`'s
  equip/buff/debuff/modify factors **and** the Luck bonus compound `×(1+pct/100)`
  (debuff `×(1−pct)`). Commit `7e27f0ea`.
- **Stat-crystal rebalance** to the stone×2 curve `6/10/14/18/22/26/30` — Opal
  (`680e9d5a`), Amber + Emerald (`3e1c86d0`); both vestigial double-table arms
  (Opal, Emerald) removed.

> **All of the above is committed but not yet PIE-verified** at time of writing.

---

## ⚠️ Design / Phase 2 — original design record (most has since SHIPPED)

Everything below was **design intent** when authored. **Status as of 2026-06-16 — trust
this list over any "not in the code" wording inside the prose below:**

- ✅ **Full stat-stone family** — `DefenseStone`, `CritStone`, `TurnSpeedStone`,
  `StatusStone`, `EfficiencyStone`, `MaxHP/MaxEPStone`, `SpellDamageStone`,
  `ResistanceStone`, `SpellSpeedStone`, `ActionSpeedStone`, `DurabilityStone`,
  `LuckStone`, `ReflexStone` — all present in `ECrystalType` (`CrystalType.h`).
- ✅ **FusionStone** — `FFusionId{HalfA,HalfB,BonusStat}` (`FFusionId.h`),
  `EAttachedItemKind::Fusion`, `FFusionAttachment`, and production fusion **wear**
  (see `CrystalWear.md`); the fusion BonusStat dropdown was widened to the 12 substats
  (`2c3b7bbe`).
- ✅ Crit additive→multiplicative (#2, `7e27f0ea`), stat-crystal rebalance (#5),
  DamageStone-consumable directional change.
- ❌ **Still genuinely unbuilt:** the speed-stone → defense-window-penalty link (the
  variable window exists but reads RAW asset speed, not gear/stone), and the Resistance
  spell-damage shave hook.

The prose below is kept as the **design rationale**.

> Terminology: the stone described here was called **MasteryStone** in earlier
> design notes. It is now **FusionStone** — created by *fusion*, and parallel in
> the family with `DamageStone` / `AbilityStone`.

### FusionStone

- **Player-created, not designer-authored.** A fusion action consumes **two
  owned attachables** (stones and/or crystals) and produces one FusionStone.
- **Symmetric** — both halves contribute their full effects; the FusionStone's
  effect is the **concatenation of both halves' contributions** plus an intrinsic
  **fusion bonus**.
- **Attach-only** (like `AbilityStone` — there is no "use" semantics for a fused
  stone's persistent contributions).
- This is the reason the enum stays unified — a FusionStone must hold a
  stone+crystal pair under one identity type.

#### Identity — `FFusionId` (composite, symmetric)

```cpp
FFusionId { FCrystalId HalfA; FCrystalId HalfB; ESubStat BonusStat; }
```

- Countable / stackable: two identical fusions are the **same item type**.
- **Equality is symmetric** — `HalfA + HalfB` must equal `HalfB + HalfA`.
  Normalize the pair (e.g. sort the two `FCrystalId`s by type) **before** compare
  and hash.
- **`FCrystalId` itself is unchanged** (stays a single `type + tier` for normal
  stones/crystals) — *Option A*. The composite lives only in `FFusionId`.

#### Fusion bonus — a formula, not a hand-authored table

The intrinsic bonus is a **percentage to one substat**, computed from the two
halves' tiers:

```
bonus%  =  (TierValue(A) + TierValue(B)) / 2
TierValue:  F=0  E=1  D=2  C=3  B=4  A=5  S=6
```

So `F+F = 0%` (lowest fusion = effects only, **no bonus**), `S+S = 6%`,
`S+F = 3%`. Symmetry is free — addition commutes. The 7×7 grid below is the
**visualisation**; the **implementation is the formula** (no 49-cell table is
authored):

| A\B | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| **F** | 0.0 | 0.5 | 1.0 | 1.5 | 2.0 | 2.5 | 3.0 |
| **E** | 0.5 | 1.0 | 1.5 | 2.0 | 2.5 | 3.0 | 3.5 |
| **D** | 1.0 | 1.5 | 2.0 | 2.5 | 3.0 | 3.5 | 4.0 |
| **C** | 1.5 | 2.0 | 2.5 | 3.0 | 3.5 | 4.0 | 4.5 |
| **B** | 2.0 | 2.5 | 3.0 | 3.5 | 4.0 | 4.5 | 5.0 |
| **A** | 2.5 | 3.0 | 3.5 | 4.0 | 4.5 | 5.0 | 5.5 |
| **S** | 3.0 | 3.5 | 4.0 | 4.5 | 5.0 | 5.5 | 6.0 |

(Cells are bonus %, i.e. `(TierValue(A)+TierValue(B))/2`.) The same tier-value
basis is intended to also feed the **Phase-3 crafting cost** — one valuation,
two uses.

#### Bonus target — player-chosen, wired substats only

- The fusion bonus applies to a **substat the player chooses at fusion time**
  (`FFusionId::BonusStat`) — not damage-only.
- **Restricted to wired substats** (see *Wired-substat audit* below) so a fusion
  bonus can never silently do nothing.

### Fusion restrictions

**Underlying principle (the "why" behind the pair list).** A valid fusion is a
**stat-stone + one other contributor** (a gem crystal *or* an `AbilityStone`).

- **Valid inputs (contributors):** stat-stones (`DamageStone` / `DefenseStone` /
  …), `AbilityStone`, gem crystals.
- **Never an input: evolution crystals** — excluded from fusion *entirely* (not
  merely "no evolution + evolution"; an evolution crystal can never be a fusion
  half at all). Evolution is a **transformation** mechanic, not a stat/element
  **contributor** — it has no contribution to concatenate into a fused stone.

Every banned pair below falls out of that principle (each one either lacks a
stat-stone or pairs two non-stat contributors). The explicit table is kept for
quick reference:

| Pair | Allowed? | Reason |
|---|---|---|
| stat-stone + crystal | ✅ | stat bonus + element/spells |
| stat-stone + `AbilityStone` | ✅ | stat bonus + slots |
| stat-stone + stat-stone | ✅ | two stat bonuses — both halves contribute |
| `AbilityStone` + `AbilityStone` | ❌ | two slot-grants, nothing to combine meaningfully |
| crystal + crystal | ❌ | element + element |
| `AbilityStone` + crystal | ❌ | slots + element |
| *anything* + evolution crystal | ❌ | evolution is a transformation, not a contributor — never a fusion input |

### Rings + AugmentStones (loosens the current ring-guard)

**Current committed behaviour (implemented):** `URingData::IsDataValid`
**hard-rejects any `AugmentStone` on a ring** — augment stones are weapon-only.

**Planned Phase 2 change (designed, not built):**

- A ring **may** carry an augment stone **only if** it is a **FusionStone that
  contains an elemental crystal** — the crystal supplies spells the ring can use.
- A **bare stat-stone** (`DamageStone`, etc.) or an **`AbilityStone`** on a ring
  stays **rejected** — a ring has no weapon damage and no use for ability slots.

> **Deliberate future contradiction of committed code.** This **loosens / will
> replace** the current hard ring-guard above. It is intentional, not a bug:
> the guard exists today because no valid ring-stone case existed yet; FusionStone
> introduces one. Cross-references the *Fusion restrictions* table — only a
> `stat-stone + crystal` FusionStone (which carries an element) qualifies for a ring.

### Rename: `WeaponStone` → `AugmentStone` — ✅ DONE (clusters C1–C4)

The family was named "**Weapon**Stone" when stones were weapon-only; the
rings-carrying-augments design above made "Weapon" inaccurate, so the whole
concept was renamed `WeaponStone` → `AugmentStone` across enum, properties, code
symbols, and this doc. Shipped in four clusters on `feature/weapon-stones`:
`e9d22103` (serialized `EAttachedItemKind` value + EnumRedirect), `4e85fae0`
(5 serialized ability-fields + PropertyRedirects), `ebdaef45` (code symbols +
EditCondition meta strings + comment sweep), and this doc rename (file + content).
The CoreRedirects chain `Whetstone → WeaponStone → AugmentStone`, so saved
attachments/loadouts from any era still resolve.

### The stat-stone family — dual-form model

> ✅ **The dual-form mechanism is implemented** (first proven on DefenseStone — see
> *Implemented this cycle*). The model below describes the whole family; the **full
> stat-stone family has since shipped** (all stat-stones present in `ECrystalType`) —
> see the Design/Phase-2 status note above.

Every stat-stone mirrors `DamageStone`'s **two forms**:

- **Attached form — permanent self-passive.** `+X%` of that stat to the
  **holder**, via the whole-percent channel (the `DamageStone` attached path).
  **Self-only** — it buffs the wielder, nothing else.
- **Consumable form — single-target, directional, timed.** The player picks
  **one target** (same duration as the `DamageStone` consumable, 3 turns):
  - **ally target → `+X%` buff** of that stat;
  - **enemy target → `−X%` debuff** of that stat.
  - Same magnitude both directions; the **sign flips on target allegiance**.
  - Applies via the status/buff system (`SkillEffectManager`), which already
    supports timed buffs/debuffs.

**Crit — now multiplicative (shipped, commit `7e27f0ea`).** Crit is uncapped
(bounded at `1.0` for the AI scorer, commit `7afd0d09`) **and multiplicative** —
`GetCriticalChance`'s equip / buff / debuff / modify factors and the Luck bonus all
compound `×(1+pct/100)` (debuff `×(1−pct)`). Crit is no longer an exception; it now
follows the universal multiplicative rule (#1). See Balance framework #2.

#### Balance framework — the crystal/stone number model

The whole stat-stone family — and the three stat *crystals* that have stone
analogues — runs on one small set of rules. **#1–#5 are now implemented** (crit
conversion #2 → `7e27f0ea`; crystal rebalance #5 → `680e9d5a` / `3e1c86d0`); #6–#8
describe scope, the open question, and order. Items that modified shipped behaviour
are marked **✅ DONE** inline.

**1. Everything multiplicative.** Every stat-stone and stat-crystal applies
`×(1 + pct/100)` on the stat's **current value** — no additive stat effects.
Defense, Speed, and raw Damage already work this way; this makes it the **universal
rule** for the family.

**2. Crit is multiplicative — across the board. ✅ SHIPPED (commit `7e27f0ea`).**
- *Shipped:* crit is **multiplicative** everywhere — stones, Opal, the
  `CritChanceBuff` / `Debuff` skill effects, equipment `BonusCritChance`, and the
  Luck bonus all apply as `×(1 + pct/100)` on the crit value (debuff `×(1−pct)`),
  not `+=` points. `GetCriticalChance` compounds these factors; Opal's
  `GetCritBuffPercent` applies multiplicatively. Crit is no longer an exception to
  the universal rule (#1).
- *Why:* a multiplicative bonus **self-scales** — `+30%` is worth more to a
  high-crit build and less to a low one — so it can't dump flat points that
  trivially fill the (uncapped, commit `7afd0d09`) bar, and it makes crit
  consistent with every other stat. This is what lets the **uniform numbers** (#3)
  work without crit being overpowered.
- *Consequence (intended):* reaching 100% crit from item bonuses alone is
  mathematically hard — multipliers approach but don't easily reach `1.0`. Crit
  builds invest in the **base crit stat**; items amplify it.

**3. Uniform numbers — one shared stone curve.** All stat-stones share **one**
per-tier curve: the current `DamageStone` curve **3 / 5 / 7 / 9 / 11 / 13 / 15**
(F..S). Because the operator is multiplicative (#1), uniform numbers do **not** mean
uniform power — the multiply scales each stat by its own value. No per-stat stone
curves.

**4. Crystal = stone × 2 (stat crystals only).** A stat crystal's magnitude is its
stone's value **×2** — the single knob for the "stones weaker than crystals"
principle. With the shared stone curve `3..15`, the stat-crystal curve is
**6 / 10 / 14 / 18 / 22 / 26 / 30**. Applies **only** to the two stat crystals that
have stone analogues: **Amber** (Defense) and **Opal** (Crit). *(Speed-augmenting is
the stone-only `TurnSpeedStone`; **Emerald is no longer a stat crystal** — it remains
an `ECrystalType::Emerald` Wind crystal, but its consumable effect is now a delayed
bonus turn, not a stat augment. See ItemSystem.md.)*

**5. Stat-crystal rebalance. ✅ DONE** (Opal `680e9d5a`; Amber `3e1c86d0`).
The stat crystals re-tuned to the stone×2 curve:

| Crystal (stat) | Before | Shipped (stone×2) | Operator |
|---|---|---|---|
| Amber (Defense) | 15/20/25/30/35/40/50 | 6/10/14/18/22/26/30 | `×(1+pct)` |
| Opal (Crit) | 5/8/10/12/15/18/25 (additive) | 6/10/14/18/22/26/30 | now **multiplicative** (per #2) |

Both converge to the **same** numbers — their power differs through the
multiplicative operator acting on different base stats, not through different numbers.

**6. Ability crystals untouched (the framework boundary).** The other **eight**
crystals have **no stone analogue** and are **not** part of this framework — their
values stay exactly as audited: Garnet (DOT), Sapphire (heal), Citrine (EP), Emerald
(bonus turn), Onyx (silence), Amethyst (gamble), Iolite (cleanse), Quartz
(status-clear). The framework applies to **exactly the two stat crystals** above and
nothing else.

**7. Attached vs consumable — RESOLVED: one curve for both. ✅** DefenseStone shipped
with the shared `3..15` curve for **both** forms (attached + consumable), matching the
`DamageStone` precedent. Revisit only if permanence proves too strong in PIE.

**8. Build order — done.** (1) ✅ Crit additive→multiplicative (`7e27f0ea`).
(2) ✅ Stat-crystal rebalance (`680e9d5a`, `3e1c86d0`). (3) ✅ Stat-stone family
proven on **DefenseStone** (`28fb8818` … `9ffc2681`). Remaining stat-stones repeat
step (3)'s template.

#### `DamageStone` consumable — ✅ DONE: self→directional (commit `9ffc2681`)

`DamageStone` is the template the dual-form model generalises from; its
**consumable form changed** under this design (shipped in C4):

| Form | Before | Now |
|---|---|---|
| **Attached** | self-passive `+damage` (Step 1.25 multiplier) | **unchanged** |
| **Consumable** | self-only: `RawDamageBuff` on the **user** (3 turns) | **targeted directional**: ally → `RawDamageBuff`, enemy → `RawDamageDebuff` (`IsAlly`, applied to the chosen Target) |

> **Shipped-behaviour change (now landed).** The attached path is unchanged; the
> **consumable** gained target selection + an enemy-debuff direction. `RawDamageDebuff`
> already existed with a correct read-path (`DamageCalculator.cpp:555`,
> physical-only). **Re-verify in PIE** that the old self-buff usage now requires
> targeting self — record so the change isn't read as a regression.

#### Pool-stat variant — `MaxHP` / `MaxEnergy` (raise/decrease-max, not heal)

Pool stats are stat-stones too, but they are **raise/decrease-max** stones — they
move the **ceiling**, not current HP/EP. They are **not** restore/heal potions.

- **Attached (self):** `+X%` max HP/EP, permanent.
- **Consumable (single-target, directional)** — same shape as the other
  stat-stone consumables:
  - **ally → `+X%` max (ceiling only).** Current HP/EP is **unchanged** — it gives
    *room to heal / regen into*, no instant gain.
  - **enemy → `−X%` max.** If the lowered max drops **below** the target's current
    HP/EP, **leave it overcapped** — *no instant loss*. They simply can't recover
    above the new cap, and it bites over time.

> **Rule (b): overcapped, not clamped.** Lowering max never deals instant damage by
> clamping current-down-to-new-max — that would make it disguised burst. Keeping
> the target overcapped preserves it as a **true debuff** that pressures sustain,
> not a damage spell.

**Mechanism — a different hook shape.** Pool stats flow through
`RecomputeMaxPools`, a **separate code path** from the whole-percent combat-stat
channel. Their hook lives in the pool-recompute path, **not** alongside the
Defense/damage hooks — so they are **not** part of the first generic-channel proof;
they are their own hook.

#### Defense / Resistance — the mitigation model

Two defensive stats with **non-overlapping** roles, grounded in the damage-pipeline
survey of `UDamageCalculator::CalculateDamage`:

**Defense — broad damage reduction, raw AND spell. ✅ Already shipped.** Pipeline
Step 5 subtracts flat `GetDefenderFlatDefense` from incoming damage with **no
`ActionType` gate** — spell damage already passes through Defense exactly like
physical. A **Defense stone just scales the existing stat** (whole-percent boost to
the defender's flat-defense value); **no new combat hook**. *Flat / subtractive.*

**Resistance — status-buildup reduction (shipped) + a slight spell-damage shave (NET-NEW).**

- Primary job unchanged: reduces status buildup in
  `StatusBuildupManager::AddStatusBuildup` (`Amount *= (1 − Resistance)`).
- **New, to build** — a slight spell-*damage* reduction:

  ```
  spellDamage *= (1 − RESISTANCE_SPELL_SHAVE_FACTOR · Resistance)
  RESISTANCE_SPELL_SHAVE_FACTOR = 0.2     // named tunable, not a magic number
  ```

  So 100% Resistance → **20%** spell reduction; 50% → 10%; 40% → 8%. **Slight by
  design** — Resistance stays primarily a *status* stat; nobody builds it *for*
  spell defense (Defense is the spell shield). *Percentage / multiplicative.*
- **Placement:** a **new defender-side step gated on `ActionType == Spell`**, near
  Step 5, using the **existing-but-currently-unconsumed `bIgnoreResistance`** field
  on `FDamageCalculationInput` to gate it. This is the **one new combat hook** in
  the entire stat-stone family.

> **Design rationale — flat vs percentage.** Defense is **flat** (subtractive):
> strong against many small hits, weak against one big hit. The Resistance shave is
> **percentage**: scales with hit size. The two reductions are legibly different and
> **non-overlapping** — Defense is the spell shield, Resistance a minor top-up that
> scales with burst.

#### Wired-substat audit

This table gates **both** stat-stones **and** the FusionStone bonus dropdown — a
stat is eligible only if it has a working read site (otherwise the stone/bonus
silently does nothing).

**Wired (have a working read site — buildable now):**

| Substat | Note |
|---|---|
| RawDamage | the `DamageStone` channel |
| Defense | flat subtractive mitigation (pipeline Step 5); **already covers raw AND spell** (no `ActionType` gate). A Defense stone scales the stat via the whole-percent channel — the proving ground (see *Build order*) |
| TurnSpeed | wired |
| StatusMultiplier | wired |
| Efficiency | cost reduction |
| CritChance | **multiplicative + uncapped** (shipped `7e27f0ea` / `7afd0d09`); compounds via `GetCriticalChance` — see Balance framework #2 |
| Luck | wired but **balance-risky** — it is the normalization basis for luck-derived chances |
| **SpellDamage** ("spell power") | **WIRED — corrected.** Three attacker-side reads, all `ActionType==Spell`-gated: `GetEvolutionModifiedSpellDamage` (Mind curve), equipment `BonusSpellDamage`, `ActionMods` spell-power. **Buildable now.** *(The earlier audit wrongly listed this unwired — it conflated offensive spell-power, wired, with defensive spell-resistance, the net-new hook above.)* |
| Resistance (status) | **wired** (`StatusBuildupManager`) for status buildup; the **spell-damage shave** is the net-new extension (see *Defense / Resistance*) |
| Reflex | **WIRED (Cluster B).** `ESubStat::Reflex` (`ActionStatModifiers.h:40`) + `FActionStatModifiers::Reflex` + gear `BonusReflex` + `ReflexStone`; the live defense-window read widens the window by `(ReflexBuff − ReflexDebuff)` via `UDefenseSystem::GetEffectiveDefenseInputWindow`. A complete gearable/buffable/stone-backed Body substat. |

**Animation play-rate read site (Pattern-P capped as of stat-composition 5g; intended defense-window effect still unwired — see *Speed stones*):**

| Substat | Note |
|---|---|
| SpellSpeed | play-rate composed via `UCharacterDataComponent::GetEffectiveSpellSpeed()` (stat ×1.5 / gear ×2.0, cluster 5g) × `BaseAnimSpeed` at the `ActionExecutor` site — so an attached SpellSpeedStone speeds the cast animation, now ×2.0-bounded. Its *intended* defense-window-pressure effect is still **unwired** (see *Speed stones* below). |
| ActionSpeed | play-rate composed via `GetEffectiveActionSpeed()` (same shape), drives ability/attack montage PlayRate, ×2.0-bounded. What's missing is the **defense-window link**, not the read. |

**Not needed / unbuilt:**

| Substat | Status |
|---|---|
| per-element / spell-type Resistance | the unbuilt `ResistsElement` system — **not pursued**; the status-buildup + flat-Defense + slight-spell-shave model is chosen instead |

**Pool stats (separate `RecomputeMaxPools` path — see *Pool-stat variant*):**

| Substat | Status |
|---|---|
| MaxHP / MaxEnergy | **settled** — raise/decrease-max stones (ceiling-only, overcapped-not-clamped); pool-recompute hook |

#### Speed stones (`ActionSpeed` / `SpellSpeed`) — blocked on a prerequisite system

Their **intended** effect is **defense-window pressure**: a faster attacker
animation gives the defender a **tighter reaction window**; slower gives more.

> **⚠️ UPDATE (`feature/realtime-defense` + stat-composition 5g) — the premise below is OBSOLETE.**
> The "variable-defense-window" feature this section describes as a *prerequisite* now **EXISTS**.
> The window is no longer a fixed `0.3s`: `UDefenseSystem::GetEffectiveDefenseInputWindow` computes
> `max(MINIMUM_DEFENSE_WINDOW 0.1s, base 0.5s + defender Reflex − attacker speed)`, and attacker speed
> **does** narrow the defender's window via `CharacterData::CalculateSpeedWindowPenalty` (capped ±0.25s
> per the `WINDOW_CAP_SECONDS` ceiling). See `StatComposition.md` and the DefenseSystem docs.
>
> **What's still unwired for STONES specifically:** the window's attacker-speed term reads the **RAW**
> `GetTotalActionSpeed()` / `GetTotalSpellSpeed()` asset points — **not** the geared/stone play-rate. So a
> speed *stone* speeds the animation (now ×2.0-bounded, cluster 5g) but does **not yet** tighten the
> defender's window. Wiring it = feeding the geared speed into `CalculateSpeedWindowPenalty` — a small,
> well-defined follow-up, **no longer "blocked on a net-new system."**

**Original (historical) analysis — the window was a fixed `0.3s` constant, hardcoded at the
`ActionExecutor` call sites and independent of play-rate; the data-driven `FDefenseWindowConfig` was
dead/unwired.** That hardcoded-window model has since been replaced by the per-impact timed window
above, so the "thread `PlayRate` into the window-open path" prerequisite is largely already done —
only the stone→penalty link remains.

> **Status: UNBLOCKED (was BLOCKED).** The variable defense window exists; speed stones now need only
> the stone→window-penalty wiring, not a net-new system. Until that small follow-up lands, a speed stone
> alters animation visuals (×2.0-bounded) but not the reaction window.

#### Build order & final classification

The attached/consumable channel is still proven on **`Defense` first** (low-risk,
self-passive, whole-percent), then the **directional consumable** is retrofitted
onto `DamageStone` and inherited by the rest. The full set, by readiness:

| Class | Stones |
|---|---|
| **Buildable now** | `DamageStone` (consumable directional retrofit), `Defense`, `TurnSpeed`, `StatusMultiplier`, `Efficiency`, `CritChance` (multiplicative, shipped `7e27f0ea`), **`SpellDamage`** (confirmed wired), `Resistance` (status + the `0.2` spell-shave hook), `MaxHP` / `MaxEnergy` (pool-recompute path) |
| **Build with care (later)** | `Luck` — the normalization basis for luck-derived chances; balance-risky |
| **Small follow-up (was blocked)** | `ActionSpeed` / `SpellSpeed` — the variable-defense-window now exists (`feature/realtime-defense`); only the stone→`CalculateSpeedWindowPenalty` wiring remains. Play-rate is already ×2.0-bounded (5g). |
| **Not needed** | per-element resistance — the status-buildup + flat-Defense + slight-shave model is chosen instead |

### Forward risk — single-field `GetRestrictedEnumValues`

The editor grey-out works **only because `AttachedItem` is a single field** on
the owning asset — `GetRestrictedCrystalTypes()` reads the one unambiguous
`AttachedItem.Kind`. If attachments ever become a `TArray<FAttachedItem>`, the
function can't tell which element is being edited and the grey-out **breaks**.
The `IsDataValid` backstop still holds (it iterates whatever it's given), so the
hard guarantee survives; only the editor affordance degrades.

---

## Parked cleanup (own survey/decision pending)

- **Broad `Crystal → Item` rename** — `ECrystalType`, `FCrystalId`,
  `CrystalEffectTable`, etc. Pending a decision on the crystal-vs-item naming
  line. *(The `WeaponStone → AugmentStone` rename it was bundled with is now
  done — see the Rename section above.)*
- **`CrystalIdentity.h` file rename** — the file already hosts the `ItemIdentity`
  namespace; the filename lags.
- **`GetPrimaryEffectType` doc mentions** — `ItemSystem.md` historically referred
  to `GetPrimaryEffectType()`; the function is now `GetItemEffectType()` (commit
  `5c22e6e`). Corrected in `ItemSystem.md`; noted here as part of the rename
  sweep.
- **`FRuntimeAttachedItem.h` "Refined" comment drift** (see *Architecture*) —
  stale code comments to clean up when the rename pass touches that file.
- ~~**Opal crit double-table bug**~~ — ✅ **done**: the vestigial Opal arm was removed
  from `GetBuffPercentage` during the rebalance (`680e9d5a`); `GetCritBuffPercent` is
  now the single Opal source.
- **Duplicate duration tables** — `GetCrystalDuration`, `GetGambleDuration`, and
  `GetResistanceDuration` are three byte-identical `4/4/3/3/3/2/2` tables — DRY-collapse
  when touched. *(The `GetBuffPercentage` Emerald arm was already removed in `3e1c86d0`;
  the table is now Amber-only.)*
- **`DAMAGESTONE_CONSUMABLE_DURATION` is now the generic stone-consumable duration**
  (used by both DamageStone and DefenseStone consumables). Rename to a neutral name
  (e.g. `STONE_CONSUMABLE_DURATION`) when convenient — the `DAMAGESTONE_` prefix is
  now misleading.

---

## Known Limitations / TODOs

- **Editor grey-out, not hide.** Wrong-Kind `CrystalType` values are greyed and
  non-selectable, not hidden. True hiding would need an `IDetailCustomization`
  editor module (declined per the `UEquipmentDataBase` cosmetic-limitation note).
- **Fresh `AugmentStone` slot trips validation immediately.** `CrystalType`
  defaults to `Garnet`, so a slot newly set to `Kind==AugmentStone` is invalid
  until the designer picks a stone — intended, but it lights up an `IsDataValid`
  error on first authoring.
- **Stone-ability UI** for the per-tier slots is not covered here (UI pass).
- Most *Design / Phase 2* content is unbuilt, **except** the items marked ✅ (the
  DefenseStone dual-form, crit conversion #2, crystal rebalance #5, DamageStone
  consumable) — see *Implemented this cycle*. Those are committed but PIE-unverified.

---

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-20 | Shared slot curve unified to `{1,2,3,4,5,6,6}` (was `{2,3,3,4,4,5,6}`) — linear F→A, S holds at 6. Added sibling helper `CrystalEffectTable::SlotsForContainerTier(EItemTier)` (same curve, keyed on a container's own tier, no crystal-type gate) now driving weapon ability + weapon/ring/evolution native spell slots; `GetAttachmentSlotsForTier(FCrystalId)` still keys augment-stone abilities + gem spells. Updated the slot table + helper description; historical 2026-06-07 rows left as-shipped. | feature/equipment-slot-tier-scaling |
| 2026-06-16 | Doc-sync (status reconcile): corrected the "implemented" architecture facts — `ECrystalType` now ~26 values (full stat-stone family, not "12 / 2 stones"); `EAttachedItemKind` gains `Fusion` (=4, `FFusionId`). Rewrote the top banner + the *Design / Phase 2* fence to a status list: the **full stat-stone family** and **FusionStone** (identity + slot + production wear) have **shipped** — only the speed-stone→window link and the Resistance spell-shave remain unbuilt. Added a **Reflex** row to the wired-substat audit (`ESubStat::Reflex`/`BonusReflex`/`ReflexStone`). Fixed retired-symbol refs in the cap decisions (`CRIT_MULTIPLIER`→`GetCritDamageMultiplier`; `LUCK_CRIT_BONUS_MAX` retired, Luck *is* crit chance; noted the cluster-A2 `UNIVERSAL_STAT_CAP` on resistance). | feature/realtime-defense |
| 2026-06-16 | **Speed-stone section corrected** — the "fixed 0.3s, play-rate-independent defense window" premise is **obsolete**: `feature/realtime-defense` made the window variable (`GetEffectiveDefenseInputWindow` = base 0.5s + Reflex − attacker speed, floored 0.1s), and stat-composition 5g made ActionSpeed/SpellSpeed play-rate Pattern-P (`GetEffectiveActionSpeed`/`GetEffectiveSpellSpeed`, ×1.5 stat / ×2.0 gear). Speed stones reclassified **BLOCKED → small follow-up**: only the stone→`CalculateSpeedWindowPenalty` wiring remains (the window penalty currently reads RAW asset points, not gear/stone). Visual-read-site + classification tables updated. | feature/realtime-defense |
| 2026-06-07 | Initial documentation — shipped weapon-stone system (unified `ECrystalType`/`FCrystalId` identity; `EAttachedItemKind::WeaponStone`; `DamageStone` whole-percent channel + 3-turn consumable; `AbilityStone` per-tier slots 2/3/3/4/4/5/6; `GetRestrictedEnumValues` grey-out + `IsDataValid` backstop; `CrystalTypeHelpers` gem/stone predicates). Recorded the `BonusRawDamage` trap, the unified-enum and attach-only rationale, and the cap decisions. Captured the Design/Phase-2 plan (Mastery stone, 7×7 fusion/cost matrix, selectable-substat fusion bonus, fusion restrictions, rings+AugmentStone rule, planned `WeaponStone→AugmentStone` rename, stat-stone family + read-site hooks, single-field grey-out risk) and parked cleanup. | feature/weapon-stones |
| 2026-06-07 | Phase-2 design revision — **Mastery → FusionStone** rename throughout; FusionStone is player-created (fusion consumes two owned attachables), `FFusionId{HalfA,HalfB,BonusStat}` composite with symmetric equality, `FCrystalId` unchanged (Option A); fusion bonus is the formula `(TierValue(A)+TierValue(B))/2` (7×7 shown as visualisation only); added `stat-stone+stat-stone` valid pairing; expanded the **stat-stone family** into the dual-form model (attached self-passive / directional timed consumable) with the `DamageStone`-consumable change flagged as a shipped-behaviour modification; added the **wired-substat audit** (wired/unwired/pool-stats) and the Defense-first **build order**. Refreshed the cap-decisions notes to reflect the now-shipped crit/resistance cap removal (commit `7afd0d09`). | feature/weapon-stones |
| 2026-06-07 | Stat-stone family extension + audit fixes — added the **pool-stat variant** (`MaxHP`/`MaxEnergy` raise/decrease-max, ceiling-only, overcapped-not-clamped, `RecomputeMaxPools` hook); the **Defense / Resistance mitigation model** (Defense already covers raw+spell, flat/subtractive — no change; Resistance gains a net-new slight spell-damage shave `×(1 − RESISTANCE_SPELL_SHAVE_FACTOR·Resistance)`, factor `0.2`, the **one** new combat hook, gated on `ActionType==Spell` via the unconsumed `bIgnoreResistance`); reclassified **Speed stones** as blocked on a net-new variable-defense-window feature (window is a fixed `0.3s`, play-rate-independent; dead `FDefenseWindowConfig`). **Audit corrections (factual):** `SpellDamage` moved unwired→wired (3 attacker reads, `Spell`-gated); `ActionSpeed` corrected (has a play-rate read; missing the window link, not the read); per-element resistance marked not-needed. Reworked build-order into a readiness classification (buildable-now / build-with-care / blocked / not-needed). | feature/weapon-stones |
| 2026-06-07 | Fusion restriction — **evolution crystals excluded from fusion entirely** (never a valid fusion half: a transformation mechanic, not a stat/element contributor). Recorded the underlying **valid-fusion principle** (stat-stone + one contributor — gem crystal or `AbilityStone`) from which the banned-pair list falls out; kept the explicit table and added the `anything + evolution` ❌ row. | feature/weapon-stones |
| 2026-06-07 | **Balance framework** for the stat-stone/crystal number model — everything multiplicative `×(1+pct/100)` on current value; one shared stone curve `3/5/7/9/11/13/15`; **crystal = stone×2** (`6/10/14/18/22/26/30`) for the three stat crystals only. **⚠️ shipped-behaviour changes flagged:** crit converts additive→multiplicative across the board (#2), and Amber/Opal/Emerald rebalance to the stone×2 curve (#5, Opal also flips to multiplicative). Ability crystals (other 7) explicitly out of scope (#6); attached-vs-consumable curve left OPEN (#7); build order crit-conversion → crystal-rebalance → stat-stone family (#8). Reframed the crit-exception note (today-additive + planned-multiplicative) and annotated the wired-audit crit row / build-order cell. Banked crystal cleanup: Opal double-table bug + duplicate duration tables. | feature/weapon-stones |
| 2026-06-07 | **Implemented this cycle** — marked the now-shipped work and moved it above the design fence. **DefenseStone** dual-form built C1–C4 (`28fb8818` type+checklist; `715e54d8` generic getter `GetStoneBasePercent`/`StoneTargetStat`/`GetAttachedStonePercent` + `ESubStat::None`; `db4f4832` attached `GetDefenderFlatDefense` hook; `9ffc2681` directional consumable + DamageStone self→directional). **Crit additive→multiplicative** shipped (`7e27f0ea`) and **stat-crystal rebalance** shipped (Opal `680e9d5a`, Amber/Emerald `3e1c86d0`, both vestigial double-table arms removed). Flipped framework #2/#5/#7/#8 + the DamageStone-consumable subsection to ✅ DONE; updated parked cleanup (Opal double-table done; Emerald arm done; added `DAMAGESTONE_CONSUMABLE_DURATION` rename). All committed, **not yet PIE-verified**. | feature/weapon-stones |
| 2026-06-08 | **WeaponStone → AugmentStone** full-concept rename ("Weapon" inaccurate now that rings can carry these). Four clusters: `e9d22103` serialized `EAttachedItemKind` enum value + EnumRedirect (chains Whetstone→WeaponStone→AugmentStone); `4e85fae0` 5 serialized ability-fields + PropertyRedirects; `ebdaef45` code symbols (`IsAugmentStone*`, `Get/ValidateAugmentStoneAbilities`, `MAX_AUGMENTSTONE_ABILITIES`) + EditCondition meta strings + comment sweep; this doc renamed (`WeaponStoneSystem.md` → `AugmentStoneSystem.md` + content sweep) and the planned-rename note marked DONE. `DAMAGESTONE_CONSUMABLE_DURATION` left as a separate cleanup. | feature/weapon-stones |
| 2026-06-09 | Doc-sync edits: reconciled the crit-conversion language to **shipped** (removed the residual "today-additive → planned-multiplicative" framing in framework #2, the Opal table row, the wired-audit crit row, and the build-order cell — all now state multiplicative, shipped `7e27f0ea`). Removed **Emerald** from the stat-crystal framework (it is no longer a stat crystal — its consumable effect is a delayed bonus turn, `affdf379`): three stat crystals → **two** (Amber, Opal); speed-augmenting is the stone-only `TurnSpeedStone`. | feature/weapon-stones |
