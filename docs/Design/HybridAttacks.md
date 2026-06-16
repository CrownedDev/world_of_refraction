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

## The refined model

Refines the two-layer vision into a **per-component** model with an authored **scaling toggle** — scaling
decoupled from delivery and visuals.

### 1. Scaling toggle (the key new piece)

Each damage component carries an authored **scale-with-Raw / scale-with-Spell** toggle that **overrides** the
automatic `ActionType` selector (today: `ActionType == Spell ? SpellDamage : RawDamage`). Scaling is thus
decoupled from delivery/visual.

**Why — the warrior-finisher problem:** a physical-combo-into-spell-finisher shouldn't force the finisher to
scale with Spell — a warrior (Raw/Body build) would get a weak finisher off a stat they don't invest in. The
toggle lets the spell finisher scale **Raw** (warrior-friendly) while a mage's version toggles it to
**Spell**. Same attack, class-flexible.

- *"Force slash"* = magic-looking VFX + cast delivery, toggled to scale **Raw** — a warrior's energy slash.

**Sketch:** the scaling selector becomes `useSpellScaling ? SpellDamage : RawDamage`, where `useSpellScaling`
defaults to *match `ActionType`* (existing behavior holds unchanged).

### 2. Two-component hybrid (kept — combines with the toggle)

An attack can carry BOTH **physical components** (`DamageSplit` hits, physical status Slash/Pierce/Impact)
AND **spell components** (cast entries, per-entry damage from cluster 5, element status). Each component's
scaling is **independently** toggleable (#1).

A warrior combo = physical hits (Raw) + a spell finisher (cast entry, toggled Raw) — a **distinct** spell
finisher that scales with the warrior's power. Both pieces are needed: the toggle alone can't add a distinct
extra spell component; the two-component model alone would force odd scaling without the toggle.

### 3. Alignment — spell components anchored to melee notifies

Two patterns, both "spell delivery anchored to a melee notify," differing only in *which* notify:

- **Simultaneous (orb-on-fist):** the spell component fires **at** a physical hit's Impact notify — same
  moment, no travel. E.g. a fire-orb punch: physical punch damage + the orb's spell damage + fire element
  status, all at the fist-connect notify. The orb is a cast entry whose delivery is **"at-melee-impact"**
  (Instant-at-notify, reusing cluster-5 per-entry damage), triggered by the melee notify alongside the
  physical hit. Alignment is automatic (same notify = same moment).
- **Sequential (spell finisher):** the spell component fires **after** the physical combo — at the finisher
  notify (its own later impact moment). E.g. physical combo → spell-blast finisher.

### 4. Defense — one defense per impact moment (notify)

Each impact moment = one timed defense.

- A **simultaneous** orb-punch is **ONE** defense covering BOTH components (one fist-connect → one defense
  mitigates physical + spell together).
- A **sequential** finisher is its own defense moment (defend the combo hits, then defend the finisher).

Status applies **per component** if undefended (physical status from physical components, element status from
spell components).

### 5. Interaction with per-cast-entry damage (cluster 5)

The spell components **are** cast entries — they reuse the `FSkillCastEntry.Damage` field + spell scaling
already built. The hybrid adds only: (a) the scaling toggle (#1); (b) anchoring a cast entry's delivery to a
melee notify (#3 — a new "at-melee-impact" delivery, or anchoring `Instant` to a notify); (c)
one-defense-covers-simultaneous-components (#4).

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

**Open authoring question (build day):** does a spell component on a combo fire on **every** hit or a
**specific** hit (the orb on every punch, or only the finisher)? — resolve by authoring, per cast entry,
which melee notify it anchors to (#3).

## Known limitations of the parked hybrid path

- **Multi-entry double-apply (the lumped-total characteristic).** A multi-cast-entry spell
  (`CastArray.Num() > 1`) — including a hybrid physical+spell skill — still opens the action-start lumped
  window and applies via `ApplyDamageAfterDefense` against the **lumped total**, because the per-impact
  conversion gates are single-entry only (`CastArray.Num() == 1`). This affects ALL unconverted multi-entry
  deliveries uniformly: a multi-entry spell with a Projectile entry (the orphan else-branch), an AOE entry,
  or an Instant entry all share it — the individual entry resolves/applies AND the lumped window applies the
  lumped total = potential double-apply. This is **not** a bug in the single-entry converted path
  (Projectile single/burst, AOE, and Instant are all clean) — it's the unconverted multi-entry path, the
  same bucket the orphan else-branch is kept for.

- **Implication for hybrids.** When the hybrid attacks feature is built (multi-entry physical+spell skills),
  the multi-entry defense/apply path needs designing — each entry should resolve **per-impact** (like the
  single-entry deliveries do now) instead of falling to the lumped window. The conversion gates would extend
  from single-entry to multi-entry, with per-entry resolve. Until then, multi-entry spells use the lumped
  path (one window, one decision, lumped-total apply) — fine for non-hybrid multi-entry, but the per-entry
  defense/damage hybrids want isn't there yet.

- **Status.** A known characteristic of the parked multi-entry/hybrid path, to address when hybrids are
  built.

## Status

PARKED, to incorporate. Captured so the vision is intact.
