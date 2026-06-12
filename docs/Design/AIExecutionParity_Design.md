# AI ↔ Execution Damage Parity — Design Note

**Status:** Design / not-yet-implemented (except the tier-gap accessor introduced by Cluster D).
**Scope:** Why the AI's damage *prediction* drifts from the player's *real* damage, and how to
make parity structural instead of hand-maintained.
**Companion:** `docs/Architecture/AISystem.md` (estimator behaviour), `DamageCalculator.md`
(the shared calc), `FusedMontageAnimationModel.md` §7.5 D9 (tier-gap, the multiplier that
triggered this note).

---

## 1. The principle

The AI should value an action at exactly what that action will *do* when executed. A combatant
deciding between two spells must score each by the damage it would actually deal — including every
multiplier the real cast applies. When the prediction and the execution disagree, enemies make
decisions against fiction: over-valuing an action that will be penalized, under-valuing one that
will be boosted.

**The goal is not "build a clever estimator." It is "make the AI run the player's damage path."**
Every quantity the AI re-derives by hand is a quantity that can silently drift.

---

## 2. What already shares (the good news)

The AI is **already designed for parity** and gets most of the way there. `EstimateSpellDamage` /
`EstimateAbilityDamage` (`AIDecisionManager.cpp`) are explicitly labelled *execution-accurate* and:

- Build a **real `FAction`** per candidate — `ActionType`, `SpellData`, and crucially `SpellSource`
  via the *same* `ULoadoutComponent::ResolveSpellSource` the execution path uses. So provenance
  (ring / weapon-crystal / evolution / innate) is resolved identically.
- Call the *same* `UActionExecutor::ComputeActionStatModifiers` — the AI sees the real
  Reality/Evolution stat-modifier walk, not a copy.
- Route through the *same* `UDamageCalculator::CalculateDamage` — attacker stats, `ActionMods`, and
  defender defense are applied by the shared calculator, once, exactly as in execution.

This is the right spine. The divergence is not "the AI reimplements damage" — it is narrower and
more specific.

---

## 3. Where it diverges — the post-calculator multiplier pattern

`UDamageCalculator::CalculateDamage` is **not** the last word on damage. Several multipliers are
applied *after* it returns, at the assembly site in `ActionExecutor`, outside the calculator:

| Multiplier | Where applied (real path) | How the AI gets it today |
| --- | --- | --- |
| Expected crit | calculator runs `bCanCrit=false`; crit folded in at assembly | AI **re-folds by hand** (`× (1 + CritChance × (CRIT_MULT − 1))`) |
| L2 charge | assembly (`× CHARGE_L2_DAMAGE_MULT`) | AI **re-applies by hand** |
| **Tier-gap (D9)** | assembly (`base × charge × tierGap`) | AI must **re-apply by hand** → this is Cluster D |

The shape is identical every time: a multiplier lives at the assembly point, so the shared
calculator never sees it, so the AI estimator hand-copies it. **Each such multiplier is a standing
parity bug** — it stays correct only as long as someone remembers to mirror it in two places. The
`PastDocumentation_Audit.md` "AIDecisionManager Audit + Extend" entry already records this class of
drift (preview wrappers omitting `ActionMods`, wrong status-buildup callsite) as an open problem.

Tier-gap is simply the newest instance. We are about to hand-mirror a *third* multiplier — which is
the signal to ask whether hand-mirroring is the right model at all.

---

## 4. Two structural fixes (and the cheap interim)

### Interim (what Cluster D does now) — shared accessor, mirrored application

Introduce a single public, non-logging `UActionExecutor::GetTierGapDamageMultiplier(Actor, Action)`.
The execution-path logger and the AI estimator both call it, so the *value* has one source of
truth even though it is *applied* in two places. This is the minimum that closes the tier-gap drift
without moving verified code.

It does **not** solve the general pattern — crit and L2 are still hand-mirrored, and the next
assembly-time multiplier will need the same treatment. It is correct, low-risk, and a stepping
stone, not the destination.

### Fix A — pull post-calculator multipliers into the shared calculation

Move crit / L2 / tier-gap (and any future assembly-time scalar) **into a single shared
"finalize" step** that both the real path and the AI path call. Two ways to shape it:

1. **Into `DamageCalculator`** — give `FDamageCalculationInput` the fields it lacks (it carries
   `ActionType`/`Element`/`InfusionLevel`/`SelectedSource` but **not** `SpellData` or `SpellSource`,
   so it cannot resolve tier-gap or provenance-dependent scalars today). Adding them lets the
   calculator own every multiplier. Cost: new input fields + moving the verified assembly
   application into the calculator + full real-damage PIE re-verification. This is why Cluster D
   chose *not* to do it inline — but it is the clean long-term home.
2. **A shared `FinalizeDamage(Actor, Action, baseResult)` on `ActionExecutor`** — a single function
   that takes the calculator's result and applies crit + L2 + tier-gap in one ordered place. The
   real assembly sites call it; the AI estimators call it. The calculator stays lean; the executor
   owns "the multipliers that depend on the full action." Lower-risk than (1) because it doesn't
   re-plumb `FDamageCalculationInput`, and it co-locates the multiplier order-of-operations that is
   currently smeared across assembly sites.

**Recommendation:** Fix A via **shape (2)** — a shared `FinalizeDamage` on the executor. It makes
parity structural (the AI cannot diverge because it runs the same finalize), it gives the
order-of-operations one authoritative location, and it absorbs every future assembly-time
multiplier for free. Shape (1) is the "purest" home but pays a re-plumb + re-verify cost that
shape (2) avoids.

### Fix B — the AI submits candidates through a dry-run of the real executor

The deepest version: the AI doesn't estimate at all — it asks the executor to *simulate* the action
and report the damage it would deal, through the exact execution path (no second code path to keep
in sync). This is the natural fit for the project's **determinism invariant** (combat resolution is
already meant to be isolatable/re-simulatable for future server validation). A `SimulateAction`
that runs the real pipeline minus side-effects (no HP applied, no wear, no animation) would make
estimation and execution *the same code* by construction.

Fix B is larger and couples to the phase-runner combat rework. It is the right end-state if the
determinism/re-sim work lands anyway; until then Fix A captures most of the benefit at a fraction
of the cost.

---

## 5. Recommended path

1. **Now (Cluster D):** ship the shared `GetTierGapDamageMultiplier` accessor; mirror it in the
   estimators. Closes the immediate tier-gap drift. *(In flight.)*
2. **Next (Fix A, shape 2):** introduce `FinalizeDamage(Actor, Action, baseResult)` on the executor;
   fold crit, L2, and tier-gap into it; have both the real assembly sites and the AI estimators call
   it. Delete the three hand-mirrored multipliers from the estimators. **This is the unification
   the AI was always reaching for** — after it, a new assembly-time multiplier is added in exactly
   one place and the AI sees it automatically.
3. **Eventually (Fix B):** if/when the determinism re-sim work lands, replace estimation with a
   side-effect-free `SimulateAction` so prediction *is* execution. Retire `FinalizeDamage`'s
   double-call in favour of one path.

---

## 6. Guardrails (whichever fix)

- **Wear stays separate.** None of this touches the durability system — it shares only tier
  comparisons, by prior decision.
- **Regression guard.** Any move of an applied multiplier must leave matched-tier / no-crit /
  L0 cases byte-identical to today. The existing tier-gap `× 1.0` identity proof is the template.
- **One order-of-operations.** The value of Fix A is a *single* place that defines
  `base × charge × tierGap × …`. Don't reintroduce per-site ordering.
- **Attacks.** Today attack/ability tier == weapon tier → tier-gap is provably `× 1.0`, and the AI's
  inline `CalculateAttackDamage` sites build no `FAction`. Leave them until abilities/attacks gain
  independent tiers; the future-proofing one-liner in `EstimateAbilityDamage` self-heals when they do.
