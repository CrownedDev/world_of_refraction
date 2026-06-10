# Class / Innate-Element Resistance System

## Overview

A code-side resistance layer that makes a character's **class and innate element** shape how fast they accrue *status buildup* (not damage). Every character resolves to one full profile row; each incoming hit reads that row for its element and physical type, and the result is folded into the target-resistance step of `UStatusBuildupManager::AddStatusBuildup` as a single additive term.

Design intent: a Fire Caster shrugs off Fire buildup but takes Water faster; a Generic (physical) fighter is tough against weapons but soft to magic; a Resonator is the mirror; Broken Darkness is universally a little fragile. The numbers are balance-tunable in one place (`ClassInnateResistanceTable`), no data assets involved.

This is the **class** resistance arc. Gear-granted resistance (rings / weapons / evolutions) is a separate, not-yet-built arc — it will slot into the same additive composition (see `TODO.md` → *Resistances — gear arc*).

## The model

- Every profile is a **full 12-value row**: 9 element columns (`Fire Water Earth Wind Light Darkness Lightning Void Reality`) + 3 physical columns (`Slash Pierce Impact`), `0`-filled where blank.
- Values are **percent**: `+` resists (slower buildup), `−` is weak (faster buildup). Converted to a fraction by `/100`.
- A hit composes **two axes** — its element *and* its physical type — both read from the same row and **summed** (a fire sword swing is Fire *and* Slash). A `0` column contributes `0`, so single-axis hits (spell-only, or pure physical) read one axis naturally.
- The combined term is `(elementColumn + physicalColumn) / 100`, added into the buildup-side `Resistance` and clamped together with everything else to `[−1, +1]`.

### The table (percent; + resist / − weak)

| Profile (selector) | Fire | Water | Earth | Wind | Light | Dark | Ltng | Void | Reality | Slash | Pierce | Impact |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **Fire** (Caster) | +15 | −15 | 0 | −5 | 0 | 0 | 0 | 0 | −15 | 0 | 0 | 0 |
| **Water** (Caster) | −15 | +15 | 0 | 0 | 0 | 0 | −15 | 0 | −15 | 0 | 0 | 0 |
| **Earth** (Caster) | 0 | −5 | +15 | 0 | 0 | 0 | −15 | 0 | −15 | 0 | 0 | 0 |
| **Wind** (Caster) | 0 | 0 | −15 | +15 | 0 | 0 | 0 | 0 | −15 | 0 | 0 | 0 |
| **Light** (Caster) | 0 | 0 | 0 | 0 | +15 | −15 | 0 | 0 | −15 | 0 | 0 | 0 |
| **Darkness** (Caster) | 0 | 0 | 0 | 0 | −15 | +15 | 0 | −10 | −15 | 0 | 0 | 0 |
| **Lightning** (Caster) | 0 | +15 | 0 | 0 | 0 | 0 | +15 | 0 | −15 | 0 | 0 | 0 |
| **Void** (Caster) | 0 | 0 | 0 | 0 | −10 | −10 | 0 | 0 | −10 | 0 | 0 | 0 |
| **Reality** (Caster) | +10 | +10 | +10 | +10 | +10 | +10 | +10 | −10 | 0 | 0 | 0 | 0 |
| **Broken Darkness** (state) | −5 | −5 | −5 | −5 | −5 | −5 | −5 | −5 | −15 | 0 | 0 | 0 |
| **Generic** (class) | −5 | −5 | −5 | −5 | −5 | −5 | −5 | −5 | −5 | +15 | +15 | +15 |
| **Resonator** (class) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | −15 | −15 | −15 |

Three named tiers span the whole table — `R_STRONG = 15`, `R_MED = 10`, `R_LIGHT = 5` — used `±`; no other magnitudes appear. The numbers live only in `ClassInnateResistanceTable.h`; rebalancing is a code change.

## Architecture

### `ClassInnateResistanceTable` (namespace — `Public/Combat/Resistance/ClassInnateResistanceTable.h`)

Header-only inline table + resolvers (the `CrystalEffectTable` convention), with one out-of-line actor query in the matching `.cpp`.

- `FResistanceRow` — plain (non-reflected) struct of 12 named `float` fields; **field order == column order**.
- `GetElementRow(InnateElement)` — the 9 Caster rows, keyed by element. Only reached for Casters (BD / Generic / Resonator are handled by the selection arms before this).
- `ResolveRow(Class, InnateElement, bIsBrokenDarkness)` — returns the one resolved row per the selection order below. Pure (no actor); shared by the runtime path, the asset getters, and the display.
- `GetElementColumn(Row, IncomingElement)` — reads the row's column for the **incoming attack** element. Includes the **BD incoming alias** (see below). `Generic` incoming → `0`.
- `GetPhysicalColumn(Row, PhysicalType)` — reads the physical column; `None` → `0`.
- `GetClassInnateResistance(AActor* Target, Element, PhysicalType)` *(out-of-line)* — resolves `Target`'s row from its `UCharacterDataComponent` (class + `GetElement()` + `IsBrokenDarkness()`), returns `(GetElementColumn + GetPhysicalColumn) / 100`. Null-safe (`0` when actor/component/CharacterData missing).

### Selection order (row resolution)

Resolves to exactly one row, in strict precedence:

1. **`IsBrokenDarkness()`** (innate **or** runtime-transformed) → the **Broken Darkness** row. *Overrides class.*
2. else **Generic** class → the **Generic** row.
3. else **Resonator** class → the **Resonator** row.
4. else (**Caster**) → `GetElementRow(InnateElement)` (the 9 standard elements; a BrokenDarkness innate is already caught by arm 1).

### Two BD rules — distinct

- **BD target row** (*which row*): a Broken-Darkness character uses the BD row regardless of underlying class/element (arm 1). Runtime path keys this on `IsBrokenDarkness()`.
- **BD incoming alias** (*which column*): an incoming attack whose element is `BrokenDarkness` reads the **Darkness** column of whatever the target's row is (`GetElementColumn`). These are independent — a BD attacker hitting a BD target reads the BD row's Darkness column (`−5`).

## How it composes — `GetTotalStatusResistance` (the single source of truth)

Total defender-side status resistance is computed in **one place**: `UStatusBuildupManager::GetTotalStatusResistance(Target, Element, PhysicalType)`. `AddStatusBuildup` just calls it: `Amount *= (1 − GetTotalStatusResistance(...))`. The class/innate profile is **one of seven** additive sources it composes (all percent-space fractions):

```
Resistance  = CharacterData->CalculateResistance()                       // 1. base Spirit (pre-clamped [0, MAX])
            + BonusResistance × RESISTANCE_PER_POINT                       // 2. equipment substat (loadout)
            + attached ResistanceStone %                                   // 3. weapon attachment
            + GetTotalElementResistance(Target, Element, PhysicalType)     // 4. element + physical effects
            + ClassInnateResistanceTable::GetClassInnateResistance(...)    // 5. class/innate profile (this system)
            + GetActiveResistanceBonus matched (element + physical) / 100   // 6. rolled/authored gear resistance
            + ModifyStatusResist / 100                                     // 7. skill-effect flat modifier
Resistance  = clamp(Resistance, RESISTANCE_MIN, RESISTANCE_MAX)            // single FINAL clamp [−1, +1]
return Resistance                                                         // POST-clamp value the caller applies
```

- **Guard inside:** returns `0.0` when `Target` has no `UCharacterDataComponent`/`CharacterData`, so `Amount *= (1 − 0)` is unchanged (behaviour-preserving for the no-data case).
- **Base pre-clamp is intentional and preserved.** `CalculateResistance()` clamps the base to `[0, MAX]` *before* the other terms are summed. Net effect: a later weakness term bites off the capped base rather than being absorbed by over-cap headroom — **weaknesses always bite** (a locked design decision, not a cleanup candidate). The aggregate then gets the single final clamp.
- **Sign-preserving / shared clamp:** a strong weakness pushes `Resistance` negative (amplifying buildup, capped at `−1` = ×2); a strong resist adds toward the `+1` ceiling.
- **Offense stays out.** Source-side amplification (`GetSourceStatusMultiplierFactor`, transient StatusMultiplier buff/debuff, BD absorption-stack multiplier) is applied to `Amount` *before* this call and is **not** part of resistance. This getter is the defender's resistance only.
- **Byte-identical for neutral matchups:** where a resolved column / effect contributes `0` (a Resonator hit by any element, a Caster hit by a neutral element with `None` physical, any `Generic`-element / `None`-physical hit), the term is an exact `0.0f` and `Resistance` is unchanged.

### Effect-based resistance keys on EITHER axis (element OR physical)

Source #4, `GetTotalElementResistance(Target, Element, PhysicalType)`, queries the active `ResistanceBuff` / `ResistanceDebuff` effects and returns `(ΣBuff − ΣDebuff)/100`. Each effect matches on **one** axis, decided by `FActiveSkillEffect::PhysicalType`:

- `PhysicalType != None` → **physical-keyed**: matches the incoming `PhysicalType` (a Slash-resistance buff reduces Slash buildup only).
- `PhysicalType == None` → **element-keyed**: matches the incoming `Element` (a Fire-resistance buff reduces Fire buildup only).

This is what lets resistance *effects* (items, spells) target a physical type, not just an element — previously the effect path was element-only and the class table was the sole physical-resistance source. The `PhysicalType` field defaults to `None`, so every pre-existing effect stays element-keyed (byte-identical until a physical-keyed effect is created). `FActiveSkillEffect` is a runtime-only struct (not in `.uasset`/SaveGame), so appending the field is serialization-safe.

## Gear resistance

Equipment grants status-buildup resistance via **two independent paths**, both folding into the composition above. Inert by default — until resistance is authored or rolled, every term is `+0` and behaviour is unchanged.

### Path 1 — authored EFFECT resistance (rides source #4)

`FSkillEffect` (the authored effect on `UEquipmentDataBase::Effects` / `UEvolutionItemData::Effects`) now carries `Element` + `PhysicalType` (mirroring the runtime `FActiveSkillEffect`). `CreateFromSkillEffect` copies both onto the runtime effect. So a piece of gear can author a `ResistanceBuff`/`ResistanceDebuff` keyed to an element **or** a physical type, applied **while equipped** (and therefore **cleansable** / conditional). It needs **no new getter term** — it lands in `ActiveEffects` and is consumed by source #4 (`GetTotalElementResistance`'s either-axis match). `FSkillEffect` **is** `.uasset`-authored, so appending the two fields is the one migration-touching change; defaults (`Generic`/`None`) make existing authored effects load identically.

### Path 2 — rolled BAKED resistance (`FResistanceBonus`, source #6)

A per-instance, permanent resistance pool — `FResistanceBonus`: 12 named float fields (9 elements + Slash/Pierce/Impact), **field order mirrors `FResistanceRow`**.

- **Rolled zero-sum.** `RerollResistance(Tier)` distributes an **own per-tier budget** across the 12 categories via the shared `ZeroSumRoll::Distribute` core — `+resist on some costs −weakness on others` (net signed sum ≈ budget). The negative spread scales with tier (low tiers can roll near-all-positive; B/A/S show meaningful weaknesses). Per-category roll bounded by `RESISTANCE_CATEGORY_CAP`; field clamped to `RESISTANCE_BONUS_MIN/MAX`.
- **Shared core, substat byte-identical.** `ZeroSumRoll::Distribute` (`Equipment/ZeroSumBrokenStick.h`) was **extracted verbatim** from the substat roll's per-pillar distribution (same offset-pool + cap + RNG draw order). The substat path now calls it (per pillar); resistance calls it once over a flat 12-slot pool (no pillar split). The substat roll is **byte-identical** for a given seed.
- **Own pool, separate from stats.** Rolling resistance does **not** compete with the stat substat budget — `RESISTANCE_BUDGET_*` is independent of `SUBSTAT_BUDGET_*`.

### Storage — template→instance (mirrors `StatBonus`)

*(Updated for the per-instance roll system, U0–U4 — see `PerInstanceRollSystem.md` for the full model.)*

`UEquipmentDataBase` (weapon + ring) holds `BaseResistance` (authored, the real shipped values — copied into the inventory entry's `ResistanceBonus` at `CreateFromWeapon`/`CreateFromRing` time) and `GeneratedResistance` (**designer PREVIEW**, gameplay-inert as of U4 — never copied). When the asset's `bRandomGenerateOnPickup` is ON, acquisition (`AddWeapon`/`AddRing`) replaces the instance copy with **Base + a fresh per-instance roll** from the instance's stored `ResistanceMaxPool`. The asset `Roll Resistance` button writes the preview only; `UEquipmentBonusGenerator::GenerateResistance` is the gear-agnostic roll core.

**Evolution** (`UEvolutionItemData`) holds authored `BaseResistance` (real) + preview `GeneratedResistance` (inert; its `GetCombinedResistance` was deleted as dead in U4). The **per-instance** rolled resistance lives on `FEvolutionInventoryEntry` (rolled at `AddInstance` when the toggle is ON) and travels to the slotted `FEvolutionAttachment` via loadout inflation. Evolution resistance is **ALWAYS-ON while slotted**, **deliberately decoupled** from evolution's infusion-scaled offensive `BaseStatBonus` (a separate path, untouched).

### Read — source #6

`ULoadoutComponent::GetActiveResistanceBonus(Actor)` aggregates the per-instance `ResistanceBonus` across equipped pieces (weapon + ring per class, same resolution as `GetActiveStatBonus`) **plus** evolution as `asset.BaseResistance + attachment.GeneratedResistance` (both the weapon-attached and standalone-slot cases, via `GetActivePrimaryEvolutionAttachment` / `PrimaryEvolution`). `GetTotalStatusResistance` reads it as **term #6** by viewing the aggregate as an `FResistanceRow` and calling the **same** `GetElementColumn`/`GetPhysicalColumn` helpers as the class profile — so the **BD→Darkness alias is identical by construction** (incoming `BrokenDarkness` reads the gear's Darkness column) and cannot drift. Additive, shares the single final clamp.

### Balance — `RESISTANCE_CATEGORY_CAP` is the immunity-stacking knob

Per-category rolled value is capped at `RESISTANCE_CATEGORY_CAP` (10) per piece. Worst-case procedural stack = 3 contributing pieces × 10 + class single-category max (15) = **45%**, far from the +100% immunity ceiling (the `[−1,+1]` clamp). `GetCombinedResistance` is **not** re-clamped (Base+Generated summed raw, like `GetCombinedStatBonus`), so authored `BaseResistance` (≤15/field) is uncapped in combination and is the designer's responsibility; the procedural cap protects generated rolls.

### Constants (`CombatConstants.h`)

- `RESISTANCE_BUDGET_F..S` = 8/12/16/20/25/30/36 — own per-tier point budget (separate pool).
- `RESISTANCE_CATEGORY_CAP` = 10 — per-category roll cap; **the primary PIE-tunable balance lever** against trivial immunity stacking.
- `RESISTANCE_BONUS_MIN/MAX` = ±15 — hard per-field clamp on the stored value.

## Inspection & display

### `UResistanceProfileDebug` (`UBlueprintFunctionLibrary` — `Combat/Resistance/ResistanceProfileDebug.*`)

On-demand inspection of a character's full resolved 12-value profile without entering the buildup path. Two views, because the resolved row can diverge from the design-time row:

- `GetResistanceProfileString(UCharacterData*)` / `LogResistanceProfile(...)` — **design-time** view (BD signal = `InnateElement == BrokenDarkness`). Footer notes that a live transform would resolve the BD row instead.
- `GetActorResistanceProfileString(AActor*)` / `PrintResistanceProfile(AActor*, ...)` — **runtime** view (BD signal = component `IsBrokenDarkness()`), i.e. the row the buildup path actually uses (shows a transformed-BD character's real row mid-transform).

Each print labels the selection arm that fired (BD/Generic/Resonator/Caster-element) and all 12 values; the arm precedence mirrors `ResolveRow` exactly.

**Combined-resistance breakdown** (`GetCombinedResistanceString` / `PrintCombinedResistance`, runtime): for a live actor + incoming `(Element, PhysicalType)`, prints the unified `GetTotalStatusResistance` total **plus the per-source breakdown** — all seven sources labelled `1–7` (incl. `6. rolled gear resistance`) with their fractions, the **pre-final-clamp sum**, the **post-clamp total**, and the clamp bounds, so the clamp's effect and each source's contribution are visible together (the "are they all noticed?" check). Every term is read via the *same* function the getter uses, and the manually-summed-then-clamped value is cross-checked against `GetTotalStatusResistance` with a `MATCH`/`MISMATCH` flag — so the breakdown cannot silently drift from the getter.

### `FResistanceProfileDisplay` on `UCharacterData` (read-only Details panel)

A transient, read-only mirror of the resolved row for the editor:

- `UPROPERTY(VisibleAnywhere, Transient, Category="Resistances") FResistanceProfileDisplay ResistanceProfile;` — 12 greyed/non-editable float fields.
- **Transient** → never serialized into the `.uasset`; the C++ table stays the single source of truth, no stale on-disk copy.
- Populated from `ResolveRow` (same source as everything else) in `PostInitProperties()` (new asset), `PostLoad()` (existing assets), and `PostEditChangeProperty()` (live re-run when `CharacterClass` / `InnateElement` change).
- **Display only** — no code path reads `ResistanceProfile` as a data source; the buildup path, the actor query, and the asset getters all call `ResolveRow` directly. Tooltips on the property and the `Darkness` field surface the design-time-vs-runtime divergence and the BD incoming-alias in the panel itself.
- Asset getters `UCharacterData::GetElementResistance(Element)` / `GetPhysicalResistance(Type)` (`BlueprintPure`, `Category="Resistances"`) expose the per-axis fraction for Blueprint/UI.

## Integration Points

- **`UStatusBuildupManager::GetTotalStatusResistance`** — the single composition point for all six defender sources; called by `AddStatusBuildup`. The class/innate term (source #5) is one line here. See `StatusBuildupSystem.md`.
- **`UStatusBuildupManager::GetTotalElementResistance`** — source #4; the element-OR-physical effect query (`ResistanceBuff`/`ResistanceDebuff`).
- **`UCharacterDataComponent`** — `IsBrokenDarkness()` (runtime BD signal), `CharacterData` (class + `GetElement()`), via `FindComponentByClass`.
- **`UCharacterData`** — hosts the display struct, the lifecycle hooks, and the `BlueprintPure` getters; `ResolveRow` reads `CharacterClass` + `GetElement()`.
- **Enums** — `ECharacterClass`, `ESpellElement` (incl. `BrokenDarkness` alias + `Generic` → 0), `EPhysicalDamageType` (`None` → 0).

### Effect-source application paths (who creates resistance effects)

Resistance *effects* (source #4) are created element- or physical-tagged by item/spell paths:

- **Quartz item** — applies an element-tagged `ResistanceBuff` for the bar's pending element (F–A) or element immunity (S).
- **Resistance Stone consumable** — applies general `ModifyStatusResist` (source #7, element-agnostic, sign-encoded ally/enemy), **not** an element-tagged effect.
- **Amethyst gamble** (`ItemExecutor.cpp`) — a gambled `ResistanceBuff`/`ResistanceDebuff` now picks a **random category across all 12** (9 elements: Fire/Water/Earth/Wind/Light/Darkness/Lightning/Void/Reality — excluding `Generic`/`BrokenDarkness` — and 3 physical: Slash/Pierce/Impact), gated to resistance rolls only. Element-keyed rolls set `Element`; physical-keyed rolls set `PhysicalType` (and leave `Element = Generic`). Previously hard-coded to `Void`, which meant a gambled resistance effect only ever applied to Void attacks. The Amethyst's separate Void *status-buildup* (its flavor) is unchanged.

## Known Limitations / TODOs

- **Gear resistance — BUILT** (see *Gear resistance* above). Both paths shipped: authored effect (`FSkillEffect` now carries `Element`/`PhysicalType`, rides source #4) and rolled `FResistanceBonus` (source #6, own pool). Per-instance for weapon/ring (template→instance copy); always-on per-asset for evolution.
- **Authored `BaseResistance` is uncapped in combination.** `GetCombinedResistance` sums Base+Generated raw (no re-clamp, like `GetCombinedStatBonus`); the `RESISTANCE_CATEGORY_CAP` only bounds *rolled* values. Extreme authored stacking is designer responsibility; the `[−1,+1]` final clamp is the hard immunity ceiling.
- **`RESISTANCE_CATEGORY_CAP` is a first-pass value (10).** The primary balance lever against trivial immunity stacking — tune in PIE.
- **Design-time display can't see runtime transforms.** The `UCharacterData` panel shows the innate row only (asset has no runtime BD state); the runtime debug view covers the transformed case. This divergence is surfaced via tooltips, not hidden.
- **Asset-side BD = `InnateElement == BrokenDarkness`.** Only character-created BD is design-time-visible; runtime-transformed BD is reflected solely through `IsBrokenDarkness()` on the component path.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-10 | Initial documentation — table, selection order, two BD rules, two-axis composition, buildup integration, debug library, transient editor display. | feature/class-innate-resistance |
| 2026-06-10 | Effect-based physical-type resistance (`FActiveSkillEffect::PhysicalType`; `GetTotalElementResistance` matches either axis). Unified `GetTotalStatusResistance` getter (6-source composition, pre-clamp preserved, post-clamp return; `AddStatusBuildup` calls it). Combined-resistance debug breakdown (per-source + pre/post-clamp + MATCH guard). Amethyst gamble: random 12-category tag (resistance rolls). Gear-arc prerequisite noted (`FSkillEffect` lacks element/physical fields). | feature/class-innate-resistance |
| 2026-06-10 | Gear resistance BUILT. Path 1 — authored effect (`FSkillEffect` gains `Element`/`PhysicalType`, copied via `CreateFromSkillEffect`; rides source #4). Path 2 — rolled `FResistanceBonus` (12 categories, own `RESISTANCE_BUDGET_*` pool, zero-sum via shared `ZeroSumRoll::Distribute` extracted from the substat roll — substat byte-identical; `RESISTANCE_CATEGORY_CAP`/`RESISTANCE_BONUS_MIN/MAX`). Template→instance storage on weapon/ring entries; evolution authored+rolled, always-on while slotted via standalone Roll button. New getter term #6 (`GetActiveResistanceBonus`, reuses class column-read + BD alias); debug source #6. Inert by default. | feature/class-innate-resistance |
| 2026-06-10 | Per-instance roll integration (U0–U4, see `PerInstanceRollSystem.md`): resistance now rolls per OWNED INSTANCE at acquisition (toggle-gated, from the instance's stored `ResistanceMaxPool`); asset `GeneratedResistance` reframed as designer PREVIEW (gameplay-inert; `CreateFrom*` copies Base only; evolution's dead `GetCombinedResistance` deleted). Evolution rolled resistance relocated to `FEvolutionInventoryEntry`/`FEvolutionAttachment`; source #6 reads `asset.Base + attachment.Generated` for both evolution slot cases. | feature/class-innate-resistance |
