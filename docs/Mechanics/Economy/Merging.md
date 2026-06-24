# Crystal Merging (value-based tier-up)

**Status:** [Built · No UI] — `UEconomyService::MergeCrystals` is a complete, authority-gated op; no
hub trigger calls it yet. Backend: [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md).
Loop context: [`Economy.md`](./Economy.md).

Crystals and stones **don't level** like gear (they have no instance tier to raise — a crystal *is*
its `{Type, Tier}`). Instead they **merge up**: combine several lower-tier crystals **of the same
type** into one higher-tier crystal of that type. This is the crystal lane's equivalent of leveling.

## How it works

Each tier is worth a number of **F-units** (the value ladder). To merge up to a target tier, the
system **sums same-type crystals lowest-first** until their combined value **meets or exceeds** the
target's value, consumes exactly that set, and produces **1 crystal of the target tier** in the same
pool. You also pay **Prisms**.

### The value ladder

| Tier | F | E | D | C | B | A | S |
|------|---|---|---|---|---|---|---|
| F-unit value | 1 | 2 | 4 | 8 | 16 | 32 | **96** |

Doubling F→A, then **S = 3×A** (the 3:1 final step mirrored in value space — S is deliberately
expensive). So, in F-units: an **E** costs two F's; a **B** costs 16 F's (or 8 E's, or one A minus…
whatever lowest-first sums to ≥16); an **S** costs 96.

### Prisms cost

| Produced tier | E | D | C | B | A | S |
|---|---|---|---|---|---|---|
| Prisms | 25 | 50 | 100 | 200 | 400 | 800 |

= **half the buy price** of the produced tier (`GetMergeCostForTier` = `GetPrismsBaseForTier / 2`).
**F is never produced** by a merge — it's the floor.

## How it behaves
- **Same type only** — you can't merge Garnet into Sapphire; element/type is preserved, only tier rises.
- **Lowest-first consumption** — it spends your cheapest crystals first to reach the target value.
- **Item-crystals + stones only** — evolution can't be merged (it isn't representable as a `FCrystalId`); evolution tiers up by **upgrading** instead ([`Upgrading.md`](./Upgrading.md)).
- **Spend + remove-first, full refund on failure** — Prisms are taken and inputs removed before the
  produce; if the produce fails, everything is refunded.

## How to test
- Hold 2× F-gems of one element + enough Prisms (25), merge to **E** → the two F's vanish, one E
  appears, Prisms drop by 25.
- Try a merge you can't afford in value or Prisms → rejected, nothing consumed.

## Known Limitations / TODOs
- **No hub UI** — backend op only.
- **Run-scoped** — the produced crystal lives in the run inventory until the Pool/save arc.

## Related
- [`Items/Crystals.md`](../Items/Crystals.md) — what crystals are + the per-tier effect tables.
- [`Economy.md`](./Economy.md) — the loop · [`Dismantle.md`](./Dismantle.md) — scrapping crystals for typed essence instead.
- [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md) — `MergeCrystals` + `GetCrystalValue`/`GetMergeCostForTier`.
