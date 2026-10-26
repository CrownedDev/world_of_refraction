# Status bars

This page covers the status buildup bar: what fills it, how much, what slows the fill, how it drains, what each element (and physical type) does when the bar fills, how long that lasts, the stun rules, and what resets between fights. It leaves the general effect model (stacking, cleanse, how timed effects count down) to [04-effects-and-turns.md](04-effects-and-turns.md), element damage and the resistance table's damage side to [02-elements-and-damage.md](02-elements-and-damage.md), infusion and class capabilities to [01-classes-and-loadouts.md](01-classes-and-loadouts.md), and HP/EP carry-over within a run to [05-economy-and-runs.md](05-economy-and-runs.md). Code paths are relative to `Source/world_of_refraction/`. The deckbuilder has no status-bar mechanic, so there is no deckbuilder page to compare against here.

## Filling the bar
| # | Rule | Status | Source |
|---|---|---|---|
| SB-01 | Every combatant has **one** status bar, not one per element. It fills to `STATUS_EFFECT_THRESHOLD` = **100**. | In game | `Combat/CombatConstants.h:359`; `Skills/Effects/StatusBuildupManager.h:191-201` |
| SB-02 | Hits from attacks, abilities and spells add buildup. A skill's authored `StatusBuildup` is scaled by its tier power, the tier gap and (when infused) the infusion charge's status bonus. Spells use the spell's element and no physical type. | In game | `Combat/Actions/ActionExecutor.cpp:1274-1283`, `:1511-1521` |
| SB-03 | A multi-hit skill spreads its buildup across its hits the same way it spreads damage: each hit adds its share, not hit 1 alone. | In game | `Combat/Actions/ActionExecutor.cpp:2030-2036` |
| SB-04 | Defending reduces each hit's buildup. A successful **Block** lets 50% through (`BLOCK_BUILDUP_MULTIPLIER`), a **Parry** 30% (`PARRY_BUILDUP_MULTIPLIER`), and a **Dodge** cancels it. | In game | `Combat/CombatConstants.h:348-354`; `Combat/Actions/ActionExecutor.cpp:2037-2044` |
| SB-05 | The attacker's Status Multiplier (Spirit, plus gear and Status Stone, plus buffs) multiplies the buildup, up to ×1.5 from the stat alone. Element-keyed Status Multiplier buffs add on top only for hits of that element. | In game | `Skills/Effects/StatusBuildupManager.cpp:380-413`; `Combat/CombatConstants.h:165` |
| SB-06 | A transformed Broken Darkness character's absorption stacks multiply buildup for hits that match its current alignment (×1 / ×1 / ×2 / ×4 at 0-3 stacks). Its forbidden casts also put buildup on **its own** bar (25% of the spell's base damage, `ForbiddenCastSelfBuildupPercent`). | In game | `Skills/Effects/StatusBuildupManager.cpp:425-428`; `Combat/Mechanics/BrokenDarknessManager.cpp:505-519`, `:857-869`; `Public/Combat/Mechanics/BrokenDarknessManager.h:527` |
| SB-07 | **The most recent hit decides.** Each hit stamps its element and physical type on the bar; whatever hit is last when the bar fills picks the effect. The bar's colour follows the last hit's element. | In game | `Skills/Effects/StatusBuildupManager.cpp:439-447`; `StatusBuildupManager.h:18-22` |
| SB-08 | The element wins over the physical type. A hit with no element (an uninfused attack) uses its physical type (Slash / Pierce / Impact). An uninfused attack's element is none; infusing it gives it the user's innate element. | In game | `Skills/Effects/BarCapTriggerResolver.h:30-72`; `Combat/Actions/ActionExecutor.cpp:1534-1551` |
| SB-09 | If the bar reaches 100 from a hit with neither an element nor a physical type, nothing fires; the bar stays full until a hit that does have one empties it. | In game | `Skills/Effects/StatusBuildupManager.cpp:460-479` |

## Resistance (slows the fill)
| # | Rule | Status | Source |
|---|---|---|---|
| SB-10 | The defender's status resistance cuts every hit's buildup: buildup × (1 − resistance). It slows the fill; it never blocks the effect once the bar is full. | In game | `Skills/Effects/StatusBuildupManager.cpp:431-437` |
| SB-11 | Resistance has two layers. **General** (Spirit stat capped at 0.5, then gear `BonusResistance` and an attached Resistance Stone multiply it) is never negative. **Matched** (resistance buffs/debuffs for that element or physical type, the class/innate table, gear per-element resistance, `ModifyStatusResist`) can be negative. | In game | `Skills/Effects/StatusBuildupManager.cpp:229-283` |
| SB-12 | Total resistance is clamped to −1 … +1 (`RESISTANCE_MIN`, `RESISTANCE_MAX`). Below 0 it **speeds up** the fill, to at most ×2 at −1. At +1 a hit adds nothing. | In game | `Combat/CombatConstants.h:282-291`; `Skills/Effects/StatusBuildupManager.cpp:288` |
| SB-13 | Immunities stop buildup completely: All-Status immunity blocks everything; Stun / Silence / DoT immunity blocks hits that would cause that effect; a per-element immunity (Fire, Water, …) blocks hits of that element. | In game | `Skills/Effects/StatusBuildupManager.cpp:323-372` |

## Draining and resetting
| # | Rule | Status | Source |
|---|---|---|---|
| SB-14 | At the start of each of the holder's own turns the bar loses 25% (`STATUS_DECAY_RATE`). Being hit again resets this count. | In game | `Combat/CombatOrchestrator.cpp:994-998`; `Skills/Effects/StatusBuildupManager.cpp:559-596`; `Combat/CombatConstants.h:361` |
| SB-15 | At the start of the holder's 3rd own turn with no new buildup, the bar empties fully (`STATUS_DECAY_FULL_RESET_TURNS` = 3). | In game | `Combat/CombatConstants.h:362`; `Skills/Effects/StatusBuildupManager.cpp:572-581` |
| SB-16 | When a bar fills and fires, it empties to 0. | In game | `Skills/Effects/StatusBuildupManager.cpp:467-474` |
| SB-17 | Items and effects can lower a bar: by a fraction (Quartz) or by a flat amount (`StatusDecrease`). Neither is affected by multipliers or resistance. | In game | `Skills/Effects/StatusBuildupManager.cpp:508-557` |
| SB-18 | Status bars reset after every fight: each fight starts with empty bars. HP, EP, absorption and durability carry over within a run; bars don't. | In game | `D-013`; `Combat/CombatOrchestrator.cpp:320-333` |

## What a full bar does
| # | Rule | Status | Source |
|---|---|---|---|
| SB-19 | **Fire** and **Slash**: a damage-over-time (Burn / Bleed) of 8% of the target's max HP at the end of each of its own turns, for 2 turns. The 8% is an unnamed literal. | In game | `Skills/Effects/BarCapTriggerResolver.h:35-36`, `:63-64`; `Skills/Effects/SkillEffectManager.cpp:2435-2451` |
| SB-20 | **Earth** and **Pierce**: defence down by 30% for 2 turns. | In game | `BarCapTriggerResolver.h:39-40`, `:65-66`; `SkillEffectManager.cpp:2453-2458` |
| SB-21 | **Lightning** and **Impact**: Stun (see "Stun" below). | In game | `BarCapTriggerResolver.h:43-44`, `:67-68`; `SkillEffectManager.cpp:2507-2512` |
| SB-22 | **Water**: healing on the target is **reduced** (not blocked) for 2 turns; reduction size is a named constant to tune. Today the code **blocks all healing** (`HealBlock`) for 2 turns. | Decided, not built (B-063) | `D-030`; `BarCapTriggerResolver.h:37-38`; `SkillEffectManager.cpp:2514-2519`, `:2104-2111` |
| SB-23 | **Wind**: pushes the target's next two turns later in the turn order. It isn't a stun; the target still acts. Today the cap applies a 1-turn `SkipTurn` effect that nothing enforces, so a full Wind bar does nothing. | Decided, not built (B-063) | `D-014`; `D-030`; `BarCapTriggerResolver.h:41-42`; `SkillEffectManager.cpp:2462-2466` |
| SB-24 | **Light**: crit chance down for 2 turns (the code's intended value is −100%). See Conflict C3-1: today a full Light bar applies **nothing**. | Conflict (see below) | `D-030`; `BarCapTriggerResolver.h:45-46`; `SkillEffectManager.cpp:2476-2481`, `:2555-2556` |
| SB-25 | **Darkness** (from a non-Broken-Darkness attacker): Silenced for 2 turns. Ruled (D-038): Silenced **seals 50% of max EP** for 2 of the holder's turns; every EP spend (spells, abilities, infusion) uses only the rest. Today the code still blocks **spells only** (B-148). | Decided, not built (B-148) | `BarCapTriggerResolver.h:47-51`; `SkillEffectManager.cpp:2521-2526`; `Combat/Actions/ActionExecutor.cpp:176-180` |
| SB-26 | **Darkness from a Broken Darkness attacker**: drains the target's EP instead of silencing. Today the cap resolves to `DrainEnergy`, which the cap handler doesn't cover, so nothing happens. | Decided, not built (B-063) | `BarCapTriggerResolver.h:47-51`; `SkillEffectManager.cpp:2555-2556` |
| SB-27 | **Void**: a random skill (loss of control) for 1 turn. Applied, but there's no forced-action behaviour yet (the log says "implementation pending"). | Decided, not built (B-063: "minimal") | `BarCapTriggerResolver.h:52-53`; `SkillEffectManager.cpp:2528-2533`, `:1425-1430` |
| SB-28 | **Reality**: instant damage of 25% of the target's max HP. It can kill. No lasting effect. | In game | `BarCapTriggerResolver.h:54-55`; `SkillEffectManager.cpp:2535-2552` |
| SB-29 | **Ice** (a Water damage type, not a tenth element): a full Ice bar slows action speed (Freeze, a timed turn-speed debuff) for 2 turns. Ice doesn't exist in code yet. | Decided, not built (B-064a, blocked by B-064) | `D-029`; `D-030` |
| SB-30 | The same attacker filling your bar again **refreshes** the effect it already put on you; a different attacker **adds** its own. Today every cap adds a new effect (random effect ID), so repeats stack. | Decided, not built (B-062) | `D-012`; `SkillEffectManager.cpp:2427` |

## How long
| # | Rule | Status | Source |
|---|---|---|---|
| SB-31 | A full bar's effect lasts **2 of the holder's own turns** (`STATUS_CAP_TURNS` = 2, which doesn't exist yet), counted the deckbuilder way: if applied during the holder's own turn, that turn is turn 1. Applies to every timed cap unless Crown says otherwise. | Decided, not built (B-063) | `D-030` |
| SB-32 | Today, cap effects count down at the end of the holder's own turns: Fire/Slash, Earth/Pierce, Water, Darkness last 2; **Stun, Wind and Void last 1**. | Code only (diverges from D-030 for Stun/Wind/Void; B-063) | `Skills/Effects/SkillEffectManager.cpp:974-991`, `:1099-1130`, `:2433-2533` |

## Stun
| # | Rule | Status | Source |
|---|---|---|---|
| SB-33 | A stunned character still gets its turn and can **Attack** (the current weapon's own basic attack) or **Defend**. Spells, abilities, items, Flee and Switch Weapon are refused. | In game | `D-001` (B); `Combat/Actions/ActionExecutor.cpp:165-173`, `:306-316`; `Tests/StunRuleSpec.cpp` |
| SB-34 | A stunned character's attack can't be infused: plain swing only, no charge. | In game | `D-023` (A); `Combat/Actions/ActionExecutor.cpp:313-315`; `UI/Combat/CombatCommandMenuSubsystem.cpp:1782-1784` |
| SB-35 | While stunned, the player's command menu shows only Attack and Defend; the AI picks attack or defend. | In game | `UI/Combat/CombatCommandMenuSubsystem.cpp:559-566`; `AI/AIDecisionManager.cpp:140-141` |
| SB-36 | A ritual (deferred spell) that was set up before the stun still goes off. | In game | `Combat/Actions/ActionExecutor.cpp:163-166` |

## Open questions
- D-038 (ANSWERED): Silenced seals half your EP for 2 holder turns (C3-2); built in B-148. Its name is D-070 (OPEN).
- D-051 (OPEN): how effect durations are counted; blocks B-062 and B-063.
- D-053 (OPEN): whether the same effect from two gear pieces adds up; blocks B-062.
- D-055 (OPEN): whether bonus and Execution turns count toward durations; blocks B-062 and B-063.
- Answered for this topic: D-001, D-012, D-013, D-014, D-023, D-029, D-030. Building them is B-062, B-063 and B-064a.

## Conflicts
- **C3-1: A full Light bar does nothing.** D-030 lists "Light crit down" as a 2-turn cap. The resolver maps Light to `CritChanceDebuff` (`Skills/Effects/BarCapTriggerResolver.h:45-46`), but `ApplyTriggeredSkillEffect` only has a case for the older `CritDebuff` (`Skills/Effects/SkillEffectManager.cpp:2476`), so `CritChanceDebuff` falls to `default: return` (`:2555-2556`). The bar still empties (`StatusBuildupManager.cpp:467-474`). B-063 doesn't list Light. No question: D-030 already rules Light = crit down. The fix is **B-112**.
- **C3-2: What does Silenced block?** The Darkness cap's Silenced only blocks **spells** in code (`Combat/Actions/ActionExecutor.cpp:176-180`; the gate-effect comment at `SkillEffectManager.cpp:1417` says "EP-cost gates check" but no other check exists). The enum calls it "No EP Spend" (`Public/Skills/Effects/ESkillEffectType.h:129`). `docs/Mechanics/Status/StatusEffects.md` says it stops "spells, abilities, or infusion charge". D-023's recommendation assumes "infusion is the EP-spend layer that Silenced already gates". Question, options and recommendation: **D-038**. **Ruled (D-038): B, reshaped**: every EP spend is affected, but as a seal of 50% of max EP for 2 holder turns, not a block. Basic attack, Defend and items still work. Built in **B-148**.
- **C3-3: Status Multiplier may count twice on infused hits.** An infused skill's buildup is multiplied by the infusion charge factor, which already includes the user's Status Multiplier (`Combat/Actions/ActionExecutor.cpp:4696-4719`). `AddStatusBuildup` then multiplies by the same Status Multiplier again (`StatusBuildupManager.cpp:389-394`). The header says callers that already include it should skip that step (`StatusBuildupManager.h:103-110`). Treated as a bug: the manager's own contract says a caller that bakes the multiplier in skips it. **B-113** confirms it in a test first. (Read from the code, not checked in PIE.)
- **C3-4: The AI estimates buildup differently from the game.** The AI scores skills with `CalculateStatusBuildup` (`AI/AIDecisionManager.cpp:849`, `:903`, `:2189`). For abilities that ignores the authored `StatusBuildup` and uses `BASE_STATUS_BUILDUP_PER_HIT` = 5 per hit (`Skills/Definitions/AbilityData.cpp:82-94`). The live hit uses the authored value with tier and infusion scaling (`ActionExecutor.cpp:1511-1521`). Treated as a bug: the AI estimate should match what the hit does (as B-029 asks for damage). Fix: **B-114**.
- **C3-5: Mechanics doc vs code on draining.** `docs/Mechanics/Status/StatusBuildup.md` says the bar drains only through gauge-reduction effects; the code also drains 25% per holder turn and empties after 3 turns (SB-14, SB-15). This is a doc fix, not a ruling. Doc fix, no ruling: **B-117**.

## Gaps
- D-030's 2-turn rule names Fire, Earth, Lightning, Light, Darkness, Wind, Water and Ice. It doesn't cover the physical caps (Slash Bleed, Pierce defence-down, Impact stun), Void's random skill, or BD-Darkness's EP drain. How big the drain is and how long it lasts has no ruling. **Ruled (Crown, 2026-10-26, rules review): the same rule for all of them, now **1 holder turn**, a "status break" (D-090)** (physical Slash/Pierce/Impact caps, Void's random skill, Broken Darkness's EP drain).
- How big each cap effect is (8% burn, 30% defence down, crit down size, Water's healing cut, Wind's push-back, Ice's slow) has no ruling. Only the Wind/Water/Ice sizes are marked to tune (D-014, D-029, D-030). The 8% and 25% are unnamed literals (`SkillEffectManager.cpp:2444`, `:2543`). **Ruled (Crown, 2026-10-26, rules review):** every size becomes a **named, tunable constant** (today's values as placeholders), **as a percentage wherever possible** (e.g. 90%), tuned by Crown in play-tests.
- Whether a target can be stunned (or have any cap) again while that effect is still active: today it just adds another effect; D-012 covers same-source refresh, but not whether a new bar can build during the effect. **Ruled (Crown, 2026-10-26, rules review): A, no chaining.** While a status break is active, that bar **can't fill**; it starts again from empty after the break ends.
- There's no rule for whether healing, buffs or support spells should build status on allies. Today any skill with a positive authored `StatusBuildup` builds it; the "buffs and heals build none" check exists only in the unused `USpellData::CalculateStatusBuildup` (`Skills/Definitions/SpellData.cpp:109-111`). **Ruled (Crown, 2026-10-26, rules review): A, heals, buffs and support skills never build status**, on anyone. Wire the existing check into the live buildup path.
- The player can't see which effect a part-filled bar will fire beyond its colour; `GetPendingTrigger` exists for UI (`StatusBuildupManager.h:81-85`), but there's no rule for what the HUD shows.
