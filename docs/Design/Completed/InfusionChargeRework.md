# Infusion Charge Rework — Exclusive → Progression (Completed)

> **RETIRED PLANNING DOC.** This was the original planning note for the charge rework. The work **shipped**
> across stages 6-1..6-5 (git history `4c10d293`..`04a783d1`), and the delivered design + numbers **differ
> from this note in places** — notably the per-mode `EInfusionMode` system (Physical/Status/Balanced) and the
> shipped per-mode multipliers, which this note predates. **Do not treat the values below as current.** For the
> authoritative current design see [`docs/Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md).
> Kept for historical context only.

**Status:** COMPLETED — shipped on `feature/realtime-defense` (6-1..6-5). Originally a "live systems change"
(edited the charge multiplier getters, wired the then-dead status-charge bonus into the live path, retired a
stub); the as-built design lives in `InfusionSystem.md`.

---

## The change

Rework the infusion **charge levels** from the current **EXCLUSIVE** model (each level boosts *either* status
*or* damage) to a **PROGRESSION** where **both** status and damage scale at each level, L2 > L1.

### Current model (to be replaced) — mutually exclusive

| Charge | Status bonus | Damage bonus | Role |
|---|---|---|---|
| L0 | ×1.0 | ×1.0 | no charge |
| **L1** | **×1.25** (+25%) | ×1.0 (none) | the "status charge" |
| **L2** | ×1.0 → **0.0** (none) | **×1.30** (+30%) | the "damage charge" |

Encoded in `InfusionConstants.h` (`CHARGE_L1_STATUS_MULT = 1.25`, `CHARGE_L2_DAMAGE_MULT = 1.30`, commented
*"Exclusive bonuses - L1 OR L2, not both"*) and the getters `GetAbilityChargeStatusMultiplier` /
`GetAbilityChargeDamageMultiplier` (`ActionExecutor.cpp:4562-4586`) plus the spell equivalents
`GetSpellChargeStatusMultiplier` / `GetSpellChargeDamageMultiplier` (`:4536-4559`). The status getter returns
a literal `0.0f` at L2 (`:4569` *"L2 gets NO status"*).

> ⚠️ **The status charge is currently DEAD.** `ExecuteAbilityAsync:1376` computes
> `StatusMultiplier = GetAbilityChargeStatusMultiplier(level)` (×1.25 at L1) and passes it **only** into the
> no-op `ApplyAbilityInfusionStatus` stub (`:1379` → `:4603-4636`), which just `UE_LOG`s *"Would apply…"* and
> applies nothing. The authored buildup (`Ability->StatusBuildup`) flows separately through `ApplyOneImpact`
> at its **raw, un-scaled** value. So today an L1-charged ability builds the *same* status as an L0-infused
> one — the +25% is thrown away.
>
> The **damage charge is LIVE**, however: `GetAbilityChargeDamageMultiplier` is applied at
> `ExecuteAbilityAsync:1338` (`FinalDamage *= DamageMultiplier`) and the spell equivalent at
> `ExecuteSpellAsync:1092`. Only the *status* side is dead.

### New model — a progression (both scale, L2 > L1)

| Charge | Status bonus | Damage bonus |
|---|---|---|
| L0 | ×1.00 | ×1.00 |
| **L1** | **×1.10** (+10%) | **×1.05** (+5%) |
| **L2** | **×1.20** (+20%) | **×1.10** (+10%) |

- **Applies to abilities + attacks** (the physical / `RawDamage` actions — one type post-merge) **AND spells.**
  The spell charge getters today carry a parallel exclusive table (same `1.25` / `1.30` values); they get the
  **identical** new progression. *(Confirmed the spell getters mirror the ability ones; assume the same table
  unless Crown diverges them.)*
- **Element and raw infusion sources get the SAME charge bonuses** — no flat bonus, no raw-only difference.
  The **only** element-vs-raw distinction stays what it is today: an **element** source **converts** the
  status to the infused element (`ExecuteAbilityAsync:1407-1423` — `AbilityElement = UserData->InnateElement`
  when `bIsInfused`); a **raw** source does **not** convert (it stays raw). The `%` charge bonuses are
  identical for both source types.

---

## What the build requires (when done)

1. **Update the multiplier values.** `InfusionConstants.h` + the four getters → the new progression
   (L1: status ×1.10, dmg ×1.05; L2: status ×1.20, dmg ×1.10; L0 ×1.0/×1.0). This also **fixes the old
   `L2 → 0.0` status problem** — L2 now returns a real `×1.20`, so there's no "can't naively multiply"
   caveat anymore; the status mult is a clean scalar at every level.

2. **Wire the status charge into the live path (currently dead).** Scale the authored buildup by the charge
   status multiplier:
   `AbilityBaseBuildup = Ability->StatusBuildup × GetAbilityChargeStatusMultiplier(level)`
   (element already converted upstream). Today this multiplier is discarded in the stub — it must **actually
   apply** to the authored buildup that flows through `ApplyOneImpact`.

3. **Confirm / keep the damage charge wired.** The damage charge is **already live** (`ExecuteAbilityAsync:1338`,
   `ExecuteSpellAsync:1092`) — it just needs the new values. No new wiring there; only the constants change.

4. **Retire the `ApplyAbilityInfusionStatus` stub** (`:4603-4636`). Its fabricated `10 * HitCount` amount is
   **not** the model — the charge bonus is *the authored buildup scaled by the charge mult* (step 2), not a
   separate invented channel. Delete the stub and the `:1376-1381` call block; fold its intent into the
   scaled-authored-buildup path. (Matches the Pass-2 consolidation recommendation —
   `docs/analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md:308`: *"becomes part of the unified buildup
   branch."*)

5. **⚠️ Per-impact interaction.** If abilities are **per-impact** by the time this lands (the
   attack/ability-merge thread), the charge-scaled buildup flows through `ApplyOneImpact` per-impact — each
   hit's share × the charge mult — which is consistent (the multiplier rides the same applier whether lumped
   or per-impact). Both this rework and the per-impact-abilities change touch the **ability status path**, so
   sequence them: ideally land per-impact first (so the scaled buildup distributes correctly), or land this
   first against the lumped path (the scaled buildup still applies at impact-0 of the lumped tail) — either
   order works, but don't develop them blind to each other.

---

## Dependencies / sequencing

- Build **after** the attack/ability merge (so "abilities + attacks" is one type and the getters apply
  uniformly), and coordinate with the per-impact-abilities change (shared status path — see step 5).
- **PIE-verify:** an L1 charge boosts both status (+10%) and damage (+5%) by the new percentages; an L2 charge
  boosts both more (+20% status, +10% damage); L0 unchanged. Confirm the **status** boost is now observable
  (it is dead today) and the **damage** boost reflects the new lower values (it was +30% at L2, now +10%).

## Status

PARKED, to incorporate after the merge. Captures the new progression table, the fact that the L1 status charge
is currently dead (computed then discarded in the stub), and that the fix is *scale the authored buildup +
retire the stub* — not implement the stub's separate `10*HitCount` channel.
