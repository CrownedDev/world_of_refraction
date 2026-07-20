# docs/ INDEX

Generated 2026-07-20 · 137 files (134 `.md` + 3 `.json`)

**This index is the entry point** — load it, not the whole `docs/` tree. Open an
individual doc only when its row shows it's the one you need.

Legend — **Bucket**: DEC decision/architecture · ANL audit/reference · BKL
backlog · PLN plan · SES session · OTH other. **Status**: Active · Completed ·
Superseded · Snapshot · Untracked. **Updated** = last-commit `MM-DD` (all 2026).
**File** = name within the section's folder.

## Architecture/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| AISystem.md | DEC | Active | 06-19 | Enemy turn + defense decisions (UAIDecisionManager) |
| AugmentStoneSystem.md | DEC | Active | 07-11 | Mechanical-bonus attachments beside crystals |
| BrokenDarkness.md | DEC | Active | 07-20 | BD Caster variant: absorb-to-cast, strain, overload |
| CharacterDataSystem.md | DEC | Active | 07-20 | Stats/identity asset + runtime state component |
| CombatCharacter.md | DEC | Active | 07-20 | Base actor: native combat component stack |
| CombatOrchestrator.md | DEC | Active | 07-20 | Top-level encounter coordinator |
| CrystalWear.md | DEC | Active | 06-22 | Per-cast percent-of-max durability wear + break |
| CurrencySystem.md | DEC | Active | 06-24 | Per-owner wallet (currencies + essence) |
| DamageCalculator.md | DEC | Active | 06-25 | Central damage formula subsystem |
| DuplicationAudit.md | ANL | Snapshot | 06-19 | **Audit, misfiled:** project-wide duplication survey |
| EconomySystem.md | DEC | Active | 07-11 | Dismantle/merge/purchase/level orchestration |
| InfusionSystem.md | DEC | Active | 06-21 | Hold-to-charge L1/L2: element, cost, effects |
| ItemSystem.md | DEC | Active | 07-11 | Crystals: 10-gem/7-tier matrix + inventory |
| LoadoutSystem.md | DEC | Active | 07-03 | Equipped combat loadout vs owned inventory |
| MerchantShopSystem.md | DEC | Active | 07-11 | Hub merchant → shop → economy purchase |
| PartySystem.md | DEC | Active | 07-20 | Session-scoped player roster across levels |
| PerInstanceRollSystem.md | DEC | Active | 06-21 | Rolled stat/resist bonuses per instance (U0–U4) |
| ResistanceSystem.md | DEC | Active | 06-21 | Class/innate status-buildup resistance profiles |
| ScalingSystem.md | DEC | Active | 06-21 | Per-skill (stat,grade) damage scaling |
| SkillEffectSystem.md | DEC | Active | 06-23 | Buff/debuff/DOT/trigger authority |
| StatComposition.md | DEC | Active | 06-24 | Stat composition + crit/Luck model |
| StatusBuildupSystem.md | DEC | Active | 06-21 | Per-actor status bar; cap fires effect |
| TierOnInstance.md | DEC | Active | 06-24 | Tier is per-instance mutable, seeded from asset |
| TurnManager.md | DEC | Active | 06-11 | Debt-based speed-ratio turn order |
| UISystem.md | DEC | Active | 07-11 | Combat HUD: C++ bases + WBP subclasses |
| WeaponSystem.md | DEC | Active | 07-11 | Design-time weapon/ring data |
| WeatherSystem.md | DEC | Active | 05-20 | Combat sky by leader HP dominance |

## Design/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| AIArchitecture.md | DEC | Active | 07-19 | Enemy-AI layer split + difficulty (unbuilt) |
| AIExecutionParity_Design.md | DEC | Active | 06-12 | Make AI damage-prediction parity structural |
| CastLaunchOrigin.md | DEC | Active | 06-20 | Projectile spawn point per delivery |
| EncounterCompositionSystem.md | DEC | Active | 07-20 | Parties + encounters; Arc1 shipped, 2–3 locked |
| InstanceBasedRuntimeLayer_Design.md | DEC | Active | 07-11 | Master reference-vs-instance layer; absorbs Inventory |
| InventorySystem_Design.md | DEC | Superseded | 06-20 | Ownership layer — superseded by InstanceBasedRuntimeLayer |
| ItemProjectiles.md | DEC | Active | 06-20 | Speed-scaled + thrown defendable projectiles |
| LoadoutDifficultyModel.md | DEC | Active | 05-29 | Enemy difficulty via loadout choice |
| LootTierOverflow.md | DEC | Active | 07-19 | Drop-quality bump + S-tier overflow (unbuilt) |
| PartyMatchSetup.md | DEC | Active | 07-20 | Dynamic party spawn to teams/grid |
| RawModeExtension.md | DEC | Superseded | 06-25 | **Dead:** base bIsRawMode removed |
| Resources_Design.md | DEC | Active | 07-11 | Resource economy: currencies, essence, hub (locked) |
| SkillGenerationPipeline.md | DEC | Active | 06-17 | Claude-designed skill seeds → assets (future) |

## Design/Completed/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| AttackAbilityMerge.md | DEC | Active | 06-17 | Merge attack data into ability; steps 2–6 pending |
| BrokenDarkness_ReactiveDefense.md | DEC | Active | 06-18 | BD reactive-defense; rate shipped, split pending |
| BrokenDarknessStrainTrigger.md | DEC | Completed | 06-22 | Deterministic strain trigger; shipped, diverged |
| CombatEconomy_StatRedesign.md | DEC | Completed | 06-17 | Locked stat decoupling + economy targets |
| DurabilityWearPercentRework.md | DEC | Completed | 06-24 | Percent-of-max wear; shipped 06-22 |
| EquipmentSlotTierScaling.md | DEC | Completed | 06-20 | Per-tier slot curve for gear containers |
| FusedMontageAnimationModel.md | DEC | Active | 06-17 | Fused notify-driven montage/VFX; spike-gated |
| GenericSpellInherit.md | DEC | Completed | 06-21 | Generic=inherit, None=non-elemental; built |
| HybridAttacks.md | DEC | Active | 06-17 | Attack with both physical+elemental (parked) |
| InfusionChargeRework.md | DEC | Completed | 06-18 | Exclusive→progression charge; shipped |
| InnateSpellPoolBudget.md | DEC | Completed | 06-20 | Tier-weighted innate/BD spell budget; built |
| PhaseRunnerCombatRework.md | DEC | Active | 06-17 | Combat side of fused-montage arc; spike-gated |
| RealTimeDefenseRework.md | DEC | Active | 06-17 | Turn-start all-defenders per-hit defense; staged |
| RequirementGapScaling.md | DEC | Completed | 06-24 | Per-pillar requirement-gap scaling; shipped |
| StatDecouplingRework.md | DEC | Superseded | 06-17 | Pre-decision exploration; see CombatEconomy_StatRedesign |
| TargetType.md | DEC | Completed | 06-19 | Two-axis target model; core shipped, 3 deferred |
| TierGapConsolidation.md | DEC | Completed | 06-24 | Channel-mismatch tier-gap; shipped |
| TierPowerScaling.md | DEC | Completed | 06-22 | Tier as single power dial (F→S ≈4.8×) |
| TurnOrderArchitecture_OnlineMigration.md | DEC | Active | 06-17 | Banked: replicated turn queue for online |

## Mechanics/Archetypes/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| BrokenDarkness.md | DEC | Active | 06-22 | Player view: BD as a Darkness-Refractor state |
| Generic.md | DEC | Active | 06-22 | Player view: physical dual-wield fighter |
| Reality.md | DEC | Active | 06-22 | Player view: Reality Refractor, broadly resistant |
| Refractor.md | DEC | Active | 06-22 | Player view: innate spellcaster (code "Caster") |
| Resonator.md | DEC | Active | 06-22 | Player view: ring caster, flexible but fragile |

## Mechanics/Character/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Classes.md | DEC | Active | 06-22 | Three-classes entry-point overview |
| Leveling.md | DEC | Active | 06-24 | Pillar progression (growth loop unbuilt) |
| Stats.md | DEC | Active | 06-22 | Pillars + 14 substats |

## Mechanics/Combat/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Abilities.md | DEC | Active | 07-03 | Attack/ability/spell taxonomy, player view |
| CombatGrid.md | DEC | Active | 06-22 | 3×3 grid rows/columns, movement, modifiers |
| Defend.md | DEC | Active | 06-22 | Brace: +50% DefenseBuff one turn |
| DefenseResolution.md | DEC | Active | 06-22 | Per-impact defense resolver (shipped) |
| Luck.md | DEC | Active | 06-22 | Crit chance/damage + chance outcomes |
| ResourcePools.md | DEC | Active | 06-22 | HP/EP derived pools + action costs |
| Targeting.md | DEC | Active | 06-22 | Two-axis TargetType/Count + multi-hit |
| TurnOrder.md | DEC | Active | 06-22 | Turn strip; faster combatants act more |

## Mechanics/Economy/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Currency.md | DEC | Active | 06-24 | Currency glossary: what each buys |
| Dismantle.md | DEC | Active | 06-24 | Salvage item → essence by tier |
| Economy.md | DEC | Active | 07-11 | Earn→spend loop map (built, no UI) |
| Hubs.md | DEC | Active | 06-24 | Two-hub shops/services (unbuilt) |
| Merging.md | DEC | Active | 06-24 | Merge same-type crystals up a tier |
| Upgrading.md | DEC | Active | 06-24 | Item tier progression (up/down/dismantle) |

## Mechanics/Gear/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| DurabilityWear.md | DEC | Active | 06-22 | Percent-of-max durability loss + breaking |
| EquipmentSlots.md | DEC | Active | 06-22 | Skill slots per gear tier (1→6) |
| Inventory.md | DEC | Active | 06-22 | Ownership warehouse of owned items |
| Loadout.md | DEC | Active | 06-22 | Combat-ready set; slot-cost budget |
| PerInstanceRolls.md | DEC | Active | 06-22 | Fresh stat rolls per acquired instance |
| Quality.md | DEC | Active | 06-24 | Rolled F→S drop grade (no consumer) |
| RerollEconomy.md | DEC | Active | 06-22 | Fill pools, spend to reroll bonuses |
| Rings.md | DEC | Active | 07-11 | Resonator spell-carrier; crystal gives element |
| Socketing.md | DEC | Active | 06-22 | Attach crystal/fusion/evolution to slot |
| Weapons.md | DEC | Active | 07-11 | 11 weapon types; physical-type→status |

## Mechanics/Items/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| AugmentStones.md | DEC | Active | 06-23 | Mechanical stones; attach or temp-buff |
| Crystals.md | DEC | Active | 06-23 | Consumable elemental crystals, 7 tiers |
| EvolutionCrystals.md | DEC | Active | 06-24 | Primary-slot pillar modifiers + spells + forms |
| FusionStones.md | DEC | Active | 06-22 | Player-made stones from two attachables |
| README.md | OTH | Active | 06-22 | Items family index |

## Mechanics/Magic/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Elements.md | DEC | Active | 06-22 | 9-element roster + Generic/None tokens |
| GenericSpells.md | DEC | Active | 06-22 | Template spells inheriting source element |
| Infusion.md | DEC | Active | 06-22 | Hold-to-charge L0/L1/L2, player view |
| SpellPoolBudget.md | DEC | Active | 06-22 | Spell pools: count cap + tier budget |
| Spells.md | DEC | Active | 06-22 | Casting flow + shared targeting |
| SpellSchools.md | DEC | Active | 06-22 | Destruction/Enhancement/Restoration roles |
| SpellSources.md | DEC | Active | 06-22 | Cast origin routes post-cast consequence |

## Mechanics/Scaling/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| ActionStatModifiers.md | DEC | Active | 06-22 | Per-action substat bundle before damage |
| RequirementGap.md | DEC | Active | 06-22 | Per-pillar under/over-level scaling |
| TierGap.md | DEC | Active | 06-22 | Action-vs-channel tier gap, 4 dimensions |
| TierPower.md | DEC | Active | 06-22 | Own tier sets raw power (F→S ≈4.8×) |

## Mechanics/Status/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Resistance.md | DEC | Active | 06-22 | Status-buildup resistance (not damage) |
| SkillEffects.md | DEC | Active | 06-22 | Effect authoring: bundles, merge rules |
| StatusBuildup.md | DEC | Active | 06-22 | The bar: fill, cap→proc, decay |
| StatusEffects.md | DEC | Active | 06-22 | Status bar catalogue + per-proc effects |

## Mechanics/World/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| EnemyAI.md | DEC | Active | 06-22 | What AI difficulty feels like |
| Weather.md | DEC | Active | 06-22 | Sky shifts with leader element/HP |

## Mechanics/ (root)

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| PlayerGuide.md | DEC | Active | 06-22 | Skim-index of what a player does/sees |
| README.md | OTH | Active | 06-24 | Mechanics index: mechanic → class → doc |

## Gaps/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Backend_Completeness_Survey_2026-06-25.md | BKL | Snapshot | 06-25 | Read-only backend system inventory |
| IntegrationGaps.md | BKL | Superseded | 06-16 | Gap catalog (base; see 06-21 update) |
| IntegrationGaps_2026-06-21_Update.md | BKL | Snapshot | 06-22 | Gap re-verify + acquisition/shop findings |
| Roadmap.md | BKL | Active | 07-20 | Working queue of not-yet-built work |
| Triage_2026-05-29.md | BKL | Snapshot | 05-30 | Disposable planning lens over gaps |

## analysis/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Codebase_Analysis_Pass1_Map.md | ANL | Superseded | 05-09 | Pass-1 structural smell map |
| Codebase_Analysis_Pass1_StatusUpdate_2026-05-11.md | ANL | Superseded | 05-11 | Pass-1 findings re-verified vs code |
| Codebase_Analysis_Pass2_ApplyConsolidation.md | ANL | Snapshot | 05-14 | ApplyHit consolidation audit |
| Codebase_Technical_Reference_2026-05-11.md | ANL | Snapshot | 05-12 | System-by-system onboarding reference |

## sessions/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| _TEMPLATE.md | OTH | Active | 05-06 | Session-doc template (not state) |
| 2026-06-20.md | SES | Snapshot | 06-20 | Session: dynamic effects + defender triggers |
| Removal_Survey_StanceOverride_RawMode_2026-06-25.md | ANL | Snapshot | 06-25 | **Audit, not session:** stance/raw-mode removal footprint |

## Refactor/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| SourceFolderReorg_Phase2_Plan.md | PLN | Active | 05-28 | Source subfolder reorg plan (git-mv only) |

## Generation/Effects/

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| effects_batch.json | OTH | Untracked | — | Effect-batch generation data (uncommitted) |
| effects_batch2.json | OTH | Untracked | — | Effect-batch generation data (uncommitted) |
| effects_sample.json | OTH | Untracked | — | Effect-generation sample data (uncommitted) |

## root

| File | B | Status | Upd | Purpose |
|---|---|---|---|---|
| Architecture_2026-05-14.md | ANL | Superseded | 06-23 | Monolith snapshot; superseded by Architecture/* |
| Conceptual_Overview_2026-05-11.md | ANL | Superseded | 05-11 | Designer overview (see 05-14) |
| Conceptual_Overview_2026-05-14.md | ANL | Active | 05-15 | Design bible: systems in designer terms |
| HighLevel_Overview_2026-05-14.md | ANL | Snapshot | 05-15 | One-page pitch overview |
| PastDocumentation_Audit.md | ANL | Snapshot | 05-15 | Audit of deleted PastDocumentation/; refs dangling |
| TODO.md | BKL | Active | 06-23 | Living deferred/watch backlog |

## Supersession chains

Read the **newer**; older kept for history.

- `Conceptual_Overview_2026-05-14` ← `-05-11` — read **-14** (design bible).
- `Architecture/*` per-system ← `Architecture_2026-05-14` monolith — read **per-system**.
- `Gaps/IntegrationGaps_2026-06-21_Update` ← `Gaps/IntegrationGaps` — read **-21 update**.
- `analysis/…Pass2` ← `…Pass1_StatusUpdate` ← `…Pass1_Map` — read **Pass2**.
- `Design/InstanceBasedRuntimeLayer_Design` ← `Design/InventorySystem_Design` — read **InstanceBased**.
- `Design/Completed/CombatEconomy_StatRedesign` ← `…/StatDecouplingRework` — read **CombatEconomy**.

## Load-bearing (in-repo source of truth — do not move/rename)

**Named in `CLAUDE.md` (16):** `Architecture/` AISystem, AugmentStoneSystem,
BrokenDarkness, CharacterDataSystem, CombatOrchestrator, DamageCalculator,
InfusionSystem, ItemSystem, LoadoutSystem, ScalingSystem, SkillEffectSystem,
StatusBuildupSystem, TurnManager, UISystem, WeaponSystem, WeatherSystem.

**Top link hubs** (most relative links break if moved): `Items/Crystals`,
`Gear/DurabilityWear`, `Architecture/EconomySystem`, `Economy/Upgrading`,
`Status/StatusEffects`, `Architecture/BrokenDarkness`.

## Oversized (>50KB — costly to open mid-session)

| File | Size |
|---|---|
| Design/Resources_Design.md | 182 KB |
| Architecture_2026-05-14.md | 70 KB |
| Gaps/IntegrationGaps.md | 70 KB |
| Design/Completed/RealTimeDefenseRework.md | 67 KB |
| Architecture/BrokenDarkness.md | 66 KB |
| analysis/Codebase_Analysis_Pass1_Map.md | 61 KB |
| Architecture/AugmentStoneSystem.md | 55 KB |
| analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md | 51 KB |

## Count check

137 = DEC 112 + ANL 11 + BKL 6 + OTH 6 + SES 1 + PLN 1. Matches `find docs -type f`. ✓
