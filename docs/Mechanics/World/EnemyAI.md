# Enemy AI

**Status:** [Live]. How opponents behave and what difficulty changes — what you *feel*, not the full algorithm. Owning code: `UAIDecisionManager`, `EAIDifficulty`. Full spec: [`../Architecture/AISystem.md`](../../Architecture/AISystem.md).

## Difficulty tiers

`EAIDifficulty` — **Easy / Medium / Hard / Expert** — scales three things you notice:

- **Think delay** — pause before the AI acts (Easy slow, Expert near-instant).
- **Decision quality** — Easy picks near-randomly; Medium+ scores targets and plays smart.
- **Defense reflexes** — higher tiers dodge/parry/block your hits more reliably and time them tighter.

## What the AI does (Medium+)

- **Targeting** — scores by kill-potential, missing HP, and threat; focuses the best target.
- **Survival first** — heals / wards itself when low, cleanses dangerous debuffs, *then* attacks.
- **Infusion** — charges spells/abilities (L0/L1/L2) when worthwhile, and guards against self-killing HP-cost infusions.
- **Defense reactions** — synthesizes a per-impact dodge/parry/block within the window (see [Defense](../Combat/DefenseResolution.md)).
- **Emerald setups** — may spend a bonus-turn item to secure a lethal damage-over-time kill before you can heal/cleanse.

The AI's damage/status predictions run through the **same execution path** the player's actions use, so its estimates match what it will actually do.

## Entry points

- `UAIDecisionManager` — `BuildAction_Smart`, `ScoreTarget`, `TrySynthesizeImpactDefense`, `Decide*InfusionLevel`, Emerald valuation.
- `EAIDifficulty`, `AIDecisionConstants` (tunable thresholds).

## Related

- [Defense](../Combat/DefenseResolution.md) (what the AI is reacting to) · [`../Architecture/AISystem.md`](../../Architecture/AISystem.md) (full decision spec)
