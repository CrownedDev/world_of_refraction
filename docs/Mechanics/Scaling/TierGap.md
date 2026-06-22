# Tier-Gap

**Status:** Live — four-dimension system shipped & PIE-verified (2026-06-22, branch `feature/tier-gap-consolidation`). Reference doc — describes shipped behaviour; do not modify the implementation from this doc.

> **Renamed** from `TierGapDamage.md` on 2026-06-22 (the system outgrew damage-only — it now covers damage + status + effect magnitude + cost). Old links/grep: search `TierGap`.

> **Related:** [`TierPowerScaling.md`](../../Design/Completed/TierPowerScaling.md) — the separate own-tier *power* knob. Build/design history: [`TierGapConsolidation.md`](../../Design/Completed/TierGapConsolidation.md) (the consolidation arc, completed).

## Concept

An action is modified by the tier **GAP** between the action and the **CHANNEL** it flows through.
A weak action carried by a strong channel is **BOOSTED**; a strong action forced through a weak channel
is **PENALIZED**. ("Did the channel have the strength to carry this?")

All three action types now carry a real gap, and the gap drives **four** output dimensions.

## Two separate knobs — don't conflate them

| Knob | Keyed on | Direction | Scales effects? |
|---|---|---|---|
| **Tier-Power** ([`TierPowerScaling`](../../Design/Completed/TierPowerScaling.md)) | the action's **own** tier (absolute strength) | higher own tier = bigger numbers (F=×1.00 … S=×4.80) | **No** |
| **Tier-Gap** (this doc) | the action's tier **vs the channel** (fit) | over-channelled = boost, mismatch = penalty | **Yes** |

They **stack multiplicatively and independently**:

```
Damage / Status = Base × Charge × TierPower(own tier) × TierGap(vs channel) × StatScaling × …
Effect magnitude =      Authored ×                       TierGap(vs channel)            ← no TierPower
Cost            = Base × Charge × Efficiency × TierPower(own tier) × TierGapCost(vs channel)
```

**Effect magnitude is the only dimension that gets the gap but NOT power** (per the TierPower "everything except effects" rule). Worked examples below make this concrete.

## How the gap resolves (`ActionExecutor.cpp`)

`Gap = ActionTierValue − ChannelTierValue` (tiers F=0 … S=6, clamped −6..+6). Action **below** channel
(negative gap) → boost; action **above** channel (positive gap) → penalty.

- **Action tier** — `ResolveActionTier` reads the action's **own authored tier**:
  - Spell → `SpellData->Tier`
  - Ability / Attack → `SkillData->Tier` (the merged `UAbilityData`; `ResolveActionSkill`). **No longer the weapon tier.**
- **Channel tier** — `ResolveChannelTier` reads what the action flows through:
  - Spell → the **catalyst** (crystal, or evolution item; a fusion uses the **gem half**)
  - Ability / Attack → the **active weapon**
  - No channel (innate/item spell, unarmed) → unset → multiplier is the matched constant (×1.0); **never invents a channel tier**.

### Per action type

| Asset | Action-tier source | Channel (gapped against) |
|---|---|---|
| `SpellData` | own `Tier` | catalyst crystal / evolution / fusion gem half |
| `AbilityData` (ability) | own `Tier` | active weapon |
| `AbilityData` (attack, `bIsAttack`) | own `Tier` | active weapon |

> `UWeaponAttackData` no longer exists — attacks are `UAbilityData` with `bIsAttack=true`, so `SkillData->Tier`
> is the single load-bearing field for both. `USkillDataBase::Tier` **defaults to `F_Tier`** (the neutral
> ×1.0 / power-floor anchor) so an unauthored skill is neutral, not silently carrying E-tier power/gap.

Weapon/catalyst **TYPE** matching stays a hard gate (unchanged) — this is the **tier** (soft) axis only.

## The ladders

### Damage + Status + Effect magnitude (same table, same direction)

`TierGapConstants.h` namespace `TierGapDamage`, constant `MATCHED_TIER = 1.00`.

| Gap | ≤−4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | ≥+4 |
|---|---|---|---|---|---|---|---|---|---|
| × | 1.30 | 1.20 | 1.13 | 1.06 | **1.00** | 0.90 | 0.78 | 0.64 | 0.50 |

### Cost (reciprocal — good fit cheaper, bad fit costlier)

Same file, constant `MATCHED_COST = 1.00` (exactly 1.00 at gap 0 — not rounding-adjacent). Its own ladder,
**not** `1 / damage` at runtime; the values approximate the inverse and are independently tunable.

| Gap | ≤−4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | ≥+4 |
|---|---|---|---|---|---|---|---|---|---|
| × | 0.77 | 0.83 | 0.88 | 0.94 | **1.00** | 1.11 | 1.28 | 1.56 | 2.00 |

> Reciprocal is the starting point — cost is a separate tunable and need not stay an exact inverse if PIE
> shows the swing is too sharp.

## The four dimensions

| Dimension | Ladder | Gets TierPower too? | Apply site(s) |
|---|---|---|---|
| Damage | same-direction | Yes | spell `:1129`, ability/attack `:1421` |
| Status buildup | same-direction | Yes | spell `:1231`, ability/attack `:1472` (source-side, on `StatusBuildup × StatusMultiplier`) |
| Effect magnitude | same-direction | **No** | `ApplySkillEffects` `:6056` (resolved once) → `:6190/:6191` + physical-DoT `:6201/:6202` |
| Cost | **reciprocal** | Yes | `CalculateActionEnergyCost` `:347`/`:377`, `ApplyCommitCosts` PreEffInfusedEP `:6340` |

**Effect magnitude** = the authored size of a skill effect (a "+20% attack" buff, a DOT's per-tick value, a
gauge nudge). Only **magnitude** scales — effect **duration** and **DrainPercent** stay as authored (there is
no proc/chance field). Both the `.Magnitude` (stat buff/debuff) and `.Value` (DOT/gauge) branches scale.

Accessors: `GetTierGapDamageMultiplier` `:496` (damage/status/effect) and `GetTierGapCostMultiplier` `:509`
(cost) — both resolve `ResolveActionTier` vs `ResolveChannelTier`; no channel → matched constant.

## Worked examples

Base **10** for damage / status / cost; authored effect magnitude **10%**. No charge (L0). TierPower curve:
F=×1.00, E=×1.30, D=×1.70, C=×2.20, B=×2.85, A=×3.70, S=×4.80. (Code rounds at each stage; cost ≈ rounded.)

| Scenario | Own tier → Power | Gap → ×dmg / ×cost | Damage | Status | Effect mag | Cost |
|---|---|---|---|---|---|---|
| **S action / F channel** | S → 4.80 | +6→+4 → 0.50 / 2.00 | 10×4.80×0.50 = **24** | **24** | 10%×0.50 = **5%** | 10×4.80×2.00 = **96** |
| **S action / S channel** (matched) | S → 4.80 | 0 → 1.00 / 1.00 | 10×4.80×1.00 = **48** | **48** | 10%×1.00 = **10%** | 10×4.80×1.00 = **48** |
| **F action / S channel** | F → 1.00 | −6→−4 → 1.30 / 0.77 | 10×1.00×1.30 = **13** | **13** | 10%×1.30 = **13%** | 10×1.00×0.77 ≈ **8** |
| **F action / F channel** (matched) | F → 1.00 | 0 → 1.00 / 1.00 | **10** | **10** | **10%** | **10** |

Reading it:

- **Power vs gap are separate.** S-action damage at matched channel is 48 (all power, no gap); drop the
  channel to F and the *gap* halves it to 24. Power didn't change — the **fit** did.
- **Effect magnitude ignores power.** The S/F cast deals 24 damage (power × gap) but its 10% effect lands at
  **5%** (gap only). Power never touches effects.
- **Worst mismatch stacks across all four.** S-on-F = half damage, half status, half effect magnitude, **and**
  double the gap-cost — on top of S's already-high base cost (96 here). That's the intended "match or suffer"
  signal, not a surprise.
- **Matched tier is the clean anchor.** Gap ×1.00 on every dimension; output is byte-identical to a world with
  no tier-gap at all (the gap multiply is a provable no-op at gap 0). Power still applies — matched ≠ ×1.0
  damage, it means *no fit penalty/bonus*.

## AI parity status

| Dimension | AI mirror |
|---|---|
| Damage | **Mirrored** — `EstimateSpellDamage` / `EstimateAbilityDamage` apply `GetTierGapDamageMultiplier` + TierPower via a locally-built `FAction`. |
| Status | **Mirrored** — both `EstimateStatusScore` overloads build a local `FAction` and apply TierPower + tier-gap. Charge `StatusMultiplier` is omitted **by design** (infusion level undecided at score time → L0 baseline; narrowed TODO documents it). |
| Effect magnitude | **No AI scorer** — the AI weighs damage and status only; effect magnitude is not scored for action selection, so there is no parity site. |
| Cost (affordability) | **Self-heals** — `CanAffordSpell` / `CanAffordAbility` route through the shared `CalculateActionEnergyCost`, which already folds gap-cost. |
| Infusion HP-guard | **Matches real basis** — `ClampInfusionLevelForHP` computes PreEffEP as `infusion-cost × TierPower(own) × TierGapCost` (`:2285`), equal to the real `ApplyCommitCosts` PreEffInfusedEP basis on all three factors. |

## Design notes / tuning

- **Authoring matters now.** Because ability/attack action tier is the asset's own `Tier` (default `F_Tier`),
  an ability cast through a higher-tier weapon gaps **negative** (boost); through a lower-tier weapon, positive
  (penalty). Author skill tiers deliberately — a left-at-default skill is F and will gap against any non-F weapon.
- **Overtake guardrail.** The ×1.30 max boost relies on per-tier base growth out-pacing it (safe above ~7%/tier).
  Not enforced in code; confirmed against the base curve at design time.
- **Wear is separate.** Durability wear shares only tier *comparisons* and keeps its own constants
  (`DurabilityConstants.h`) — untouched by this system.
