# Effects and turns

This page covers how a fight starts, who acts when (the speed schedule, ties, bonus and Execution turns, the turn-order strip), what you may do on a turn and what it costs (EP, HP, overflow), and every rule for effects: how long they last, when they act and expire, re-applying and stacking, charges, Last Stand, absorb and reflect, cleanse, gear effects on a mid-fight swap, auras and effect text. It ends with how a fight is won or lost. Status bars and what a full bar does are in 03-status-bars.md; damage and elements in 02-elements-and-damage.md; classes, loadouts and infusion sources in 01-classes-and-loadouts.md; rewards and runs in 05-economy-and-runs.md; party control in 06-party.md. Code paths are relative to `Source/world_of_refraction/`.

## Starting a fight
| # | Rule | Status | Source |
|---|---|---|---|
| ET-01 | A fight needs at least one combatant per side. Your party is team 0, the enemies team 1. | In game | `Private/Combat/CombatOrchestrator.cpp:168-177`; `Private/Combat/TurnManager.cpp:58-88` |
| ET-02 | At the start, the once-per-fight record is wiped. Then each combatant's always-on gear effects (weapon, ring, primary-slot evolution) are applied once and its conditional gear effects are armed. Evolutions attached to gear add no effects. | In game | `Private/Combat/CombatOrchestrator.cpp:200-207, 1181-1200` |
| ET-03 | The first turn goes to whoever the speed schedule picks (ET-06). Nobody gets a set first-turn advantage. | In game | `Private/Combat/TurnManager.cpp:112-118, 593-658` |

## Turn order
| # | Rule | Status | Source |
|---|---|---|---|
| ET-04 | Turn speed comes from Spirit: base `TURN_SPEED_BASE` 10, capped at `TURN_SPEED_CAP` 15 by stats alone. Dedicated Bonus Turn Speed gear multiplies past that, up to `TURN_SPEED_GEAR_CEILING` 20. The result is rounded. | In game | `Public/Combat/CombatConstants.h:294-302`; `Private/Combat/TurnManager.cpp:402-414` |
| ET-05 | Effective speed = turn speed × (1 + (Turn Speed Up + Turn Speed Buff − Turn Speed Debuff) / 100) × an attached Turn Speed stone. Never below 1. | In game | `Private/Combat/TurnManager.cpp:218-249` |
| ET-06 | The slowest living combatant has ratio 1.0; everyone else has their speed ÷ the slowest's. When no living combatant is owed a turn, each living one is owed its ratio. The one owed most acts next, so someone twice as fast acts twice per round. The dead are owed nothing. | In game | `Private/Combat/TurnManager.cpp:251-270, 593-658` |
| ET-07 | The ratio has no cap in play, even though a 2:1 cap constant exists. | Conflict (see below) | `Public/Combat/CombatConstants.h:330-331`; `Private/Combat/TurnManager.cpp:265-270` |
| ET-08 | Ties, in order: higher base turn speed (buffs ignored); higher action speed; **lower** Mind+Body+Spirit world-stat total (the underdog); higher Body; higher Mind; higher Spirit; your party before enemies; lower slot. World stats are read when the fight starts. | In game | `Private/Combat/TurnManager.cpp:352-385, 417-422` |
| ET-09 | Applying or losing a speed effect, switching weapon or ring, a death or a revive all recalculate the ratios at once. | In game | `Private/Combat/TurnManager.cpp:435-462`; `Private/Skills/Effects/SkillEffectManager.cpp:2370-2412` |
| ET-10 | The turn-order strip shows the upcoming turns: `TURN_BELT_HORIZON` 16 of them, of which the UI asks for up to 10. They come from the same scheduler, so they include pinned bonus and Execution turns. The strip is rebuilt at each turn start. | In game | `Public/Combat/CombatConstants.h:333-335`; `Private/Combat/TurnManager.cpp:196-199, 469-481, 671-686` |
| ET-11 | A full Wind bar pushes the holder's next two turns later in the order. Details are in 03-status-bars.md. | Decided, not built (B-063) | `D-014`; `D-030` |

## Bonus and Execution turns
| # | Rule | Status | Source |
|---|---|---|---|
| ET-12 | Emerald schedules a pinned bonus turn after a delay by tier, F→S: 6, 5, 4, 3, 2, 1, 0 turns. S grants two turns back to back. When the delay runs out, the pinned turn comes before the speed pick. | In game | `Public/Equipment/Crystals/AugmentStoneConstants.h:48`; `Private/Inventory/ItemExecutor.cpp:505-514`; `Private/Combat/TurnManager.cpp:549-590` |
| ET-13 | Pinned-turn delays count every turn anyone takes, other pinned turns included. A pinned turn whose holder is dead is dropped. | In game | `Private/Combat/TurnManager.cpp:502-519, 577-589, 660-666` |
| ET-14 | An Emerald bonus turn counts as a turn taken, so the holder's next speed-picked turn comes later. It moves a turn earlier rather than adding one. | Conflict (see below) | `Private/Combat/TurnManager.cpp:566-575` |
| ET-15 | The Extra Action effect adds one owed turn, a real extra turn in the speed schedule. | Code only (no ruling) | `Private/Skills/Effects/SkillEffectManager.cpp:1591-1599`; `Private/Combat/TurnManager.cpp:286-313` |
| ET-16 | A skill with an activation delay (a ritual) is armed when cast: all EP, HP and wear are paid then, and that turn ends. After the delay (counting every turn) an Execution turn fires it. That turn does only this: no start-of-turn effects and no choice. A stun or silence gained meanwhile can't stop it. Dead targets are dropped, and with none left it fizzles. If the caster is dead it is dropped. When several are ready together, the one with the faster spell speed (frozen at arming) fires first; ties go in arming order. | In game | `Private/Combat/Actions/ActionExecutor.cpp:162-202, 602-710`; `Private/Combat/CombatOrchestrator.cpp:657-665, 695-780`; `Private/Combat/TurnManager.cpp:332-350, 536-547` |

## What a turn allows
| # | Rule | Status | Source |
|---|---|---|---|
| ET-17 | Turn start, in order: per-turn flags reset; start-of-own-turn effects (heals and EP over time); always-on effects; "on turn start" triggers; status-bar decay; the Broken Darkness overload tick. If any of these kills the actor, the turn ends with no action. | In game | `Private/Combat/CombatOrchestrator.cpp:670-693, 983-1002`; `Private/Skills/Effects/SkillEffectManager.cpp:952-972` |
| ET-18 | You get one action per turn: Attack (the active weapon's basic attack), an Ability, a Refraction (spell), an Item, or Defend. The action ends the turn. | In game | `Public/Combat/Actions/EActionType.h:12-25`; `Private/UI/Combat/CombatCommandMenuSubsystem.cpp:568-588`; `Private/Combat/CombatOrchestrator.cpp:463-477, 553-568` |
| ET-19 | Switch Weapon (Generic) and Switch Ring (2+ rings) on the menu are free: they don't use the action and can be repeated. Sent as an action instead, Switch Weapon fails as "Unhandled action type". | Code only (no ruling) | `Private/UI/Combat/CombatCommandMenuSubsystem.cpp:906-949`; `Private/Combat/Actions/ActionExecutor.cpp:781-784, 887-900` |
| ET-20 | Defend costs 0 EP and gives you +50% Defense for 1 turn. Because durations tick down at the end of your own turn, the buff is gone before anyone attacks. | Conflict (see below) | `Private/Combat/Actions/ActionExecutor.cpp:2769-2804`; `Private/Skills/Effects/SkillEffectManager.cpp:974-1008` |
| ET-21 | Flee exists as an action type, but it isn't offered and does nothing if sent ("Unhandled action type"). | Code only (no ruling) | `Public/Combat/Actions/EActionType.h:24`; `Private/Combat/Actions/ActionExecutor.cpp:781-784` |
| ET-22 | Stunned: only a plain basic attack (no infusion charge) or Defend. No refractions, abilities, items, Flee or switching. | In game | `D-001` (B); `D-023` (A); `Private/Combat/Actions/ActionExecutor.cpp:165-174, 306-317`; `Private/UI/Combat/CombatCommandMenuSubsystem.cpp:559-566` |
| ET-23 | Silenced: only refractions are blocked. | Conflict (see below) | `Private/Combat/Actions/ActionExecutor.cpp:176-180` |
| ET-24 | An action needs its targets, all of them alive, and enough EP. If it is refused, a player is asked again and an AI passes its turn. | In game | `Private/Combat/Actions/ActionExecutor.cpp:183-224`; `Private/Combat/CombatOrchestrator.cpp:507-524` |

## Costs
| # | Rule | Status | Source |
|---|---|---|---|
| ET-25 | EP cost = base × infusion charge (L1 ×1.5 `L1_ENERGY_MULT`, L2 ×2.0, plus a surcharge that only rises with Raw/Spell damage) × Efficiency (stats alone up to 50% off, `EFFICIENCY_MAX`; with gear up to 90% off, `EFFICIENCY_GEAR_CEILING`) × tier power × tier gap. The cost you see is the cost you pay. | In game | `Private/Combat/Actions/ActionExecutor.cpp:329-432, 4789-4813`; `Public/Infusion/InfusionConstants.h:81-90`; `Public/Combat/CombatConstants.h:153-158` |
| ET-26 | These cost no EP: Defend; items; spells cast from a ring or weapon crystal; crystal-sourced abilities; Broken Darkness evolution casts (unless infused with the innate element); attacks with 0 base cost. | In game | `Private/Combat/Actions/ActionExecutor.cpp:348-372, 397-405, 420-425` |
| ET-27 | Infusing from Raw, from Innate on a spell, or from an Evolution costs HP = (infused EP cost ÷ MaxEP) × MaxHP × (1 − Resistance). It is paid after the action resolves and can kill you. Crystal sources pay durability instead (01-classes-and-loadouts.md). | In game | `Private/Combat/Actions/ActionExecutor.cpp:6380-6440`; `Private/Infusion/InfusionCostHelper.cpp:16-31` |
| ET-28 | Costs are paid when you commit, even if the action then misses or fails. | In game | `Private/Combat/Actions/ActionExecutor.cpp:752-755` |
| ET-29 | EP doesn't refill on its own. It comes back only from effects (Energy Restore, Restore EP %), Absorb, items and drains, and gains stop at MaxEP. A Resonator with nothing to spend EP on gains none. | Code only (no ruling) | `Private/Character/CharacterDataComponent.cpp:346-370`; `Public/AI/AIDecisionConstants.h:125-131` |
| ET-30 | Only Broken Darkness overflows. Its absorption can push EP above MaxEP, up to MaxEP + 30% (`OVERLOAD_CAPACITY_FRACTION`). While overloaded, each of its turn starts deals aura damage nearby, damages itself and leaks energy. It gets no normal EP gains. | In game | `Private/Character/CharacterDataComponent.cpp:351-355, 398-408`; `Private/Combat/Mechanics/BrokenDarknessManager.cpp:56`; `Private/Combat/CombatOrchestrator.cpp:1322-1360` |

## Effects: how long and when
| # | Rule | Status | Source |
|---|---|---|---|
| ET-31 | Every effect lasts one of three ways. **Instant** (duration 0, not permanent): acts once and is never kept. **N turns**. **Permanent** (an explicit switch, used by gear and auras): never expires. | In game | `Public/Skills/Effects/ActiveSkillEffect.h:282-289, 648-651`; `Private/Skills/Effects/SkillEffectManager.cpp:113-130` |
| ET-32 | A kept effect acts at one of these points: the start of the holder's own turn; the end of it (DoTs); always on (stat changes, read live); when its trigger condition holds; or once immediately. On spells, DoTs, heals over time and EP restores get their timing automatically. | In game | `Public/Skills/Effects/ESkillEffectTiming.h:20-46`; `Public/Skills/Effects/ActiveSkillEffect.h:305-343` |
| ET-33 | A new effect acts once the moment it lands, then again at its timing points, unless it opts out (`bDelayFirstExecution`). | Conflict (see below) | `Private/Skills/Effects/SkillEffectManager.cpp:226-237`; `Public/Skills/Effects/ActiveSkillEffect.h:219-223` |
| ET-34 | Durations count the holder's own turns. They tick down at the end of each of the holder's turns, and the effect is removed at 0, so it ends at the end of the holder's Nth turn. D-030 instead counts the turn it lands in as turn 1 and ends it as the next holder turn starts. | Conflict (see below) | `D-030`; `Private/Skills/Effects/SkillEffectManager.cpp:974-1008, 1099-1153` |
| ET-35 | DoTs can kill (no 1-HP floor). | In game | `Private/Skills/Effects/SkillEffectManager.cpp:1267-1279` |
| ET-36 | Charges: 0 means unlimited. Above 0, each trigger fire uses one, and at 0 the effect is removed whatever its duration. DoT ticks don't use charges. | In game | `Public/Skills/Effects/ActiveSkillEffect.h:124-133`; `Private/Skills/Effects/SkillEffectManager.cpp:1039-1047, 1155-1208` |
| ET-37 | Last Stand: a lethal blow instead leaves you at the effect's HP % (Sapphire: 50%, `LAST_STAND_HP_PERCENT`; 0 falls back to 30%) and uses a charge. With several Last Stands, the highest one fires. Sapphire's window, F→S: 2, 2, 3, 3, 4, 4, 5 turns, with 1 charge. Sapphire on a dead ally revives them at 30% (`REVIVE_HP_PERCENT`). | In game | `Private/Character/CharacterDataComponent.cpp:410-440`; `Private/Skills/Effects/SkillEffectManager.cpp:1210-1249`; `Private/Inventory/ItemExecutor.cpp:315-342`; `Public/Equipment/Crystals/CrystalEffectTable.h:123-145`; `Public/Inventory/ItemConstants.h:20-26` |
| ET-38 | Absorb: that % of each hit's final damage becomes EP for the target instead of HP loss (several Absorbs add up). Reflect: that % goes back to the attacker; Reflect Physical works only on attacks and abilities. There are no element-keyed wards and no HP shields. | In game | `Private/Combat/Actions/ActionExecutor.cpp:2897-2945` |

## Re-applying, stacking and identity
| # | Rule | Status | Source |
|---|---|---|---|
| ET-39 | An effect is "the same" when its effect ID matches. The same effect again on the same target: **stronger** replaces the value and refreshes the duration; **equal** refreshes the duration only; **weaker** is ignored. An effect can opt out of refreshing. | In game | `Private/Skills/Effects/SkillEffectManager.cpp:135-218`; `Public/Skills/Effects/ActiveSkillEffect.h:181-183, 633-640` |
| ET-40 | A stackable effect gains a stack instead, up to `MaxStacks` (default 3, range 1–99). The value is multiplied by the stacks, and the strongest per-stack value is kept. A weaker application still adds a stack but doesn't refresh. | In game | `Private/Skills/Effects/SkillEffectManager.cpp:152-182`; `Public/Skills/Effects/ActiveSkillEffect.h:168-179, 628-631` |
| ET-41 | Effects with different IDs add up: stat changes are summed. | In game | `Private/Skills/Effects/SkillEffectManager.cpp:1962-1980` |
| ET-42 | The same effect definition coming from two pieces of gear merges into one effect (+15% and +15% gives +15%). | Conflict (see below) | `Private/Skills/Effects/SkillEffectManager.cpp:319-330`; `docs/Mechanics/Status/SkillEffects.md:85, 92` |
| ET-43 | A once-per-fight effect fires once **per character**: two wearers each get their own use. Today the record is kept per effect ID, so the second wearer is blocked. | Decided, not built (B-062) | `D-011`; `Public/Skills/Effects/SkillEffectManager.h:423`; `Private/Skills/Effects/SkillEffectManager.cpp:98-105, 259-264` |
| ET-44 | A full-bar effect: the same source again refreshes the effect it already put on you; a different source adds its own. Identity = (status, element, source). Today each one gets a random ID, so they always stack. | Decided, not built (B-062) | `D-012`; `Private/Skills/Effects/SkillEffectManager.cpp:2427` |

## Removing effects, gear and auras
| # | Rule | Status | Source |
|---|---|---|---|
| ET-45 | Cleanse Self removes all your debuffs. Cleanse Allies removes every teammate's except your own. Iolite on an ally removes up to N debuffs; on an enemy, up to N buffs (`IOLITE_REMOVE_ALL` 99 = all). | In game | `Private/Skills/Effects/SkillEffectManager.cpp:1561-1588`; `Private/Inventory/ItemExecutor.cpp:1014-1045` |
| ET-46 | Cleanse and Iolite remove only effects **someone else applied**. Gear effects (yours or the enemy's) are immune. Today gear effects carry no "from gear" mark, so cleanse strips them by type. | Decided, not built (B-065) | `D-015`; `Private/Skills/Effects/SkillEffectManager.cpp:302-332, 746-785` |
| ET-47 | The plain Cleanse effect type and the Cleanse spell do nothing yet; the type has no handler. | Code only (no ruling); fix queued as B-066 | `Private/Skills/Effects/SkillEffectManager.cpp:1658-1661`; `docs/Gaps/Roadmap.md:13` |
| ET-48 | Aura spells should take on the caster's element for their buffs. Today they ship with no effects and do nothing when cast. | Code only (no ruling); design intent queued as B-066 | `docs/Gaps/Roadmap.md:11` (design intent, not in code) |
| ET-49 | A mid-fight weapon or ring swap changes gear **stats** now. Gear **effects** must follow too: the old gear's come off, the new gear's go on, and swapping back doesn't stack. Today effects are set once, at the start of the fight. | Decided, not built (B-081) | `D-008` (A); `Private/Combat/CombatOrchestrator.cpp:1181-1200`; `Private/Combat/TurnManager.cpp:404-412` |
| ET-50 | Item and skill text is generated from the data as the deckbuilder's tree: trigger → nested conditions → effects with numbers, duration and charges. There is also a one-line short form ("On Hit*: …"). In fights, conditional lines light green (buff) or red (debuff) while they hold. Today only per-effect display names exist. | Decided, not built (B-045, blocked by B-044) | `D-031`; `Public/Skills/Effects/SkillEffectDisplayNames.h` |

## Ending a fight
| # | Rule | Status | Source |
|---|---|---|---|
| ET-51 | After each turn's action (and after a death at turn start), the game checks: all enemies down = Victory; all of your side down = Defeat; both = Draw. It checks only at the end of a turn. | In game | `Private/Combat/CombatOrchestrator.cpp:588-643, 679-685, 1065-1084` |
| ET-52 | Win: the trial continues and that enemy stays beaten. Lose: the run ends and you go back to the hub. | Decided, not built (B-072, blocked by B-003; "stays beaten" needs B-090) | `D-002` |
| ET-53 | Every fight that ends clears all effects and status bars. A fight won, lost or drawn also destroys crystals that broke in it, then repairs the rest. A forced end doesn't repair. | In game | `Private/Combat/CombatOrchestrator.cpp:299-333, 601-613` |

## Open questions
- D-038 (ANSWERED): Silenced seals half your EP for 2 holder turns (C4-6); built in B-148.
- D-049 (OPEN): cap turns at 2 per round of the slowest (C4-1).
- D-050 (OPEN): how long Defend lasts (C4-2).
- D-051 (OPEN): how effect durations are counted (C4-3, C4-4).
- D-052 (OPEN): Emerald's bonus turn, extra or moved earlier (C4-5).
- D-053 (OPEN): the same effect from two gear pieces (C4-7).
- D-054 (OPEN): whether Turn Speed Up is a buff (C4-8).
- D-055 (OPEN): bonus and Execution turns and durations (Gaps).
- D-056 (OPEN): whether the generic Speed Buff/Debuff move turn order (C4-9).
- D-028 (not raised yet): B-044's effects-overhaul review will raise it, covering the effect model (durations, stacking by source, refresh policy) against GAS and the deckbuilder. It will touch ET-31…ET-44.

## Conflicts
- **C4-1 — Turn ratio cap.** `MAX_TURN_RATIO` 2 and `TURN_SPEED_THRESHOLD_DOUBLE` 15 say speed is "capped at 2:1" (`Public/Combat/CombatConstants.h:330-331`), and the gear-ceiling comment relies on it (`:297-302`). The scheduler never reads either constant, and the ratio is uncapped (`Private/Combat/TurnManager.cpp:265-270`); a strong Turn Speed Debuff can push a rival toward speed 1. Question, options and recommendation: **D-049**. The deckbuilder has no speed scheduler.
- **C4-2 — Defend wears off before anyone attacks.** `ExecuteDefend` gives +50% Defense for 1 turn, "until next turn" (`Private/Combat/Actions/ActionExecutor.cpp:2787-2792`; `docs/Mechanics/Combat/Defend.md:9`). The end-of-turn tick runs straight after, on the defender's own turn, and removes it (`Private/Combat/CombatOrchestrator.cpp:589`; `Private/Skills/Effects/SkillEffectManager.cpp:1125-1145`). Question, options and recommendation: **D-050** (the deckbuilder's Defend covers the enemy's turn).
- **C4-3 — How durations count, for every timed effect.** D-030 sets the deckbuilder's counting for status-bar caps: the turn it lands in counts as 1, and it ends when the holder's next turn after those starts. The code ends at the end of the holder's Nth turn (`Private/Skills/Effects/SkillEffectManager.cpp:1099-1153`). An effect applied on your own turn loses a turn; one applied by an enemy ends before the enemies act again. Question, options and recommendation: **D-051**.
- **C4-4 — DoT tick count.** A new DoT ticks when it lands and again at each of the holder's turn ends. So a 2-turn DoT from an enemy ticks 3 times (`Private/Skills/Effects/SkillEffectManager.cpp:232-237, 974-991`). The deckbuilder says "a damage over time ticks exactly N times" (Crown; `wor-deckbuilder/docs/reference/04-effects.md` "Lasts"). Part of **D-051** (exactly N ticks is the deckbuilder's rule).
- **C4-5 — An Emerald "bonus" turn is borrowed.** A pinned turn counts as taken with no credit, so it costs a later normal turn (`Private/Combat/TurnManager.cpp:566-575`). Extra Action, by contrast, adds a real turn (`:305`). The deckbuilder's `AddTurn` adds a turn (`wor-deckbuilder/docs/reference/04-effects.md` verb 7). Question, options and recommendation: **D-052**.
- **C4-6 — What Silenced blocks.** The code blocks only refractions (`Private/Combat/Actions/ActionExecutor.cpp:176-180`). The enum says "Silenced (No EP Spend)" (`Public/Skills/Effects/ESkillEffectType.h:129`), `docs/Mechanics/Status/StatusEffects.md:25, 42` says no spells, abilities or infusion, and D-023's context says infusion is "the EP-spend layer that Silenced already gates". Question, options and recommendation: **D-038** (same question as 03-status-bars.md C3-2). **Ruled (D-038):** a seal of 50% of max EP for 2 holder turns on every EP spend (B-148).
- **C4-7 — Same effect from two gear pieces.** Here one definition referenced by two items merges into one effect (`docs/Mechanics/Status/SkillEffects.md:85, 92`; `Private/Skills/Effects/SkillEffectManager.cpp:319-330`). The deckbuilder adds boosts from different cards, even two copies of one face, and refreshes only a repeat from the same card (`wor-deckbuilder/docs/reference/04-effects.md` `Stat` duration row). D-012 uses the same "different source adds" idea for caps. Question, options and recommendation: **D-053**.
- **C4-8 — "Turn Speed Up" counts as a debuff.** `ModifyTurnSpeed` is displayed "Turn Speed Up" (`Public/Skills/Effects/ESkillEffectType.h:152`) and adds speed (`Private/Combat/TurnManager.cpp:228-231`). Yet `IsDebuff` lists it (`ESkillEffectType.h:390`), so cleansing an ally strips it, and Iolite can't strip it from an enemy. Question, options and recommendation: **D-054**.
- **C4-9 — Speed Buff/Debuff (generic) don't move turns.** `docs/Mechanics/Combat/TurnOrder.md` says Slow (`SpeedDebuff`) pushes turns back. The scheduler reads only Turn Speed Up/Buff/Debuff (`Private/Combat/TurnManager.cpp:228-231`). `SpeedBuff`/`SpeedDebuff` trigger a recalculation (`Private/Skills/Effects/SkillEffectManager.cpp:2374-2375`) that changes nothing. Question, options and recommendation: **D-056**.
- **C4-10 — TurnManager.md is out of date.** It says pinned turns don't count down other delays and that dead combatants are owed turns (`docs/Architecture/TurnManager.md:62-63, 144-147`). The code says every turn counts down every delay, and the dead are owed nothing (`Private/Combat/TurnManager.cpp:577-589, 605-618`). Doc fix, no ruling: **B-117**.

## Gaps
- Flee: no rule for whether you can flee, from which fights, or what it costs (ET-21).
- Draw: D-002 covers win and lose but not a draw (both sides down at once, e.g. by Reflect or a DoT).
- Do bonus and Execution turns count as the holder's own turns for durations? Today both run the end-of-turn tick, but Execution turns skip turn-start effects (`Private/Combat/CombatOrchestrator.cpp:589, 661-665`). D-030 doesn't say. Raised as **D-055**.
- The turn-order strip isn't rebuilt when speed changes mid-turn (`Private/Combat/TurnManager.cpp:435-448`), only at the next turn start.
- No HP shields or element wards. The deckbuilder has Barrier, Shield and Ward as one absorb mechanism (`wor-deckbuilder/docs/reference/04-effects.md` "the absorbing family").
- A failed item use still uses up the item (B-060). A refused player action can close the re-opened menu (B-110).
- Defend's 50% and 1 turn are inline numbers, with no named constant to tune them by.
- No turn or round limit on a fight.
