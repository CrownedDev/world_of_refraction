# Spell Pools — Count & Budget

**Status:** Live and working as designed. Player-facing rules view — for the implementation see `docs/Architecture/LoadoutSystem.md` and `docs/Architecture/BrokenDarkness.md`.

## Concept

A Caster's spells aren't limited by a single flat count — they're bound by **two limits at once**: how *many* spells fit in a pool, and how much *weight* the pool can carry. Heavier (higher-tier) spells eat more of the budget, so you trade breadth (many small spells) against power (a few big ones). World-stat mastery then loosens the budget as your character grows.

## The two limits (both must pass)

1. **Count** — each pool holds at most **6 spells**. For an innate Caster a "pool" is a school; for a Broken Darkness character it's the Darkness pool and each absorbed element's pool. This is tier-blind — six is six whether they're F-tier or S-tier.
2. **Budget** — every spell costs **points equal to its tier**, and the total can't exceed your pool budget:

| Tier | F | E | D | C | B | A | S |
| ---- | - | - | - | - | - | - | - |
| Cost | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

- **Innate Caster:** 24 points.
- **Broken Darkness:** 48 points (double).

So at base an innate Caster fits three S-tier spells (3×7 = 21 ≤ 24), or many cheaper ones, or any mix that sums to 24 or less — as long as no single school exceeds 6 spells.

## Mastery discount

Investing in your world stats makes every spell cheaper. Each pillar that reaches its bar knocks **1 point off every spell's cost** (never below 1), stacking up to −3 when all three are met:

| Pillar | Reach |
| ------ | ----- |
| Mind   | level 4 |
| Body   | level 7 |
| Spirit | level 5 |

A fully-invested Caster (all three bars met) pays 3 less per spell down to a floor of 1 — so an S-tier spell costs 4 instead of 7, and the 24-point pool now fits six S-spells instead of three. Mastery widens your arsenal without raising the raw point total.

## Broken Darkness — one shared budget

A Broken Darkness character has the Darkness pool **plus** a pool for each element it has absorbed. The 48-point budget is **shared across all of them** — there's no separate per-element budget. Pouring most of your 48 into one element is a deliberate counter-pick that leaves you thin everywhere else; absorption itself is the natural limiter (you only get an element's pool once you've absorbed that element), so no artificial per-element cap is needed. The per-pool 6-spell count still applies to each.

## Worked examples

- **Six S-spells in one school → fails the budget.** 6 × 7 = 42 > 24. Even though the count (6) is legal, the weight blows the 24-point budget. (At full mastery each S costs 4 → 6 × 4 = 24 ✓ — mastery is what makes six S-spells fit.)
- **Seven cheap spells in one school → fails the count.** Seven F-tier spells cost only 7 points (well under budget), but 7 > 6 in a single pool — the count cap rejects it. Spread them across schools and they fit.
- **Mixed innate loadout → fits.** Two A-tier (6+6) + three D-tier (3+3+3) = 21 ≤ 24, with no school over 6 spells. Legal.
- **Broken Darkness spread → fits.** 4 S-tier Darkness (28) + 2 B-tier Fire (10) + 1 A-tier Water (6) = 44 ≤ 48, each pool ≤ 6. Legal — and a mastery discount would leave room for more.
