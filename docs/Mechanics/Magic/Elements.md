# Elements

**Status:** Live (with one [Stub] called out below). Player-facing roster of elements and what an element actually changes. Owning enum: `ESpellElement.h`.

## The roster (9)

`Fire · Water · Earth · Wind · Light · Darkness · Lightning · Void · Reality`

Plus two **non-roster tokens** in the enum, not pickable elements:
- **Generic** — "inherit" sentinel; a Generic spell adopts its source's element (see [`GenericSpells.md`](./GenericSpells.md)).
- **None** — "no element" (non-elemental actions).

## What an element changes

| Channel | Effect | Status |
|---|---|---|
| **Innate identity** | A Refractor picks one element at creation; it shapes their spells and class-innate resistance row. | [Live] |
| **Status proc naming** | Element + proc type = the named effect (`DOT` + `Fire` = "Burn", `DOT` + `Lightning` = "Shocked"). | [Live] |
| **Status buildup & resistance** | Incoming buildup is keyed by element; resistance (Spirit + gear + class-innate) slows it per element/physical type. | [Live] |
| **Weather colour** | The team leader's element tints the battlefield weather. | [Built · No UI] (BP consumer unverified — see [`../Architecture/WeatherSystem.md`](../../Architecture/WeatherSystem.md)) |
| **Broken Darkness pool** | BD holds one active element at a time and rotates it on absorb. | [Live] — see [`Archetypes/BrokenDarkness.md`](../Archetypes/BrokenDarkness.md) |

## The damage caveat (read this)

**Element does NOT change raw damage.** The element-interaction damage multiplier is **not implemented** — `ElementMultiplier` is hardcoded `1.0` (`DamageCalculator.cpp:208`). [Stub]

What element **does** affect is **status buildup / resistance** — that interaction **is live** (`UStatusBuildupManager::GetTotalStatusResistance` + `ClassInnateResistanceTable`). [Live]

So "Fire vs Water" makes no raw-damage difference today; it only changes how fast the **status bar** fills and what proc lands. See [`StatusEffects.md`](../Status/StatusEffects.md).

## Reality (the odd one)

`Reality` is a roster element but behaves specially: an any-element caster, a hard counter to Broken Darkness, and a flat stat boost. See [`Archetypes/Reality.md`](../Archetypes/Reality.md).

## Entry points

- `ESpellElement.h` — the enum (9 elements + Generic + Reality + None).
- `ClassInnateResistanceTable` — per-class/element status-resistance rows.
- `DamageCalculator.cpp:208` — `Result.ElementMultiplier = 1.0f;` (the damage stub).

## Related

- [`GenericSpells.md`](./GenericSpells.md) — Generic-element inheritance.
- [`StatusEffects.md`](../Status/StatusEffects.md) — element → proc names and the bar.
- [`../Architecture/ResistanceSystem.md`](../../Architecture/ResistanceSystem.md) — resistance deep spec.
