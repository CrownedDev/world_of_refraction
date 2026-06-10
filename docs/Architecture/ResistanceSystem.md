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

## How it composes (the buildup-side resistance step)

Inside `UStatusBuildupManager::AddStatusBuildup`, the target-resistance `Resistance` accumulator (all percent-space fractions) sums, then clamps once to `[RESISTANCE_MIN, RESISTANCE_MAX]` (`[−1, +1]`) and applies `Amount *= (1 − Resistance)`:

```
Resistance  = CharacterData->CalculateResistance()          // base Spirit resistance
            + BonusResistance × RESISTANCE_PER_POINT          // equipment (loadout)
            + attached ResistanceStone %                      // weapon attachment
            + GetTotalElementResistance(Target, Element)      // element ResistanceBuff/Debuff effects
            + ClassInnateResistanceTable::GetClassInnateResistance(Target, Element, PhysicalType)  // ← this system
            + ModifyStatusResist / 100                        // skill-effect flat modifier
Resistance  = clamp(Resistance, RESISTANCE_MIN, RESISTANCE_MAX)
Amount     *= (1 − Resistance)
```

The class/innate term is **one additive line** ([`StatusBuildupManager.cpp`], right after `GetTotalElementResistance`). It is sign-preserving and shares the single clamp, so a strong weakness can push `Resistance` negative (amplifying buildup, capped at `−1` = ×2) and a strong resist adds toward the `+1` ceiling.

**Byte-identical for neutral matchups:** where the resolved column is `0` (e.g. a Resonator hit by any element, a Caster hit by a neutral element with `None` physical, any `Generic`-element / `None`-physical hit), the term is an exact `0.0f` and `Resistance` is unchanged — no behaviour change vs. before this system.

## Inspection & display

### `UResistanceProfileDebug` (`UBlueprintFunctionLibrary` — `Combat/Resistance/ResistanceProfileDebug.*`)

On-demand inspection of a character's full resolved 12-value profile without entering the buildup path. Two views, because the resolved row can diverge from the design-time row:

- `GetResistanceProfileString(UCharacterData*)` / `LogResistanceProfile(...)` — **design-time** view (BD signal = `InnateElement == BrokenDarkness`). Footer notes that a live transform would resolve the BD row instead.
- `GetActorResistanceProfileString(AActor*)` / `PrintResistanceProfile(AActor*, ...)` — **runtime** view (BD signal = component `IsBrokenDarkness()`), i.e. the row the buildup path actually uses (shows a transformed-BD character's real row mid-transform).

Each print labels the selection arm that fired (BD/Generic/Resonator/Caster-element) and all 12 values; the arm precedence mirrors `ResolveRow` exactly.

### `FResistanceProfileDisplay` on `UCharacterData` (read-only Details panel)

A transient, read-only mirror of the resolved row for the editor:

- `UPROPERTY(VisibleAnywhere, Transient, Category="Resistances") FResistanceProfileDisplay ResistanceProfile;` — 12 greyed/non-editable float fields.
- **Transient** → never serialized into the `.uasset`; the C++ table stays the single source of truth, no stale on-disk copy.
- Populated from `ResolveRow` (same source as everything else) in `PostInitProperties()` (new asset), `PostLoad()` (existing assets), and `PostEditChangeProperty()` (live re-run when `CharacterClass` / `InnateElement` change).
- **Display only** — no code path reads `ResistanceProfile` as a data source; the buildup path, the actor query, and the asset getters all call `ResolveRow` directly. Tooltips on the property and the `Darkness` field surface the design-time-vs-runtime divergence and the BD incoming-alias in the panel itself.
- Asset getters `UCharacterData::GetElementResistance(Element)` / `GetPhysicalResistance(Type)` (`BlueprintPure`, `Category="Resistances"`) expose the per-axis fraction for Blueprint/UI.

## Integration Points

- **`UStatusBuildupManager::AddStatusBuildup`** — the sole runtime consumer; one additive term in the target-resistance step. See `StatusBuildupSystem.md`.
- **`UCharacterDataComponent`** — `IsBrokenDarkness()` (runtime BD signal), `CharacterData` (class + `GetElement()`), via `FindComponentByClass`.
- **`UCharacterData`** — hosts the display struct, the lifecycle hooks, and the `BlueprintPure` getters; `ResolveRow` reads `CharacterClass` + `GetElement()`.
- **Enums** — `ECharacterClass`, `ESpellElement` (incl. `BrokenDarkness` alias + `Generic` → 0), `EPhysicalDamageType` (`None` → 0).

## Known Limitations / TODOs

- **Gear resistance not built.** Rings / weapons / evolutions granting resistance is a separate design arc; the additive composition already supports more terms (banked in `TODO.md`).
- **Design-time display can't see runtime transforms.** The `UCharacterData` panel shows the innate row only (asset has no runtime BD state); the runtime debug view covers the transformed case. This divergence is surfaced via tooltips, not hidden.
- **Asset-side BD = `InnateElement == BrokenDarkness`.** Only character-created BD is design-time-visible; runtime-transformed BD is reflected solely through `IsBrokenDarkness()` on the component path.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-10 | Initial documentation — table, selection order, two BD rules, two-axis composition, buildup integration, debug library, transient editor display. | feature/class-innate-resistance |
