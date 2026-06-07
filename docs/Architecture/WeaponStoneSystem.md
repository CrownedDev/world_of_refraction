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

Valid and invalid input pairings for a fusion (the validity guard):

| Pair | Allowed? | Reason |
|---|---|---|
| stat-stone + crystal | ✅ | stat bonus + element/spells |
| stat-stone + `AbilityStone` | ✅ | stat bonus + slots |
| stat-stone + stat-stone | ✅ | two stat bonuses — both halves contribute |
| `AbilityStone` + `AbilityStone` | ❌ | two slot-grants, nothing to combine meaningfully |
| crystal + crystal | ❌ | element + element |
| `AbilityStone` + crystal | ❌ | slots + element |

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

**Crit is the exception to the channel shape** — it is **additive** (not a
whole-percent multiplier) and, since commit `7afd0d09`, **uncapped** (bounded at
`1.0` for the AI scorer). A crit stone uses crit's own additive hook, not the
multiplicative whole-percent one.

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

#### Wired-substat audit

This table gates **both** stat-stones **and** the FusionStone bonus dropdown — a
stat is eligible only if it has a working read site (otherwise the stone/bonus
silently does nothing).

**Wired (buildable now):**

| Substat | Note |
|---|---|
| RawDamage | the `DamageStone` channel |
| Defense | clean whole-percent target — the proving ground (see *Build order*) |
| TurnSpeed | wired |
| StatusMultiplier | wired |
| Efficiency | cost reduction |
| CritChance | **additive + now-uncapped**; uses crit's own hook shape, not the multiplicative channel |
| Luck | wired but **balance-risky** — it is the normalization basis for luck-derived chances |
| SpellSpeed | wired but **visual-only** play-rate — not worth a stone |

**Unwired (need a read site built first — a stone/bonus does nothing until then):**

| Substat | Blocker |
|---|---|
| SpellDamage ("spell power") | no read site wired |
| Resistance (element / spell-type) | the **unbuilt** `ResistsElement` system — distinct from the status-buildup `Resistance` uncapped in `7afd0d09` |
| ActionSpeed | no read site, and unclear vs `TurnSpeed` — needs definition first |

**Pool stats (different code path — `RecomputeMaxPools`, not the whole-percent channel):**

| Substat | Status |
|---|---|
| MaxHP / MaxEnergy | "health/energy item" intent **undecided** (raise-max stone vs restore consumable) — parked |

#### Build order (design intent, not a commitment)

1. **Generic attached stat channel, proven on `Defense`** — low-risk,
   self-passive, whole-percent. Establishes the reusable attached hook.
2. **Directional consumable** — target select + ally-buff / enemy-debuff —
   retrofitted onto `DamageStone` first, then inherited by every stat-stone.

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
