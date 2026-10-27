# Stat maths redesign — plan (B-146)

**Job:** B-146 (review and plan, no code until Crown approves; Q-146). **Crown (2026-10-26):** most systems work but are bespoke and could be done better. **Builds on:** B-041 (attribute sets, `GASSystem.md`), B-044 (`EffectsOverhaul.md`), B-145 (`ResistanceRedesign.md`, resistance is planned there), `StatComposition.md`, `docs/Mechanics/Scaling/`. **Also folds in:** D-086 (Ignore Defense as an amount) and D-082 / B-162 (weapon requirements scale the weapon's output).

**How to read it:**
- Code is cited as `path:line` under `Source/world_of_refraction/`.
- "Standard" means the GAS shape Epic's Lyra and the GASDocumentation community guide use: values on **attributes**, every bonus or penalty a **modifier** (GameplayEffect), stat-derived values from a **Modifier Magnitude Calculation** (MMC), action-time maths in **Execution Calculations**, tuning numbers in **curve / data tables** (`FScalableFloat`).
- Questions: **Q-146, Q-146b, Q-146c** in `DesignDecisions.md`.

---

## 1. What stays the same

**The numbers.** Every system below keeps its formula, caps and order. The goal is the same game in a standard shape, so each step is proven with the existing compares and specs:

| Tool | Pins |
|---|---|
| `wor.CompareStats` (`Private/Debug/Character/StatModifierDebug.cpp:37`) | every composed stat per combatant |
| `wor.CompareAttributes` (`Private/Debug/GAS/WoRAttributeDebug.cpp:35`) | attributes vs the getters |
| `WoR.ScalingSnapshot`, `WoR.AbsorptionSnapshot`, `WoR.StrainSnapshot` | scaling, Broken Darkness |
| Specs: `WoR.Scaling`, `WoR.Damage`, `WoR.Stats.Modifiers`, `WoR.Infusion.Cost`, `WoR.Status.Buildup`, `WoR.Status.InfusedBuildup`, `WoR.AI.DamageEstimate`, `WoR.AI.BuildupEstimate`, `WoR.Items.Stones`, `WoR.GAS.Attributes` | the formulas |

The only intended changes are the two Crown already ruled: Ignore Defense becomes an amount (D-086) and weapon requirements scale the weapon (D-082).

## 2. Per system

### 2.1 Stat composition (Pattern P)

- **Today:** `UCharacterDataComponent` getters compose each stat: the stat-derived part capped **alone** (×1.5 or 0.5), then gear, stone and buffs multiply past it toward the gear ceiling (`StatComposition.md` §2–3). Live getters: `GetEvolutionModifiedMind/Body/Spirit` (`Private/Character/CharacterDataComponent.cpp:748-767`), `GetEffectiveSpellDamage` (`:1003`), `GetEffectiveActionSpeed` (`:1080`), `GetEffectiveEfficiencyMultiplier` (`:1273`), `GetEffectiveStatusMultiplier` (`:1348`), `RecomputeMaxPools` (`:776`). Raw and Spell Damage are capped a second way inside the damage step (`DamageCalculator.cpp:156-206`, a correction factor that divides out the uncapped product). Per-point values derive from `STAT_DERIVE_DENOM` (`Public/Combat/CombatConstants.h:137`).
- **Standard:** pillars are attributes (they are, B-041). Each sub-stat's **base** is an MMC: `min(pillar × points × per-point, stat cap)`, the "capped alone" part. Today each stat multiplies **three summed groups** on top: gear points `(1 + Σ)`, stones `(1 + Σ%)`, buffs `max(0, 1 + net%)` (e.g. Defense, `DamageCalculator.cpp:408-448`; crit sums each category first, `:485-493`). GAS can't express "sum within a group, multiply between groups, floor one group at 0" with plain modifiers: MultiplyAdditive sums *all* multiply modifiers into one group, and one MultiplyCompound per effect multiplies effects together. So each group is **one aggregated modifier**: the loadout applies one GE with the summed gear factor and one with the summed stone factor, and buffs feed one MMC-backed modifier that sums the active buffs and floors at 0 (or the whole stat is one MMC). The gear ceiling is a clamp in the attribute set's `PreAttributeChange`, as Lyra clamps Health. Pools (HP/EP) keep their exception, `(min(stat, 1000) + gear) × (1 + (stone% + buff%)/100)` (`CharacterDataComponent.cpp:830-831`): gear is **AddBase** (before the multiply), stone and buff one summed multiply.
- **What changes:** the `Calculate*` display twins go away: one value, read everywhere (the "twin trap" in `StatComposition.md` §3 can't happen any more). The damage step's **correction factor stays** inside the damage core (B-219): it multiplies `target / raw` onto the whole running damage (`DamageCalculator.cpp:156-206`), which also carries ActionMods (`:53`) and the per-skill scaling bonus (`:91-100`), so swapping it for one capped attribute would change numbers once a cap binds.
- **Risk:** the group order above (cap, then gear, stone, buffs). Proven by `wor.CompareStats` before any getter switches; same grouping rule as the resistance plan (B-145 §3.2).

### 2.2 Scaling: requirement gap, tier gap, tier power, per-skill scaling

- **Today:**
  - Requirement gap: a ±5 ladder (1.40 … 0.32) per pillar, `GetRequirementGapMultiplier` (`Public/Combat/Damage/TierGapConstants.h:136`), turned into ActionMods percents (`Private/Skills/Definitions/SkillDataBase.cpp:262-282`; consumed at `ActionExecutor.cpp:6987-6995`).
  - Tier gap: damage and cost ladders, clamped ±6 (`TierGapConstants.h:18-26, 34, 75-88`), applied in the damage preview, buildup and cost (`ActionExecutor.cpp:673, 1351, 1573, 412, 442`).
  - Tier power: `TIER_POWER` F–S 1.0 … 4.8 (`Public/Combat/Damage/TierPowerConstants.h:17`), on damage, buildup, EP cost and gear per-point.
  - Per-skill scaling: Σ grade coefficient × stat fraction, added inside the attacker multiplier (`DamageCalculator.cpp:91-100`; coefficients `Public/Skills/Definitions/EScalingTier.h:31`; five fraction buckets `:77`).
- **Standard:** each ladder is a **curve table** row read through `FScalableFloat` (Epic's standard for "a value by level"): `TierPower` by tier, `TierGapDamage` / `TierGapCost` / `RequirementGap` by gap, `ScalingGrade` by grade. The execution reads them; designers tune them without a rebuild.
- **What changes:** the numbers move from `constexpr` arrays to curve tables with the same points. The AI's estimate reads the same curves (no copy).
- **Risk:** curve interpolation must be **constant** (step) between integer keys, not linear. A spec checks every key against today's array.

### 2.3 ActionMods

- **Today:** `FActionStatModifiers` (`Public/Combat/Actions/ActionStatModifiers.h:49`): additive percents per sub-stat, built per action from the Reality boost, evolution infusion and the requirement gap (`ActionExecutor.cpp:6901-6995`), snapshot on the action context (`:1006`), applied as `stat × (1 + mod/100)` inside the damage maths (`DamageCalculator.cpp:53, 219`).
- **Standard:** a per-action **GameplayEffectSpec** carrying SetByCaller magnitudes (`SetByCaller.ActionMod.<Stat>`), read by the execution. That's how GAS passes "this action only" numbers.
- **What changes:** the struct becomes the spec's SetByCaller values; the build steps stay.
- **Risk:** low; the struct already is a snapshot.

### 2.4 Crit

- **Today:** chance = Luck-driven 5% → 50% (stat) → 100% (gear), then ×(1 + buff) ×(1 − debuff) ×(1 + modify), clamped [0, 1] (`DamageCalculator.cpp:456`; `CharacterDataComponent.cpp:844, 889, 927`). Damage = ×1.0 base, stat to ×1.5, gear to ×2.5 (D-034), clamped [1.0, 2.5] (`DamageCalculator.cpp:573`; `CombatConstants.h:186-193`). `GuaranteedCrit` forces it (`:229`).
- **Standard:** `CritChance` and `CritDamage` attributes (both exist, B-041) composed by 2.1's rule; the roll happens in the execution on the battle random stream (B-010). `GuaranteedCrit` becomes a tag the execution checks.
- **What changes:** crit chance's buff, debuff and modify factors are each a summed category, multiplied together (`DamageCalculator.cpp:485-493`): one aggregated modifier per category (2.1's rule).
- **Risk:** RNG order: the execution must draw at the same point as today, and draw **nothing** when `bCanCrit` is false or `GuaranteedCrit` is held (today's short-circuit, `DamageCalculator.cpp:212-231`), or seeded fights replay differently. `WoR.Combat.BattleRandom` pins it.

### 2.5 Defense and Ignore Defense (D-086)

- **Today:** reduction = min(stat, 0.5) × gear × stone × buffs, clamped [0, 0.85] (`DamageCalculator.cpp:408`; `CombatConstants.h:209`), applied as damage × (1 − reduction) (`:256-264`). Ignore Defense is all-or-nothing: `bIgnoreDefense` or an `IgnoreDefense` effect skips the step (`:246-255`).
- **Standard:** `Defense` attribute (exists) composed by 2.1; the execution applies it. Ignore Defense is an amount: **`IgnoreDefense` 0–100%** on the skill (and as a SetByCaller from an `IgnoreDefense` effect), reduction × (1 − ignore). Old all-or-nothing skills map to 100% (D-086).
- **What changes:** the rule (D-086). Which protection it ignores: Q-146b (the resistance half applies once B-064 makes resistance cut damage).
- **Risk:** low; 100% reproduces today.

### 2.6 Infusion cost and charge damage

- **Today:** EP = base × (charge 1.5 / 2.0 × (1 + stat fraction)) × Efficiency × tier power × tier-gap cost; an infused ability also × 1.5 (D-035) (`ActionExecutor.cpp:400-442, 4896`; `InfusionConstants.h:74-83`; `SkillDataBase.cpp:312`). HP backlash = EP / max EP × max HP × (1 − Resistance) (`Infusion/InfusionCostHelper.cpp:16-34`). Charge damage and status multipliers per mode (`ActionExecutor.cpp:4775, 4803`; `InfusionConstants.h:20-35`).
- **Standard:** cost is a **cost GameplayEffect** whose magnitude is an MMC (Epic's standard for ability costs, `UGameplayAbility::CostGameplayEffectClass`); charge bands are curve rows. The HP backlash is an Instant GE applied after the action lands.
- **What changes:** cost moves with the abilities (B-043). Numbers unchanged.
- **Risk:** the backlash reads the wear-twin Resistance (`GetEffectiveResistance`, uncapped, `CharacterDataComponent.cpp:1387`), not the buildup one; keep that exact value (B-145 keeps it as the `Resistance` attribute).

### 2.7 Status buildup (source side)

- **Today:** authored buildup × charge status × tier power × tier gap (`ActionExecutor.cpp:1342-1351, 1573`), then in `AddStatusBuildup` × StatusMultiplier (`StatusBuildupManager.cpp:396`), × element-keyed buffs (`:407-414`), × Broken Darkness stacks (`:430`), × (1 − resistance, B-145) (`:440`).
- **Standard:** the buildup execution (B-145 §3.4) with these source steps before the resistance step.
- **What changes:** nothing in the numbers.
- **Note:** when infused, the StatusMultiplier stat counts twice (inside the charge status multiplier's stat fraction, and at `:396`). This is intended (Q-113, `docs/Rules/03-status-bars.md`).

### 2.8 Broken Darkness strain and overload

- **Today:** strain = deficit × 3.2 × min(Spell Damage, 3.0) × max(1 − Defense, 0.333) (`Combat/Mechanics/BrokenDarknessManager.cpp:271`), threshold max EP × 2.0 (`:306`), Luck can skip accrual (`:317`); overload tick 15 × Spell Damage (`:760`); absorption (`:1095`). Constants in `BrokenDarknessManager.h:21-26` and the `.cpp` namespace (`:31-63`).
- **Standard:** **stays custom** (D-P01, parked: only after stage 3 with a zero-difference `wor.CompareBD`). It reads attributes instead of getters once they are the truth; its constants move to the tuning data (§3) like the rest.
- **What changes:** where it reads Spell Damage from. Strain reads the **stat-only** Defense (`BrokenDarknessManager.cpp:292`), not the composed one, so it keeps reading the stat-only value (an MMC output or the getter), not the `Defense` attribute. `RollForBreak` (`:219`) is kept on purpose as the rollback path (`BrokenDarkness.md`).

### 2.9 Weapon requirements scale the weapon (D-082, B-162)

- **Today:** the weapon's requirement only feeds Broken Darkness strain (`GetTotalDeficit`, `WorldStatRequirements.cpp:92`). The skill's requirement uses the ±5 gap ladder (2.2).
- **Standard:** the same ladder on the weapon's own contribution, built beside the skill's (`ActionExecutor.cpp:6987`) but **not** through ActionMods: `AddPillarPercent` (`Public/Combat/Actions/ActionStatModifiers.h:131`) spreads a percent to every sub-stat of the pillar (Efficiency, Defense, …) and adds percents with no floor (`ApplyTo`, `:188`), so a skill's and a weapon's −68% would sum below −100%. The weapon's gap is a **separate damage factor** on attacks and abilities through that weapon, `clamp(ladder, WEAPON_REQ_FACTOR_MIN, 1)`, multiplied in (the 1 is Q-146c; the floor a named constant, `// TODO: tune`: the ladder's lowest rung is 0.32, so it matters only if set above that). Being over the requirement: Q-146c.
- **What changes:** the rule (D-082). B-162 builds it.

## 3. Tuning numbers in data (Q-146)

Every ladder, cap and per-point value is a C++ `constexpr` today (`CombatConstants.h`, `TierGapConstants.h`, `TierPowerConstants.h`, `InfusionConstants.h`, `RealityBoost.h`, `EScalingTier.h`, `BrokenDarknessManager.h/.cpp`, `DamageCalculator.h:29-33`). Only per-skill and per-character values are on assets.

- **Standard:** ladders as **curve tables** (`FScalableFloat`), single tuning values (caps, per-point bases, BD rates) in one **`UCombatTuningData`** primary data asset (CLAUDE.md: immutable design data). Code reads them through one accessor with the C++ value as the fallback, so a missing asset can't change the game.
- **Stays C++:** structural values that aren't tuning (enum counts, `STAT_DERIVE_DENOM`'s formula, which is computed from the caps).

## 4. Steps (no big bang)

| Step | Job | What | Proof |
|---|---|---|---|
| 1 | **B-218** (new, now) | Tuning data: curve tables + `UCombatTuningData` with today's values; every reader goes through one accessor (fallback = the C++ constant); a debug print | every spec above unchanged; a spec checks each curve key = the old constant |
| 2 | **B-219** (new, now) | One damage maths core: `CalculateDamage`'s steps (§2.1–2.5, with the correction factor) as one pure function over an inputs struct; `UDamageCalculator` gathers the inputs from the actors today, and stage 3's execution supplies them from attributes. (The AI already calls `CalculateDamage`, `AIDecisionManager.cpp:841-898`, B-029, so it follows for free.) | `WoR.Damage`, `WoR.AI.DamageEstimate` unchanged |
| 3 | **B-162** (now on step 1) | Weapon requirement as a separate damage factor with a floor (2.9, Q-146c) | its tests |
| 4 | **B-216** (B-145) | the resistance seam | B-145 |
| 5 | **B-042** (GAS stage 3) | sources as GEs, MMCs for the sub-stat bases, the getters read attributes; the damage and buildup **executions wrap step 2's core**; Ignore Defense as an amount (D-086, Q-146b) | `wor.CompareStats`, `wor.CompareAttributes`, the B-134 snapshot identical (except D-086) |
| 6 | **B-043** (GAS stage 4) | costs as cost GEs (2.6); abilities apply the damage execution | every skill plays the same in PIE |

Steps 1–2 make stage 3 smaller: the maths is already in one place and already reads data, so B-042 only changes *where the inputs come from*.

## 5. Risks across the port

- **Rounding:** the damage preview rounds to whole numbers after each step (`ActionExecutor.cpp:670-675`); the execution must round at the same points.
- **Modifier grouping** (2.1) and **RNG order** (2.4), above.

## 6. Found while surveying (B-217)

Kept as they are in the port (same numbers); checked separately. **B-217 (Q-217): combat now reads the live levels; `CalculatePenalty` recorded as intended (Blueprint-only).**
- The requirement gap reads the character asset's world levels (`SkillDataBase.cpp:276-278`), while the stat getters read the live world levels (`CharacterDataComponent.cpp:755`, `CharacterDataComponent.h:543`): a levelled-up character may get the old gap. The same asset read is in the requirement checks and the Broken Darkness strain deficit (`WorldStatRequirements.cpp:13, 40, 97`).
- `FWorldStatRequirements::CalculatePenalty` is Blueprint-only and unused by damage.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Plan written | chore/B-146-stat-maths-plan |

## Sources

- [GASDocumentation: Modifier Magnitude Calculation](https://github.com/tranek/GASDocumentation#concepts-ge-mmc), [Execution Calculations](https://github.com/tranek/GASDocumentation#concepts-ge-ec), [Scalable floats and curve tables](https://github.com/tranek/GASDocumentation#concepts-as-init); [Epic API: `EGameplayModOp`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/EGameplayModOp__Type); [Lyra sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) (`ULyraDamageExecution`, `ULyraHealthSet` clamps).
