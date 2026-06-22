# Spell Schools

**Status:** [Live]. Owning code: `ESpellSchool`, `USpellData::School`.

## What a school is

Every spell belongs to a **school** that signals its role:

- **Destruction** — offense (damage).
- **Enhancement** — buffs (boost allies' stats).
- **Restoration** — healing.
- (+ further schools per the `ESpellSchool` enum.)

A spell's school is authored on the asset (`USpellData::School`, default `Destruction`).

## Why it matters to the player

- **Role at a glance** — pick the school for what you need (kill / buff / heal).
- **Pooling** — innate casters pool spells **per school** (count + weight budget), so school spread shapes your loadout. See [Spell pool budget](./SpellPoolBudget.md).

## Entry points

- `ESpellSchool` — the enum.
- `USpellData::School` (`SpellData.h:51`).

## Related

- [Spells](./Spells.md) · [Spell pool budget](./SpellPoolBudget.md)
