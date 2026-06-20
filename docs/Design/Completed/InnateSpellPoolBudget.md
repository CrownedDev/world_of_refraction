# Innate & BD Spell Pool — Weighted Slot Budget

> **Status: COMPLETED — built + PIE-verified, merged to main (2026-06-20).**
> The "tier as slot cost" loadout-economy mechanic for the **Caster innate spell pool and the Broken
> Darkness pools** — one shared model, one shared constant (BD is double innate). Equipment
> abilities/spells use a *different* model (flat slot-count by tier — see
> [`EquipmentSlotTierScaling.md`](./EquipmentSlotTierScaling.md)).

## What changes

Previously the innate pool was a **flat total count** (`MAX_INNATE_SPELLS_TOTAL = 24`), each spell taking
one slot regardless of tier — per-school count was not actually enforced.

This adds a **weighted budget** on top: tier becomes the slot *cost*, so heavier spells consume more of a
fixed point pool. Breadth (many low-tier spells) trades against power (few high-tier spells). World-stat
mastery then discounts those costs, widening the arsenal as the character invests.

## The two caps (both apply, independently)

Two SEPARATE limits — a loadout must pass BOTH:

- **Count cap** — each pool (innate school / BD element pool) holds at most
  `SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL` (**6**) spells. Tier-blind, character-blind. This is a NEW
  shared constant that replaced both `MAX_INNATE_SPELLS_PER_SCHOOL` and `MAX_BD_POOL_SPELLS` (now retired).
  *(Per-school count was never actually enforced before — the old flat `MAX_INNATE_SPELLS_TOTAL = 24` total
  is gone.)*
- **Weight budget** — a spell's cost = its tier; the sum of equipped spell costs (after the mastery
  discount) must not exceed the pool type's budget.

One shared constant drives the budget, with the 2:1 relationship locked in code:

```
BD_SPELL_BUDGET     = 48              // Broken Darkness — shared across Darkness + all element pools
INNATE_SPELL_BUDGET = BD_SPELL_BUDGET / 2   // = 24, non-BD Caster
```

Change `BD_SPELL_BUDGET` and innate tracks automatically at half — one edit point.

Innate: per-school count ≤6 **and** Σ effective cost ≤24. BD: per-pool count ≤6 **and** one shared
Σ ≤48 across the Darkness pool + every element pool (the absorption gate naturally limits which element
pools exist — see BD section).

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

### Threshold — per-pillar tunable constants

**As built the threshold is per-pillar, not a single shared "ship 7 / 777" bar.** Each pillar clears its
own world-level bar to grant its −1 (floor 1, up to −3 when all three are met):

| Pillar | Constant | Ship value |
| ------ | -------- | ---------- |
| Mind   | `SPELL_DISCOUNT_MIND_THRESHOLD`   | 4 |
| Body   | `SPELL_DISCOUNT_BODY_THRESHOLD`   | 7 |
| Spirit | `SPELL_DISCOUNT_SPIRIT_THRESHOLD` | 5 |

All three are named constants (tunable post-PIE without a code change), as are the discount-per-pillar
(`SPELL_DISCOUNT_PER_PILLAR`, default 1) and the floor (`SPELL_SLOT_COST_FLOOR`, default 1). The discount
reads the character's **raw** world-pillar levels — not the `GetEffectiveX` multipliers.

> Pillar identity (Spirit = breadth / Mind = power, each discounting only its half of the ladder) was
> considered and set aside — it reopened the half-split boundary argument and the uniform −1 is cleaner.
> If revisited, it's a discount-distribution change only; the budget + cost curve are unaffected.

## Broken Darkness pools

BD keeps a per-pool count cap (≤ `MAX_EQUIPPED_SLOT_POOL` = 6, each pool element-matched) and adds the
**same weighted model at double budget**:

- **One shared 48-point budget** across the Darkness pool *and* every absorbed element pool — not 7+
  independent budgets. This kills the "48 S-tier spells" problem: at 7 pts each you fit ~6 S-spells base,
  ~12 fully discounted.
- Same cost curve (F1…S7), same per-pillar mastery discount.
- **No per-element *weight* sub-cap.** The **absorption gate is the limiter** — a BD only has an element pool for
  elements it has absorbed, so concentrating most of the 48 into one element is a deliberate counter-pick
  that leaves the character blank elsewhere. The system self-balances through absorption; an artificial
  per-element cap is unnecessary.

| | Budget | Base S-spells | Maxed S-spells (−3) |
| - | ------ | ------------- | ------------------- |
| Innate | 24 | 3 | 6 |
| BD | 48 | ~6 | ~12 |

BD is "the same caster, wider and roughly double" — a real power step, far short of unbounded.

## Build scope (as built)

Built across 5 clusters on `feature/innate-bd-spell-budget`:

- **Constants + helpers** — new header `Source/world_of_refraction/Public/Loadout/SpellPoolConstants.h`:
  `MAX_EQUIPPED_SLOT_POOL` (count), `BD_SPELL_BUDGET = 48` / `INNATE_SPELL_BUDGET = BD/2`, the F1…S7
  `SPELL_SLOT_COST` curve, per-pillar thresholds (Mind 4 / Body 7 / Spirit 5), `SPELL_DISCOUNT_PER_PILLAR`,
  `SPELL_SLOT_COST_FLOOR`. Inline helpers: `SpellSlotBaseCost(EItemTier)`,
  `SpellSlotEffectiveCost(EItemTier, int32 Discount)`, `SpellSlotDiscount(WorldMind, WorldBody, WorldSpirit)`
  (raw int levels in — no character/loadout types, so the header stays dependency-light).
- **Effective cost** = `max(FLOOR, BaseCost − Discount)` per spell.
- **Validation split (as built):**
  - *Count cap (per-school / per-pool ≤6):* enforced at **both** the asset gate
    (`FSavedLoadout::GetValidationErrors`) and the runtime gate (`ULoadoutComponent::GetValidationErrors` +
    `CollectInvalidSlotFindings`).
  - *Weight budget:* **runtime gate only** — the asset path has no character, so it can't compute the
    discount. Null `CharData` → discount 0, weight still enforced (conservative).
  - `FCombatLoadout::ValidateBDSpellLoadout` gained `(int32 Discount = 0, bool bCheckWeight = false)`: the
    asset caller binds the defaults (count + element only); runtime callers compute the discount and pass
    `bCheckWeight = true`. Element-match + `MAX_BD_ELEMENT_POOLS` (≤7 pools) gating unchanged.
- **Retired constants** — `MAX_INNATE_SPELLS_PER_SCHOOL`, `MAX_INNATE_SPELLS_TOTAL`, `MAX_BD_POOL_SPELLS`
  deleted (superseded; zero live reads). `MAX_BD_ELEMENT_POOLS` retained.
- **Debug** — `UInventoryDebug::LogActiveLoadout` prints per-school/per-pool count vs cap, budget
  used/total, and the active discount (BD shows the shared total across pools).
- **UI** — loadout-screen points-used/budget + per-spell cost is a later Blueprint pass (out of scope for
  this C++ arc).

Touches the innate + BD pool validation, constants, and debug — not the combat pipeline.

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
| 2026-06-20 | Built (5 clusters): new `SpellPoolConstants.h` (F1…S7 cost curve, `BD_SPELL_BUDGET=48` / `INNATE_SPELL_BUDGET=24`, **per-pillar** thresholds Mind 4 / Body 7 / Spirit 5, `MAX_EQUIPPED_SLOT_POOL=6`, floor, + `SpellSlotBaseCost`/`SpellSlotEffectiveCost`/`SpellSlotDiscount` helpers). Innate: per-school count ≤6 (both gates) + weight ≤24 (runtime gate). BD: shared 48-pt budget across Darkness + element pools + per-pool count ≤6 (`ValidateBDSpellLoadout` gained `Discount`/`bCheckWeight`; element-match + ≤7-pool gating unchanged). Spell-pool debug readout added. Retired `MAX_INNATE_SPELLS_PER_SCHOOL` / `MAX_INNATE_SPELLS_TOTAL` / `MAX_BD_POOL_SPELLS`. Pending final PIE sign-off. | feature/innate-bd-spell-budget |
