# Loot Tier Bump & Overflow

**Status:** [Design — not built]. Specifies how zone difficulty and persistent buffs improve drop
quality, and what happens when the improvement would exceed S tier.

Related: [`AIArchitecture.md`](./AIArchitecture.md) §6 (the difficulty half of the same lever),
[`Resources_Design.md`](./Resources_Design.md) (essence curves),
[`../Mechanics/Economy/Economy.md`](../Mechanics/Economy/Economy.md) (faucets/sinks).

---

## 1. The model — roll, then bump

Drop tables are **never reweighted per zone**. The roll happens normally against the enemy's own
table; the *result* is then upgraded.

```
RolledTier = <normal drop roll>
N          = ZoneModifier + PersistentBuffLevel
FinalTier  = RolledTier + N        (capped at S)
Overflow   = (RolledTier + N) - S  (0 if no overflow)
```

### Why bump the result rather than reweight the table

- One drop table per enemy, forever. Zones never fork them.
- The bump is a single function at the end of the roll.
- Every "better loot" source uses the same lever — zone, buff, event, anything added later.
- Unit-testable in isolation: feed a tier and an N, assert the output. No PIE required.

### Sources of N

| Source | Value |
|---|---|
| Zone modifier | Per-zone, TBD (see §5) |
| Persistent buff L1 | +1 |
| Persistent buff L2 | +2 |

N is additive across sources.

---

## 2. Overflow — past S converts to essence

S is the cap. A bump that would push past it is **not wasted** — the excess tiers convert to
essence. This is what stops high-tier zones feeling like they discard your bonus.

**Payout = `Overflow × (S-tier yield for the item's category)`**

### Yield row, not buy row

Overflow uses the **dismantle/yield** row (S = 24 for leveling essence), *not* the purchase row
(S = 48). Overflow is a payout; the buy row is what the player pays. Using the buy row would make
overflow strictly more valuable than receiving the item, inverting the incentive.

---

## 3. Routing — overflow pays by what dropped

| Drop | Overflow pays |
|---|---|
| Weapon / ring / evolution | Gear essence |
| Spell — elemental | That element's typed essence |
| Spell — Generic | **Skill essence** |
| Ability | Skill essence |
| Crystal / stone | Its typed essence |

Routing by category preserves the self-balancing Gear/Skill lane split: gear overflow funds gear
progression, skill overflow funds skill progression. Elemental spell overflow feeds the element
lines from drops rather than only from crystal dismantling.

### ⚠️ Do not modify `SpellElementToEssenceType`

The Generic-spell case crosses lanes deliberately — from the typed lane into the leveling lane —
because `EEssenceType::Generic` (Quartz's element) is a niche currency and generic-spell overflow
routed there would be a dead-end payout.

It cannot be folded into the existing mapping function, for two reasons:

1. **Type mismatch.** `SpellElementToEssenceType` returns `EEssenceType`. Skill essence is
   `ECurrencyType::SkillEssence` — a different enum. The function cannot return it.
2. **Shared with purchase pricing.** §4.3 uses that function for a spell's element-essence *cost*
   at purchase. Changing it would make Generic spells cost Skill essence to buy, breaking the
   lane split (Skill essence is earned by scrapping skills and spent leveling them, never on
   purchases).

**Overflow gets its own routing function** returning the destination currency, with the Generic
case branched out. Nothing existing is touched.

### Existing hooks to reuse

- `GetLevelingEssenceYieldForTier(S_Tier)` — gear / skill categories
- `GetTypedEssenceYieldForTier(S_Tier)` — crystals / stones, typed
- `SpellElementToEssenceType` — **read-only**, for the elemental-spell case only

---

## 4. ⚠️ This is the third faucet

Today the economy has exactly **two** essence faucets: dismantle and combat-break. There is no
drop/reward faucet at all — `Economy.md` flags this explicitly under Known Limitations.

Overflow would be the **first reward-side essence source in the game**. It needs an eye on
inflation during balance. Not a blocker, but it should not land unnoticed: it changes the shape of
the economy from "value only enters by destroying items" to "value enters by winning".

---

## 5. Deferred — do not tune yet

**Blocked on `TierGapConsolidation`.** Zone modifier values and overflow payout rates must not be
tuned until that arc lands, or the tuning targets a moving baseline.

Open items:
- Zone modifier magnitude per zone tier
- Whether persistent buff levels cap at L2
- Whether overflow applies to currency/consumable drops or only tiered items
- Whether an already-S roll with N ≥ 1 pays full overflow, or is discounted (currently: full)

---

## 6. Worked examples

Assuming leveling-essence yield row (F 5 · E 7 · D 9 · C 11 · B 15 · A 19 · S 24).

| Rolled | N | Final | Overflow | Payout (weapon) |
|---|---|---|---|---|
| C | 0 | C | 0 | — |
| C | 2 | A | 0 | — |
| A | 1 | S | 0 | — |
| A | 3 | S | 2 | 48 Gear essence |
| S | 2 | S | 2 | 48 Gear essence |

A Fire crystal in the last row would instead pay 2 × `GetTypedEssenceYieldForTier(S_Tier)` in Fire
essence. A Generic spell would pay 2 × 24 Skill essence.
