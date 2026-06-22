# Turn Order

**Status:** [Live]. Owning code: `UTurnManager` (`PreviewTurnOrder`).

## What the player sees

A **turn-order strip** showing who acts next — roughly the next ~10 turns, with the current actor highlighted and **bonus turns** (e.g. Emerald) pinned inline. `UTurnManager::PreviewTurnOrder`.

## How order is decided

Turn order is **not** fixed or round-robin: **faster combatants act more often.** Speed (Spirit-driven `TurnSpeed` + buffs) sets each combatant's pacing through a speed-debt schedule. You feel it as quicker characters taking extra/earlier slots; the scheduling math itself is internal.

- **TurnSpeed** is a Spirit substat (see [Stats](../Character/Stats.md)); buffs/debuffs (`TurnSpeedBuff`/`Debuff`) shift it.
- **Bonus turns** (Emerald and similar) insert extra slots, shown in the preview.

## Player levers

- Raise **TurnSpeed** (Spirit / gear / buffs) → act sooner/more often.
- Land **Slow** (`SpeedDebuff`/`TurnSpeedDebuff`) on enemies → push their turns back (see [Status effects](../Status/StatusEffects.md)).

## Entry points

- `UTurnManager` — `PreviewTurnOrder`, the speed-debt sim, bonus-turn insertion.

## Related

- [Stats](../Character/Stats.md) (TurnSpeed) · [Status effects](../Status/StatusEffects.md) (Slow) · [`../Architecture/TurnManager.md`](../../Architecture/TurnManager.md) (full spec)
