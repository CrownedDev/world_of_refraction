# Generic Spells

**Status:** Live. Player-facing reference.

## Concept

A **Generic spell** is a basic, classless spell that anyone can use — a template like
**"Ball"** or **"Bolt"** with no fixed element of its own. It **takes the element of wherever
you cast it from**. The same "Ball" becomes a Fire Ball, a Wind Ball, or a Dark Wind Ball
depending on its source — one spell, many flavours.

Contrast with a **fixed-element spell** (e.g. an authored "Fireball"): that always casts as its
written element and ignores the source. Generic spells are the flexible, fill-any-slot option.

## What element does a Generic spell become?

It inherits the element of the **source it is slotted into**:

| Source it's slotted in | Becomes | Shown as |
|---|---|---|
| Your innate element (Caster) | that element | e.g. **Fire Ball** |
| A ring crystal | the crystal's element | **Wind Ball** |
| A weapon crystal | the crystal's element | **Water Ball** |
| An evolution | the evolution's element | **Earth Ball** |
| **Reality** | Reality | **Reality Ball** |
| A Broken Darkness pool | that pool's element (darkened) | **Dark Fire Ball** |

If a Generic spell ever ends up with no element to inherit (an empty/element-less source), it
falls back to non-elemental and just shows its plain name ("Ball").

## How it's named

The element is shown as a prefix on the spell's name — **"[Element] [Name]"**:

- **Fire** source + "Ball" → **"Fire Ball"**
- **Wind** source + "Bolt" → **"Wind Bolt"**
- **Reality** → **"Reality Ball"**
- **Broken Darkness**, absorbing **Wind** → **"Dark Wind Ball"** (BD darkens the element)
- **Broken Darkness** on its base **Darkness** → **"Darkness Ball"** (never "Dark Darkness")

The spell's colour follows the same resolved element (a Fire Ball glows red, a Wind Ball green,
a Dark Wind Ball is the darkened Broken-Darkness tint). A non-elemental / physical hit shows a
neutral colour.

## Broken Darkness: one active pool at a time

A Broken Darkness caster doesn't have every element available at once. They cast from **one
active pool at a time**:

- On transforming, they start on their **Darkness** (base) pool.
- **Absorbing** an element (by parrying/blocking that element) **switches** the active pool to
  it — absorb Wind, and you now cast from the Wind pool ("Dark Wind ..."), while the previous
  pool goes dormant.
- **Absorbing Darkness** switches back to the base Darkness pool.

So a Generic spell in a Broken Darkness loadout becomes whatever the **currently active pool**
is — and only the spells of that one pool are castable until you absorb something else and
rotate again.

## Why use Generic spells?

- **Flexibility** — one Generic spell fits any element slot you have; you don't need a separate
  spell authored per element.
- **They scale with your source** — slot a Generic "Ball" into a stronger or different-element
  source and it adopts that element automatically.

## See also

- `docs/Design/Completed/GenericSpellInherit.md` — the full design + as-built record.
- `docs/Architecture/InfusionSystem.md` — *Generic Spell Resolution* (the resolver).
- `docs/Architecture/BrokenDarkness.md` — the single-active-pool model.
