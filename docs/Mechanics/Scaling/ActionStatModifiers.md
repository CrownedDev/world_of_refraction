# Action Stat Modifiers

**Status:** Live — reference doc (created 2026-06-22). Describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`README.md`](../README.md), [`RequirementGap.md`](./RequirementGap.md) (the per-pillar contributor), [`TierGap.md`](./TierGap.md) / [`TierPower.md`](./TierPower.md) (assembly-layer multipliers applied alongside), [`Archetypes/Reality.md`](../Archetypes/Reality.md) (Reality boost), [`../Architecture/DamageCalculator.md`](../../Architecture/DamageCalculator.md) (where the assembled mods are consumed).

## Concept

Before an action's damage/cost is computed, `UActionExecutor::ComputeActionStatModifiers` assembles a single
per-action substat bundle (`FActionStatModifiers`) from every active source. This bundle is applied at the
**assembly layer** — outside `UDamageCalculator`'s subsystem pipeline — and the calculator then folds it in
via `ActionMods.ApplyTo(...)`.

## The bundle — `FActionStatModifiers`

An additive accumulator over **11 combat substats**: Efficiency, SpellDamage, StatusMultiplier, CritDamage,
SpellSpeed, Defense, ActionSpeed, RawDamage, **Reflex**, Resistance, TurnSpeed.

> **`Reflex` is INERT** — the field exists for `ESubStat` symmetry but is **not** read by the defense window,
> which reads `ReflexBuff`/`ReflexDebuff` status effects instead. Do not assume Reflex flows through here.

Pools (MaxHealth / MaxEnergy) and — for the requirement-gap contributor — `TurnSpeed` are deliberately
**excluded** from per-action modifiers (turn order must not shift mid-action).

## Stacking order (`ComputeActionStatModifiers`, `Combat/Actions/ActionExecutor.cpp`)

Contributors accumulate **additively** into one `FActionStatModifiers`:

1. **Reality boost** — Reality innate / evolved-slot / infusion-level substat amplification (`RealityBoost.h`).
2. **Evolution** — `Evolution.Item->GetInfusionStatModifiers(...)` + `MapToInfusionModifiers(GeneratedStatBonus, ...)` for the active infusion source.
3. **Requirement gap (per-pillar)** — `GetRequirementGapPillarPercents` → `AddPillarPercent(MindPct, BodyPct, SpiritPct)`. **Single-counted**: the old global `(1−√deficit)` penalty is **retired** and is *not* applied to damage; the per-pillar percents are the only requirement-gap contributor. See [`RequirementGap.md`](./RequirementGap.md).
4. **Infusion** — the infusion mode's damage/status multipliers for the selected source.

The assembled bundle is returned and consumed downstream by `DamageCalculator::CalculateDamage` (per-action
mods step) and by the EP-cost path. Tier-power and tier-gap multipliers are applied at this same assembly
layer but as **multipliers on the action**, not members of `FActionStatModifiers` (see [`TierGap.md`](./TierGap.md)).

## Integration points

- **Damage** — `DamageCalculator::CalculateDamage` calls `ActionMods.ApplyTo(mult, stat)` early in the pipeline.
- **Cost** — Efficiency in the bundle (moved by the Mind requirement-gap pillar) shifts EP cost; there is no separate requirement-gap cost term.
- **AI parity** — the AI's estimators call the same `ComputeActionStatModifiers`, so scoring sees identical modifiers to execution.

## Known TODOs

- No dedicated doc existed before this; the Reality/Evolution/req-gap/infusion stack lived only in code comments.
- `Reflex` remains a placeholder substat — either wire it to the defense window or remove it from `FActionStatModifiers` once the reflex-via-effects model is final.
