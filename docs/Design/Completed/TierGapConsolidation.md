# Tier Gap Consolidation

> **Related:** `TierPowerScaling.md` — the separate own-tier *power* knob (volume dial, F→S scales the authored base up). They STACK: `Final = Base × TierPower(own tier) × TierGap(vs channel) × StatScaling × envelope`. Power scales everything **except** effects; this Gap doc scales effects too.

> **Status: COMPLETED — built + PIE-verified, merged to main 2026-06-22.**
> Consolidates the tier-gap axis across all action types and all output dimensions. Superseded the
> "abilities matched by design → ×1.0" decision in [`docs/Mechanics/Scaling/TierGap.md`](../../Mechanics/Scaling/TierGap.md)
> — that doc has been rewritten to the shipped four-dimension system, and this design doc now lives in
> `docs/Design/Completed/`.

## What it consolidates

> **Which multiplier this is.** This arc extends the **tier-GAP** multiplier — the action's tier vs the
> *channel* it flows through (crystal for spells, weapon for abilities/attacks). It is NOT the separate
> own-tier "power knob" (a flat strength dial on the action's own tier). The power knob's "scales everything
> *except* effects" rule is unrelated and unchanged. Here, effect **magnitude** explicitly DOES scale on the
> gap — wrong-tier gear weakens effects alongside damage and status.

Today the tier-gap mechanic is **damage-only** and **spell-only in practice**:

- Spells gap their own tier against the **crystal** they channel through → real multiplier. Live.
- Abilities/attacks resolve both action tier and channel tier to the **weapon**, so the gap is always 0 →
  ×1.0. A deliberate no-op.

This arc unifies it along **two axes**:

### Axis 1 — all action types get a real gap

The ability/attack branch stops inheriting the weapon's tier and reads its **own authored tier**, gapped
against the weapon. After this, all three action types behave identically:

| Action  | Action tier (own)   | Channel (gapped against) |
| ------- | ------------------- | ------------------------ |
| Spell   | `SpellData->Tier`   | the crystal (live today) |
| Ability | `AbilityData->Tier` | the weapon (NEW)         |
| Attack  | `AbilityData->Tier` (attacks are `UAbilityData` with `bIsAttack=true`) | the weapon (NEW) |

> **Note:** `UWeaponAttackData` no longer exists — it was merged into `UAbilityData` (`bIsAttack=true`). So
> `AbilityData->Tier` is the single load-bearing field for both abilities and attacks after the flip.
> `ResolveActionTier`'s non-spell branch reads it instead of the weapon tier.

"High-level technique through a weak weapon" now lands weaker; a low-tier action through a strong weapon is
boosted. Weapon **TYPE** matching stays a hard gate (unchanged) — this is the **tier** (soft) axis only.

### Axis 2 — the gap drives all four output dimensions

| Dimension        | Today        | After arc                          |
| ---------------- | ------------ | ---------------------------------- |
| Damage           | live (spell) | + ability/attack flip              |
| Status buildup   | —            | NEW site, same ladder, same direction |
| Skill cost       | —            | NEW site, **reciprocal** ladder (option C) |
| Effect magnitude | —            | NEW site, same ladder, same direction |

**Effect magnitude** = the authored size of a skill effect (e.g. an effect that does "10% damage", a
"+20% attack" buff, a DOT's per-tick %). The tier-gap multiplier scales that magnitude exactly like damage:
S-spell through F-crystal (×0.50) turns a 10% effect into 5%; F-spell through S-crystal (×1.30) into 13%;
matched → the authored 10%. **Only magnitude scales** — effect **duration** and **proc/apply chance** stay
as authored (scaling those reads oddly and gets fiddly). Same direction as damage/status (over-channel
boosts, mismatch weakens), NOT the reciprocal cost direction.

## The ladders

### Damage + status (same table, same direction)

Gap = ActionTier − ChannelTier (F=0 … S=6, clamped −6..+6). Action below channel → boost; above → penalty.

| Gap | ≤−4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | ≥+4 |
| --- | --- | -- | -- | -- | - | -- | -- | -- | --- |
| ×   | 1.30 | 1.20 | 1.13 | 1.06 | 1.00 | 0.90 | 0.78 | 0.64 | 0.50 |

### Cost (reciprocal — option C: good fit cheaper, bad fit costlier)

Cost multiplier = 1 / (damage multiplier). Boost side discounts; penalty side surcharges.

| Gap | ≤−4 | −3 | −2 | −1 | 0 | +1 | +2 | +3 | ≥+4 |
| --- | --- | -- | -- | -- | - | -- | -- | -- | --- |
| ×   | 0.77 | 0.83 | 0.88 | 0.94 | 1.00 | 1.11 | 1.28 | 1.56 | 2.00 |

> Reciprocal is the starting point — cost is a separate tunable and need not stay an exact inverse if PIE
> shows the swing is too sharp.

## Full damage/status matrix (rows = action's own tier, cols = channel tier)

| Action ↓ / Channel → | F | E | D | C | B | A | S |
| -------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **F**                | 1.00 | 1.06 | 1.13 | 1.20 | 1.30 | 1.30 | 1.30 |
| **E**                | 0.90 | 1.00 | 1.06 | 1.13 | 1.20 | 1.30 | 1.30 |
| **D**                | 0.78 | 0.90 | 1.00 | 1.06 | 1.13 | 1.20 | 1.30 |
| **C**                | 0.64 | 0.78 | 0.90 | 1.00 | 1.06 | 1.13 | 1.20 |
| **B**                | 0.50 | 0.64 | 0.78 | 0.90 | 1.00 | 1.06 | 1.13 |
| **A**                | 0.50 | 0.50 | 0.64 | 0.78 | 0.90 | 1.00 | 1.06 |
| **S**                | 0.50 | 0.50 | 0.50 | 0.64 | 0.78 | 0.90 | 1.00 |

Diagonal = matched = ×1.0. Cost matrix is the reciprocal of each cell.

## Worked examples

| Scenario                       | Gap | Damage | Status | Effect mag | Cost  |
| ------------------------------ | --- | ------ | ------ | ---------- | ----- |
| S ability through F weapon     | +6 → +4 | ×0.50 | ×0.50 | ×0.50 | ×2.00 |
| F spell through S crystal      | −6 → −4 | ×1.30 | ×1.30 | ×1.30 | ×0.77 |
| Matched (any)                  | 0   | ×1.00 | ×1.00 | ×1.00 | ×1.00 |
| Innate spell (no catalyst)     | —   | ×1.00 | ×1.00 | ×1.00 | ×1.00 |

> Note the worst mismatch (S-on-F) now stacks: half damage, half status, half effect magnitude, **and**
> double cost. That's the intended "match or suffer" signal — flagged so it's a deliberate choice, not a
> surprise. (A 10%-damage effect on that cast lands as 5%.)

## Build scope (survey-corrected)

Build **consolidation-first** to avoid tripling the AI parity-drift surface (per
`AIExecutionParity_Design.md`). Order:

1. **Ability/attack tier flip** — `ResolveActionTier`'s non-spell branch reads `AbilityData->Tier`
   (single field for both abilities and attacks) instead of the weapon tier. Smallest change; makes
   tier-gap non-trivial for abilities and exposes the parity surface.
   - ⚠️ **AUTHORING AUDIT FIRST (in-editor, Crown):** `AbilityData->Tier` defaults to `E_Tier`. If existing
     ability/attack assets sit at the default, the flip gaps every ability E-vs-weapon on day one — a real
     balance shift. Confirm assets carry meaningful tiers before the flip lands.

2. **Shared finalize** — introduce `FinalizeDamage`/`FinalizeAction` on the executor folding crit + L2 +
   tier-gap into one place that both the execution path AND the AI estimator call, so the AI stops
   hand-mirroring. (Today: value source `GetTierGapDamageMultiplier` is shared, but APPLICATION is mirrored
   across damage sites + a damage-only AI mirror.) This is the spine the new dimensions hang off.

3. **Damage** — already applied at TWO sites (Spell ~:1089, merged Ability/Attack ~:1356 in
   `ExecuteSkillAsync`) — NOT three; the ability/attack merge collapsed them. Route through the finalize.

4. **Status buildup** — same gap multiplier at the source-assembly points (Spell ~:1175, Ability/Attack
   ~:1418, on `USkillDataBase::StatusBuildup × StatusMultiplier`). NOT in `StatusBuildupManager` (that's
   target-side: attacker amp + resistance + BD stacks). Move the AI status estimate onto the shared path in
   the same step (it currently omits even `StatusMultiplier` — pre-existing drift to fix here).

5. **Cost (reciprocal)** — at `CalculateActionEnergyCost`'s final `RoundToInt` (so preview, validation, and
   charge agree). ⚠️ The HP-penalty basis in `ApplyCommitCosts` (`PreEffInfusedEP`) must apply the same
   reciprocal or HP cost won't track the discounted EP.

6. **Effect magnitude** — at `ApplySkillEffects` (~:6050). ⚠️ TWO branches: stat buffs/debuffs read
   `FSkillEffectPayload.Magnitude`; DOTs/gauges read `.Value`. Scale BOTH or DOTs silently don't scale.
   Duration + DrainPercent untouched; there is no proc/chance field.

Cross-system → already surveyed. Cluster the steps; compile between; PIE-verify each dimension in isolation.

## Open / carry-over

- **Cost site** — exact assembly point(s) for resource cost confirmed at survey time.
- **Reciprocal vs authored cost ladder** — ship reciprocal, retune after PIE if the penalty surcharge is too
  steep.
- **Overtake guardrail** — relies on per-tier base damage growing faster than the ×1.30 boost (safe above
  ~7%/tier). Not enforced in code; Crown confirmed the base curve clears it.

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (design) | Design locked: tier-gap unified across all action types (ability/attack flip to own-tier) and all four dimensions (damage + status + effect magnitude, same direction; cost = reciprocal, option C). Effect duration/proc-chance untouched. Supersedes the abilities-no-op decision. | (design) |
| 2026-06-22 | **Built + PIE-verified + matched-tier regression-clean.** Shipped in four clusters: **(1)** `ResolveActionTier` non-spell branch reads the action's own `SkillData->Tier` (ability/attack flip), `USkillDataBase::Tier` default `E→F` (neutral ×1.0 anchor); **(2)** status buildup scales on the gap at both real-path sites + AI `EstimateStatusScore` parity (builds local FAction, applies TierPower + tier-gap; charge StatusMultiplier omitted = L0 baseline by design); **(3a)** reciprocal **cost** dimension — new `MATCHED_COST` ladder + `GetTierGapCostMultiplier` accessor, applied at `CalculateActionEnergyCost` (spell+ability) and `ApplyCommitCosts` PreEffInfusedEP HP basis; **(3b)** AI infusion HP-guard `ClampInfusionLevelForHP` matches the real basis (infusion-cost × own-tier power × tier-gap cost — folds in the previously-omitted PowerMult too); **(4)** effect **magnitude** scales on the gap in `ApplySkillEffects` (both `.Value` and `.Magnitude` branches, RuntimeValue pick + physical-DoT branch; duration/DrainPercent untouched). No new accessor for damage/status/effect — all reuse `GetTierGapDamageMultiplier`. | `feature/tier-gap-consolidation` |
