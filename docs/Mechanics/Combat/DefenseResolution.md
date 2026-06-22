# Defense Resolution

**Status:** Live — reference doc (created 2026-06-22). Describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`README.md`](../README.md), [`../Architecture/CombatOrchestrator.md`](../../Architecture/CombatOrchestrator.md) (per-impact Cast-notify dispatch). Design/build history: [`../Design/Completed/RealTimeDefenseRework.md`](../../Design/Completed/RealTimeDefenseRework.md) — that is the *rework arc*; **this doc describes the shipped resolver**.

## Concept

Melee attacks resolve defense **per impact**. The defender buffers timestamped inputs (type + direction +
press-time) during the attack's defense window; each impact frame match-and-consumes one unconsumed in-window
entry and resolves that impact's reduction independently. Non-melee (spell/ability) uses a legacy lumped
first-input decision evaluated at window close.

## Live path (`UDefenseSystem`, `Combat/Defense/DefenseSystem.cpp`)

- `SubmitDefenseInput` — append a timestamped `FTimestampedDefenseInput` (Type + Direction + InputTime + `bConsumed`) to the buffer.
- `MatchAndConsumeInput` — per-impact, find and consume an unconsumed in-window entry (perfect = within the perfect threshold).
- `CalculateDefenseResult` (`:466`) — resolve one impact's reduction from the matched type.
- Per-impact melee driver: `UActionExecutor::ResolveImpactDefense` (`Combat/Actions/ActionExecutor.cpp:1811`).

Enums/structs: `FDefenseDifficultyTriple` (Parry/Dodge/Block per-impact tiers, `Combat/Defense/DefenseDifficulty.h`),
`EDefenseType` (None/Block/Parry/Dodge), `EDefenseDirection` (None/Left/Right — dodge only),
`EDefenseDifficulty` (Inherit/Easy/Medium/Hard/Impossible).

## Reduction values (`CalculateDefenseResult`, member tunables in `DefenseSystem.h`)

| Defense | Damage taken | Reflect | Source |
|---|---|---|---|
| **Block** | **50%** (`FinalDamage = Base × (1 − BlockReduction)`, `BlockReduction = 0.5`) | — | `:489` / `DefenseSystem.h:416` |
| **Parry** | **30%** (`ParryReduction = 0.7`) | **30% of base** reflected to attacker (`ParryReflect = 0.3`) | `:495–496` / `DefenseSystem.h:420,424` |
| **Dodge** | **0% — full avoid** (`FinalDamage = 0`) | — | `:499–504` |

> **No attack-size gate.** Dodge is **100% avoidance on TIMING alone** — the old attack-size/threshold gate was
> removed, so any well-timed dodge fully avoids regardless of attack size (player + AI, uniform).
> `DefenseSystem.cpp:138` and `:500–501`. A missed timing (`!bDefenseSuccessful`) takes full damage.

## Difficulty windows

`EDefenseDifficulty` scales the effective timing window via a multiplier; harder = tighter:

| Difficulty | Window × |
|---|---|
| Easy | 1.0 |
| Medium | 0.7 |
| Hard | 0.4 |
| Impossible | 0.1 |

A hard floor (`MINIMUM_DEFENSE_WINDOW`) and a post-impact grace (`DEFENSE_AFTER_GRACE_SECONDS`) bound the
effective window (`CombatConstants.h`). Difficulty is authored per-skill and can be overridden per-impact via
`FDefenseDifficultyTriple`.

## Buildup pass-through (separate axis — do not conflate)

Block/Parry also reduce **status buildup** through the defended hit, via constants that are **independent** of
the damage reductions above: `BLOCK_BUILDUP_MULTIPLIER = 0.5` (50% buildup through), `PARRY_BUILDUP_MULTIPLIER = 0.3`
(30% through) (`CombatConstants.h:338–339`). Dodge cancels buildup entirely (full avoid). These govern the
status bar, not the damage number.

## Integration points

- **Parry reflect** — `UDefenseSystem::ApplyParryReflect` applies reflected damage to the attacker and broadcasts `OnParryReflect`.
- **AI** — `UAIDecisionManager::TrySynthesizeImpactDefense` is invoked per impact from `ResolveImpactDefense`; difficulty governs the AI's aim precision.
- **Animation/finalize** — the async action context holds until all impacts' defenses resolve and the montage ends, then finalizes.

## Known TODOs

- No Architecture/Mechanics doc covered the shipped buffer/match-consume resolver before this (only the design-rework doc).
- Spell/ability defense remains the legacy lumped first-input path (per-impact applies to melee only).
