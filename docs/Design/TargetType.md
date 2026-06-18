# Target Type (Parked)

**Status:** PARKED / future thread. Not scheduled. Captured so it's not lost. Build **after** the Instant
per-impact conversion + the Stage 6 delivery-defense wrap.

A design note, not a spec — just enough context for a future session to pick this up.

## The idea

A new **Target Type** axis on spells, **separate from delivery type** (Projectile / AOE / Instant). It
controls **how many** targets a spell hits and **how the cast manifests per count** — decoupling targeting
from delivery, so any delivery can be 1 / 2 / 3 / all.

Delivery answers *how it travels and is defended*; Target Type answers *how many it hits and how the cast
fans out*. The two are orthogonal.

## The counts

| Targets | Selection | Projectile / Instant | AOE |
|---|---|---|---|
| **1** | Player picks ONE target | One delivery at the picked target | One AOE at the picked location |
| **2** | Player picks the targets | **2 deliveries spawned simultaneously** (one per picked target) | **2 separate AOEs** (one per picked target) |
| **3 / "all"** | NO picking — hits EVERYONE | Multiple spawned simultaneously (one per target, all enemies) | **ONE large AOE** covering all of them (a single big area, NOT separate AOEs) |

**The pattern:** low counts (1–2) = the player **picks** specific targets; the highest (3 / all) = **no pick**,
it just hits everyone. Projectile/Instant are always "spawn N simultaneous deliveries, one per target"; AOE
is "separate AOEs" at low counts but **one large AOE** at the all/3 level.

## Why it fits the plumbing

The dispatch **already loops over a `Targets` list** — Instant / AOE / Projectile each resolve **per-target**,
so the plumbing is already N-target-capable ("single-target" was only design intent, never a code
constraint). Target Type is therefore the **authoring layer over existing N-capable plumbing**: it makes the
target **count** + the per-count manifestation (separate vs. one-large AOE) explicit and authored, instead of
implicit.

It also rides the per-impact defense for free: that path already handles N defenders as **N independent
windows**, so a multi-target spell defends **per-target** with no extra defense work once Target Type drives
the count.

## What it needs

- **(a) Data** — a `TargetType` field/enum on the spell (1 / 2 / 3 / all).
- **(b) Target selection UI** — let the player pick 1 or 2 targets; the "all" case **skips selection** and
  auto-targets everyone.
- **(c) Per-count spawn logic** — N simultaneous deliveries for Projectile/Instant; **separate-vs-large** AOE
  (separate AOEs at counts 1–2, one large AOE at all/3).
- **(d) Display UI** — show the target type / how many the spell hits.

The UI work (target picking + display) is a meaningful chunk — this is a **targeting + UI feature**, distinct
from the per-impact defense work.

## Dependencies / sequencing

Build **after** the Instant per-impact conversion + the Stage 6 delivery-defense work. Multi-target defense
is already free (N independent per-target windows) — Target Type just drives the target **count**; the
remaining work is the targeting layer (count + selection UI) and the per-count spawn manifestation.

## Status

PARKED, to build after Instant / Stage 6. Captured so the vision is intact.
