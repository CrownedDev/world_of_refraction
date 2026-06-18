# Skill Generation Pipeline — Claude-Designed Seed → Generated Assets (Future)

**Status:** FUTURE / not scheduled. A **separate system from the Attack/Ability merge** — build it
**AFTER** the merge (the merge's steps 5–6). On `feature/realtime-defense`. Captured so it's not lost.

A design note, not a spec — just enough context for a future session to pick this up. **No code yet.**

## The goal

Use **Claude as the content-design engine**: have Claude design a *range* of skills
(attacks / abilities / spells) with predicted, balanced values, then **generate starter assets** from those
definitions that Crown tunes in-editor. A generator bridges **definitions → assets**. This removes
from-scratch authoring drudgery while keeping final tuning in the editor.

## The pipeline (SEED model — not a permanent source)

1. **Claude designs skills.** Given an element / family / role, Claude produces balanced skill definitions —
   damage, `HitCount`, `DamageSplit`, status type + amount, difficulty tiers, cost, delivery, the hybrid
   flags, etc. — with **predicted values** *and* **balance relationships across the set** (e.g. a fire
   family balanced relative to each other, element coverage, damage/status tradeoffs).
2. **Definitions → a SEED.** Those values land in a seed: a `DataTable` or a C++ struct array holding the
   rows.
3. **A GENERATOR reads the seed and CREATES assets.** An editor utility / commandlet reads the seed and
   programmatically creates `UAbilityData` `.uasset` files, pre-populated with the balanced values and
   `bIsAttack` set appropriately.
4. **SEED model — one-time baseline, NOT permanent source.** Once generated, the assets are **normal editor
   assets that Crown tunes LIVE in-editor**. The seed is a *starting point*, not the system of record. This
   sidesteps the recompile/regenerate-to-retune problem while removing the from-scratch authoring work.
   Re-running the generator creates new assets — and **could optionally skip existing assets so it never
   clobbers tuned ones**.
5. **Crown authors VISUALS per-asset** (montage, VFX, icon). The mechanical values arrive pre-filled from
   the seed.

## ⚠️ The visual/mechanical seam

"Code owns mechanics, editor owns visuals" has a **seam at per-hit timing**: montages need **Impact notifies
matching `HitCount`** (per the per-impact work), and **VFX ties to cast entries**. The generator fills the
mechanics (including `HitCount`), but **Crown's visual pass must align montage notifies to the generated
`HitCount`**. The seam is unavoidable — note it so the visual pass accounts for it rather than discovering it
late.

## Interaction with the Attack/Ability merge

The generator produces **`UAbilityData` assets** — the *post-merge* type. So it could **simplify the merge's
step 5**:

- Instead of **reparenting** the existing `UWeaponAttackData` attack assets (LFS-risky; reparenting has
  corrupted `.uasset` files in this project before), **DELETE them and GENERATE fresh `UAbilityData` attack
  assets from a seed** with `bIsAttack = true`.
- **Regenerate-instead-of-reparent sidesteps the reparent corruption risk.** Consider folding the merge's
  step 5 into the generator's first use.

**Decision deferred:** decide this *when the generator is built*. For now the merge's step 5 proceeds as
planned (reparent) **unless Crown redirects**.

## Scope / status

- A **separate system** from the merge. Build **after** the merge.
- The generator is an **editor utility** — a **commandlet** *or* an **Editor Utility Widget/Blueprint** —
  doing UE-specific programmatic creation of `UPrimaryDataAsset` instances (`UEditorAssetLibrary` / asset
  tools).
- Sized when built.

## Design questions for later (when building)

- **Seed format:** `DataTable` vs C++ struct-array.
- **Generator form:** commandlet vs Editor Utility Widget/Blueprint.
- **Naming & placement:** how the generator names and locates the generated assets.
- **Skip-existing:** the don't-clobber-tuned-assets behavior on re-run.
- **Getting Claude's designs INTO the seed:** Claude outputs the struct/table rows; Crown pastes/imports
  them.
