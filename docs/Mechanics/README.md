# Mechanics Index

Skim-sheet for every gameplay mechanic in the module, grouped as in the mechanics survey.
**Source of truth is the shipped `.cpp/.h`** — these docs describe behaviour, they do not drive it.
Each line: *mechanic — one-clause behaviour — owning class/file — → dedicated doc (if any)*.

A blank link means no dedicated doc exists yet (small mechanic, folded into a neighbouring doc).

---

## A. Combat Core

- **Per-impact defense windows** — buffer timestamped inputs, match-and-consume one entry per impact, resolve each slice independently — `UDefenseSystem` (`Combat/Defense/DefenseSystem.cpp`) — → [`DefenseResolution.md`](./Combat/DefenseResolution.md)
- **Turn scheduling** — per-round speed-debt scheduler with pinned bonus/execution turns and a 16-deep preview belt — `UTurnManager` (`Combat/TurnManager.cpp`) — → [`../Architecture/TurnManager.md`](../Architecture/TurnManager.md)
- **Resource pools (HP/EP)** — HP clamp[0,Max], EP per-action with BD overflow buffer; pools recomputed from pillars+gear+status — `UCharacterDataComponent` (`Character/CharacterDataComponent.h`) — → [`ResourcePools.md`](./Combat/ResourcePools.md)
- **Combat grid / positioning** — 3×3 per team, row gives passive ±5% dmg/def, movement is by-row within a column — `UCombatGridSubsystem` (`Combat/Grid/CombatGridSubsystem.cpp`) — → [`CombatGrid.md`](./Combat/CombatGrid.md)
- **Action execution + stat modifiers** — single validate→pay→route pipeline; per-action substat stack assembled outside the damage calc — `UActionExecutor` (`Combat/Actions/ActionExecutor.cpp`) — → [`ActionStatModifiers.md`](./Scaling/ActionStatModifiers.md), [`../Architecture/CombatOrchestrator.md`](../Architecture/CombatOrchestrator.md)
- **Deferred activation (Execution turn)** — `ActivationDelay>0` skills arm (pay once), fire free on a pinned Execution turn — `UActionExecutor::TryArmDeferredActivation` / `ACombatOrchestrator::FireScheduledExecution` — → [`../Architecture/CombatOrchestrator.md`](../Architecture/CombatOrchestrator.md) *(un-exercised — no shipping asset sets the delay)*

## B. Damage Pipeline & Durability

- **Damage calculation** — multiplicative pipeline (stat→action mods→gear→scaling→stones→grid→status→crit→defense→min-1) — `UDamageCalculator::CalculateDamage` (`Combat/Damage/DamageCalculator.cpp`) — → [`../Architecture/DamageCalculator.md`](../Architecture/DamageCalculator.md)
- **Tier gap** — action-vs-channel fit boosts/penalises across damage/status/effect/cost — `TierGapDamage` (`Combat/Damage/TierGapConstants.h`) — → [`TierGap.md`](./Scaling/TierGap.md)
- **Tier power** — own-tier F..S power curve (×1.00…×4.80) — `TierPowerScaling` (`Combat/Damage/TierPowerConstants.h`) — → [`TierPower.md`](./Scaling/TierPower.md)
- **Stat scaling fractions** — per-skill `FStatScaling` grade S–F × normalised stat — `UDamageCalculator::CalculateDamage` step 1 (`EScalingTier.h`) — → [`../Architecture/ScalingSystem.md`](../Architecture/ScalingSystem.md)
- **Requirement gap** — per-pillar world-level vs skill requirement, single-counted into the substat stack — `USkillDataBase::GetRequirementGapPillarPercents` — → [`RequirementGap.md`](./Scaling/RequirementGap.md)
- **Damage split** — author-side per-hit % allocation across a multi-hit skill — `USkillDataBase::ResolveDamageSplit` —
- **Resistance** — class+innate-element+BD → 12-value row feeding status-buildup resistance — `ClassInnateResistanceTable` (`Combat/Resistance/`) — → [`../Architecture/ResistanceSystem.md`](../Architecture/ResistanceSystem.md)
- **Durability / wear** — deterministic crystal wear on cast (tier-mismatch+infusion, power-vs-control modulated), break at 0, +10/battle repair — `UBreakCalculator::CalculateDurabilityWear` (`Equipment/Durability/`) — → [`DurabilityWear.md`](./Gear/DurabilityWear.md), [`../Architecture/CrystalWear.md`](../Architecture/CrystalWear.md)

## C. Status Effects

- **Effect lifecycle** — apply/tick/expire on the affected actor's turn, with a re-apply strength gate — `USkillEffectManager::ApplyEffect` (`Skills/Effects/SkillEffectManager.cpp`) — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md), [`SkillEffects.md`](./Status/SkillEffects.md)
- **Charges** — fixed-fire-count governor independent of duration — `USkillEffectManager::ConsumeCharge` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **Wards / AbsorbDamage** — passive marker consumed in the damage path (no-op in `ApplyEffectLogic`) — `ESkillEffectType::AbsorbDamage` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **Last Stand** — intercepts a lethal blow, restores HP%, consumes a charge (best-of-N) — `USkillEffectManager::ConsumeLastStandCharge` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **DOTs** — end-of-own-turn tick via `ServerTakeDamage`, optional per-tick buildup — `ESkillEffectType::DOT` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **Instant vs durational vs permanent** — Duration-0 runs-and-discards; >0 stored & ticked; permanent removed only explicitly/charge-exhaust — `FActiveSkillEffect::IsInstant` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **Effect-strength policy** — STRONGER overwrites+refresh / EQUAL refresh / WEAKER ignored; strength = `Abs(EffectValue)` — `USkillEffectManager::ApplyEffect` — → [`../Architecture/SkillEffectSystem.md`](../Architecture/SkillEffectSystem.md)
- **Status buildup → proc** — bar fills per hit, fires an element/physical-typed effect at threshold, decays; resistance reduces intake — `UStatusBuildupManager::AddStatusBuildup` — → [`../Architecture/StatusBuildupSystem.md`](../Architecture/StatusBuildupSystem.md)

## D. Elements / Refraction / Classes

- **9-element system** — Fire/Water/Earth/Wind/Light/Darkness/Lightning/Void/Reality + Generic + None — `ESpellElement` (`Skills/Definitions/ESpellElement.h`) — → [`GenericSpells.md`](./Magic/GenericSpells.md)
- **Generic polymorphism** — a Generic spell adopts its source/pool element once at cast, threaded to damage/VFX/resistance — `UActionExecutor::ResolveSpellCastElement` — → [`GenericSpells.md`](./Magic/GenericSpells.md)
- **"Refraction"** — *not a unified system in code*: it is game branding + the Caster's UI display name "Refractor". Element interaction is distributed across `UDamageCalculator` (resistance), `UActionExecutor` (element lock), `HybridSpellColors`/`ElementColors` (VFX). No dedicated doc. —
- **Infusion** — hold-to-charge L0/L1/L2 modifies cost/damage/status/size; HP cost EP-derived & lethal at finalize — `UInfusionChargeManager` / `UActionExecutor` — → [`../Architecture/InfusionSystem.md`](../Architecture/InfusionSystem.md)
- **Character classes** — Generic / Caster ("Refractor") / Resonator; class (not InnateElement) gates magic — `ECharacterClass` + `CharacterClassHelpers` — → [`../Architecture/CharacterDataSystem.md`](../Architecture/CharacterDataSystem.md), [`Archetypes/`](./Archetypes/)
- **Spell sources** — 5 origins drive post-cast wear/break routing — `ESpellSource` (`Skills/Definitions/ESpellSource.h`) — → [`SpellSources.md`](./Magic/SpellSources.md)
- **Spell schools / pool budget** — 4 schools + per-school Caster spell-pool budget — `ESpellSchool` (`Skills/Definitions/SpellSchool.h`) — → [`SpellPoolBudget.md`](./Magic/SpellPoolBudget.md)

## E. Equipment / Crystal Economy

- **Crystals & socketing** — count-pool refined crystals socket onto holders with per-instance durability — `UCrystalManager` (`Equipment/Crystals/`) — → [`Items/Crystals.md`](./Items/Crystals.md), [`../Architecture/ItemSystem.md`](../Architecture/ItemSystem.md)
- **Evolutions** — instance-tracked evolution items (cap 5) with per-instance roll pools — `UEvolutionInventoryComponent` — → [`../Architecture/ItemSystem.md`](../Architecture/ItemSystem.md)
- **Augment stones** — stat/ability stone family, fuse in, never wear — `AugmentStoneConstants.h`, `EAttachedItemKind::AugmentStone` — → [`Items/AugmentStones.md`](./Items/AugmentStones.md), [`../Architecture/AugmentStoneSystem.md`](../Architecture/AugmentStoneSystem.md)
- **Fusions** — two-crystal pairs (gem+stone wears; stone+stone doesn't) — `FFusionAttachment` / `FFusionId` — → [`Items/FusionStones.md`](./Items/FusionStones.md)
- **Weapons** — wield modes Single/Dual/OffHandShield, merged attack/ability, preset abilities — `UWeaponData` — → [`../Architecture/WeaponSystem.md`](../Architecture/WeaponSystem.md), [`EquipmentSlots.md`](./Gear/EquipmentSlots.md)
- **Rings** — Resonator-primary spell-slot equipment, element via attachment — `URingData` — → [`../Architecture/WeaponSystem.md`](../Architecture/WeaponSystem.md)
- **Loadout** — per-character active gear (cap 5), validated vs inventory at battle start — `ULoadoutComponent` — → [`../Architecture/LoadoutSystem.md`](../Architecture/LoadoutSystem.md)
- **Inventory** — ownership warehouse, distinct from loadout — `UInventoryComponent` — → [`../Architecture/ItemSystem.md`](../Architecture/ItemSystem.md)
- **Equipment generation / rolls** — fixed substat budget + tier-scaled pillar budget, zero-sum broken-stick — `UEquipmentBonusGenerator` / `ZeroSumBrokenStick` — → [`../Architecture/PerInstanceRollSystem.md`](../Architecture/PerInstanceRollSystem.md), [`../Architecture/StatComposition.md`](../Architecture/StatComposition.md)

## F. World & AI

- **Broken Darkness** — Caster strain-break: no passive EP regen, fuel via parry/block absorption, stack-scaled status multiplier, overload — `UBrokenDarknessManager` — → [`../Architecture/BrokenDarkness.md`](../Architecture/BrokenDarkness.md), [`Archetypes/BrokenDarkness.md`](./Archetypes/BrokenDarkness.md)
- **Reality boost** — Reality innate/evolved/infusion substat amplification — `RealityBoost.h` (consumed in `ComputeActionStatModifiers`) — → [`Archetypes/Reality.md`](./Archetypes/Reality.md)
- **Weather** — weather asset swaps on per-team leadership dominance, broadcasts a blend value — `UWeatherStateManager` — → [`../Architecture/WeatherSystem.md`](../Architecture/WeatherSystem.md)
- **AI** — difficulty-tiered action scoring + per-impact defense synthesis + infusion HP-affordability guard — `UAIDecisionManager` — → [`../Architecture/AISystem.md`](../Architecture/AISystem.md)

---

## Legacy / non-mechanics

- **Stances** (`UStanceData`, `Character/StanceData.h`) — cosmetic only: idle-pose montage + icon, **no stat or gameplay effect**, no combat-relevant runtime consumer. Listed here so it is not mistaken for a live mechanic.
