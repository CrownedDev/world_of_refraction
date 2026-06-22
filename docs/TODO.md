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
- **DONE** — feature/evolution-pillar-turnspeed: innate evolution flat stats feed `GetActiveStatBonus` (pillar % zeroed); turn speed reads pillar-modified Spirit (`CalculateTurnSpeedWithSpirit` + `GetEvolutionModifiedSpirit`); SpiritBuff/Debuff notify; TurnSpeed dropped from all three infusion writers. See `Architecture/TurnManager.md` changelog. | 2026-06-11
- **DONE** — feature/fusion-wear-pipeline: elemental fusions wear + break in production (gem-half tier keys wear; augmented never wears); break fires speed-notify + `RecomputeMaxPools` at the break instant; `FBrokenCrystalPayload.FusionId`; RingManager + debug commands fusion-aware. See `Architecture/CrystalWear.md` → *Fusion wear*. | 2026-06-11

## Done — stat redesign + combat economy arc (2026-06)

- **DONE** — Combat economy + stat redesign (LOCKED): Path A decouple, universal +50% stat cap / gear-beyond, world +7%/level, HP/EP→1000, no EP regen, skill-tier base power. Built to designed targets, clustered. Own balance session. See `docs/Design/CombatEconomy_StatRedesign.md` (supersedes `StatDecouplingRework.md`). **Shipped** — clusters 1–5g: `UNIVERSAL_STAT_CAP` + `WORLD_{MIND,BODY}_SCALING_BONUS=0.07` (`CombatConstants.h:40-41,134`), Pattern-P getters in `CharacterDataComponent`. See `Architecture/StatComposition.md`. | 2026-06-16

## Effect-expansion arc (2026-06)

- **DONE** — Charges + Last Stand + Sapphire reshape + Healing Stone + AI. Shipped clusters: C1 (`Charges` field + `ConsumeCharge`, independent charge-expiry removal), C2a (charge-aware + value-driven death intercept), Revive→LastStand rename (+ `EnumRedirects` Revive→LastStand), C2b (Sapphire = defy-death: Last Stand on a living target / revive on a dead one, tier-scaled window), C2c (Healing Stone consume-only instant heal, any target), AI-1 (heal-detection → `RestoreHealth`, Sapphire → `DefyDeath` effect-type), AI-2 (AI self-wards with Last Stand). | 2026-06-22

### Cooperative AI — own arc (the AI's first ally-targeting)

- **BLOCKED** — Ally-targeting: AI heals/wards an **ally** (not just self). The AI is self-survival-only today — no ally enumeration in `AIDecisionManager`. First ally logic; build on `CombatOrchestrator::GetLivingAllies`.
- **BLOCKED** — Revive-AI: AI uses Sapphire on a **dead** ally. Needs a new `GetDeadAllies()` in `CombatOrchestrator` (dead allies are filtered out of every existing getter via `IsActorAlive`) + confirm `ActionExecutor` accepts a dead item-target. The demo-risk piece — own PIE pass.

### Parked — this arc

- **CLEANUP** — Rename `UItemExecutor::ExecuteHealingEffect` → a defy-death-accurate name (misnomer: it's Last Stand / revive now, not a heal). Deferred to avoid touching the dispatch site mid-arc.
- **CLEANUP** — `CrystalType.h` inline enum comments drifted (`=12`/`=26` off by 1–2 from the real sequential values). Trust position, not the comment numbers; fix when next editing the enum.
- **POSSIBLE** — `+healing-received` as a real effect (cut from the Healing Stone, which went consume-only). Direction: "stones can grant **effects**, not just substats" — a recipient-side `HealingReceivedBuff` (the **mirror of HealBlock**: HealBlock blocks, this amplifies), read in `ServerHeal`. Needs the attach→effect-production path attached stat-stones lack today.

### Parked — prior arcs (surfaced during this one)

- **WATCH** — DOT retune: Burn/Chill now hit **on apply** (~33% more total) since the fire-on-apply default — wants a balance pass.
- **CLEANUP** — `FActiveSkillEffect::CreateFromSpellEffect` (~:245) duration-0→1 clamp is out of sync with the new duration model (0 = instant). Reconcile.
- **BLOCKED** — C4 spell-heal routing: direct `ApplyHealing` spell heals still **bypass HealBlock** (only item/effect heals route through the gate). Route spell heals through the gate.

### Queued arcs (post effect-expansion)

- **BLOCKED** — BD-states-as-effects: Elemental Charge (absorption stacks) + Overload as real effects. **HIGH risk** (demo-critical BD; ~8–10 readers, overload is derived state) — deferred from this arc's survey. Own arc + PIE pass.
- **POSSIBLE** — OnDeath actual-death effects: real revive / death-rattle on **actual** death (`UCharacterDataComponent::OnDied` broadcast is the hook). Distinct from Last Stand, which pre-empts death.
- **DONE** — TierGapConsolidation: four-dimension tier-gap (damage + status + effect magnitude + reciprocal cost) built + PIE-verified + matched-tier regression-clean; **merged to main 2026-06-22**. See `Design/Completed/TierGapConsolidation.md`. | 2026-06-22
- **BUILT — pending merge** — RequirementGapScaling: per-pillar requirement-gap substat scaling (±5 ladder) replaces the √deficit penalty; built + PIE-verified on `feature/requirement-gap-scaling`, **not yet merged to main**. See `Design/Completed/RequirementGapScaling.md`.
- **NEXT (active target)** — Original design queue advances to **DurabilityWearPercentRework** → then BrokenDarknessStrainTrigger, ItemProjectiles.

## Small / unblocked

- **CLEANUP** — Remove `UDamageCalculator::CalculateAttackDamage` — dead code, no live callers after RequirementGapScaling Cluster 5 (AI attack scoring routes through `EstimateAbilityDamage`); retirement note in source. Delete once confirmed unreferenced.
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

## Banked — future systems

- **BLOCKED** — Party/Match Setup system (spawn party → assign teams → assign grid → hand to orchestrator) — planned, core to solo/co-op/PvP. See `docs/Design/PartyMatchSetup.md`. Build after reactive-defense (Stage 3+).

## Refactor — banked

- **POSSIBLE** — 5 Group-B attachment-accessor variants (banked from the accessor migration).
- **POSSIBLE** — `StatusMultiplier` base-extract: only if base composition grows beyond ~3 terms (currently keep-both).

## Fusion BonusStat dropdown drift (system-wide, NOT Reflex-specific)

- **POSSIBLE** — `GetRestrictedFusionBonusStats` / the "IsWired six" (RawDamage, Defense, CritDamage, TurnSpeed, StatusMultiplier, Efficiency) has drifted from `StoneTargetStat`, which now returns TWELVE non-None stats. Six stats — SpellDamage, Resistance, SpellSpeed, ActionSpeed, Luck, Reflex — are locked out of the fusion BonusStat (averaged-bonus) dropdown. Their fusion-HALF path works; only the explicit BonusStat target excludes them. The comment at `EquipmentDataBase.cpp:142-144` ("exactly the non-None outputs") now misdescribes the set. DECISION (future, batch): should the newer six be fusion-BonusStat-targetable? If yes, add all six together + fix the comment. Not a Reflex one-off.

## feature/starting-effects (in flight)

- **WATCH** — Clusters 1+2 shipped and PIE-verified (filter to non-conditional, coverage mirrors `GetActiveStatBonus`, AlwaysActive→StartingEffect rename, `WOR_StartingEffects` debug Exec, caller-less appliers deleted). Promote to DONE at merge.
