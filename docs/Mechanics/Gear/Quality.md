# Quality (rolled drop-grade)

**Status:** [Built · No UI] — the grade is **rolled and stored** on every owned instance
(`EconomyYield::RollQuality` at the drop mint points); it has **no gameplay consumer wired yet** and
no UI surfaces it. Code: `ItemQuality.h`, `EconomyYield::RollQuality`. Axis detail:
[`../../Architecture/TierOnInstance.md`](../../Architecture/TierOnInstance.md).

When you acquire a fresh drop that opts in (`bRandomGenerateOnPickup`), it rolls a **Quality grade**
— **F → E → D → C → B → A → S** — stamped on that **instance**. Two copies of the same item can roll
different quality. It's a **separate axis from Tier**: *Tier* is power/rarity and the axis leveling
mutates; *Quality* is the rolled drop-grade and is **never changed by leveling**.

## The roll

Weighted toward the low end (good drops are rare), summing to 100 before Luck bias:

| Grade | F | E | D | C | B | A | S |
|-------|---|---|---|---|---|---|---|
| Base weight % | 26 | 22 | 18 | 14 | 10 | 6 | **4** |

- **Luck bias** — your normalized Luck tilts the curve around **C** (the pivot): toward high grades
  when Luck > 0, toward low when "cursed" (< 0). The tilt is **modest by design** —
  `QUALITY_LUCK_MAX_TILT = 0.5`, so maxed Luck roughly *doubles* S's share (≈4% → ≈7%), never
  trivializing it.
- **`C` is the placeholder** — items that **don't roll** (purchased, factory-seeded, or
  `bRandomGenerateOnPickup` off) are stamped **C-Quality**, not rolled. So a C on an item may mean
  "rolled average" *or* "never rolled" — don't read C as a roll result.
- **Perk seam** — a future "drop-grade-shift" perk will add a flat grade-index shift at the roll
  (the reason Quality is roll-based, not authored). Not built.

## What Quality affects (today: nothing yet)

⚠️ **Quality is stored groundwork.** It's rolled and persisted on weapons, rings, evolutions, spells,
and abilities, but **no code reads it to drive gameplay** — it does not yet bias roll budget, loot
value, stats, or price. The `ItemQuality.h` header describes the *intended* role ("biases roll
budget / loot value"); that consumer is **unbuilt**. Documented so the doc doesn't overstate it.

## Quality vs the stat rolls
Distinct from [Per-Instance Rolls](./PerInstanceRolls.md): that is the **stat/resistance** roll
axis (`GeneratedStatBonus` / pools — *what numbers* a drop got). **Quality** is a single **grade** on
the same drop (*how good* the drop is, as one letter), rolled beside it at the same mint point.

## How to test
- `InventoryDebug` dumps owned instances with their per-instance Quality alongside Tier.
- Acquire several copies of a `bRandomGenerateOnPickup` item → grades vary per copy.
- A **purchased** item is always C-Quality (no roll).

## Known Limitations / TODOs
- **No consumer** — nothing reads Quality for gameplay yet (the intended budget/value bias is unbuilt).
- **No UI** — not surfaced anywhere player-facing.
- **`C` is ambiguous** (rolled-average vs never-rolled) until a non-rolled sentinel or a consumer disambiguates.

## Related
- [`../../Architecture/TierOnInstance.md`](../../Architecture/TierOnInstance.md) — the tier + quality instance axis.
- [`PerInstanceRolls.md`](./PerInstanceRolls.md) — the stat-roll axis (the other half of a drop).
- [`Economy.md`](../Economy/Economy.md) — where rolls/quality sit in the wider loop.
