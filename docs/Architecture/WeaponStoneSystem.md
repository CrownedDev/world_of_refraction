# Weapon Stone System

## Overview

Weapon stones are a second family of equipment **attachments**, sitting alongside
crystals and evolution items in the same attachment slot. Where a gem crystal
grants an element and spells, a weapon stone grants a **mechanical bonus** to the
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

> This document covers the **shipped** weapon-stone work on `feature/weapon-stones`
> (per-tier ability-stone slots, the whole-percent damage channel, the Kind/type
> dropdown filter + validation backstop) **and** a large body of **designed but
> not-yet-built** follow-on work (FusionStone fusion model, stat-stone family,
> rings-carrying-augments). The two are kept in strictly separate sections.
> **Anything under *Design / Phase 2* is not in the code.**
>
> Related docs: `ItemSystem.md` (crystals + `FCrystalId`), `WeaponSystem.md`
> (`UEquipmentDataBase` / `UWeaponData` / `URingData`), `LoadoutSystem.md`
> (ability slots, crystal queries), `DamageCalculator.md` (the damage pipeline).

---

## Architecture (implemented)

### Unified attachment identity — `ECrystalType` / `FCrystalId`

`ECrystalType` (`Equipment/Crystals/CrystalType.h`) holds **12 values in one
enum**: `None` (0, hidden) + the ten gems `Garnet … Quartz` (1–10) + the two
stones `DamageStone` (11) and `AbilityStone` (12). `AbilityStone` is appended
last; the comment in the header marks the value as the serialized `.uasset` /
SaveGame identity, so it must not be reordered or inserted above.

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
| `WeaponStone` | 3 | weapon stone (appended last; serialized value is stable) |

A slot is exactly one `Kind`. Note the **serialized order is `None, Crystal,
Evolution, WeaponStone`** — `WeaponStone` was appended after `Evolution` to keep
on-disk `Kind` values stable, so it is value `3`, not `2`.

### `FAttachedItem` (design-time) vs `FRuntimeAttachedItem` (runtime)

The attachment point is a **discriminated union**, authored design-time on the
equipment asset and inflated to a runtime struct on the owned inventory entry.

**`FAttachedItem`** (`Equipment/FAttachedItem.h`, on
`UEquipmentDataBase::AttachedItem`):

| Field | Type | Visible when |
|---|---|---|
| `Kind` | `EAttachedItemKind` | always |
| `CrystalType` | `ECrystalType` | `Kind == Crystal \|\| WeaponStone` |
| `CrystalTier` | `EItemTier` | `Kind == Crystal \|\| WeaponStone` |
| `Evolution` | `UEvolutionItemData*` | `Kind == Evolution` |

`CrystalType` / `CrystalTier` are shared by gems and stones — a weapon stone's
identity is `FCrystalId{CrystalType (= DamageStone/AbilityStone), CrystalTier}`.
The `CrystalType` dropdown is filtered by `Kind` (see *Kind/type filter* below).

**`FRuntimeAttachedItem`** (`Equipment/FRuntimeAttachedItem.h`, on
`FWeaponInventoryEntry::AttachedItem` / `FRingInventoryEntry::AttachedItem`):

| Field | Type | Notes |
|---|---|---|
| `Kind` | `EAttachedItemKind` | `SaveGame` |
| `Crystal` | `FCrystalAttachment` | identity (`Crystal.Id`) + per-instance durability — carries **both** `Crystal` and `WeaponStone` Kinds |
| `Evolution` | `FEvolutionAttachment` | evolution branch |

Predicates: `IsEmpty()`, `IsCrystal()`, `IsEvolution()`, `IsWeaponStone()`.
`FromAttachedItem(const FAttachedItem&)` is the bridge factory. A weapon stone
stores its `FCrystalId` in `Crystal.Id` so downstream code reads stone identity
through the same `Crystal` branch a gem uses.

> **Flagged code-comment drift (not fixed here — code only, doc task):** the
> header comments in `FRuntimeAttachedItem.h` (the type summary and the
> `FromAttachedItem` doc-comment, ~lines 6–10 and 99–114) still say
> "Refined" / `Source.RefinedType` / `Source.RefinedTier`. The fields are
> actually `Crystal` / `CrystalType` / `CrystalTier`, and `Kind` no longer has a
> `Refined` value. The comments predate the Crystal/WeaponStone rename and are
> stale; the *behaviour* is correct.

### The WeaponStone family

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
count an attachment grants. The helper is **generic** (named for "attachment",
not "ability") so refined crystals can adopt it for spell-slots later; today only
`AbilityStone` returns non-zero:

| Tier | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| Ability slots | 2 | 3 | 3 | 4 | 4 | 5 | 6 |

`DamageStone`, gems, evolution, and `None` all return `0`. The progression is
deliberately non-sequential (E and D both 3; C and B both 4).

`AbilityStone` has **no consumable effect**: `ItemIdentity::GetItemEffectType`
maps it to `EItemEffectType::None`, so `UItemExecutor::UseItem` finds no handler
and falls to its "not a usable consumable" default. It is attach-only.

### AbilityStone ability plumbing (loadout)

The slots an `AbilityStone` grants are a **separate ability list** from the
weapon's own `PresetAbilities`, threaded through the loadout layer:

- `FWeaponLoadoutEntry::AssignedWeaponStoneAbilities` — the per-loadout override
  list for stone-granted slots (parallel to `AssignedAbilities`).
- `FWeaponLoadoutEntry::GetWeaponStoneAbilities()` — sequential override merge,
  same shape as `GetAllAbilities()`.
- `FWeaponLoadoutEntry::ValidateWeaponStoneAbilities(OwnedAbilities)` — validates
  ownership and caps the count at `GetAttachmentSlotsForTier(Attachment.Crystal.Id)`.
  With no weapon stone attached the slot limit is `0`, so any assigned
  stone-abilities are rejected.
- `FSavedLoadout::PrimaryWeaponStoneAbilities` / `SecondaryWeaponStoneAbilities`
  — the authored storage; `FCombatLoadout::CreateFromSavedLoadout` copies them
  into `PrimaryWeapon` / `SecondaryWeapon.AssignedWeaponStoneAbilities`.

### Kind/type dropdown filter + `IsDataValid` backstop

The single `CrystalType` field serves both `Crystal` and `WeaponStone` Kinds, so
its raw dropdown would offer all 12 enum values for either Kind. Two mechanisms
constrain it (full engine trace recorded in the `feature/weapon-stones` survey):

1. **Editor grey-out — `meta=(GetRestrictedEnumValues="GetRestrictedCrystalTypes")`**
   on `FAttachedItem::CrystalType`. `UEquipmentDataBase::GetRestrictedCrystalTypes()`
   runs on the owning asset (UE resolves `GetRestrictedEnumValues` against the
   outer `UObject`, not the struct — which is why it can read the sibling
   `AttachedItem.Kind`, exactly like `IsWeaponStoneAttached()` does for its
   `EditCondition`). It returns the **wrong-Kind** value names (short form, e.g.
   `"Garnet"` / `"DamageStone"`) to restrict: stones greyed when `Kind==Crystal`,
   gems greyed when `Kind==WeaponStone`. Restricted values are **greyed and
   non-selectable, not hidden** — true hiding would need an `IDetailCustomization`
   editor module, which the project declines. The list is built from the
   `CrystalTypeHelpers` predicates via enum reflection, so it cannot drift.
2. **Hard backstop — `UEquipmentDataBase::IsDataValid`.** Editor-independent
   enforcement: `Kind==Crystal` with a non-gem `CrystalType` → error;
   `Kind==WeaponStone` with a non-stone `CrystalType` → error. Lives on the base
   so both `UWeaponData` and `URingData` inherit it (both call
   `Super::IsDataValid`). `URingData::IsDataValid` keeps its complementary hard
   rejection of *any* `WeaponStone` on a ring.

The gem/stone split is centralised in `CrystalTypeHelpers`:

```cpp
inline bool IsWeaponStoneType(ECrystalType T)  // DamageStone || AbilityStone
inline bool IsGemType(ECrystalType T)           // != None && !IsWeaponStoneType (Garnet..Quartz)
```

`None` is neither. These predicates are the single source of truth for both the
grey-out filter and the validation backstop.

---

## How It Works (implemented)

### DamageStone — attached (the whole-percent damage channel)

In `UDamageCalculator::CalculateDamage` (`Combat/Damage/DamageCalculator.cpp`),
**Step 1.25** runs immediately after the Step-1 attacker multiplier:

```cpp
// Step 1.25: Attached weapon-stone raw-damage multiplier (physical actions only)
RunningDamage *= (1.0f + DamageStonePercent / 100.0f);
```

It live-resolves the attacker's active weapon attachment
(`Loadout->GetActiveWeaponLoadout()->WeaponEntry.GetAttachedItem()`), gates on
`Attachment.IsWeaponStone()` and `ActionType != Spell` (physical only, matching
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
(`GetWeaponStoneAbilities`) and validation (`ValidateWeaponStoneAbilities`) fill
and police those slots from `AssignedWeaponStoneAbilities`.

### Stones are non-elemental

`UEquipmentDataBase::GetCrystalElement()` switches `Crystal` / `Evolution` /
`None` only; a `WeaponStone` attachment falls through to the `Generic` default.
The runtime `FRuntimeAttachedItem::GetElement()` resolves a stone's identity
through `ItemIdentity::GetElement`, which has no case for `DamageStone` /
`AbilityStone` and likewise returns `Generic`. **Intended:** stones grant
mechanics, not an element. (Flagged because the code expresses it as a `default:`
fall-through rather than an explicit `WeaponStone` case — correct, but easy to
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
(`IsGemType` / `IsWeaponStoneType`), not by separate types.

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
  expected-value scorer reads crit as `1 + CritChance·(CRIT_MULTIPLIER−1)`, which
  assumes `CritChance ≤ 1.0`; unbounded crit would corrupt AI action scoring.
- **Resistance — bound at `1.0` for correctness only (done, commit `7afd0d09`).**
  A resistance `> 1.0` flips the buildup term `Amount *= (1 − Resistance)`
  negative and **heals** the gauge. The design cap was removed by raising
  `CombatConstants::RESISTANCE_MAX` `0.40 → 1.0`; it is now a hard correctness
  ceiling, not a balance knob. (The separate dead `DamageConstants::MAX_RESISTANCE
  = 0.50` is unrelated element-interaction leftover, still pending deletion.)
- **Luck — untouched.** Luck's "cap" (`LUCK_RAW_MAX`) is a **normalisation
  basis** for the crit-bonus curve (`RawLuck / LUCK_RAW_MAX × LUCK_CRIT_BONUS_MAX`),
  not a ceiling on luck itself. Nothing to lift.

---

## Integration Points

- **`DamageCalculator`** — Step 1.25 reads the active weapon attachment and
  applies the `DamageStone` multiplier (physical actions only).
- **`ItemExecutor`** — the `DamageStone` consumable self-buff path.
- **`LoadoutSystem`** — `FWeaponLoadoutEntry` stone-ability slots; `FSavedLoadout`
  / `FCombatLoadout` plumbing; `GetEquippedCrystals` surfaces `WeaponStone` slots.
- **`WeaponSystem`** — `UEquipmentDataBase` owns `AttachedItem`,
  `IsWeaponStoneAttached()`, `GetRestrictedCrystalTypes()`, the `IsDataValid`
  backstop, and the `DefaultAbilities` field seeded onto stone-granted slots.
- **`CrystalEffectTable` / `CrystalTypeHelpers` / `ItemIdentity`** — the value
  tables and the gem/stone predicates.

---

## ⚠️ Design / Phase 2 — NOT IMPLEMENTED

Everything below is **design intent only**. None of it is in the committed code.
Do not document these as behaviour; do not treat them as bugs where they
contradict shipped code (the contradictions are deliberate future changes,
flagged as such).

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
**hard-rejects any `WeaponStone` on a ring** — weapon stones are weapon-only.

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

### Planned rename: `WeaponStone` → `AugmentStone`

The family was named "**Weapon**Stone" when stones were weapon-only. The
rings-carrying-augments change above makes "Weapon" inaccurate, so
`EAttachedItemKind::WeaponStone` (and the surrounding `WeaponStone*` surface) is
**slated to be renamed `AugmentStone`**. Parked for the **naming phase**,
alongside the broad `Crystal → Item` rename (see *Parked cleanup*).

> Docs use the **current** name (`WeaponStone`) throughout. **Do not rename in
> code or docs now** — this is recorded intent only.

### The stat-stone family — dual-form model

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

**Crit — currently additive (shipped), being converted to multiplicative.** Today
crit is **additive** (not a whole-percent multiplier) and, since commit `7afd0d09`,
**uncapped** (bounded at `1.0` for the AI scorer). Under the **Balance framework**
below (#2 — ⚠️ modifies shipped behaviour) it is being **converted to
multiplicative** like every other stat, so crit stops being an exception. Until that
conversion lands, the shipped hook is still additive.

#### Balance framework — the crystal/stone number model

The whole stat-stone family — and the three stat *crystals* that have stone
analogues — runs on one small set of rules. All of this is **design / not built**;
two parts (**#2 crit conversion**, **#5 stat-crystal rebalance**) **modify shipped
behaviour** and are flagged inline.

**1. Everything multiplicative.** Every stat-stone and stat-crystal applies
`×(1 + pct/100)` on the stat's **current value** — no additive stat effects.
Defense, Speed, and raw Damage already work this way; this makes it the **universal
rule** for the family.

**2. Crit becomes multiplicative — across the board. ⚠️ MODIFIES SHIPPED BEHAVIOUR.**
- *Today (shipped):* crit is **additive** everywhere — `GetCriticalChance`
  (`DamageCalculator.cpp:~341-368`) does `BaseCrit += pct/100` for the skill-effect
  `CritChanceBuff` / `CritChanceDebuff`, equipment `BonusCritChance`, and
  `ModifyCritChance`; Opal applies `GetCritBuffPercent` additively.
- *Planned:* crit becomes **multiplicative** everywhere — stones, Opal, the
  `CritChanceBuff` / `Debuff` skill effects, and equipment `BonusCritChance` all
  apply as `×(1 + pct/100)` on the crit value, not `+=` points.
- *Why:* a multiplicative bonus **self-scales** — `+30%` is worth more to a
  high-crit build and less to a low one — so it can't dump flat points that
  trivially fill the (now-uncapped, commit `7afd0d09`) bar, and it makes crit
  consistent with every other stat. This is what lets the **uniform numbers** (#3)
  work without crit being overpowered.
- *Consequence (intended):* reaching 100% crit from item bonuses alone becomes
  mathematically hard — multipliers approach but don't easily reach `1.0`. Crit
  builds invest in the **base crit stat**; items amplify it.
- *Touches:* the shipped additive sites above **+** Opal's `GetCritBuffPercent`
  application; rebalances Opal. **To-build + re-verify.**

**3. Uniform numbers — one shared stone curve.** All stat-stones share **one**
per-tier curve: the current `DamageStone` curve **3 / 5 / 7 / 9 / 11 / 13 / 15**
(F..S). Because the operator is multiplicative (#1), uniform numbers do **not** mean
uniform power — the multiply scales each stat by its own value. No per-stat stone
curves.

**4. Crystal = stone × 2 (stat crystals only).** A stat crystal's magnitude is its
stone's value **×2** — the single knob for the "stones weaker than crystals"
principle. With the shared stone curve `3..15`, the stat-crystal curve is
**6 / 10 / 14 / 18 / 22 / 26 / 30**. Applies **only** to the three stat crystals that
have stone analogues: **Amber** (Defense), **Opal** (Crit), **Emerald** (Speed).

**5. Stat-crystal rebalance. ⚠️ MODIFIES SHIPPED BEHAVIOUR.** The three stat crystals
re-tune to the stone×2 curve:

| Crystal (stat) | Shipped today | → Planned (stone×2) | Operator |
|---|---|---|---|
| Amber (Defense) | 15/20/25/30/35/40/50 | 6/10/14/18/22/26/30 | stays `×(1+pct)` |
| Emerald (Speed) | 10/15/20/25/30/35/40 | 6/10/14/18/22/26/30 | stays `×(1+pct)` |
| Opal (Crit) | 5/8/10/12/15/18/25 (**additive**) | 6/10/14/18/22/26/30 | **→ multiplicative** (per #2) |

All three converge to the **same** numbers — their power differs through the
multiplicative operator acting on different base stats, not through different numbers.

**6. Ability crystals untouched (the framework boundary).** The other **seven**
crystals have **no stone analogue** and are **not** part of this framework — their
values stay exactly as audited: Garnet (DOT), Sapphire (heal), Citrine (EP), Onyx
(silence), Amethyst (gamble), Iolite (cleanse), Quartz (status-clear). The framework
applies to **exactly the three stat crystals** above and nothing else.

**7. Attached vs consumable — OPEN.** Is the shared `3..15` curve the **consumable**
value (timed), with the **attached** form discounted for permanence — or **one curve
for both** forms (as `DamageStone` does today)? **To-decide at build time**, leaning
toward `DamageStone`'s one-curve-both precedent unless permanence proves too strong
in play.

**8. Build order.** (1) **Crit additive → multiplicative conversion** — core combat;
survey-first, re-verify. (2) **Stat-crystal rebalance** to stone×2. (3) **Stat-stone
family** on the shared curve. Crit conversion is **first** because everything else
assumes the multiplicative model.

#### `DamageStone` consumable — planned change ⚠️ MODIFIES SHIPPED BEHAVIOUR

`DamageStone` is the template the dual-form model generalises from, and its
**consumable form changes** under this design:

| Form | Shipped today | Planned |
|---|---|---|
| **Attached** | self-passive `+damage` (Step 1.25 multiplier) | **unchanged** |
| **Consumable** | self-only: `RawDamageBuff` on the **user** (3 turns) | **targeted directional**: pick a target — ally → `+damage` buff, enemy → `−damage` debuff |

> **Flag — re-verify when built.** The attached path is unchanged; the
> **consumable** path gains (a) **target selection** and (b) an **enemy-debuff
> direction** (negative application to an enemy target) it does not have today.
> Both are **new mechanics to build**, and this contradicts the currently-shipped
> self-only consumable — record both states so the change isn't read as a
> regression.

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
| CritChance | **today additive + now-uncapped**; uses crit's own additive hook *(planned → multiplicative across the board — see Balance framework #2, ⚠️ shipped-behaviour change)* |
| Luck | wired but **balance-risky** — it is the normalization basis for luck-derived chances |
| **SpellDamage** ("spell power") | **WIRED — corrected.** Three attacker-side reads, all `ActionType==Spell`-gated: `GetEvolutionModifiedSpellDamage` (Mind curve), equipment `BonusSpellDamage`, `ActionMods` spell-power. **Buildable now.** *(The earlier audit wrongly listed this unwired — it conflated offensive spell-power, wired, with defensive spell-resistance, the net-new hook above.)* |
| Resistance (status) | **wired** (`StatusBuildupManager`) for status buildup; the **spell-damage shave** is the net-new extension (see *Defense / Resistance*) |

**Visual-only read site (animation play-rate — intended effect not wired; see *Speed stones*):**

| Substat | Note |
|---|---|
| SpellSpeed | has a montage play-rate read (`CalculateSpellSpeed`), commented **"purely visual"**; its *intended* defense-window-pressure effect is **unwired** |
| ActionSpeed | **corrected** — it **has** a read site (`CalculateAnimationSpeed` / `ANIMATION_SPEED_PER_POINT`, drives ability/attack montage PlayRate). What's missing is the **defense-window link**, not the read |

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
animation gives the defender a **tighter reaction window**; slower gives more. This
**does not work today.**

**Why it doesn't work (factual, from the defense-window survey):** the real-time
defense window is a **fixed `0.3s` constant** (`0.5s` for AOE), **hardcoded at the
`ActionExecutor` call sites** and **independent of animation play-rate**. The
data-driven `FDefenseWindowConfig` (`WindowStartTime` / `WindowDuration`) exists in
`DefenseSystem.h` but is **dead / unwired** — never read. So changing a montage's
play-rate (which `ActionSpeed` / `SpellSpeed` already do) changes only the visual,
not the window.

**The prerequisite — a net-new "variable-defense-window" feature:**
- Thread the attacker's effective `PlayRate` into the window-open path.
- Replace the hardcoded `0.3f` with `WindowDuration = BaseWindow / AttackerPlayRate`
  (faster → tighter). Optionally revive `FDefenseWindowConfig` for per-attack
  authoring of `BaseWindow`.
- Mostly touches **`ActionExecutor`** (the 3 execute paths + the
  `OpenDefenseWindowsForTargets` signature). **`DefenseSystem` needs no core
  change** — it already takes a per-call duration. Downstream: the AI defense
  scheduler gets variable timing **for free** (intended).

> **Status: BLOCKED.** Speed stones are deferred until the variable-defense-window
> feature exists (its own future task). Until then a speed stone would only alter
> animation visuals, not the reaction window — not worth shipping.

#### Build order & final classification

The attached/consumable channel is still proven on **`Defense` first** (low-risk,
self-passive, whole-percent), then the **directional consumable** is retrofitted
onto `DamageStone` and inherited by the rest. The full set, by readiness:

| Class | Stones |
|---|---|
| **Buildable now** | `DamageStone` (consumable directional retrofit), `Defense`, `TurnSpeed`, `StatusMultiplier`, `Efficiency`, `CritChance` (additive today → multiplicative, Balance framework #2), **`SpellDamage`** (confirmed wired), `Resistance` (status + the `0.2` spell-shave hook), `MaxHP` / `MaxEnergy` (pool-recompute path) |
| **Build with care (later)** | `Luck` — the normalization basis for luck-derived chances; balance-risky |
| **Blocked on a system** | `ActionSpeed` / `SpellSpeed` — need the variable-defense-window feature above |
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
  line. Bundled with the `WeaponStone → AugmentStone` rename above for a single
  naming pass.
- **`CrystalIdentity.h` file rename** — the file already hosts the `ItemIdentity`
  namespace; the filename lags.
- **`GetPrimaryEffectType` doc mentions** — `ItemSystem.md` historically referred
  to `GetPrimaryEffectType()`; the function is now `GetItemEffectType()` (commit
  `5c22e6e`). Corrected in `ItemSystem.md`; noted here as part of the rename
  sweep.
- **`FRuntimeAttachedItem.h` "Refined" comment drift** (see *Architecture*) —
  stale code comments to clean up when the rename pass touches that file.
- **Opal crit double-table bug** — `GetCritBuffPercent` (the live crit handler)
  tops at **S=25**, but the vestigial Opal arm inside `GetBuffPercentage` tops at
  **S=20** — two different S values for the same crystal. Reconcile when the crit
  rebalance (*Balance framework #5*) touches Opal. *(Surfaced by the crystal
  magnitude audit.)*
- **Duplicate duration tables** — `GetCrystalDuration`, `GetGambleDuration`, and
  `GetResistanceDuration` are three byte-identical `4/4/3/3/3/2/2` tables; and
  `GetBuffPercentage` carries a redundant Emerald arm duplicating
  `GetSpeedBuffPercent`. DRY-collapse when touched.

---

## Known Limitations / TODOs

- **Editor grey-out, not hide.** Wrong-Kind `CrystalType` values are greyed and
  non-selectable, not hidden. True hiding would need an `IDetailCustomization`
  editor module (declined per the `UEquipmentDataBase` cosmetic-limitation note).
- **Fresh `WeaponStone` slot trips validation immediately.** `CrystalType`
  defaults to `Garnet`, so a slot newly set to `Kind==WeaponStone` is invalid
  until the designer picks a stone — intended, but it lights up an `IsDataValid`
  error on first authoring.
- **Stone-ability UI** for the per-tier slots is not covered here (UI pass).
- All *Design / Phase 2* content above is unbuilt.

---

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-07 | Initial documentation — shipped weapon-stone system (unified `ECrystalType`/`FCrystalId` identity; `EAttachedItemKind::WeaponStone`; `DamageStone` whole-percent channel + 3-turn consumable; `AbilityStone` per-tier slots 2/3/3/4/4/5/6; `GetRestrictedEnumValues` grey-out + `IsDataValid` backstop; `CrystalTypeHelpers` gem/stone predicates). Recorded the `BonusRawDamage` trap, the unified-enum and attach-only rationale, and the cap decisions. Captured the Design/Phase-2 plan (Mastery stone, 7×7 fusion/cost matrix, selectable-substat fusion bonus, fusion restrictions, rings+AugmentStone rule, planned `WeaponStone→AugmentStone` rename, stat-stone family + read-site hooks, single-field grey-out risk) and parked cleanup. | feature/weapon-stones |
| 2026-06-07 | Phase-2 design revision — **Mastery → FusionStone** rename throughout; FusionStone is player-created (fusion consumes two owned attachables), `FFusionId{HalfA,HalfB,BonusStat}` composite with symmetric equality, `FCrystalId` unchanged (Option A); fusion bonus is the formula `(TierValue(A)+TierValue(B))/2` (7×7 shown as visualisation only); added `stat-stone+stat-stone` valid pairing; expanded the **stat-stone family** into the dual-form model (attached self-passive / directional timed consumable) with the `DamageStone`-consumable change flagged as a shipped-behaviour modification; added the **wired-substat audit** (wired/unwired/pool-stats) and the Defense-first **build order**. Refreshed the cap-decisions notes to reflect the now-shipped crit/resistance cap removal (commit `7afd0d09`). | feature/weapon-stones |
| 2026-06-07 | Stat-stone family extension + audit fixes — added the **pool-stat variant** (`MaxHP`/`MaxEnergy` raise/decrease-max, ceiling-only, overcapped-not-clamped, `RecomputeMaxPools` hook); the **Defense / Resistance mitigation model** (Defense already covers raw+spell, flat/subtractive — no change; Resistance gains a net-new slight spell-damage shave `×(1 − RESISTANCE_SPELL_SHAVE_FACTOR·Resistance)`, factor `0.2`, the **one** new combat hook, gated on `ActionType==Spell` via the unconsumed `bIgnoreResistance`); reclassified **Speed stones** as blocked on a net-new variable-defense-window feature (window is a fixed `0.3s`, play-rate-independent; dead `FDefenseWindowConfig`). **Audit corrections (factual):** `SpellDamage` moved unwired→wired (3 attacker reads, `Spell`-gated); `ActionSpeed` corrected (has a play-rate read; missing the window link, not the read); per-element resistance marked not-needed. Reworked build-order into a readiness classification (buildable-now / build-with-care / blocked / not-needed). | feature/weapon-stones |
| 2026-06-07 | Fusion restriction — **evolution crystals excluded from fusion entirely** (never a valid fusion half: a transformation mechanic, not a stat/element contributor). Recorded the underlying **valid-fusion principle** (stat-stone + one contributor — gem crystal or `AbilityStone`) from which the banned-pair list falls out; kept the explicit table and added the `anything + evolution` ❌ row. | feature/weapon-stones |
| 2026-06-07 | **Balance framework** for the stat-stone/crystal number model — everything multiplicative `×(1+pct/100)` on current value; one shared stone curve `3/5/7/9/11/13/15`; **crystal = stone×2** (`6/10/14/18/22/26/30`) for the three stat crystals only. **⚠️ shipped-behaviour changes flagged:** crit converts additive→multiplicative across the board (#2), and Amber/Opal/Emerald rebalance to the stone×2 curve (#5, Opal also flips to multiplicative). Ability crystals (other 7) explicitly out of scope (#6); attached-vs-consumable curve left OPEN (#7); build order crit-conversion → crystal-rebalance → stat-stone family (#8). Reframed the crit-exception note (today-additive + planned-multiplicative) and annotated the wired-audit crit row / build-order cell. Banked crystal cleanup: Opal double-table bug + duplicate duration tables. | feature/weapon-stones |
