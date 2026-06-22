# Infusion (Hold-to-Charge)

**Status:** Live. Player-facing reference. Deep spec: [`../Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md).

## What the player does

Hold the action button to **charge** a spell or ability before it fires. Charge has three levels:

| Level | Hold | What it does |
|---|---|---|
| **L0** | tap (no hold) | baseline — authored size / damage / status, base EP cost |
| **L1** | short hold | bigger + stronger, at higher cost |
| **L2** | full hold | biggest + strongest, at the highest cost |

Charging is a **commitment trade**: more output for more resource spend. You feel it as a hold-then-release, with charge-level feedback while held.

## What each level changes

Higher charge raises **all four** of these together:

- **Size** — the spell's visual/hit scale grows (`GetSpellInfusionSizeMultiplier`; L1 ×1.5, L2 ×2.0).
- **Damage** — per-mode, stat-scaled charge bonus (`UActionExecutor::GetChargeDamageMultiplier`). The exact bonus depends on the infusion *mode* (which source you charge through) and your stats.
- **Status buildup** — scales by the per-mode charge status multiplier (`GetChargeStatusMultiplier`).
- **Cost** — EP cost rises with the charge multiplier (`ComputeInfusionCostMultiplier`; L1 ×1.5, L2 ×2.0), on top of efficiency and own-tier power.

Exact per-mode multipliers live in code / `InfusionSystem.md` — they are stat- and source-dependent, not flat.

## The HP cost (can be lethal)

HP-paying infusion sources (**Raw**, **Innate-on-spell**, **Evolution**) charge **health** on top of (or instead of) EP. This HP cost is **deferred to finalize**: it lands *after* the infused effect resolves (`PendingInfusionHPCost` → `FinalizeAsyncAction`). So a greedy charge **can kill you** — the damage/heal goes out first, then the backlash is paid. (Crystal sources — ring / weapon crystal — pay durability wear instead of HP; see [`DurabilityWear.md`](../Gear/DurabilityWear.md).)

## Source binding — spells vs abilities

- **Spells are origin-bound.** A spell can only be infused through the source it belongs to (innate / ring crystal / weapon crystal / evolution). You don't get a free pick — the legal 1:1 source is resolved for you (`ULoadoutComponent::ResolveSpellSource`); no legal source → it casts uninfused.
- **Abilities are free-choice.** An ability/attack can be infused from whichever source is available (the heuristic picks one), since it isn't tied to a spell origin.

The chosen source sets the **mode**, which determines the per-mode damage/status/cost multipliers above — so *what* you charge through changes *how much* the charge gives.

## Entry points

- `UInfusionChargeManager` — `BeginCharge` / `UpdateCharge` / `CompleteCharge`, `FChargeStatus` (level + progress, `OnChargeLevelChanged`).
- `UActionExecutor::GetChargeDamageMultiplier` / `GetChargeStatusMultiplier` / `ComputeInfusionCostMultiplier` — the applied multipliers (shared with the AI preview, so AI estimates match).
- `UActionExecutor::ApplyCommitCosts` → `PendingInfusionHPCost`, paid at `FinalizeAsyncAction` (the lethal-at-finalize path).

## Related

- [`SpellSources.md`](./SpellSources.md) — what each source costs (break / wear / consume).
- [`TierGap.md`](../Scaling/TierGap.md) / [`TierPower.md`](../Scaling/TierPower.md) — the other multipliers that stack on an action.
- [`../Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md) — full charge/mode/cost spec.
