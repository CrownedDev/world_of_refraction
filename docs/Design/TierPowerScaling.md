# Tier Power Scaling

> **Related:** TierGapConsolidation.md (a separate channel-mismatch multiplier, arriving later). They stack: Final = Base × TierPower(own tier) × TierGap(vs channel) × StatScaling × envelope. Power scales everything EXCEPT effects; Gap scales effects too.

**Status:** Design locked, not yet built. Two bugs gate the documentation/build (see §7).
**Scope:** Unify how tier (F→S) drives power across gear, spells, and abilities.
**Goal:** One dial — tier — controls power. Author once, stamp a tier, the system scales. Less hand-tuning, one consistent rule.

---

## 1. The core idea

Tier becomes the single power axis. This is **not** a new system — it is a tidier version of what already exists (tier already hands out bigger gear budgets today). The rework makes the same behaviour formula-driven and extends it to skills.

Two scaling concepts stay **orthogonal** and must not be merged:

- **Tier (power)** — how strong this thing is. Drives the new uniform multiplier. Lives on the existing `EItemTier` field.
- **EScalingTier (stat conversion, "Souls grade")** — how hard a skill leans on the *caster's* stats. Already implemented. Untouched by this work. Multiplies on top.

Model: `Final = (Base × TierPower) × StatScaling(EScalingTier) × envelope(crit/infusion/grid)`

Tier scales the authored base (first stage). Stat scaling converts the caster's stats (second stage). They multiply, never compete.

---

## 2. Skills (spells / abilities)

**Uniform tier multiplier on the authored base.**

Author a skill once — e.g. Fireball at 20 damage / 10 status / 5 EP cost. Tier multiplies all of it by one factor. S-Fireball = the same authored asset with the tier dial turned up.

- One multiplier scales **everything on the skill except effects** (damage, status, cost).
- "Uniform" = ratios never change. S-Fireball is F-Fireball, proportionally bigger. Same damage-per-cost, same damage-per-status. Tier is a volume knob, not a design lever — correct for the demo and for an upgrade path.
- **Upgrade path:** raising a skill's tier *is* the upgrade. One uniform multiplier keeps it honest — a stronger spell costs proportionally more.
- Folds onto the existing `EItemTier` field. One helper: `GetTierPowerMultiplier(tier)`. No new enum.

**No EP-cost ceiling problem.** Earlier concern retracted — the EP pool is large relative to costs (base 50, mid ~250, late ~460; F-cost ~5, S-cost ~38). Cost can ride the same curve as damage. EP does not bound the multiplier in any realistic range.

---

## 3. Gear

**Tier scales the *value of each point*, not the count.**

Today: higher tier = more stat points (F=6 → S=45 substat budget).
New: budget fixed (~20) at every tier; an S-tier point *converts to more* than an F-tier point.

Why this version (vs. scaling the budget, or scaling the final block):
- **No clamp problem.** Stored field values stay in range (a +18 roll is still +18, under the ±21 cap). The cap never has to move, because the stored number doesn't grow — only its conversion rate does.
- **No budget inflation.** ~20 points at every tier. Zero-sum broken-stick distribution untouched. Cursed-gear (signed/negative rolls) intact — and *intended* to scale harder at higher tier (bigger upside AND bigger downside).

Mechanism: one tier factor multiplies the per-point conversion rate. Distribution and caps unchanged.

---

## 4. Per-point reference (what +21 does today)

Base rate shared by most fields: `STAT_MULT_PER_POINT ≈ 0.003606`/point → ×21 ≈ **+7.57%**.
(Derivation: cap 0.5 / `STAT_DERIVE_DENOM` 138.57, where 138.57 = MAX_STAT_POINTS 93 × WORLD_MAX_MULT 1.49.)

There are **14** substat fields (not 13). Note: there is **no BonusCritChance** — the field is `BonusCritDamage`. Crit *chance* comes only from Luck.

| Field | Effect at +21 | Pattern |
|---|---|---|
| BonusRawDamage | +7.57% raw-damage mult | multiplicative |
| BonusSpellDamage | +7.57% spell-damage mult | multiplicative |
| BonusEfficiency | ×1.0757 on EP-cost reduction | multiplicative |
| BonusStatusMultiplier | +7.57% status-mult factor | multiplicative |
| BonusCritDamage | +7.57% crit-damage mult (NOT chance) | multiplicative |
| BonusSpellSpeed | +7.57% cast speed | multiplicative |
| BonusDefense | ×1.0757 on defense reduction | multiplicative |
| BonusActionSpeed | +7.57% action/anim speed | multiplicative |
| BonusReflex | +7.57% reflex-window factor | multiplicative |
| BonusLuck | +7.57% normalized luck | multiplicative |
| BonusResistance | +0.0757 (≈ +7.57 pts) added | additive, uncapped |
| BonusMaxHP | +21 HP **flat** | flat (no rate) |
| BonusMaxEnergy | +21 EP **flat** | flat (no rate) |
| BonusTurnSpeed | **+75.7%** turn speed | 10× rate (see §5) |

11 of 14 fields are uniform multiplicative. Three are special cases — resolved in §5.

---

## 5. The three special cases — resolved

**TurnSpeed — LEAVE THE RATE. It is load-bearing, not a bug.**

The 10× per-point rate (0.036 vs 0.0036) is correct. Turn order runs on **integer** speed values in a narrow [10,20] band. The scheduler is a debt accumulator: `SpeedRatio = CachedSpeed / SlowestSpeed`, and the actor with the highest accrued debt acts next. If TurnSpeed used the generic 0.0036 rate, every build would collapse into CachedSpeed {10,11}, all ratios ≈1.0, and turn order would degenerate into a tiebreak-decided round-robin — the stat would go inert.

The earlier "outlier" flag was **wrong**: TurnSpeed is an initiative value on a base-10 scale, not a 1.0-centred damage multiplier. The rates aren't comparable. The 20-cap also absorbs the nominal +75.7% down to a realised 15→20 (+33%).

→ Action: **do not normalise the rate.** Any tier/budget curve must treat one TurnSpeed point as worth ~10× a generic point and budget/price it accordingly. (Do not normalise without simultaneously re-scaling the scheduler — out of scope.)

**MaxHP / MaxEnergy — GIVE THEM A REAL PER-POINT RATE.**

These are the genuine inconsistency. Gear adds them **flat** (+1 point = +1 HP), while the *stat* path uses 6.487 HP / 6.849 EP per point — so gear pool-points are ~7× weaker than stat pool-points for no defended reason.

→ Action: give gear a real rate at ~45–50% of the stat path (gear should help, not replace stat investment):
- **BonusMaxHP: 3.0 / point** → +21 gear = +63 HP
- **BonusMaxEnergy: 3.5 / point** → +21 gear = +73.5 EP

(3.0/3.5 keeps HP and EP proportionally even since the EP pool runs slightly larger. Round numbers, retune in PIE.)

---

## 6. What is NOT in scope

The census mapped ~90–100 distinct scaling sites across 10 axes. Most are already fine and stay where they are:

- **Stat scaling (the `GetEffective*()` getters)** — already one unified pipeline. World-level and transient buffs feed through the same getters. **This is already the resolver for stats.** Nothing to build.
- **Gear generation (tier→budget→distribute)** — self-contained, clean, fires once at gear-gen. Only the per-point-value change above touches it.
- **Situational modifiers** (grid, durability/wear, crystal effect tables, status decay, defense timing, weather, Broken Darkness) — belong at their call sites. Not swept into a skill/gear resolver.

The only genuine scatter worth consolidating later is the **cast-time assembly layer** (tier-gap, infusion charge, status-buildup all scale *outside* `DamageCalculator`). Out of scope for this pass — noted for a future "resolver facade" refactor (wrap, don't rewrite).

---

## 7. Bugs gating documentation & build

Both surfaced by the scaling census, **both unconfirmed.** Confirm/fix before finalising docs, or the docs enshrine the bugs.

**Bug 1 — two infusion damage systems, different numbers.**
- Live path: `ActionExecutor::GetChargeDamageMultiplier` → 1.15 / 1.30 (L1/L2).
- Second path: `DamageCalculator::GetInfusionDamageMultiplier` → 1.3 / 1.6. NOT wired into main `CalculateDamage`; called only by Broken Darkness and AI estimation.
- Consequence: AI estimates infused damage at 1.3/1.6 but execution applies 1.15/1.30 → AI mispredicts every infused action. Silent decision bug, not a crash.

**Bug 2 — grid scaling may be dead.**
- One survey says grid position multiplies damage in `DamageCalculator` (front/mid/back rows); another says position is cosmetic, zero scaling.
- Likely defaults to ×1.0 (present-but-inert). Confirm with a direct read of `FCombatGridPosition::GetDamageModifier` before trusting either count.

---

## 8. Decision log

| Decision | Status |
|---|---|
| Tier = power axis, folds onto `EItemTier` | LOCKED |
| EScalingTier (stat grade) stays separate, untouched | LOCKED |
| Skills: uniform tier multiplier on authored base, effects excluded | LOCKED |
| Skill uniform scaling = volume knob (ratios fixed), doubles as upgrade path | LOCKED |
| EP-cost ceiling concern | RETRACTED (pool is large enough) |
| Gear: tier scales point *value*, ~20 fixed budget | LOCKED |
| Gear cursed/negative rolls scale harder at higher tier | LOCKED |
| 11 multiplicative fields scale uniformly | LOCKED |
| TurnSpeed rate stays (load-bearing), budgeted as 10× | LOCKED |
| MaxHP per-point rate = 3.0 | LOCKED |
| MaxEnergy per-point rate = 3.5 | LOCKED |
| Bug 1 (double infusion) confirm/fix | OPEN |
| Bug 2 (grid dead?) confirm | OPEN |
| Resolver-facade refactor of cast-time assembly layer | DEFERRED |
| TurnSpeed gear repricing/rarity (worth-10×) | DEFERRED |

---

## 9. Next steps

1. Confirm/fix Bug 1 and Bug 2 (§7).
2. Finalise this doc into `docs/mechanics/scaling/` once bugs are resolved.
3. Scope the build: `GetTierPowerMultiplier` helper + skill base application; gear per-point-value tier scaling + HP/EP rate fix.
