# Status Effects (Bar → Proc)

**Status:** Live. Player-facing reference for the status **bar**, what fills it, and what each proc does. Deep specs: [`../Architecture/StatusBuildupSystem.md`](../../Architecture/StatusBuildupSystem.md) (the bar/trigger engine), [`../Architecture/ResistanceSystem.md`](../../Architecture/ResistanceSystem.md) (resistance). This page is the *catalogue*; the effect-authoring/payload model lives in [`SkillEffects.md`](./SkillEffects.md).

## The bar

Every combatant has a **status bar** that fills as it's hit. [Live]

- Each hit adds **buildup**, keyed to the attack's **element** (Fire/Lightning/…) or **physical type** (Slash/Pierce/Impact). `UStatusBuildupManager::AddStatusBuildup`.
- When the bar **caps**, it **procs** the matching effect for that element/type, then resets. The trigger is resolved from (element, physical type) by `BarCapTriggerResolver`.
- **Resistance slows the fill** (it does not block the proc): Spirit + gear + class-innate profile, summed in `UStatusBuildupManager::GetTotalStatusResistance` (the 7-source aggregate). See [`Elements.md`](../Magic/Elements.md) for the per-element angle. [Live]
- Effects can **clear** the bar by a flat amount — gauge reduction (`StatusDecrease` → `ReduceStatusBuildupByAmount`). [Live]

> Element/physical-type only changes **which** proc fires and how fast the bar fills — it does **not** change raw damage (that's a [Stub]; see [`Elements.md`](../Magic/Elements.md)).

## Proc families (`ESkillEffectType`)

Element/type supplies the display name (e.g. `DOT` + `Fire` = "Burn", `DOT` + `Lightning` = "Shocked").

**Damage-over-time** [Live]
- **DOT** — damage each turn for several turns (Burn, Shocked, Chill, Bleed, … by element/type).

**Bar-cap debuffs** [Live]
- **Stun** — target can only **Attack or Defend** next turn (no Ability/Spell/Item). `Stun`.
- **Silenced** — target **can't spend EP** (no spells, abilities, or infusion charge). `Silenced`.
- **Heal Block** — all healing on the target **returns 0** while active. `HealBlock`.
- **Armor-break / Slow / Energy-drain / Crit-weak / Random** — stat debuffs (`DefenseDebuff`, `SpeedDebuff`, `EnergyDebuff`, `CritDebuff`, `RandomDebuff`) plus the substat variants (`SpellSpeedDebuff`, `ActionSpeedDebuff`, `TurnSpeedDebuff`, …).

**Buffs** [Live]
- Pillar and substat boosts applied to allies (`MindBuff`/`BodyBuff`/`SpiritBuff`, `DefenseBuff`, `SpellDamageBuff`, `StatusMultiplierBuff`, …) — see [`SkillEffects.md`](./SkillEffects.md).

**Immunities** [Live]
- `GrantDOTImmunity`, `GrantStunImmunity`, … — block a proc family for a duration.

**Gauge manipulators** [Live]
- `StatusIncrease` / `StatusDecrease` — push or clear a target's bar directly (the cleanse/pressure levers).

## What the player decides

- **Who to pressure** — concentrate hits of one element/type on one target to cap its bar before it heals/cleanses.
- **When to cleanse / clear** — use gauge reduction or debuff-removal (`CleanseSelf`/`CleanseAllies`, `RemoveSpeedDebuff`, …) before a dangerous proc lands.
- **Play around your own debuffs** — **Stunned** → only Attack/Defend; **Silenced** → no EP actions (Attack/Defend/Item only); **Heal-blocked** → healing does nothing until it expires.
- **Stack resistance** — high-Spirit / resistance gear / a favourable class-innate matchup makes enemies fill your bar slower.

## Status-affecting crystals & stones

Items that interact with the status system. Full item entries (tiers, stats) live in [`Items/Crystals.md`](../Items/Crystals.md) and [`Items/AugmentStones.md`](../Items/AugmentStones.md) — this table is only the *status action*.

| Item | Element / kind | What it does to status |
|---|---|---|
| **Onyx** | Darkness crystal | applies **Silenced** |
| **Quartz** | None crystal | **clears** the status bar (gauge reduction) |
| **Iolite** | Reality crystal | **Cleanse** — removes debuffs |
| **Sapphire** | Water crystal | **Last Stand** ward (defy-death) — see [`Items/Crystals.md`](../Items/Crystals.md) |
| **Status Stone** | attach | raises your **StatusMultiplier** → you build enemy bars faster |
| **Resistance Stone** | attach | raises your **status resistance** → enemies build your bar slower |
| **Elemental crystals** (Garnet, …) | per element | their spells build **that element's** status on hit (Garnet → Burn, etc.) |

Owning code: `ECrystalType` (`CrystalType.h`), `CrystalEffectTable`, applied via `USkillEffectManager`.

## Entry points

- `UStatusBuildupManager` — `AddStatusBuildup`, `GetTotalStatusResistance`, `ReduceStatusBuildupByAmount`; `OnStatusBuildupChanged` (UI fill/tint).
- `BarCapTriggerResolver` — (element, physical type) → proc type.
- `ESkillEffectType.h` — the full proc/buff/immunity/gauge enum.

## Related

- [`SkillEffects.md`](./SkillEffects.md) — how effects are authored/applied (the payload layer).
- [`Elements.md`](../Magic/Elements.md) — element → proc name, and the status-vs-damage distinction.
- [`../Architecture/StatusBuildupSystem.md`](../../Architecture/StatusBuildupSystem.md), [`../Architecture/ResistanceSystem.md`](../../Architecture/ResistanceSystem.md) — deep specs.
