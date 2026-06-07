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
Mastery stone (see *Design / Phase 2*) fuses **any two attachables**, so both
fusion inputs need a common identity type.

> This document covers the **shipped** weapon-stone work on `feature/weapon-stones`
> (per-tier ability-stone slots, the whole-percent damage channel, the Kind/type
> dropdown filter + validation backstop) **and** a large body of **designed but
> not-yet-built** follow-on work (Mastery, fusion matrix, stat-stone family,
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

**Why the enum stays unified (no gem/stone split).** The planned Mastery stone
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
nominal caps when a future stone/Mastery targets them:

- **Crit — safe to leave fully uncapped.** The crit roll is `FRand() < chance`;
  a `chance > 1.0` simply always crits, which is harmless. *Current code state:*
  `GetCriticalChance` still clamps to `[0, MAX_CRIT_CHANCE]` (`0.60`) — a
  crit-targeting stat-stone would need that clamp lifted/raised. Flagged so the
  design intent (uncapped) and the present clamp are reconciled when built.
- **Resistance — bound at `1.0` for correctness only.** A resistance `> 1.0`
  flips the damage-reduction term negative and **heals** the target. The cap is
  a correctness floor, not a balance lever. *Current code state:*
  `MAX_RESISTANCE = 0.50`.
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

### Mastery Stone

- Its **own** `EAttachedItemKind` (and `ECrystalType` identity), attach-only.
- **Fuses any two attachables** (stones and/or crystals) into one stone whose
  effect is the **concatenation of both source contributions** plus an intrinsic
  **fusion bonus**.
- Identity = a **composite key** of the two source `(type, tier)` pairs.
- This is the reason the enum stays unified — a Mastery stone must be able to
  hold a stone+crystal pair under one identity type.

### The 7×7 tier matrix → overall tier → fusion bonus

- A **symmetric 7×7** matrix over the two sources' tiers (F…S) yields the fused
  stone's **overall tier**, which sets the magnitude of the intrinsic fusion
  bonus.
- The **same matrix doubles as the crafting cost matrix** (Phase 3) — one table,
  two uses.

### Fusion bonus targets a selectable substat

- The intrinsic fusion bonus is **not damage-only** — it targets a
  **designer/player-selectable substat**.
- **Restricted to wired substats**: a fusion bonus may only target a stat that
  has a working read site. The "no read site" stats (see *stat-stone family*)
  are ineligible until their read is wired.

### Fusion restrictions

Valid and invalid input pairings for a Mastery fusion:

| Pair | Allowed? | Reason |
|---|---|---|
| stat-stone + crystal | ✅ | stat bonus + element/spells |
| stat-stone + `AbilityStone` | ✅ | stat bonus + slots |
| `AbilityStone` + `AbilityStone` | ❌ | two slot-grants, nothing to combine meaningfully |
| crystal + crystal | ❌ | element + element |
| `AbilityStone` + crystal | ❌ | slots + element |

### Rings + AugmentStones (loosens the current ring-guard)

**Current committed behaviour (implemented):** `URingData::IsDataValid`
**hard-rejects any `WeaponStone` on a ring** — weapon stones are weapon-only.

**Planned Phase 2 change (designed, not built):**

- A ring **may** carry an augment stone **only if** it is a **Mastery stone that
  contains an elemental crystal** — the crystal supplies spells the ring can use.
- A **bare stat-stone** (`DamageStone`, etc.) or an **`AbilityStone`** on a ring
  stays **rejected** — a ring has no weapon damage and no use for ability slots.

> **Deliberate future contradiction of committed code.** This **loosens / will
> replace** the current hard ring-guard above. It is intentional, not a bug:
> the guard exists today because no valid ring-stone case existed yet; Mastery
> introduces one. Cross-references the *Fusion restrictions* table — only the
> `stat-stone + crystal` Mastery (which carries an element) qualifies for a ring.

### Planned rename: `WeaponStone` → `AugmentStone`

The family was named "**Weapon**Stone" when stones were weapon-only. The
rings-carrying-augments change above makes "Weapon" inaccurate, so
`EAttachedItemKind::WeaponStone` (and the surrounding `WeaponStone*` surface) is
**slated to be renamed `AugmentStone`**. Parked for the **naming phase**,
alongside the broad `Crystal → Item` rename (see *Parked cleanup*).

> Docs use the **current** name (`WeaponStone`) throughout. **Do not rename in
> code or docs now** — this is recorded intent only.

### The stat-stone family

- Future stones, **one per substat**, each granting its substat via the
  **whole-percent channel** + **one per-read-site hook**.
- **Crit is the exception** — it is additive and capped, with its own hook shape
  (not a whole-percent multiplier).
- **"No read site" stats** — `SpellDamage`, `Resistance`, `ActionSpeed`,
  `MaxEnergy` — currently have **no read wired**. A stone (or a Mastery fusion
  bonus) targeting one of these does **nothing** until its read site is built.
  Wiring the read is a prerequisite, not part of authoring the stone.

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
