# Stats (Pillars & Substats)

**Status:** [Built · No UI] — stats are data/editor-authored; no in-game allocation screen. Owning code: `CharacterData` substat fields, `GetEffectiveMind/Body/Spirit`. Deep math: [`../Architecture/StatComposition.md`](../../Architecture/StatComposition.md).

## Pillars (Mind / Body / Spirit, 0–7)

Three world pillars, each leveled **0–7** (3 points per level, max 93 across all three). Pillar level applies a world multiplier (`1.0 + level × 0.07`, range 1.0–1.49) and **gates skill tiers** (you need the pillar level a skill requires — see [Requirement gap](../Scaling/RequirementGap.md)).

- **Mind** — magical power & finesse.
- **Body** — physical power & durability.
- **Spirit** — energy, speed, fortune, status.

## The 14 substats (which pillar owns each)

| Pillar | Substats |
|---|---|
| **Mind (4)** | Efficiency, SpellDamage, CritDamage, SpellSpeed |
| **Body (5)** | Defense, ActionSpeed, RawDamage, **MaxHealth** (pool), Reflex |
| **Spirit (5)** | **MaxEnergy** (pool), Resistance, TurnSpeed, Luck, **StatusMultiplier** |

(`CharacterData.h` `Sub-Stats|{Mind,Body,Spirit}` categories.) **StatusMultiplier is Spirit.** `Reflex` widens your real-time defense window (see [Defense](../Combat/DefenseResolution.md)).

> Action-time modifiers (Reality/evolution/requirement-gap via `FActionStatModifiers::AddPillarPercent`) operate on a **subset** — Mind 4 / Body 4 / Spirit 3 — because they drop the two **pool** stats (MaxHealth/MaxEnergy) and **TurnSpeed** (must not reorder turns). The pillar *ownership* above is the canonical one.

## Caps & ceilings

Every stat's **stat-derived part caps at +50% alone** (`UNIVERSAL_STAT_CAP = 0.5` for %-stats, `STAT_MULT_CAP = 1.5` for multiplier stats); **gear/buffs then multiply past it toward a +100% ceiling**. Pools (HP/EP) cap at a 1000 magnitude with gear **additive** outside the clamp. Full rules + the "live getter vs display twin" trap: [`../Architecture/StatComposition.md`](../../Architecture/StatComposition.md).

## Entry points

- `CharacterData` — `WorldMind/Body/SpiritLevel`, the 14 substat fields, `GetEffectiveMind/Body/Spirit`, `Calculate*`.
- `CombatConstants` — `UNIVERSAL_STAT_CAP`, `STAT_MULT_CAP`.

## Related

- [Requirement gap](../Scaling/RequirementGap.md) (pillar level vs skill req) · [Action stat modifiers](../Scaling/ActionStatModifiers.md) (the per-action subset) · [Tier power](../Scaling/TierPower.md) · [`../Architecture/StatComposition.md`](../../Architecture/StatComposition.md)
