# Generic Spell Inheritance + `ESpellElement::None` Sentinel

**Status:** ✅ BUILT — PIE-verified (`feature/generic-spell-inherit`). See *As-Built* + *Changelog* at the foot of this doc; several locked-design points evolved during build (Reality, colour, BD model). Architecture: `docs/Architecture/BrokenDarkness.md` (Model-B), `docs/Architecture/InfusionSystem.md` (resolver). Player-facing: `docs/Mechanics/GenericSpells.md`.

---

## Summary

Two coupled changes to `ESpellElement`:

1. **`Generic` is repurposed** from "non-elemental default" to **"inherit the source element at cast time."** A spell authored with `Element = Generic` is a polymorphic template (e.g. "Ball") that adopts whatever element its source provides.
2. **`None` is added** as the dedicated non-elemental sentinel — it takes over the job `Generic` used to do (default / "no element").

Once resolved, a Generic spell behaves **exactly like a normal spell of the resolved element**. No special damage, resistance, status, or colour logic — it is a real elemental spell after resolution.

---

## Behaviour

A Generic spell resolves to the element of whatever source it is slotted into. Display name prepends the source element.

| Source | Resolved element | Display name |
|---|---|---|
| Fire (innate / evolution / crystal / ring) | Fire | Fire Ball |
| Wind | Wind | Wind Ball |
| Darkness | Darkness | Darkness Ball *(no "Dark" prefix doubling)* |
| Broken Darkness (pool = Wind) | per BD rules | Dark Wind Ball |
| **Reality** | — | **cannot cast** |

Rules:
- Sources that can host/resolve a Generic spell: **innate, evolution, crystal, ring.**
- Naming prepends the source element name. **Darkness does not double** ("Darkness Ball", not "Dark Darkness Ball").
- **Reality cannot use Generic spells** — rejected at resolve.
- Fixed-element spells (`Element != Generic`) are unaffected — they keep their authored element.

---

## Enum change

Append-only preserved. `None` is appended at the end; existing values keep their numbers.

```cpp
Generic=0  Fire=1  Water=2  Earth=3  Wind=4  Light=5
Darkness=6  Lightning=7  Void=8  Reality=9
BrokenDarkness=10 (Hidden, deprecated)
None=11                                   // appended
```

### `None` is the default — via member initializer, not enum position

UE takes a UPROPERTY's default from its C++ initializer, not the underlying zero. So `None` is the default despite being value 11:

```cpp
ESpellElement Element = ESpellElement::None;   // was = ESpellElement::Generic
```

This matches the existing `None`-sentinel convention elsewhere (`EPhysicalDamageType::None`, `EPrimarySlotType::None`, etc.).

---

## Migration surface

### Sentinel sites — flip `Generic` → `None`

These currently use `Generic` to mean "non-elemental" and must become `None`:

- `FActiveSkillEffect::Element` default (`ActiveSkillEffect.h`)
- `ActionExecutor` — `AbilityElement = Generic` (uninfused default)
- `EquipmentDataBase::GetCrystalElement()` — no-source return
- `ItemExecutor` — gamble physical-keyed effects
- `StatusBuildupManager` — pending-element fallback
- Resistance table `default:` arms (`ClassInnateResistanceTable.h`) — see note below

### Zero-init audit — paths that bypass the member initializer

These resolve to value `0 = Generic` and would silently become "inherit" instead of "None":

- raw `static_cast<ESpellElement>(0)`
- bare `ESpellElement X;` declarations with no `= ...`
- zero-initialized memory blobs

These are code paths, not asset defaults — a grep finds them all.

### Resistance-table invariant

Post-change, a **Generic spell should never reach the resistance tables** — it is resolved to a real element first. `Generic` arriving at `GetElementColumn` / `GetElementRow` indicates an *unresolved spell* and should be logged as a bug. `None` reaching them is legitimate (non-elemental / physical) and returns the zero result.

### BD loop bound

`ULoadoutComponent::InitializeBDPools` iterates `for (i = 0; i <= ESpellElement::BrokenDarkness; ++i)`. `None` is appended **past** `BrokenDarkness`, so it is not iterated — correct by construction. Keep `None` off this bound. (Phase-2 TODO to move this to an explicit Max sentinel still stands.)

---

## Build shape (3 parts)

1. **Append `None`** to the enum; keep the BD loop bound off it.
2. **Flip defaults** — every explicit/implicit `Generic`-as-non-elemental default → `None` (sentinel sites + zero-init audit).
3. **Resolve-at-cast** — add the one new step: `Generic → source element`, **Reality rejected**, naming prefix applied (no Darkness doubling).

The only genuinely new logic is the resolve-at-cast step. Parts 1–2 are mechanical migration.

---

## Notes

- `Generic` the **element** and `Generic` the **character class** remain distinct concepts that share a name. Any read must be unambiguous about which type it checks (`ESpellElement::Generic` vs the class `IsGeneric()`).
- Survey before build: confirm the full migration surface (sentinel sites + zero-init grep) before any edits, per multi-file discipline.

---

## As-Built — design evolutions during the build

The build held to the locked plan except for these points, which evolved (this doc predates them):

1. **Reality USES Generic spells — NOT rejected.** The original design rejected a Generic spell on a Reality source. As built, a Reality source resolves the Generic spell to **Reality** and casts it normally; the display name is **"Reality [Name]"** ("Reality Ball"). There is no Reality reject in `ResolveSpellCastElement` or the slot gates. *(A cast-time "Reality cannot cast Generic" reject remains available as a future option but was not built — Reality-as-valid is the shipped behaviour.)*
2. **Generic / None colour = BROWN everywhere.** `ElementColors::GetColorForElement` maps both `Generic` and `None` to the neutral brown (`0.6,0.4,0.2`). The status bar's old **white** neutral early-return was changed to the same brown, so physical/None damage paints identically across every surface (status bar, projectile, VFX, getter).
3. **Broken Darkness = single-active-pool rotation (Model-B).** This is a real BD behaviour change. A BD casts from **one** active pool at a time (`GetActivePool()` = `AbsorbedElements.Last()`), with **Darkness seeded on transform** (`SeedBaseElement`, element axis only, no energy). Absorbing rotates the active pool; absorbing Darkness returns to the base pool. This replaced the prior "Darkness always-on + all absorbed pools castable at once" model. See `docs/Architecture/BrokenDarkness.md`.
4. **Two-axis element model.** `None` = "no spell **element**"; physical-ness lives on the separate `EPhysicalDamageType` axis. A non-elemental physical hit is `Element = None` + `PhysicalDamageType = Slash/Pierce/Impact`. `Generic` now means **"inherit the source element at cast"**, never "non-elemental".

## Build clusters (as shipped)

1. Append `ESpellElement::None` (value 11, inert).
2. **Sentinel migration** (~40 sites across 2a–2f-5): every `Generic`-as-"no element" default/return → `None`, across crystal/equipment, ring/attached-item, DamageCalculator, ActionExecutor (resolver arms + remaining), status/effects (+ colour parity), VFX/projectile/weather, struct defaults, AI/debug, and the final missed-site sweep.
3a. `UActionExecutor::ResolveSpellCastElement` — the Generic→source/pool resolver (innate/ring/weapon/evolution/Reality via the existing chain; BD pool via a dedicated walk; unresolvable → None + log).
3b / 3b-2a / 3b-2b. BD **Model-B**: `CanAbsorbElement` allowlist (+None reject); `GetActivePool()` + `SeedBaseElement()` (Darkness seeded on transform); the three castable consumers (`IsElementCastable`, `GetAvailableSpells`, `FCombatCapabilities`) routed through `GetActivePool()`.
3c-1 / 3c-2. **Cast-boundary wiring**: resolve once in `ExecuteSpellAsync`, feed AttackElement + forbidden-cast + defense-window + the deferred VFX (muzzle / delivery colours, via a `PendingResolvedElement` cache) + the projectile tint (incl. burst continuation). Still-`Generic` result → `None` + log safety net. Zero raw `Spell->Element` reads remain in the cast damage/colour/resistance pipeline.
3d / 3d-2. **Gates**: `ElementHelpers::SpellElementMatchesHost` (Generic wildcard) across the 4 host-element gates; `IsElementCastable` Generic-always-castable short-circuit; Generic allowed in BD-pool authoring gates.
3e. **Naming**: `USpellData::GetDisplayName(ResolvedElement, bIsBrokenDarkness)` → "[Element] [Name]" / BD "Dark [Element] [Name]" (Darkness no-double) / "Reality [Name]"; wired into the combat spell menu label + tint.
3f-1. `UGenericSpellResolveDebug` — resolve-chain + BD pool-rotation readout.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-21 | Arc built + PIE-verified across clusters 1–3f-1 (above). Status → BUILT; moved to `Completed/`. Design evolutions captured: Reality uses Generic ("Reality Ball"), Generic/None brown everywhere, BD Model-B single-active-pool rotation, two-axis None/physical model. | feature/generic-spell-inherit |

## Deferred follow-ups (recorded, not built this arc)

- ~~**BD-value deletion (Phase 2).**~~ ✅ **DONE** (`feature/bd-value-deletion`, PIE-verified): `ESpellElement::BrokenDarkness` deleted, `InitializeBDPools` loop bound moved to the `None` sentinel, dead PostLoad migration removed, BD asset re-saved (`None` is now value 10). See `docs/Architecture/BrokenDarkness.md` Known Gaps.
- **Debug-sweep.** `ItemDataDebug.cpp:79` `== ESpellElement::Generic` is now dead (the getter returns `None`) → should become `== None`.
- **Blueprint-naming check.** Confirm no `.uasset` spell-name widget shows raw `Spell->Name` (the C++ menu is wired to the resolved name; a Blueprint surface would need the resolved-name path — a `BlueprintPure` convenience if found).
- **`ActionExecutor.cpp:4435` `!= InnateElement`** transitive-Generic edge — confirmed benign in 3c (a Generic-innate Caster + sourceless infusion now early-returns; arguably a correctness improvement), recorded for awareness.
