# Hybrid Attacks + Interrupt Notify (Parked)

**Status:** PARKED / future thread. Not scheduled. Captured so it's not lost. Belongs *after* Stage 6
completes (spell per-impact defense + AOE land + lumped-path retire) **and** the beam/homing delivery-type
removals.

A design note, not a spec — just enough context for a future session to pick this up.

## The vision

A **hybrid attack** is one attack carrying **both damage layers** at once:

- **Physical layer** — `HitCount` / `DamageSplit`, `RawDamage`-stat scaling. Resolves per-impact through
  the melee Impact notifies.
- **Spell layer** — per-cast-entry damage, `SpellDamage`-stat scaling. Resolves per-impact through the
  projectile-arrival path.

Each layer resolves through its **own** per-impact path, and **both defense paths fire on one attack** —
the defender times defenses against the physical hits AND the spell deliveries. The damage pipeline already
supports this: `FActionHitInput.ActionType` selects Raw-vs-Spell scaling per hit, so a single attack can
emit hits of both kinds. The architecture allows it today; the missing piece is authoring + the multi-stage
defense outcome below.

## Examples (Crown's)

1. **Ignited melee combo (the "layered" hybrid).** A physical combo (physical hits, physical mechanics)
   with the fist ignited — spell VFX/element layered on top. Physical damage, magical presentation; maybe
   an elemental status rider. Both layers, same beat.

2. **The grab (sequential / conditional hybrid).** A grab attempt where the grab-**connect** point is
   **parryable**:
   - Parried → the grab is **interrupted** (escaped); the rest does not fire.
   - Lands (not parried) → a spell VFX/effect fires (the grab succeeds → magical payload).

   A multi-stage attack: physical grab → **[defense check]** → spell effect *only if it connected*.

## The interrupt notify (new mechanic)

A notify marking a defense point where a **successful** defense (parry) **aborts the rest of the attack** —
distinct from normal defense, which *reduces damage but the attack continues*.

This is a **new defense outcome**: "defend successfully → attack interrupted/aborted", beyond today's
"defend → damage mitigated". The grab needs it: parry the grab → the whole sequence STOPS, the spell payload
never fires.

**Implementation sketch (for build day):** an interrupt notify + a successful-defense check at that point
that, if the defender parried, **cancels the remaining montage / cast steps** — aborts the action cleanly,
like the existing action-cancel path but triggered by a *successful defense* rather than by the attacker.

## Why it's rich

Hybrids + interrupt give attacks **multi-stage structure** and **meaningful defense decisions** (parry the
grab to escape; eat it to take the spell). Adds depth and "things to play around with" (Crown's framing). It
composes with the per-impact defense + telegraph + difficulty systems already built — no parallel system,
just new structure on top of them.

## Dependencies / sequencing

Build **after** Stage 6 completes (spell per-impact defense + AOE + lumped-path retire) and **after** the
beam/homing delivery-type removals. Needs, in order:

1. **Hybrid authoring** — an attack carrying both a `DamageSplit` AND a `CastArray`. Confirm the type model
   permits both on one attack (flagged in the earlier hybrid survey).
2. **Interrupt notify + successful-defense-aborts-action** mechanic (the new defense outcome above).
3. **Conditional spell-fires-if-grab-connects** logic (the grab's payload gated on the connect not being
   parried).

## Status

PARKED, to incorporate. Captured so the vision is intact.
