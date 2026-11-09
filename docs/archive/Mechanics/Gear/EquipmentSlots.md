# Equipment Slots — How Many Skills Gear Holds

**Status:** Live and working as designed. Player-facing rules view — for the implementation see `docs/Architecture/LoadoutSystem.md` and `docs/Architecture/AugmentStoneSystem.md`.

## Concept

Better gear holds more skills. Every piece of equipment that grants abilities or spells has a number of slots set by **its own tier** — a higher-tier weapon, ring, or evolution simply has more room. ("Did this piece of gear have the quality to carry that many skills?")

## Slots by tier

| Tier | F | E | D | C | B | A | S |
| ---- | - | - | - | - | - | - | - |
| Slots | 1 | 2 | 3 | 4 | 5 | 6 | 6 |

Slots climb one per tier from F to A, then **A and S both hold 6**. S-tier doesn't grant *more* slots than A — its edge is power and quality, not slot quantity.

## What it applies to

- **Weapon abilities** — keyed on the weapon's tier.
- **Ring spells** — keyed on the ring's tier.
- **Evolution spells** — keyed on the evolution's tier.
- **Crystal-provided spell slots** — when a spell-granting crystal (a gem, or a fusion's gem half) is socketed into a weapon or ring, the slot count comes from **the crystal's** tier instead. The crystal effectively brings its own room; with no such crystal, the slot count falls back to the container's own tier.
- **Augment-stone ability slots** — the extra ability slots an augment stone grants are set by the **stone's** tier (same curve).

So an F-weapon holds 1 ability; an A- or S-weapon holds 6. A B-tier ring holds 5 spells — unless you socket an S-tier spell crystal into it, in which case the crystal's tier (6) sets the count.

## Locked skills

Some gear comes with built-in skills you can't remove (conjured weapons' fixed abilities, locked rings' fixed spells). Those occupy slots from the same tier-based total — your **customizable** room is the gear's slot count minus whatever is locked in.

## Examples

- **F-tier starter weapon:** 1 ability slot. Room to grow as you upgrade.
- **B-tier ring:** 5 spell slots. Socket an A/S spell crystal → 6 (the crystal's tier wins).
- **A-tier vs S-tier weapon:** both hold 6 abilities — pick S for the stronger gear, not for extra slots.
- **C-tier evolution:** 4 evolution-spell slots.
