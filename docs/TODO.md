# TODO / Backlog

Deliberately-deferred and watch-later items. One line each + a status tag:
**WATCH** (verify in PIE) · **BLOCKED** (needs a prerequisite) · **POSSIBLE** (do only
if PIE/usage shows the need) · **CLEANUP** (remove after verification) · **DONE**.
Living backlog — keep entries short; promote to a real doc/issue when worked.

## Done — per-instance roll arc (2026-06)

- **DONE** — Gear resistance system: rings, weapons, and evolutions granting status-buildup resistance (element + physical), authored-effect + rolled `FResistanceBonus` paths, term #6 in `GetTotalStatusResistance`. See `Architecture/ResistanceSystem.md`. **Open balance knob:** `RESISTANCE_CATEGORY_CAP` (PIE-tune).
- **DONE** — Per-instance roll system U0–U4: toggle + preview-inert assets, loadout instance bridge, weapon/ring roll at acquisition, evolution rolled stats (U3, incl. read migration), Base-only `CreateFrom*` enforcement. See `Architecture/PerInstanceRollSystem.md`.
- **DONE** — Evolution infusion roll-drop fix: `ResolveInfusionAttachment`'s `Evolution` case whole-struct-copies the attachment, so rolled ints reach standalone-slot infusion (was Item + durability only).
- **DONE** — Legacy cleanup: anon-namespace `AccumulateBonus`/`AccumulateResistance` replaced by the struct `Accumulate` members; `WITH_EDITOR` guard fix so the legacy pillar→`BaseStatBonus` migration runs in packaged builds; crystal assets re-saved.
- **DONE** — `[AI Emerald]` + `[BONUSDIAG]` diagnostic logs stripped (verified zero occurrences in Source).
- **DONE** — Infusion lethal-at-finalize documented (`CombatOrchestrator.md` → *Infusion HP cost — lethal, paid at finalize*).

## Small / unblocked

- **CLEANUP** — Delete the legacy pillar fields (`Mind/Body/SpiritModifierPercent` on `UEvolutionItemData`) + their `PostLoad` copy block. Safe post-re-save — but first confirm the re-saved `.uasset`s are actually committed (none visible in git as of 2026-06-11; `DA_Test_EvoCrystal_Water` last committed pre-migration). Quick future commit.
- **WATCH** — Double-reference effect-ID hazard: the future equip UI must forbid referencing the same owned instance in TWO slots of one loadout (shared `int32 InstanceID` effect-ID window, `ID*100+i` → apply/remove collisions). Cross-loadout sharing is safe (one active at a time). Enforce in the equip UI when built.
- **POSSIBLE** — Preview-side `GetCombined*` helpers on `UEquipmentDataBase`: kept deliberately (no gameplay caller); revisit/delete only if no preview-display tooling materializes.
- **CLEANUP** — `SpellData.h` + `CastableSkillDataBase.h` embed `FWorldStatRequirements` with the same Requirements▸Requirements double-header collapsed on `UEquipmentDataBase`; same one-line `ShowOnlyInnerProperties` fix available if wanted.

## System arcs (build on the shipped per-instance roll)

- **BLOCKED** — Player equip UI: the runtime flow that binds a loadout slot to an owned instance (sets the `FSavedLoadout` instance-ref FGuids). The U1 bridge is built and inert until this exists — it's what makes per-instance rolls carry into combat. See `Architecture/PerInstanceRollSystem.md`.
- **BLOCKED** — Reroll economy: Pool charging (rewards fill `Stat/ResistancePool`), the reroll trigger (unlock at Pool==MaxPool, spend to 0, re-roll from the stored MaxPool), and its UI. Storage shipped (U0); economy not built.
- **BLOCKED** — Cross-session persistence of instance rolls: the owned pool is rebuilt from the asset every spawn (GUIDs re-mint, toggle-ON gear re-rolls). Needs the save system before per-instance rolls (and instance refs) survive a session. SaveGame tags already in place on the persistent fields.

## Pitch demo (north star)

- Polished combat encounter — the vertical-slice fight that shows the 9-element / 3-class system at its best.
- Publisher materials — pitch deck + demo build.
- Stylized artist recruitment.
- Funding targets.

## Combat — lethality & infusion

- **WATCH** — Corpse-walk-back: after a lethal infusion, the dead caster may visibly slide back to position (`SignalActionComplete` plays return-movement on a flag-dead pawn). If janky in PIE → gate return-movement on `bIsAlive`. Not yet observed; confirm next PIE.

## Emerald-AI

- **BLOCKED** — Self-target Emerald-AI: wired but DORMANT (`ESTIMATED_EP_REGEN_PER_TURN = 0`). Activate by setting it >0 **only** once a passive per-turn EP-regen mechanic exists. Cross-ref `AISystem.md`.
- **POSSIBLE** — Enemy all-target Emerald scan: AI evaluates Emerald only on `BestTarget`. If PIE shows missed one-tick-lethal kills on non-selected targets, add an all-enemy scan.
- **POSSIBLE** — Loosen the one-tick-lethal Emerald gate: currently requires next-tick ≥ HP (guaranteed kill). If too conservative in PIE, consider an accumulated-over-exposure-window check.

## Refactor — banked

- **POSSIBLE** — 5 Group-B attachment-accessor variants (banked from the accessor migration).
- **POSSIBLE** — `StatusMultiplier` base-extract: only if base composition grows beyond ~3 terms (currently keep-both).

## feature/evolution-pillar-turnspeed (NEXT FEATURE, after this branch merges)

- **BLOCKED** — Job 1: innate (primary-slot) evolution flat TurnSpeed feeds turn order. Gate: `PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item` — NOT `GetActivePrimaryEvolutionCrystal` (that resolves the ATTACHED-to-primary-weapon case, the opposite). Two-branch precedent: `ApplyEvolutionPillarModifier` (CharacterDataComponent.cpp:482).
- **BLOCKED** — Job 2: `CalculateTurnSpeed` should read pillar-MODIFIED Spirit (`GetEvolutionModifiedSpirit` family) instead of raw `GetEffectiveSpirit`, so gear Spirit% (weapon/ring/evolution) feeds turn speed like every other Spirit-stat. BALANCE CHANGE — activates the whole equipment stack at once.
- **BLOCKED** — Infusion carve-out: make "TurnSpeed never infuses" EXPLICIT — drop TurnSpeed from `MapToInfusionModifiers` (EvolutionItemData.cpp:246) + comment the exclusion at mapping and struct. Today it's only unenforced-by-missing-reader; a generic re-wire would pull it in.
- Known asymmetry (deliberate per design): the turn-speed primary-only gate is stricter than pillar-modifier/resistance, which allow both primary AND attached evolutions.

## feature/fusion-wear-pipeline

- **BLOCKED** — Elemental fusions never wear in production: `ProcessPostCastWear` early-returns on `!IsCrystal()` (CrystalManager.cpp:47), so `FFusionAttachment::ApplyWear`/whole-fusion durability is dispatch-reachable but unrouted (only `DebugForceWearActiveCrystal` can break one). Building it needs: fusion tier source for the wear math (gem half), fusion-aware `FBrokenCrystalPayload`, and a speed notify on break gated on `GetAttachedStonePercent(..., TurnSpeed) > 0` evaluated pre-break (read returns 0 post-break since the IsBroken guard landed). `OnCrystalBroken(Actor, ...)` is the clean hook — Actor is in the signature.
- **CLEANUP** — `GetAttachedStonePercentForType` (pool stones, CrystalEffectTable.h:614) fusion branch needs the same `IsBroken` guard as `GetAttachedStonePercent` ("broken = dead stat" for MaxHP/MaxEP halves too). Sibling flagged during Cluster D, deliberately not expanded mid-cluster.

## feature/always-active-effects

- **BLOCKED** — `GetAlwaysActiveEffects()` has zero callers (weapon `UEquipmentDataBase` + evolution `UEvolutionItemData`) — dormant/unbuilt. Wire when the always-active passive system is built; speed-type effects will then notify for free via the `IsSpeedEffect` apply/remove gates.
