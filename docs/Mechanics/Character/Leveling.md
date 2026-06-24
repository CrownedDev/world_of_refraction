# Leveling (character progression)

**Status:** [Design substrate · growth unbuilt] — the per-pillar **world stat levels** exist as data
and **drive stats today**, but the **growth loop that raises them** (XP → level → points) is **not
built**. Code substrate: `FCharacterData` (`Character/CharacterData.h`), `StatConstants.h`,
`CombatConstants.h`, `WorldStatRequirements`.

> **Leveling = CHARACTER progression** (this doc) — your pillars (Mind / Body / Spirit) getting
> stronger. Distinct from **ITEM tier progression** — for upgrading a weapon/spell's tier see
> [`Upgrading.md`](../Economy/Upgrading.md). The two were once both called "leveling"; the item system is now
> **upgrading**, and this doc keeps the **character** meaning.

## What exists (the substrate)

Each character carries three **world stat levels**, one per pillar, on `FCharacterData`:

- `WorldMindLevel`, `WorldBodyLevel`, `WorldSpiritLevel` — `int32`, **default 0**, range **0 → 7**
  (`MIN_WORLD_STAT_LEVEL` … `MAX_WORLD_STAT_LEVEL`).

These are **live inputs**, not cosmetic — they already feed combat:

- **Pillar scaling (+7% / level)** — `GetMindScaling`/`BodyScaling`/`SpiritScaling` return
  `1 + WorldXxxLevel × 0.07` (`WORLD_MIND/BODY/SPIRIT_SCALING_BONUS = 0.07`). At the **L7 cap** that's
  **+49%** to the pillar (`WORLD_MAX_MULT = 1.49`).
- **Stat-point budget** — each level grants **3 points** (`POINTS_PER_WORLD_STAT_LEVEL = 3`), so a
  maxed pillar adds 21 points (`MAX_SUBSTAT_POINTS_PER_PILLAR = 7 × 3`). The intended ceiling is
  **93 total** (`MAX_STAT_POINTS = 30 base + 7 levels × 3 points × 3 pillars`).
- **Skill gating** — spells/abilities require minimum world levels (`WorldStatRequirements`:
  `RequiredWorldMind/Body/Spirit`); falling short triggers the **requirement-gap** penalty
  ([`Scaling/RequirementGap.md`](../Scaling/RequirementGap.md)).
- **Turn speed + weather** — world levels also feed turn-speed seeding (`TurnManager`) and per-team
  weather dominance (`WeatherStateManager`).

## ⚠️ What's NOT built (the growth loop)

**There is no way to *raise* these levels in play.** The "gain XP → level up → spend pillar points"
loop does **not exist**:

- **No XP / experience** — no accumulation field, no XP table, no award on enemy defeat.
- **No level-up event** — nothing increments `WorldXxxLevel` at runtime as a reward.
- **No point allocation / spend UI** — no respec, no distribute-points screen.
- **Only the test harness writes them** — `TurnManagerTestActor` sets the fields directly for
  combat tests; in normal play they stay at their authored values.

So today pillar strength comes from **authored base values + (statically-set) world levels +
gear/rolls** — the *character-leveling system itself is a design placeholder.* When it lands it will
likely tie into the **World Stat Points** concept (run power on the character component — see
[`../../Architecture/CurrencySystem.md`](../../Architecture/CurrencySystem.md) "World Stat Points are not
here") and the Pool/persistence arc.

## How to test (substrate only)
- In a test actor, set `WorldMindLevel = 7` → confirm the Mind pillar scales ×1.49 and Mind-gated
  skills stop incurring the requirement-gap penalty.
- There is **no in-game action** that changes a world level — that's the unbuilt part.

## Known Limitations / TODOs
- **⚠️ Growth loop unbuilt** — XP, level-up, and point allocation do not exist (design-only).
- **Run-scoped + no save** — even once growth lands, persistence awaits the Pool/save arc.
- **No UI** — no character sheet / level-up screen.

## Related
- [`Upgrading.md`](../Economy/Upgrading.md) — **item** tier progression (the other "leveling"), a different system.
- [`../../Architecture/CharacterDataSystem.md`](../../Architecture/CharacterDataSystem.md) — `FCharacterData` + pillar/stat derivation.
- [`Scaling/RequirementGap.md`](../Scaling/RequirementGap.md) — the world-level skill-requirement penalty.
- [`../../Architecture/CurrencySystem.md`](../../Architecture/CurrencySystem.md) — the World-Stat-Points note (the future home of growth).
