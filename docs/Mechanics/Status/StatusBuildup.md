# Status Buildup (the bar)

**Status:** [Live]. The bar-fill mechanic. Owning code: `UStatusBuildupManager::AddStatusBuildup`, `BarCapTriggerResolver`. (What the procs *do* → [Status effects](./StatusEffects.md); what *slows* the fill → [Resistance](./Resistance.md).)

## The loop

1. **Fill** — each hit adds buildup to the target's bar, keyed to the attack's **element** (Fire/Lightning/…) or **physical type** (Slash/Pierce/Impact). `AddStatusBuildup`.
2. **Cap → proc** — when the bar caps, the matching effect fires (resolved from element/physical type by `BarCapTriggerResolver`) and the bar resets. See [Status effects](./StatusEffects.md).
3. **Decay / clear** — buildup can be reduced by gauge-manipulation effects (`StatusDecrease` → `ReduceStatusBuildupByAmount`).

## What changes the fill rate

- **Attacker StatusMultiplier** (a **Spirit** substat; + StatusStone, BD absorption stacks) — more output → bar fills faster.
- **Multi-hit** (`HitCount`) — each hit adds buildup, so multi-hit fills faster. See [Targeting](../Combat/Targeting.md).
- **Defender resistance** — Spirit + gear + class-innate **slows** the fill, per element/physical type. See [Resistance](./Resistance.md).

## Player levers

- Concentrate one element/type on a target to cap before it heals/cleanses.
- Stack StatusMultiplier to pressure faster; stack Resistance to soak slower.
- Use gauge-reduction to clear a bar before a dangerous proc.

## Entry points

- `UStatusBuildupManager` — `AddStatusBuildup`, `ReduceStatusBuildupByAmount`, `GetTotalStatusResistance`; `OnStatusBuildupChanged` (UI fill/tint).
- `BarCapTriggerResolver` — (element, physical type) → proc.

## Related

- [Status effects](./StatusEffects.md) (the procs) · [Resistance](./Resistance.md) (slows fill) · [Targeting](../Combat/Targeting.md) (multi-hit) · [`../Architecture/StatusBuildupSystem.md`](../../Architecture/StatusBuildupSystem.md)
