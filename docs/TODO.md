# TODO / Backlog

Deliberately-deferred and watch-later items. One line each + a status tag:
**WATCH** (verify in PIE) · **BLOCKED** (needs a prerequisite) · **POSSIBLE** (do only
if PIE/usage shows the need) · **CLEANUP** (remove after verification) · **DONE**.
Living backlog — keep entries short; promote to a real doc/issue when worked.

## Combat — lethality & infusion

- **WATCH** — Corpse-walk-back: after a lethal infusion, the dead caster may visibly slide back to position (`SignalActionComplete` plays return-movement on a flag-dead pawn). If janky in PIE → gate return-movement on `bIsAlive`. Not yet observed; confirm next PIE.
- **DONE** — Infusion lethal-at-finalize documented (`CombatOrchestrator.md` → *Infusion HP cost — lethal, paid at finalize*).

## Emerald-AI

- **BLOCKED** — Self-target Emerald-AI: wired but DORMANT (`ESTIMATED_EP_REGEN_PER_TURN = 0`). Activate by setting it >0 **only** once a passive per-turn EP-regen mechanic exists. Cross-ref `AISystem.md`.
- **POSSIBLE** — Enemy all-target Emerald scan: AI evaluates Emerald only on `BestTarget`. If PIE shows missed one-tick-lethal kills on non-selected targets, add an all-enemy scan.
- **POSSIBLE** — Loosen the one-tick-lethal Emerald gate: currently requires next-tick ≥ HP (guaranteed kill). If too conservative in PIE, consider an accumulated-over-exposure-window check.

## Diagnostics & cleanup

- **CLEANUP** — Strip `[AI Emerald]` + `[BONUSDIAG]` diagnostic logs after final PIE verification of Emerald-AI + the bonus-turn visual.

## Refactor — banked

- **POSSIBLE** — 5 Group-B attachment-accessor variants (banked from the accessor migration).
- **POSSIBLE** — `StatusMultiplier` base-extract: only if base composition grows beyond ~3 terms (currently keep-both).
