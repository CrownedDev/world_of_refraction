# Elements and damage

This page covers the elements, how a skill's damage element is picked, every step that turns a skill's base damage into the number taken off HP, resistance, infusion levels L0-L2, real-time defense (block, parry, dodge) as a damage modifier, and Broken Darkness and Reality where they change damage or resistance. Status bars and what a full bar does are in 03-status-bars.md; effects, buffs and turn counting are in 04-effects-and-turns.md; classes and which slots give which skills are in 01-classes-and-loadouts.md. Code paths are relative to `Source/world_of_refraction/`.

## Elements

| # | Rule | Status | Source |
|---|---|---|---|
| ED-01 | There are nine elements: Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality. | In game | `Skills/Definitions/ESpellElement.h:16-34` |
| ED-02 | Two more values are not elements. **Generic** means "inherit": a Generic spell takes its source's element when cast. **None** means non-elemental (physical, or no source). | In game | `ESpellElement.h:18,28-34` |
| ED-03 | An element by itself does not change damage: there is no weakness wheel, and the element step is always x1.0. Today elements only steer status buildup, resistance and Broken Darkness. | In game | `Combat/Damage/DamageCalculator.cpp:207-208` |
| ED-04 | Ice is a **damage type of Water**, not a tenth element: no Ice crystal, ring or Refractor. Ice skills are Water-keyed and need a Water source; Ice resistance also resists Water; Water boosts don't boost Ice; "on Water hit" triggers hear Ice. A full Ice bar slows action speed (Freeze). | Decided, not built (B-064a) | `D-029`; `D-030`; deckbuilder `02-enums.md` "Ice — a damage type, not an element" |

## A skill's damage element

| # | Rule | Status | Source |
|---|---|---|---|
| ED-05 | Every skill has a damage type. A physical skill (attack or ability) is **Physical** unless infused; infusing it gives it **the element of the chosen source** (the crystal on the weapon, a ring, ...). | Decided, not built (B-080) — see CED-4, CED-5 | `D-007` |
| ED-06 | Today an uninfused attack or ability deals None. An infused one deals the **user's innate element**, and only Casters have one, so a Generic or Resonator infused ability deals None. The status bar it fills uses the chosen source's element instead. | Code only (no ruling) — conflicts with ED-05 | `Combat/Actions/ActionExecutor.cpp:1451-1457`; `:2258-2268`; `Character/CharacterData.h:267` |
| ED-07 | A fixed-element spell deals its own element. A Generic spell deals the element of where it is slotted (innate, ring, weapon crystal, evolution); for a Broken Darkness caster, the element of the spell pool it sits in. If no element can be found it deals None (never Generic). | In game | `ActionExecutor.cpp:4292-4358`, `:1201-1209`; `D-007` |
| ED-08 | Physical hits also carry a physical type, Slash, Pierce or Impact, authored on the ability. Spells carry none. It is read by resistance (ED-24) and physical status buildup. | In game | `Combat/Damage/EPhysicalDamageType.h:12-15`; `ActionExecutor.cpp:2272-2278` |

## Damage: from base to HP

Steps run in this order. All are multipliers, so order only matters for rounding and the caps.

| # | Rule | Status | Source |
|---|---|---|---|
| ED-09 | **Base.** Damage starts from the skill's authored Base Damage. A spell's or ability's total is split across its impacts (by the skill's damage-split table, else evenly). | In game | `Skills/Definitions/SkillDataBase.cpp:288-298`; `Skills/Definitions/SpellData.cpp:84-101`; `ActionExecutor.cpp:1891-1897` |
| ED-10 | **Charge (infusion level).** L1/L2 multiply damage by the source's mode band, then by (1 + how far the attacker's Raw/Spell Damage stat is above x1.0). See ED-28. | In game | `ActionExecutor.cpp:4668-4694`, `:1174-1175`, `:1462-1463` |
| ED-11 | **Tier gap.** The skill's tier vs the tier of what it is used through (weapon for attacks/abilities; crystal, fusion gem or evolution for spells). Gap = skill tier − channel tier: −4 or more x1.30, −3 x1.20, −2 x1.13, −1 x1.06, 0 x1.00, +1 x0.90, +2 x0.78, +3 x0.64, +4 or more x0.50. No channel (innate spells, unarmed) = x1.00. | In game | `Combat/Damage/TierGapConstants.h:18-26,34-67`; `ActionExecutor.cpp:472-558`, `:1179-1180`, `:1468-1469` |
| ED-12 | **Tier power.** The skill's own tier scales it: F x1.00, E x1.30, D x1.70, C x2.20, B x2.85, A x3.70, S x4.80 (`TIER_POWER`). | In game | `Combat/Damage/TierPowerConstants.h:17`; `ActionExecutor.cpp:1184`, `:1474` |
| ED-13 | **Attacker stat.** Spells scale with Spell Damage, everything else with Raw Damage. Each runs from x1.0 to x1.5 with full investment (`STAT_MULT_CAP`); gear, stones and buffs multiply past that, up to x2.0 in total (`STAT_MODIFIER_MAX`). | In game | `DamageCalculator.cpp:50-52`, `:155-205`; `Combat/CombatConstants.h:140-142,374` |
| ED-14 | **Requirement gap.** Per pillar (Mind, Body, Spirit): gap = the skill's required world level − yours. It changes every stat of that pillar for this action (Spell Damage is Mind, Raw Damage is Body): −5 or more +40%, −4 +30%, −3 +20%, −2 +13%, −1 +6%, 0 none, +1 −10%, +2 −22%, +3 −36%, +4 −50%, +5 or more −68%. | In game | `TierGapConstants.h:130-172`; `SkillDataBase.cpp:260-281`; `ActionExecutor.cpp:6846-6860`; `Combat/Actions/ActionStatModifiers.h:131-148` |
| ED-15 | **Skill stat scaling.** A skill can list extra (stat, grade) pairs; each adds grade × how invested that stat is to the attacker multiplier. Grades: S 1.0, A 0.8, B 0.65, C 0.5, D 0.35, E 0.2, F 0.05. | In game | `DamageCalculator.cpp:90-99`; `Skills/Definitions/EScalingTier.h:35-41` |
| ED-16 | **Grid row (attacker).** Front row deals x1.05, middle x1.00, back x0.95. | In game | `DamageCalculator.cpp:135-140`; `Combat/Grid/CombatGridConstants.h:25,31,37` |
| ED-17 | **Buffs and debuffs.** Damage Up/Down and Raw/Spell Damage Up/Down on the attacker, and Damage Taken Down/Up on the defender, each multiply in. Damage Taken Down is capped at 90%; Damage Taken Up has no cap. | In game | `DamageCalculator.cpp:644-714` (cap `:708`) |
| ED-18 | **Crit chance** is Luck: 5% base (`CRIT_CHANCE_BASE`), up to 50% with a maxed Luck stat (`CRIT_CHANCE_LUCK_BONUS` 0.45), gear and buffs toward 100%. A Guaranteed Crit effect always crits. | In game | `DamageCalculator.cpp:211-230`, `:463-507`; `CombatConstants.h:174-175` |
| ED-19 | **Crit damage** is a separate Mind stat. With no investment a crit does **x1.0, nothing extra** (`CRIT_DMG_BASE`). The stat raises it to x1.5 (`CRIT_DAMAGE_STAT_CAP`); gear, Crit Stones and buffs multiply past that up to x2.5 (`CRIT_DAMAGE_GEAR_CEILING`). A crit-damage debuff never makes a crit weaker than a normal hit. | In game (D-034) | `DamageCalculator.cpp:580-642`; `CombatConstants.h:185-192` |
| ED-20 | **Defense.** The defender's Defense is a % cut: up to 50% from the stat alone (`UNIVERSAL_STAT_CAP`), then gear, Defense Stones and buffs multiply it, hard-capped at 85% (`DEFENSE_REDUCTION_CEILING`). An Ignore Defense skill or effect skips this step. | In game | `DamageCalculator.cpp:245-263`, `:407-461`; `CombatConstants.h:145,208` |
| ED-21 | **Grid row (defender).** Damage taken is divided by the row value: front row takes about 5% more (÷0.95), back row about 5% less (÷1.05). | In game | `DamageCalculator.cpp:266-273`; `CombatGridConstants.h:28,34,40` |
| ED-22 | **Minimum damage.** A hit that gets through always deals at least 1 (`MIN_DAMAGE`), unless its base is 0 or it was dodged. | In game | `DamageCalculator.cpp:36-39`, `:276`; `Combat/Damage/DamageCalculator.h:33` |
| ED-23 | Reality boosts: a Reality Refractor gets +10% to all stats on every action, a Reality evolution in the primary slot +5%, infusing through a Reality source +2.5% (L1) or +5% (L2). These feed ED-13 and ED-18. | In game | `Combat/Mechanics/RealityBoost.h:13-22`; `ActionExecutor.cpp:6776-6826` |

## Resistance

| # | Rule | Status | Source |
|---|---|---|---|
| ED-24 | Every character has a resistance row: nine element columns + Slash/Pierce/Impact, in steps of 15 / 10 / 5 (percent, + resists, − weak). The row is picked in this order: Broken Darkness row, else Generic class row, else Resonator row, else the Caster's innate-element row. An incoming hit reads its element column plus its physical column. | In game | `Combat/Resistance/ClassInnateResistanceTable.h:31-37,70-125,134-178` |
| ED-25 | Rows: Generic +15 to all three physical types, −5 to every element. Resonator −15 to all three physical types. Each Caster element resists its own element +15 and is −15 to Reality (except Reality and Void); e.g. Fire is −15 Water, −5 Wind; Lightning also +15 Water; Reality +10 to every element but −10 Void; Void −10 Light, Darkness and Reality. | In game | `ClassInnateResistanceTable.h:74-92,112-123` |
| ED-26 | Today resistance changes **status buildup only**: buildup × (1 − resistance), where resistance = the general Resistance stat (≤50% alone, gear to 100%) + resistance buffs + the class row + rolled gear resistance, clamped between −100% (double buildup) and +100%. Infusion HP backlash is also cut by the Resistance stat. | In game | `Skills/Effects/StatusBuildupManager.cpp:214-288,437`; `CombatConstants.h:286,291`; `Infusion/InfusionCostHelper.cpp:16-31` |
| ED-27 | Resistance **also changes damage**: the element step (ED-03) becomes 1 − resistance, **1:1** (+20% Fire resistance = 20% less Fire damage; −15% = 15% more), capped at **±50%**. Only **per-element** resistance counts (class table + gear + element-specific effects); the general Resistance stat and blanket resistance buffs don't. | Decided (D-036), not built (B-064, after B-145) | `D-005` |

## Infusion levels

| # | Rule | Status | Source |
|---|---|---|---|
| ED-28 | Damage at each level, by the source's mode: **Physical** mode L1 x1.15, L2 x1.30; **Status** mode L1 x1.10, L2 x1.20; **Balanced** L1 x1.125, L2 x1.25. L0 is x1.0. The same bands, swapped, scale status buildup. Then × (1 + stat above x1.0) per ED-10. | In game | `Infusion/InfusionConstants.h:20-35`; `ActionExecutor.cpp:4668-4720` |
| ED-29 | Spell size grows with level: L1 x1.5, L2 x2.0. | In game | `ActionExecutor.cpp:120-131`; `InfusionConstants.h:66-69` |
| ED-30 | EP cost at each level: L1 x1.5, L2 x2.0 (spells and abilities), × (1 + Raw/Spell Damage above x1.0), × Efficiency, × own tier power (ED-12 values), × tier-gap cost (−4 x0.77 … +4 x2.00). An infused ability paying EP also pays a flat x1.5 (`INFUSION_ENERGY_MULTIPLIER`), on purpose (D-035). | In game (D-035) | `ActionExecutor.cpp:133-144`, `:374-416`, `:4774-4813`; `SkillDataBase.cpp:301-314`; `TierGapConstants.h:75-83` |
| ED-31 | Free sources: spells cast from a ring or weapon crystal cost no EP; abilities infused from a ring or weapon crystal cost no EP (they wear the crystal instead). | In game | `ActionExecutor.cpp:345-352`, `:397-405` |
| ED-32 | HP backlash at L1/L2: Raw source always, Innate source on spells only, Evolution source. HP lost = (infused EP before Efficiency ÷ max EP) × max HP × (1 − Resistance). It is paid after the action lands and **can kill you**. | In game | `ActionExecutor.cpp:6374-6437`, `:6667-6672`; `InfusionCostHelper.cpp:16-46` |

## Real-time defense

| # | Rule | Status | Source |
|---|---|---|---|
| ED-33 | Per impact, a timed press: **Block** takes 50% (`BlockReduction` 0.5); **Parry** takes 30% (`ParryReduction` 0.7) and reflects 30% of that impact's base back at the attacker (`ParryReflect` 0.3); **Dodge** takes nothing and the impact applies no status. A missed or absent press takes full damage. The reduction applies to the impact's base slice, before ED-10 to ED-22. | In game | `Combat/Defense/DefenseSystem.h:426,430,434`; `Combat/Defense/DefenseSystem.cpp:477-522`; `ActionExecutor.cpp:1928-1953`, `:2000-2003` |
| ED-34 | Status through a defended hit: Block lets 50% of buildup through, Parry 30%, Dodge 0%. | In game | `CombatConstants.h:353-354`; `ActionExecutor.cpp:2037-2044` |
| ED-35 | Timing window: 0.5 s before the impact (`DefenseInputWindow`), widened by the defender's Reflex (up to +0.25 s, +0.5 s with gear) and narrowed by the attacker's Action/Spell Speed the same way, never below 0.1 s (`MINIMUM_DEFENSE_WINDOW`); a late press up to 0.15 s after the impact still counts (`DEFENSE_AFTER_GRACE_SECONDS`). Each press is matched to one impact. | In game | `DefenseSystem.h:443`; `DefenseSystem.cpp:280-386,434-471`; `CombatConstants.h:233-247,265` |
| ED-36 | Difficulty, set per skill and per impact, separately for Block, Parry and Dodge, shrinks all timing bands: Easy x1.0, Medium x0.7, Hard x0.4, Impossible x0.1 (floor 0.04 s). Unset = Easy. | In game | `Combat/Defense/DefenseDifficulty.h:21-73`; `CombatConstants.h:253-260`; `DefenseSystem.cpp:338-347` |
| ED-37 | A **Perfect** defense is a press within 0.1 s of the impact (`PerfectThreshold`, scaled by difficulty). It does not change damage; it fires "on perfect" triggers and doubles Broken Darkness absorption (ED-40). | In game | `DefenseSystem.h:447`; `DefenseSystem.cpp:375-379`; `ActionExecutor.cpp:1972-1976` |
| ED-38 | Which defenses a spell allows: Block always; Parry and Dodge only if the spell's delivery allows them. | In game | `SpellData.cpp:231-250` |

## Broken Darkness and Reality (damage side)

| # | Rule | Status | Source |
|---|---|---|---|
| ED-39 | A Broken Darkness character uses its own resistance row: −5 to every element and −15 to Reality, no physical columns. | In game | `ClassInnateResistanceTable.h:112-115` |
| ED-40 | Broken Darkness gains EP by blocking (5% base rate) or parrying (10%) elemental hits, scaled by Efficiency, x2 on a Perfect; it can overfill EP by 30% (overload). It cannot absorb None, Generic or Reality hits. | In game | `Combat/Mechanics/BrokenDarknessManager.cpp:43-46,56,449-466` |
| ED-41 | Light and Void are forbidden to Broken Darkness: casting one costs the caster 25% of the spell's base damage × its Spell Damage as HP, plus 25% of base as self-buildup in that element. Waived when the caster's world levels exactly match the spell's requirements. | In game | `BrokenDarknessManager.cpp:444-447,469-533`; `BrokenDarknessManager.h:519,527`; `ActionExecutor.cpp:1253-1268` |

## Open questions
- D-P01 (PARKED): move Broken Darkness onto GAS, only after GAS stage 3 and with a zero-difference compare. No rule change expected.
- Answered 2026-10-26: D-034 (x2.5), D-035 (keep the surcharge), D-036 (per-element, 1:1, ±50%), D-037 (a separate damage type, with B-145). See the conflicts below.

## Conflicts
- **CED-1 — Infused abilities pay two cost surcharges.** `InfusionConstants.h:75-78` says "ability and spell now share the same charge tax" (L1 x1.5, L2 x2.0), and `docs/Mechanics/Magic/Infusion.md` says the same. But an infused ability that pays EP also pays the flat x1.5 `INFUSION_ENERGY_MULTIPLIER` (`SkillDataBase.cpp:301-314`, `AbilityData.cpp:75`), so it costs about x2.25 at L1 and x3.0 at L2; a spell pays only x1.5 / x2.0 (`SpellData.cpp:130-137`). Comments also give wrong numbers (`ActionExecutor.cpp:138-140` "30%/60%", `:4781-4783` "1.15x/1.30x"). Question, options and recommendation: **D-035**. **Ruled (D-035): A**, abilities keep the extra x1.5. The stale comments are fixed in B-147.
- **CED-2 — Crit damage ceiling: x2.0 or x2.5?** Code caps crit damage with gear at x2.5 (`CombatConstants.h:192`). The comment above it says "gear pushes past toward x2.0" (`CombatConstants.h:184`), as do `docs/Architecture/DamageCalculator.md:36,48` and `docs/Mechanics/Combat/Luck.md:14`. Question, options and recommendation: **D-034**. No deckbuilder rule (cards have no crits). **Ruled (D-034): A, x2.5**, the code; the docs are fixed, the `CombatConstants.h:184` comment is B-147.
- **CED-3 — Which resistance scales damage under D-005?** D-005's notes say "the same composed resistance the buildup path reads (class table + gear)". The buildup path's value also includes the general Resistance stat (up to 100% with gear) and resistance buffs (`StatusBuildupManager.cpp:214-288`). Question, options and recommendation: **D-036**. **Ruled (D-036): A**, per-element only, 1:1, ±50% (ED-27); built after the B-145 resistance plan.
- **CED-4 — What "Physical" is in code.** D-007 says an uninfused physical skill's damage type is **Physical**. The code has no Physical element: it deals None (`ActionExecutor.cpp:1453`), with a separate Slash/Pierce/Impact type (ED-08). Question, options and recommendation: **D-037**. **Ruled (D-037): C**, a proper damage type (Physical + Slash/Pierce/Impact, the nine elements, Ice), separate from the element a character or crystal *is* (`ESpellElement`). Built inside B-145; until then "Physical" is shown for None + physical type.
- **CED-5 — D-007 vs today's infused-ability element.** → **B-080** Covered by ED-05/ED-06: damage uses the innate element, status uses the source's. Already queued as B-080; no new question.
- **CED-6 — Stale comments.** → **B-117** `ActionExecutor.cpp:6846-6851` says the old √deficit requirement penalty is still live and double-counts; `SkillDataBase.h:377-383` says damage = BaseDamage × (1 − penalty). The code no longer applies it (`SkillDataBase.cpp:288-298`, `SpellData.cpp:84-101`). `ActionExecutor.cpp:6667` says Evolution HP backlash is 5% / 10% of max HP, but the code uses the EP-based formula (ED-32); `EVOLUTION_L1/L2_HP_COST_PERCENT`, `SPELL_L2_DAMAGE_MULT` and `LEVEL_1/2_CHARGE_TIME` (`InfusionConstants.h:40-57,72`) have no readers. No design question: code is right; fix or remove the comments and dead constants.

## Gaps
- A Perfect defense changes nothing about damage (ED-37). The deckbuilder has no real-time defense to borrow from. **Ruled: D-084** (Block and Parry merge into **Guard**: normal = block, Perfect = parry; Dodge unchanged).
- Luck dodge: `LUCK_DODGE_MAX` (25%) exists with no reader (`CombatConstants.h:315,319-326`). **Ruled: D-085** (Luck auto-dodge, max 15%, only when no press succeeded).
- Ignore Defense is all-or-nothing here; the deckbuilder's Ignore Defense N removes up to N, element Resistance first (`04-effects.md:351,362`). **Ruled: D-086** (an amount per skill; built in the GAS damage calculation, B-146).
- No rule on whether parry-reflected damage goes through the attacker's Defense, crit or resistance. Not checked in the code. **Ruled: D-087** (reflection is an attachable effect, e.g. on Perfect Guard; reflected damage is pure).
- The deckbuilder's extra costs for Generic infusion (1 Action to switch sources) are not ruled (`D-007` notes).
- Nothing shows the player which elements a target is weak to; with B-064 this becomes a damage rule the player can't see.
