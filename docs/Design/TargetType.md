# Target Type (Shipped — core)

**Status:** SHIPPED (core two-axis model). Branch `feature/target-type-count`, 2026-06-19.
The targeting model is live; three pieces remain deferred — the active Double pick UI, the
per-count AOE manifestation, and the Phase-2 legacy cleanup. See **Shipped** and **Deferred**
below.

The original parked vision is preserved (from "The idea" down) for rationale. The **Shipped**
section is the source of truth for current behaviour.

## Shipped (2026-06-19, `feature/target-type-count`)

The count axis was split out from targeting role into **two orthogonal enums**, replacing the
old 7-value conflated `ETargetType`:

- **`ETargetType` → `{Self, Ally, Enemy, Anyone}`** (`Combat/TargetType.h`) — the role only.
- **`ETargetCount` → `{Single, Double, All}`** (same header) — how many. `Double` = exactly 2.

**Fields:**
- `USkillDataBase`: `TargetType` + `TargetCount` (default `Enemy` / `Single`). Inherited by
  spells, abilities, attacks.
- `FSkillEffect`: per-effect `Target` + `TargetCount` (default `Enemy` / `Single`).

**Resolution:**
- `UActionExecutor::GetEffectTargets` rewritten to the two-axis model, with **passive Double
  auto-resolve**: `Enemy` → 2 random distinct enemies; `Ally` → self + 1 random teammate;
  `Anyone` → 2 random distinct combatants. `All` → the full role pool; `Single` → the action
  target (or self for `Ally`).
- Command menu (`UCombatCommandMenuSubsystem`) is fully TargetType + TargetCount aware.
  `ResolveTargets` returns the **role pool only**; the count is applied at selection time.
  Group confirm (auto, no pick) when `Self` or count `All`; per-actor pick otherwise;
  `Anyone` (non-All) still routes through the Allies/Enemies category step.

**Migration (skill-level auto, effect-level manual):**
- Skill-level: legacy 7-value data deserializes into a `LegacyTargetType` field (typed
  `ETargetType_Legacy`, the old 7 names preserved), routed there by `PropertyRedirects` on the
  leaf class names (`AbilityData`/`SpellData.TargetType`) in `DefaultEngine.ini`.
  `USkillDataBase::PostLoad` maps it to `TargetType` + `TargetCount` (idempotent via
  `bTargetAxisMigrated`). ⚠️ Assets **re-saved under `SkillDataBase.TargetType`** after the
  ability/spell/attack base-merge are not caught by the leaf-name redirects and need **manual
  re-authoring**. The legacy field is named `LegacyTargetType` (not `TargetType_DEPRECATED`)
  so UHT does not strip the suffix and collide its FName with the live `TargetType` field.
- Effect-level: **manual re-authoring** of each `FSkillEffect.Target` + `TargetCount` (struct
  member redirects can't auto-migrate without hijacking new saves).

## Deferred

- **Active Double pick UI** — authoring a `Double` skill should let the player pick exactly 2
  targets (sequential single-picks). Today the picker falls through to a single pick (inert —
  no Double skills authored yet); passive Double effects auto-resolve in `GetEffectTargets`.
  Implement when the first Double skill is authored.
- **Per-count AOE manifestation** — the "separate AOEs at low counts vs. one large AOE at
  all" behaviour (below) did **not** ship; the cast still loops per-target spawning separate
  deliveries/AOEs for every count.
- **Phase 2 cleanup** — delete `LegacyTargetType` + `ETargetType_Legacy` + the leaf-name
  redirects once the skill-asset re-save pass is complete.

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

Core two-axis model **SHIPPED** (2026-06-19). Deferred: active Double pick UI, per-count AOE
manifestation, Phase-2 legacy cleanup (see **Shipped** / **Deferred** above).

## Changelog

- **2026-06-19** (`feature/target-type-count`) — Shipped the core two-axis targeting model:
  split the 7-value `ETargetType` into `ETargetType {Self, Ally, Enemy, Anyone}` +
  `ETargetCount {Single, Double, All}`; added `TargetCount` to `USkillDataBase` and
  `FSkillEffect`; rewrote `GetEffectTargets` (with passive Double auto-resolve) and the
  command-menu targeting flow; added skill-level migration (`LegacyTargetType` +
  `DefaultEngine.ini` redirects, idempotent `PostLoad`). Effect-level + post-merge-saved
  skill assets re-authored by hand. Active Double UI and per-count AOE manifestation deferred.
