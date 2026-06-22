# Luck

**Status:** [Live]. The stat behind chance-based outcomes. Owning code: `Luck` substat (Spirit), `UDamageCalculator` crit functions, `BASE_CRIT_CHANCE`.

## Crit chance — Luck-sourced

Your **critical-hit chance** ramps with Luck:

- **5% base → 50% (maxed Luck stat alone) → 100% (gear ceiling).** `UDamageCalculator::GetCriticalChance` via `GetLuckModifiedChance(BASE_CRIT_CHANCE, …)`.
- `BASE_CRIT_CHANCE = 0.05`; Luck is a **Spirit** substat (see [Stats](../Character/Stats.md)).

## Crit damage — variable multiplier

A crit multiplies damage by your **crit-damage multiplier** (`UDamageCalculator::GetCritDamageMultiplier`) — *not* a fixed ×1.5: a low-CritDamage attacker crits near ×1.0 (little uplift), a maxed one toward ×2.0. CritDamage is a **Mind** substat; Luck decides *whether* you crit, CritDamage decides *how hard*.

> AI note: estimates fold in **expected** crit (chance × multiplier), so AI predictions match average crit output.

## Wear-skip

Luck can **skip a per-cast durability wear event** before it applies — a lucky cast costs no durability that turn. See [Durability & wear](../Gear/DurabilityWear.md).

## Gambling

The **Amethyst** crystal (Void) is the "gambling" catalyst — Luck-driven variance effects. See [Items: Crystals](../Items/Crystals.md).

## Player levers

- Raise **Luck** (Spirit / gear / LuckStone) → crit more often, skip wear more often.
- Raise **CritDamage** (Mind) → bigger crits when they land.

## Entry points

- `Luck` substat (Spirit) — `CharacterData`.
- `UDamageCalculator::GetCriticalChance` / `GetCritDamageMultiplier`; `GetLuckModifiedChance`; `BASE_CRIT_CHANCE`.

## Related

- [Stats](../Character/Stats.md) (Luck = Spirit, CritDamage = Mind) · [Durability & wear](../Gear/DurabilityWear.md) (wear-skip) · [Items: Crystals](../Items/Crystals.md) (Amethyst)
