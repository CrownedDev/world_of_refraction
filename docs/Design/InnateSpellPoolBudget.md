# Innate & BD Spell Pool — Weighted Slot Budget

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> The "tier as slot cost" loadout-economy mechanic for the **Caster innate spell pool and the Broken
> Darkness pools** — one shared model, one shared constant (BD is double innate). Equipment
> abilities/spells use a *different* model (flat slot-count by tier — see
> [`EquipmentSlotTierScaling.md`](./EquipmentSlotTierScaling.md)). When shipped, this moves to
> `docs/Design/Completed/`.

## What changes

Today the innate pool is a **flat count**: `MAX_INNATE_SPELLS_PER_SCHOOL = 6` × 4 schools = 24 slots, each
spell taking exactly one slot regardless of tier.

This adds a **weighted budget** on top: tier becomes the slot *cost*, so heavier spells consume more of a
fixed point pool. Breadth (many low-tier spells) trades against power (few high-tier spells). World-stat
mastery then discounts those costs, widening the arsenal as the character invests.

## The two caps (both apply)

- **Count cap (existing):** 6 per school × 4 schools = **24 spells** absolute (innate).
- **Weight budget (new):** a spell's cost = its tier; the sum of equipped spell costs must not exceed the
  budget for that pool type.

One shared constant drives both pool types, with the 2:1 relationship locked in code:

```
BD_SPELL_BUDGET     = 48              // Broken Darkness — shared across Darkness + all element pools
INNATE_SPELL_BUDGET = BD_SPELL_BUDGET / 2   // = 24, non-BD Caster
```

Change `BD_SPELL_BUDGET` and innate tracks automatically at half — one edit point.

Both caps apply for innate (≤24 spells *and* ≤24 points). BD has no school count-cap; its budget plus the
absorption gate are the limiter (see BD section).

## Cost curve

| Tier | F | E | D | C | B | A | S |
| ---- | - | - | - | - | - | - | - |
| Slot cost | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

Base: 24 ÷ 7 = **3 S-tier spells** (or 24 F-tier, or any mix that sums ≤ 24).

## Mastery discount

Each world pillar that meets its threshold removes **1 point** from every spell's cost, **floored at 1**,
stacking up to **−3** when all three are met. Body now has a role — all three pillars contribute equally.

| Discount | F | E | D | C | B | A | S |
| -------- | - | - | - | - | - | - | - |
| 0 pillars (base) | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| −1 (1 pillar) | 1 | 1 | 2 | 3 | 4 | 5 | 6 |
| −2 (2 pillars) | 1 | 1 | 1 | 2 | 3 | 4 | 5 |
| −3 (all three) | 1 | 1 | 1 | 1 | 2 | 3 | **4** |

Fully invested: 24 ÷ 4 = **6 S-tier spells** (vs 3 at base). The floor-1 rule means no spell is ever free,
so the budget always holds — even maxed, 24 points caps at 24 spells, and the count cap holds it at 24.

### Threshold — tunable constant

Ship trigger: each pillar grants its −1 only at **level 7** (full −3 at 777). This is deliberately the
simple end. The trigger level is a **named constant** (`INNATE_DISCOUNT_PILLAR_THRESHOLD`, default 7) so it
can be pulled down (e.g. 5 → a reachable 555 build earns the full discount) after PIE without a code
change. Discount-per-pillar (`INNATE_DISCOUNT_PER_PILLAR`, default 1) and the floor
(`INNATE_SLOT_COST_FLOOR`, default 1) are likewise constants.

> Pillar identity (Spirit = breadth / Mind = power, each discounting only its half of the ladder) was
> considered and set aside — it reopened the half-split boundary argument and the uniform −1 is cleaner.
> If revisited, it's a discount-distribution change only; the budget + cost curve are unaffected.

## Broken Darkness pools

BD replaces its per-pool flat-6 caps with the **same model, double budget**:

- **One shared 48-point budget** across the Darkness pool *and* every absorbed element pool — not 7+
  independent pools. This kills the "48 S-tier spells" problem: at 7 pts each you fit ~6 S-spells base,
  ~12 fully discounted.
- Same cost curve (F1…S7), same per-pillar mastery discount.
- **No per-element sub-cap.** The **absorption gate is the limiter** — a BD only has an element pool for
  elements it has absorbed, so concentrating most of the 48 into one element is a deliberate counter-pick
  that leaves the character blank elsewhere. The system self-balances through absorption; an artificial
  per-element cap is unnecessary.

| | Budget | Base S-spells | Maxed S-spells (−3) |
| - | ------ | ------------- | ------------------- |
| Innate | 24 | 3 | 6 |
| BD | 48 | ~6 | ~12 |

BD is "the same caster, wider and roughly double" — a real power step, far short of unbounded.

## Build scope (for the survey)

- **`SpellSlotCost(EItemTier)`** — the 1–7 cost curve.
- **`SpellSlotDiscount(CharacterData)`** — counts pillars ≥ threshold, returns 0–3.
- **Effective cost** = `max(FLOOR, BaseCost − Discount)` per spell.
- **Budget constants** — `BD_SPELL_BUDGET = 48`, `INNATE_SPELL_BUDGET = BD_SPELL_BUDGET / 2`, plus cost
  curve, discount-per-pillar, threshold, floor. New header (`SpellPoolConstants`) or into
  `InventoryConstants`.
- **Innate validation** — the innate branch in `FSavedLoadout`/`LoadoutComponent` gains the weight check:
  Σ effective cost ≤ `INNATE_SPELL_BUDGET`, alongside the existing count cap.
- **BD validation** — `FCombatLoadout::ValidateBDSpellLoadout` swaps its per-pool ≤6 checks for one summed
  weight check across InnateSpells (Darkness) + all `BDSpellPools` ≤ `BD_SPELL_BUDGET`. Element-match and
  absorption gating unchanged.
- **UI** — loadout screen surfaces points used / budget + per-spell cost (Blueprint pass, out of scope for
  the C++ arc).
- **Debug tooling** — budget usage, per-spell effective cost, active discount; for BD, the shared total
  across pools.

Touches the innate + BD pool validation and constants, not the combat pipeline.

## Open / carry-over

- **Threshold value** — ships at 7; tune after PIE via the constant.
- **BD count behaviour** — BD has no school count-cap; budget + absorption gate are the only limiters
  (intended).
- **Equipment translation** — equipment uses a separate flat slot-count model
  (`EquipmentSlotTierScaling.md`), not this budget. Deliberate split.

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: innate + BD pools gain a weighted budget (tier = slot cost 1–7) with a per-pillar −1 mastery discount (floor 1, up to −3, threshold ship 7). One shared constant — BD 48, innate = BD/2 = 24. BD is single shared budget across Darkness + element pools, no per-element sub-cap (absorption is the limiter). Replaces innate count-only + BD per-pool flat-6. Not yet built. | (tbd) |
