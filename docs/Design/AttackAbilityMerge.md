# Attack / Ability Merge (Parked)

**Status:** PARKED / future thread. Not scheduled. Needs its own survey to size it, then a decision. Belongs
*after* the hybrid-attacks work. A design note, not a spec — just enough context for a future session.

## The idea

Merge `UWeaponAttackData` (basic attacks) and `UAbilityData` (abilities) into **one type**. They're
mechanically near-identical:

- both **physical**,
- both use **`DamageSplit`** (per-hit damage),
- both scale **`RawDamage`**,
- both should derive **status from the weapon's damage type**.

The distinction is **categorical** (basic attacks vs. special abilities), not mechanical.

## Why

- **Less duplication** — one data type + one execution path instead of two parallel ones (`ExecuteAttackAsync`
  / `ExecuteAbilityAsync`, two `OpenDefenseWindowsForTargets` callers, two dispatch branches, etc.).
- The **weapon-damage-type → status** linkage gets built **once**, not per-type.
- Consistent handling across dispatch, defense, AI, and authoring.

## Cost — why it's a separate foundational effort

`UWeaponAttackData` and `UAbilityData` are **separate types used across dispatch, the lumped path, AI, and
authoring** — merging touches a lot. They also have **some real differences** to reconcile (the realtime-
defense survey noted abilities route differently — `ExpectedImpacts=0`, lumped-only apply via
`OnDefenseWindowClosed`, vs. the per-impact attack path). This is a **cross-cutting refactor** (>3 files,
foundational) — it needs its **own survey + decision**, NOT folding into feature work.

## Trigger

Surfaced during the hybrid-attacks **status** work — realizing **attacks lack the weapon-damage-type → status
linkage that abilities have**. The targeted fix (attacks derive status from the weapon's damage type) was done
instead; the **merge is the bigger structural answer**, parked.

## Status

PARKED — survey to size it, then decide. After hybrid attacks.
