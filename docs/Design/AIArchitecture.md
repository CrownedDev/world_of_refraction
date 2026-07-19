# AI Architecture

**Status:** [Design — not built]. Captures the layer split for enemy AI and the difficulty
resolution model. Nothing here enters the build queue until the five locked design-queue arcs
(TierGapConsolidation, RequirementGapScaling, DurabilityWearPercentRework,
BrokenDarknessStrainTrigger, ItemProjectiles) are cleared.

Owning code today: `UAIDecisionManager`, `EAIDifficulty`, `AIDecisionConstants`.
Current spec: [`AISystem.md`](../Architecture/AISystem.md).

---

## 1. The core split

Today `UAIDecisionManager` decides **everything** — which options exist, and which one to take.
Both live as hardcoded control flow (`BuildAction_Smart` → `TrySurvivalBranch` →
`TryCleanseBranch` → `BuildOffensiveAction`).

The split:

- **Code = capability.** Enumerate what the character *can* do (derived from its loadout) and
  score each candidate. Pure maths. No decisions.
- **Data = policy.** Decide which candidates are allowed right now, and how their scores are read.

This is the standard capability/policy seam. The capability layer is not authored — it is
generated from the loadout, because the loadout *is* the action set.

---

## 2. Four layers

| Layer | What it is | Asset type | Count | Churn |
|---|---|---|---|---|
| **Archetype** | Who the enemy is — CharacterData, loadout, mesh, anims | `UPrimaryDataAsset` | One per enemy kind | Rare |
| **Overworld policy** | How it acts in the world — patrol, notice, chase, disengage | Behavior Tree | A few, shared | Rare |
| **Combat policy** | How it fights — rank/score rules | `UPrimaryDataAsset` | A few, shared | Often |
| **Difficulty template** | How sharp it is — stat multipliers, think delay, decision quality, defence reflex | `UPrimaryDataAsset` | ~4 total | Often |

Combat policies and overworld BTs are **shared, not per-enemy**. Most enemies use a default
"fight sensibly" policy. Bespoke policies are authored only for signature behaviour — a healer
that hangs back, a berserker that never defends.

A spawned enemy = **Archetype + Difficulty template**.

---

## 3. Why BT for overworld, data asset for combat

Different problems, different tools. They share nothing; the BT's job ends when combat starts.

**Overworld → Behavior Tree.** Real-time, ongoing, actions run across many frames. Enemies are
pawns with AIControllers and Blackboards already, so the plumbing is free. This is what BTs are
built for.

**Combat → data asset.** One decision, resolved instantly, then done.

| | Behavior Tree | Data asset |
|---|---|---|
| Fits turn-based | Poorly — latent/running-node machinery unused | Yes |
| Required plumbing | AIController + Blackboard (`UAIDecisionManager` is a subsystem — has neither) | None |
| Logic visibility | Visual graph, but custom node logic hides in C++ anyway | Flat list in Details panel |
| Debugging | Built-in visual debugger (shows a tree ticking — little to watch when resolution is instant) | Hand-written |
| Fits project conventions | New pattern | Same as SpellData / RingData |

Decision: **data asset for combat.** The BT would supply a graph the turn-based case doesn't need,
at the cost of plumbing that doesn't exist.

---

## 4. Combat policy — dual-utility

Every candidate action gets **two** numbers.

- **Rank** — integer priority band. Hard. Survival candidates sit in a high band; routine
  offence sits in the normal band.
- **Score** — utility within that band. Soft.

Resolution: take the highest band that has any candidate in it, then the best score inside it.
One pass. Hard rules and soft preference in the same mechanism.

Reference: Kevin Dill, *Dual-Utility Reasoning* (Game AI Pro 2). Related prior art — Bill Merrill,
*Building Utility Decisions into Your Existing Behavior Tree* (Game AI Pro, ch. 10);
Project Borealis IAUS (utility inside UE's BT); `kamrann/BTUtilityPlugin`.

### Why bands beat the current branch ordering

The survival → cleanse → offence priority is currently encoded in the *order the functions are
called*. Changing it means changing code. As bands, it is a number in an asset.

### What survives, what goes

**Goes** (policy hardcoded as control flow):
`BuildAction_Smart`, `TrySurvivalBranch`, `TryCleanseBranch`, `ChooseActionType`, and the
`switch (ChosenType)` action-construction block.

**Stays** (the valuable part — scoring and estimation):
`ScoreTarget`, `SelectBestTarget`, `EstimateBestDamage`, `EstimateSpellDamage`,
`EstimateAbilityDamage`, `EstimateStatusScore`, `CanAffordSpell`, `CanAffordAbility`,
`CalculateThreatLevel`, `CanKillTarget`. These become considerations the policy reads.

`ExecuteDecision` stays as-is — it is the submit boundary and belongs there.

### Hard wall — the tree never executes

Policy selects a candidate `FAction`. It never applies effects, never touches `ActionExecutor`,
never mutates state. Leaf/candidate builders are `const`; that is the enforcement.

This preserves the existing guarantee that AI estimates are execution-accurate: estimates route
through the real `UDamageCalculator` with real `FActionStatModifiers`, and the submitted `FAction`
travels the identical player pipeline. A second execution path would destroy that.

### Defence stays reactive C++

`TrySynthesizeImpactDefense` is per-impact and latency-sensitive. Wrong place for asset
indirection. Unchanged by this arc.

---

## 5. Difficulty resolution

**Difficulty is an overlay, not a variant.**

The trap: authoring `Bandit_Easy` / `Bandit_Medium` / `Bandit_Hard` / `Bandit_Expert`. Four
archetypes × four difficulties = sixteen assets, and every balance tweak touches all of them.

Instead: **one Bandit.** A difficulty template is applied on spawn.

Template holds:
- Stat multipliers (HP, damage)
- Think delay range
- Decision quality (how often the best candidate is taken vs a merely good one)
- Defence reaction quality

Most of this exists already — `EAIDifficulty` covers think delay, decision quality and defence
reflex today. This moves it from a switch statement into an asset and adds stat scaling.

### Base + modifier

- **Base difficulty comes from the archetype.** Uniques carry their own floor — a Lord is
  dangerous wherever it appears.
- **Zone applies a modifier on top**, it does not replace.

Replacement would flatten both cases. Base-plus-modifier keeps a Lord threatening in an easy zone
and scales a common Bandit up in a hard one.

---

## 6. Zone scaling — deferred

Model: a zone raises **difficulty and reward together**. One lever, both effects — the
Nightreign-church shape. That coupling is deliberate design intent and must not later be split
into two unrelated knobs.

The reward half is specified in [`LootTierOverflow.md`](./LootTierOverflow.md).

**Dependency:** zone difficulty scaling touches loot tiers, which sits adjacent to
**TierGapConsolidation** in the design queue. Do not tune zone modifier numbers until that arc
lands — otherwise the tuning targets a moving baseline.

---

## 7. Build order

1. **Combat policy asset** — replaces the existing branch logic. The only step touching existing code.
2. **Difficulty template** — extracts `EAIDifficulty` into an asset, adds stat multipliers.
3. **Archetype asset** — ties archetype + policy + difficulty together.
4. **Overworld BT** — last. Needs the world to exist first.

Steps 2–4 are additive.

**Scope check before step 4:** how much overworld AI does the pitch demo actually need? Enemies
that stand still in a trial room until entered are a trigger volume, not a BT. Only patrol-and-chase
justifies the BT plumbing.

---

## 8. Open questions

- Ranks: fixed enum of bands, or free integers?
- Does the offensive pass score action × target as a matrix, or keep `SelectBestTarget` as a
  separate prior step? The matrix is more correct (a cheap AoE across three enemies can beat a
  large single-target hit) but is a larger change to candidate generation.
- Debug tooling: the policy asset needs a "why did it pick this?" dump — per-candidate rank and
  score — or it will be untestable without PIE. Required per project convention.

---

## Related

- [`../Architecture/AISystem.md`](../Architecture/AISystem.md) — current built spec
- [`../Mechanics/World/EnemyAI.md`](../Mechanics/World/EnemyAI.md) — player-facing behaviour
- [`LootTierOverflow.md`](./LootTierOverflow.md) — the reward half of zone scaling
