# Generic Spell Inheritance + `ESpellElement::None` Sentinel

**Status:** Design locked. Survey pending before build.

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
