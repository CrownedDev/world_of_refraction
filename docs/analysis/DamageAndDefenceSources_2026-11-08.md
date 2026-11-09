# Damage and defence sources: this game vs the deckbuilder (2026-11-08)

Crown asked for this (via the brain) to answer **Q-164b**: does this game need an Amp stat? The measure is **"granular and composable"**, which is what Crown likes about the deckbuilder.

The code was read on `integration/cloud-2026-11-08` (dev plus the unbuilt cloud branches). Items marked **[cloud]** exist only on those branches:
- apparel: B-380
- curse penalties: B-378b

Paths are under `Source/world_of_refraction/` (`Pri/` = Private, `Pub/` = Public). Nothing here is built or changed; Crown decides.

## 1. How this game computes a hit

Every hit goes through one function, `WoRDamageMath::Compute` (`Pri/Combat/Damage/DamageMath.cpp:18`). Its inputs come from `UDamageCalculator::GatherDamageMathInputs` (`Pri/Combat/Damage/DamageCalculator.cpp:48`), and `UActionExecutor::ApplyHit` calls it once per hit (`Pri/Combat/Actions/ActionExecutor.cpp:3149`). The GAS path (`Pri/GAS/WoRDamageExecution.cpp:77`) runs the same math.

**A. The action's base** (`ActionExecutor::PreviewActionDamage`, `ActionExecutor.cpp:630-715`)

1. The skill's `BaseDamage` (`Pri/Skills/Definitions/SkillDataBase.cpp:293`; spells `SpellData.cpp:94`).
2. × the infusion charge band × the stat surcharge (`ActionExecutorCosts.cpp:194-220`).
3. × the tier gap between the skill and its weapon or crystal, 0.50–1.30 (`TierGapConstants.h:19-27`).
4. × the skill tier's power, F 1.0 … S 4.8 (`TierPowerConstants.h:18`).
5. × 0.6 if the skill's stat requirement is not met (`WorldStatRequirements.h:114`).
6. **+ the weapon part**, a flat add for non-spells: `Power × tier ratio × (1 + Σ grade × pillar curve)` (`WeaponPower.cpp:48-75`).

**B. Per impact:** the total is split across the hits (`DamageSplit`). Then the target's defence:
- Guard ×0.5
- Perfect Guard ×0
- timed dodge 0
- luck dodge: up to a 15% chance of 0 (`DefenseSystem.cpp:495-536`, `LuckDodge.h:16`)

**C. `Compute`** (`DamageMath.cpp`)

1. × the attacker multiplier: `Evo × (1 + ActionMods%) + EquipTerm + Σ skill scaling grade × stat` (`:32-43`). Evo is the Raw (Body) or Spell (Mind) stat, with the pillar percent inside it.
2. × the augment stones' damage factor (`:48`).
3. × the row: front ×1.2, back ×0.8 (`:52`; `CombatGridConstants.h:31-37`).
4. × the status factor. It is a product of four parts (`DamageCalculator.cpp:471-532`):
   - (1 + DamageBuff − DamageDebuff)
   - × (1 + ModifyDamageDealt)
   - × (1 + Raw or Spell Buff − Debuff)
   - × the defender's (1 − ReduceDamageTaken ≤ 90%) × (1 + IncreaseDamageTaken)
5. × the stat-cap correction `clamp(min(Evo, 1.5) × (1 + Eq) × S × T, 0, 2) / ((Evo + Eq) × S × T)` (`:58-68`). Below the caps this is not a no-op: it turns gear damage from added into multiplied.
6. × the element multiplier `1 − clamp(resistance × (1 − IgnoreDef), ±0.5)`, for **elemental hits only** (`:71-72`).
7. × crit damage, if the crit roll succeeds (`:75-84`).
8. × (1 − Defence × (1 − IgnoreDef)) (`:94-99`).
9. ÷ the defender's row factor (`:103-105`).
10. The floor: at least 1, and at least 10% of the hit before Damage Taken Down and Guard (`:89-113`).

**D. After `Compute`:**
- Ward / Shield / Barrier absorbers, then AbsorbDamage to EP (`ActionExecutor.cpp:3159-3171`).
- DoubleHit: a second hit at 50% (`:3255-3263`).

## 2. What raises damage here

| Source | Code | Scope | How it stacks |
|---|---|---|---|
| Skill base damage | `SkillDataBase.cpp:293` | that skill | the base |
| Infusion charge | `InfusionConstants.h:22-37` | an infused action, any element | × (A2) |
| Tier gap / tier power | `TierGapConstants.h:70`, `TierPowerConstants.h:31` | that skill | × (A3, A4) |
| Weapon Power + pillar grades | `WeaponPower.cpp:65` | physical, active weapon | **flat +**, after A's multipliers |
| Sub-stat points (RawDamage = Body, SpellDamage = Mind) | `CharacterDataComponent.cpp:1032-1052` | physical **or** spell | base of C1 |
| Pillar percent (evolution × gear × Mind/Body/Spirit buff) | `CharacterDataComponent.cpp:761-833` | one pillar's sub-stats | three factors ×, clamp [0, 2] |
| Over-cap pillar points (B-364) | `CharacterDataComponent.cpp:861-866` | that pillar | × |
| ActionMods (Reality boost, evolution infusion, requirement gap) | `ActionExecutor.cpp:5744-5829` | physical or spell, per action | % added together, × Evo |
| Gear `BonusRawDamage` / `BonusSpellDamage` (weapons, adapters, primary evolution; tier-scaled) | `LoadoutComponent.cpp:4343-4403` | physical **or** spell | added in C1, multiplied by C5 |
| Skill StatScaling grades | `EScalingTier.h:33-51` | that skill | added in C1 |
| Augment stones Raw / Spell damage (3–15%) | `CrystalEffectTable.h:607-675`; `LoadoutComponent.cpp:3207` | physical or spell | stones added together, × (C2) |
| Rows | `CombatGridConstants.h:31-37` | all | × (C3) |
| DamageBuff / Debuff (also difficulty: Easy 0.8, Hard 1.2) | `DamageCalculator.cpp:471-473`; `TrialGate.cpp:142-149` | **all** | summed, × (C4) |
| ModifyDamageDealt | `DamageCalculator.cpp:476-477` | all | a separate × |
| Raw / SpellDamageBuff (Amethyst gamble, damage stone consumable) | `DamageCalculator.cpp:484-506` | physical or spell | × |
| Target weakness (resistance ≤ 0) and ResistanceDebuff | `DamageMath.cpp:8-10`; `ResistanceQuery.cpp:62-85` | **the hit's element** (on the target) | added into resistance, ±50% cap |
| Crit chance / crit damage (gear, stones, buffs, GuaranteedCrit) | `CharacterDataComponent.cpp:1065, 1342-1356`; `DamageCalculator.cpp:309-361` | all | × factors, clamped |
| IgnoreDefense, DefenseDebuff, Pierce / Earth break | `DamageCalculator.cpp:265-280`; `ElementTable.cpp:33,41` | that skill / target | cuts Defence and positive resistance |
| DoubleHit | `ActionExecutor.cpp:3255` | all | an extra 50% hit |
| Boss phase effects, reaction triggers | `BossPhaseComponent.cpp:278-297`; `ReactionTrigger.cpp:246-269` | as authored | through the effect types above |

**Not damage sources** (each checked):
- **Weather:** no combat reads (`WeatherStateManager.cpp`).
- **Combat Tier:** enemy HP and level drops only (`CombatTier.h:4-5`).
- **Status buildup and Broken Darkness:** status only. Their own damage ticks skip this pipeline (`BrokenDarknessManager.cpp:803`, `SkillEffectManager.cpp:1677`).
- **Gems:** pick the infused element; their buffs arrive as the effect types above.
- **Random variance:** none, apart from the crit roll and the luck dodge.

## 3. What reduces damage here

| Source | Code | Scope | How it stacks |
|---|---|---|---|
| Defence: `min(Body × points × per-point, 0.5)` | `CharacterDataComponent.cpp:1068-1080` | **all** hits | the base of C8 |
| × (1 + BonusDefense) from weapons, adapters, evolution, **apparel [cloud]**, **curses − [cloud]** | `CharacterDataComponent.cpp:1335-1338`; `LoadoutComponent.cpp:4405-4435` | all | ×; total clamp 0.75 (`CombatConstants.h:214`) |
| × DefenseStone × (1 + DefenseBuff − DefenseDebuff) | `CharacterDataComponent.cpp:1339-1340` | all | × |
| Element resistance = effect buffs + class / innate profile + gear (weapons, adapters, evolution, **apparel and its crystal's +5 [cloud]**, **curses − [cloud]**); Ice also resists Water | `ResistanceQuery.cpp:104-144`; `LoadoutComponent.cpp:4440-4479`; `FApparelEntry.h:77` | **one element**, elemental hits only | **summed from every source**, then ±50% (C6) |
| Physical resistance (Slash / Pierce / Impact) | `ResistanceQuery.cpp:55-56` | **status buildup only**: physical damage is Defence-only (`DamageCalculator.cpp:124-125`) | — |
| ReduceDamageTaken (≤ 90%) / IncreaseDamageTaken | `DamageCalculator.cpp:527-532` | all | × (C4) |
| Rows | `CombatGridConstants.h:34-61` | all | ÷ (C9) |
| Guard 50%, Perfect Guard 100%, dodge, luck dodge | `DefenseSystem.cpp:517-528` | per impact | × before C; the floor ignores Guard |
| Ward (one element) / Shield (physical) / Barrier (any), AbsorbDamage | `ActionExecutor.cpp:3159-3171`; `WoRTurnEffectTracker.cpp:381` | keyed charges / all | flat − / % − (D) |
| Max HP: apparel BonusMaxHP **[cloud]**, curses **[cloud]**, HP stones and buffs | `CharacterDataComponent.cpp:921-960` | effective HP | (stat + gear flat) × % |

## 4. The deckbuilder's way

The deckbuilder resolves a hit in `DamagePipeline.Hit` (`wor-deckbuilder/wor.core/Match/SkillCommands.cs:332-473`). Its rules are in `docs/reference/01-card-model.md` §Stats, `06-bonuses.md` §In the match and `04-effects.md` §Pierce / absorbers. Every Amp and Def is a **small flat integer**.

**The lines:**
- `+2 Amp` raises all your skills. `+1 Fire Amp` raises only Fire skills (01:127-134). A line names its own element: none, one of the nine, `Physical`, `Ice` or `Elemental` (`StatElement.cs:19-43`).
- `+1 Def` reduces all damage, `+2 Fire Def` only Fire, `+2 Physical Def` only physical (01:130-132). "Resistance" is retired and reads as element Def (01:158).
- Every card kind's default tiles carry them: Weapon and Ring `Amp · Def`, Apparel `HP · Amp · Def` (01:184-191).
- Crystals and evolutions grant Amp / Def of their own element by tier: F0 E0 D1 C1 B2 A2 S3 (`TierLadders.cs:20-30`).
- Class passives add more: the Caster ("Refractor") gets +3 Amp and +1 Def to its innate element; the Generic class +1 Physical Def; the Resonator +1 Def per distinct ring element (`ClassPassives.cs:32-92`).

**One sum for everything.** `StatTotal(stat, element)` (`SkillCommands.cs:539-550`) adds every source: card lines, crystal tier grants, grant bonuses, class passives, timed and permanent stat effects. A line counts if it is unkeyed or keyed to the hit's element: "does this match? then add" (06).

**Resolution:**
1. `Power + Amp total + Damage total`.
2. The attacker's Pierce / IgnoreDefense eats the element Def first, then the plain Def.
3. Invert, then Reflect.
4. Absorbers: Ward of the element, then Shield (physical), then Barrier.
5. − (plain Def + element Def).
6. Block halves what is left.
7. At least 1, unless an absorber stopped the hit (`SkillCommands.cs:355-473`).

**Example:** `+1 Fire Amp` on a weapon plus `+1 Amp` on a ring, on a Power 5 Fire hit, gives 7. The same lines on a physical hit give 6.

## 5. Side by side

| | This game | Deckbuilder | Gap or duplicate |
|---|---|---|---|
| **Boost one element's damage** (outgoing) | **None.** `FEquipmentStatBonus` has no element fields (`FEquipmentStatBonus.h:54-112`). Even an element-tagged DamageBuff boosts every element: `GetTotalStatModifier` ignores the effect's element (`Pri/Skills/Effects/SkillEffectManager.cpp:2626-2648`). An exact-element read already exists, `GetTotalStatModifierForElement` (`:2650`), but only status buildup uses it (`StatusBuildupManager.cpp:415-416`, generic and keyed read in two calls so they never double-count). | `+1 Fire Amp` on any card, from crystals by tier, from class passives | **Gap**: the main thing the deckbuilder has that this game lacks |
| **Boost all damage** | Four % channels that multiply each other: DamageBuff, ModifyDamageDealt, Raw / SpellDamageBuff, difficulty (itself a DamageBuff) | `+N Amp`, one sum | **Duplicates**: four channels for one job |
| **Boost physical / spell** | Five ways: sub-stat points, pillar %, gear BonusRaw / SpellDamage, the stone, Raw / SpellDamageBuff | `Physical Amp` (no spell axis: element covers it) | Rich but overlapping; the scope "physical or spell" is the only granularity |
| **Flat damage on gear** | Only the active weapon's Power (physical) | Amp on every card | **Gap** for spells and off-weapon gear |
| **Reduce one element's damage** | Element resistance, **summed from every source** | `+2 Fire Def`, one sum | **Already matches**: resistance is this game's element Def |
| **Reduce physical damage** | Defence only. Physical resistance feeds status buildup, not damage. | `Physical Def` | Different by design; "physical resistance" is a misleading name for a damage reader |
| **Reduce all damage** | Defence (×, clamp 0.75), DefenseBuff, ReduceDamageTaken, back row, Guard | `+1 Def`, Block | Duplicates (DefenseBuff vs ReduceDamageTaken) |
| **Absorbers** | Ward / Shield / Barrier | the same three | **Matches** (ported) |
| **Stacking** | Mostly %, multiplied step by step | Flat integers, one sum | Different by design: this game's numbers are larger and tier-scaled (tier power up to ×4.8) |
| **One place to see it** | Spread over about ten gatherers | `StatTotal` | `wor.Damage` breakdown helps; no single gatherer |

**Odd code worth knowing** (not part of the question):
- `ActionMods.CritDamage` is only logged (`ActionExecutor.cpp:1052`), and `ActionMods.Defense` is never read for damage.
- A missed stat requirement penalises twice: ×0.6, plus it loses its scaling grades and the pillar % (`ActionExecutor.cpp:3141, 5823`).
- The C5 cap correction makes gear damage multiply with the stat instead of adding to it. That may or may not be meant.

## 6. Verdict for Q-164b

- **Defence side:** nothing to add. Element resistance is already the deckbuilder's element Def: keyed, summed from every source, on gear and apparel. Defence is its plain Def.
- **Damage side:** one real gap. **Nothing raises one element's damage.** Raw / Spell lines are the only scope, and the all-damage effects ignore their element. So this game can't express "+Fire damage" on a gear piece, a crystal, a class passive or an effect.
- **Do we need Amp?** Yes, as **element- (and all-) keyed lines, summed into one total**, the way resistance already works. Not as a fifth "all damage %" channel. The recommendation is in Q-164b.
