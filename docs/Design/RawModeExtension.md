# Raw Mode — Extension to Physical Attacks (Parked)

**Status:** PARKED / future thread. Not scheduled. Captured so it's not lost. Belongs *after* the current
reactive-defense / Stage-6 per-cast-entry spell-damage work.

A design note, not a spec — just enough context for a future session to pick this up.

## Today: raw mode is a spell/ability flag

`bIsRawMode` (on `USkillDataBase`, used by `USpellData`/abilities) is an authoring flag that **folds the
ability/spell's STATUS BUILDUP into damage** — trade the status effect for more immediate damage — in
exchange for an **increased energy cost**. Applied via `RAW_MODE_DAMAGE_MULTIPLIER` in
`USpellData::CalculateDamage` (status buildup is suppressed in raw mode; the multiplier compensates as
damage).

> **Naming trap:** this `bIsRawMode` is UNRELATED to `ESubStat::RawDamage` — the physical, Body-driven
> damage stat selected in `UDamageCalculator::CalculateDamage` when `ActionType != Spell`. Different
> concepts, same word "raw". Don't conflate them.

## Proposed extension: raw mode on physical attacks

Extend raw mode to **attacks**, as a symmetric tradeoff across all three action types:

- **Spell/Ability raw mode (today):** fold ELEMENTAL/magical status buildup → damage, RAISE the existing
  energy cost.
- **Attack raw mode (proposed):** fold the PHYSICAL status buildup (Slash / Pierce / Impact) → damage, and
  ADD an energy cost. Attacks are normally free/cheap, so raw mode introduces an **energy decision to basic
  attacks**.

## Why it's coherent

Physical attacks build Slash/Pierce/Impact status, so raw mode has real status to fold on an attack — just
a different status TYPE than spells' elemental buildup. The tradeoff (sacrifice status pressure + pay energy
for higher immediate damage) works uniformly across all action types, and gives attacks a risk/reward lever
(spend energy to make a basic hit harder) — build/playstyle depth.

## Open implementation questions (for when it's built)

1. **Is raw mode's status-folding STATUS-TYPE-AGNOSTIC or SPELL-STATUS-SPECIFIC?**
   - Agnostic ("fold whatever status this action builds" — elemental for spells, Slash/Pierce/Impact for
     attacks) → extends trivially.
   - Spell-status-specific (reads elemental buildup specifically) → needs generalizing to also read physical
     buildup for attacks.
   - A survey for that day.
2. **The added-energy-cost-on-attacks mechanism.** Attacks normally free → raw mode adds a cost; needs wiring
   on the attack energy path.

## Changelog
- 2026-06-16 (feature/realtime-defense): Parked the idea. Captured while mapping the raw/spell damage
  scaling paths during the Stage-6 per-cast-entry spell-damage survey.
