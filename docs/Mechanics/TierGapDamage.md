# Tier-Gap Damage

**Status:** Live and working as designed. Reference doc — do not modify the implementation.

## Concept
An action's damage is modified by the tier GAP between the action and the CHANNEL it flows through.
A weak action carried by a strong channel is BOOSTED; a strong action forced through a weak channel is
PENALIZED. ("Did the channel have the strength to carry this?")

## The multiplier (TierGapConstants.h, namespace TierGapDamage)
Gap = ActionTierValue − ChannelTierValue (tiers F=0 … S=6, clamped −6..+6).
Action BELOW channel (negative gap) → boost; action ABOVE channel (positive gap) → penalty.

| Gap | Multiplier | Effect |
|---|---|---|
| ≤ −4 | ×1.30 | strong channel boosts weak action (max) |
| −3 | ×1.20 | |
| −2 | ×1.13 | |
| −1 | ×1.06 | |
| 0 (matched) | ×1.00 | no gap |
| +1 | ×0.90 | |
| +2 | ×0.78 | |
| +3 | ×0.64 | |
| ≥ +4 | ×0.50 | weak channel penalizes strong action (max) |

## How it resolves (ActionExecutor.cpp)
- GetTierGapDamageMultiplier (:458) → ResolveActionTier vs ResolveChannelTier; no channel → ×1.0.
- Applied AFTER CalculateDamage returns, at three assembly sites: Spell :1027, Ability :1188,
  Attack :1334. (Separate from durability wear, which has its own constants.)

## Per action type (IMPORTANT — the current, intended behaviour)
- **SPELLS — LIVE.** ResolveActionTier reads the spell's OWN tier (SpellData->Tier); ResolveChannelTier
  reads the CATALYST tier (the crystal it channels through, or the evolution item; fusion uses the gem
  half). So a spell's gap = spell tier vs catalyst tier — NOT the weapon.
  - S-spell through F-catalyst → +6 gap → ×0.50 (weaker).
  - F-spell through S-catalyst → −6 gap → ×1.30 (stronger).
  - Innate spell (no catalyst) → ×1.0 (no gap).
- **ABILITIES & ATTACKS — matched by design.** Both ResolveActionTier and ResolveChannelTier resolve to
  the active WEAPON's tier, so the gap is always 0 → ×1.0. Abilities/attacks take their tier from the
  weapon they're performed with; there is no independent ability/attack-vs-weapon gap. (The asset's
  authored AbilityData->Tier / WeaponAttackData->Tier is used elsewhere — break/difficulty — not in this
  damage path.)

## Tier fields summary
| Asset | Damage-gap tier source |
|---|---|
| SpellData | own authored Tier (independent) — keyed vs catalyst |
| AbilityData | inherits active weapon tier (matched → ×1.0) |
| WeaponAttackData | inherits active weapon tier (matched → ×1.0) |

## Design note
This is intentional: spells channel through a catalyst, so a tier mismatch there is meaningful; abilities
and attacks are bound to the weapon, so they move in lockstep with it (no self-gap). The system is built
this way deliberately — leave as-is.
