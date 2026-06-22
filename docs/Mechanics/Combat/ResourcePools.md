# Resource Pools (HP / EP)

**Status:** Live — reference doc (created 2026-06-22). Describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`README.md`](../README.md), [`ActionStatModifiers.md`](../Scaling/ActionStatModifiers.md) (what scales the pools' max values), [`TierGap.md`](../Scaling/TierGap.md) (gap-cost folds into EP), [`../Architecture/BrokenDarkness.md`](../../Architecture/BrokenDarkness.md) (EP overflow path), [`../Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md) (HP/EP/durability infusion costs).

## Concept

Two per-combatant pools: **HP** (health) and **EP** (energy). HP gates death; EP gates actions. Both maxima
are **derived**, not authored — recomputed from pillars + equipment bonuses + status buff/debuff effects.
Action costs are paid in EP (and crystal durability), with infusion's HP cost deferred to finalize.

## Pools (`UCharacterDataComponent`, `Character/CharacterDataComponent.h`)

- `CurrentHP` / `MaxHP` — HP clamped to `[0, MaxHP]`; every change broadcasts `OnHPChanged`.
- `CurrentEP` / `MaxEP` — EP per-action; the Broken-Darkness gain path may overflow above `MaxEP` into an absorption buffer (capped by overload capacity).
- `RecomputeMaxPools()` — single recompute of `MaxHP`/`MaxEP` from pillars + equipment + pool-effect status. **Pools are excluded from action-time substat modifiers** (MaxHealth / MaxEnergy never move per-action).
- Mutators: `ServerTakeDamage`, `ServerSpendEnergy`, `ServerGainBrokenDarknessEnergy`.

## Costs (`UActionExecutor`, `Combat/Actions/ActionExecutor.cpp`)

- `CalculateActionEnergyCost` — **single source of truth** for an action's EP cost (base × charge × efficiency × tier-power × tier-gap-cost). Both execution and the AI affordability checks route through it.
- `ApplyCommitCosts` — paid at **commit**: EP deduction + crystal/ring durability wear. Spell/ability deduct EP here.
- `ApplyPendingInfusionHPCost` — infusion's HP cost is **deferred to finalize** (not commit). It is **lethal** (no leave-≥1-HP clamp) and the pay-on-miss behaviour is preserved.

### Cost lifecycle

1. **Commit** — EP + crystal/ring durability paid. Item actions are zero-EP (separate item executor); Defend is zero-cost.
2. **Finalize** — infusion HP cost applied (can kill the caster). Deferred-fire actions (`bIsDeferredFire`) skip cost paths entirely — they were paid once at arm time (see [`../Architecture/CombatOrchestrator.md`](../../Architecture/CombatOrchestrator.md)).

## Which source pays what (infusion)

| Infusion source | Cost currency |
|---|---|
| Raw / Innate (spell) / Evolution | **HP** (EP-derived, lethal, at finalize) |
| Ring crystal / Weapon crystal | **Durability** wear (at commit) |

## Integration points

- **Damage** — incoming damage routes through `ServerTakeDamage`; DOTs and SelfDamage also land here (lethal).
- **Broken Darkness** — no passive EP regen for BD casters; EP is fuelled by parry/block absorption and can overflow into the absorption buffer.
- **Last Stand** — a lethal HP result is intercepted by `ConsumeLastStandCharge` before death is finalized (see [`../Architecture/SkillEffectSystem.md`](../../Architecture/SkillEffectSystem.md)).

## Known TODOs

- No dedicated pool doc existed before this; HP/EP lifecycle was previously split across `CharacterDataSystem.md` and `CombatOrchestrator.md`.
- Overload/absorption buffer capacity constants live with the Broken Darkness system, not here.
