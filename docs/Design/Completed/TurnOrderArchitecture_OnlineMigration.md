# Turn-Order Architecture — Online Migration (Materialized Turn Queue)

**Status:** Banked design decision. Not for current scope.
**Trigger:** When netcode / online play begins (the shared-world milestone). Not needed for the single-player pitch demo.
**Current system stays:** recompute-from-debt (`UTurnManager`) is the correct choice for single-player and ships as-is.

---

## Summary

Migrate turn-order from the current **recompute-from-debt** model to a **server-authoritative materialized turn queue** (the "conveyor belt"). The debt math is retained — it *populates* the belt in speed-correct order — but the belt becomes the replicated source of truth that clients display rather than recompute.

This is an **evolution, not a rewrite of the sequencing logic**: the accumulation/selection math survives; what changes is that its output is *materialized into a replicated list* instead of *recomputed on demand*.

---

## The two models

### Current — recompute-from-debt (single-player-correct)

- Each combatant has `TurnsOwed` (accrues by `SpeedRatio` per round) and `TurnsTaken`.
- "Who's next" = highest net debt (`TurnsOwed − TurnsTaken`), tie-broken by `ShouldBreakTieInFavor`.
- There is **no stored sequence** — turn order is computed on demand; `PreviewTurnOrder` simulates the algorithm forward on a scratch copy.
- A bonus turn is just `+1.0` of fungible debt — it has **no identity** (indistinguishable from "this character is fast").

**Strengths:** elegant fractional speed (emergent from accumulation); deterministic *as a pure function* of speed + actions.

**Weakness for online + features:** turns are derived, not materialized, so they carry no per-turn metadata, and synchronization across machines depends on every client recomputing bit-identically.

### Proposed — materialized turn queue (online-correct)

- An explicit ordered list of upcoming turn entries: `{ Combatant, bIsBonus, bIsForced, ... }`.
- Taking a turn = pop the front; granting a bonus = insert an entry; the preview **is** the belt (just read it).
- The **server** computes the belt (using the retained debt math) and **replicates it** to clients. Clients display the authoritative belt; they do not recompute.

---

## Why the shared-world / online vision favors the belt

The single-player determinism argument (which favored debt) **flips** under networked play:

- **Recompute-everywhere is fragile online.** Each client running the debt math and *hoping* to match requires bit-identical inputs and float behavior across platforms — a classic desync source.
- **An authoritative belt is naturally replicable.** One source of truth (the server's belt) is sent to clients; no divergent-recomputation desync. The belt *is* the synchronized state — exactly what networked games want.
- **Per-turn metadata becomes intrinsic.** Bonus / forced / delayed / triggers-X are just fields on a belt entry — no inference, no consume-before-observe class of bug. (The Emerald bonus-turn-flag work hit that class of bug repeatedly precisely because the debt model has no materialized turn to tag.)

So for online, determinism is better served by *replicating an authoritative list* than by *independent recomputation*.

## Why it is NOT done now

- **Debt math doesn't disappear — it moves.** The server still runs accumulation/selection to populate the belt in speed-correct order. So it is "debt-math feeds a belt," retaining the logic; the restructure is in *how output is stored and read*, not the sequencing rules.
- **The payoff is networking, which is future scope.** Today (single-player), the belt only buys per-turn-metadata convenience; the big win (server-authoritative sync) lands only when netcode exists.
- **The pitch demo is single-player.** The debt model plus the small fixes already deliver polished combat. Rewriting the turn core now spends demo time on infrastructure whose payoff is post-funding.
- **Core-rewrite risk.** Turn sequencing is load-bearing (selection, preview, UI, AI "what's next"). Restructuring it carries regression risk best taken on deliberately, with the online requirements actually in hand.

## Middle path (if pressure arrives before full online)

Keep debt as the **selector** (preserves fractional speed + single-player determinism) and grow a **thin materialized layer only for the metadata that's awkward** — a small tagged-upcoming-turns list alongside debt, not replacing it. The current `bCurrentTurnIsBonus` + `PendingTurns` approach is already an incremental step in this direction. If the **RealTimeDefenseRework** turns out to need heavy per-turn metadata (per-hit timing, reaction windows), grow this thin layer rather than rewriting the core — and re-evaluate the full belt against that feature's actual requirements.

---

## Decision record

- The current debt model is **not "wrong"** — it is the correct single-player choice that **evolves into** the belt when networking arrives.
- Revisit this doc at the **online milestone**, or earlier if the defense rework surfaces per-turn-metadata needs the debt model genuinely cannot carry.
- Retain the debt accumulation/selection math regardless — it populates the belt; it is not thrown away.
