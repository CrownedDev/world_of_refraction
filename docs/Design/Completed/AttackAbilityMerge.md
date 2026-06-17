# Attack / Ability Merge

**Status:** ACTIVE — decisions LOCKED (Crown). Staged on `feature/realtime-defense`. Step 1 done & committed
(`3721a684`); steps 2–6 are the merge proper. A design note + plan, not a full spec.

## The idea

Merge `UWeaponAttackData` (basic attacks) into `UAbilityData` (the surviving type). They're mechanically
near-identical: both **physical**, both use **`DamageSplit`** (per-hit damage), both scale **`RawDamage`**,
both derive **status from the weapon's damage type**. The distinction is **categorical** (basic attack vs.
special ability), not mechanical.

## Why

- **Less duplication** — one data type + one execution path instead of two parallel ones
  (`ExecuteAttackAsync` / `ExecuteAbilityAsync`, two dispatch branches, two consumer sets, etc.).
- The **weapon-damage-type → status** linkage gets built **once**, not per-type.
- Consistent handling across dispatch, defense, AI, and authoring.

## LOCKED decisions (Crown)

- **Surviving type:** merge `UWeaponAttackData` **INTO** `UAbilityData`.
- **`bIsAttack` flag** (per-asset, on the merged type) = "this is a basic attack, can't be slotted as an
  ability." The **only preserved distinction.** Default `false` (abilities are common); basic attacks author
  `true`.
- **`EActionType::Attack` collapses into `Ability`** via CoreRedirect in `DefaultEngine.ini` (the project's
  `+ValueChanges` form). `bIsAttack` carries the distinction; `ActionType` becomes `{Ability, Spell, ...}`.
  Scaling-safe — both `Attack` and `Ability` already route to `RawDamage`.
- **Uniform cost:** use `UAbilityData`'s cost field; author `0` for free basic attacks.
- **Abilities become per-impact:** DONE (step 1). `bUseCountBasedClose` includes `Ability`; graceful
  degradation (no Impact notifies → unchanged-lumped; authored notifies → per-impact). Content pass is
  incremental.
- **Asset migration = HARD REPARENT.** Crown confirmed few attack assets → low risk, and **overrides the
  CLAUDE.md "never reparent .uasset" rule for this merge specifically.** Reparent attack assets to
  `UAbilityData` with `bIsAttack=true`. ⚠️ **TAG before reparent** (rollback point); **verify each asset
  post-reparent**; reparent only **after** the merged type exists + compiles.

## Staged order (commit + tag between stages)

1. ✅ **Per-impact abilities** — DONE, committed `3721a684`. Defense-model unification; de-risks the rest.
2. **Data-type merge:** `UWeaponAttackData`'s unique fields → `UAbilityData`; add `bIsAttack` (default
   `false`); confirm the cost field (author `0` for attacks); weapon→attack linkage references the merged
   type. Both types still coexist (`UWeaponAttackData` not yet removed).
3. **`EActionType::Attack` → `Ability` CoreRedirect.** `bUseCountBasedClose` simplifies to
   not-Spell/`bPerImpact` (no longer lists `Attack`); every other `Attack`-branch → `bIsAttack` or unified.
4. **`FAction` field collapse:** the two pointers (`AbilityData` + `AttackData`) → one merged-type pointer
   (touches every `Action.AttackData`/`Action.AbilityData` site); consumers (loadout/slotting, AI scoring,
   UI, `DamageCalculator` typed entry, debug tools) updated to the merged type + `bIsAttack` where they
   branched on Attack-vs-Ability.
5. **HARD REPARENT** the (few) attack assets → `UAbilityData` with `bIsAttack=true`. **TAG FIRST.** Verify
   each.
6. **Remove `UWeaponAttackData`** (now-empty old type) + its debug tooling.

## Risk ranking

1. **`FAction` collapse + consumers** — many sites; mechanical but broad.
2. **`EActionType` redirect** — serialized; verify saved actions/assets re-resolve.
3. **Hard reparent** — low (few assets + tag + verify).
4. **Per-impact content** (notifies) — incremental, done as Crown authors them.

## Notes

- **Independently shippable:** step 1 (done). Steps 2–6 are the merge proper, staged.
- **Related parked work:** the infusion-charge rework (`docs/Design/InfusionChargeRework.md`) touches the same
  ability status path — sequence it **after** the merge.

## Changelog

- 2026-06-17 (`feature/realtime-defense`) — Locked decisions recorded; step 1 (per-impact abilities) committed
  `3721a684`. Doc moved from PARKED → ACTIVE/staged.
