# Effects overhaul review — 2026-10

**Job:** B-044 (research only, no code). **Compares:** (1) our effect system, (2) the deckbuilder's effect model, (3) GAS GameplayEffects as the standard.

**Crown's direction (2026-10-26):** use the standard (GAS) where it already does the job. Don't reinvent it. Write custom code only for real gaps, for example turn-based durations.

**How to read it:**
- Our code is cited as `path:line`, relative to `Source/world_of_refraction/`. Docs are cited from the repo root.
- Deckbuilder pages are cited as `wor-deckbuilder/docs/...`. Its C#/card names are translated: Affinity / `EWoRElement` → element, card → item or skill, Character → combatant, "Refraction" → spell, Block → Guard (D-084).
- GAS details are from my own knowledge of Unreal 5.x. The plugin is not enabled yet (B-004). Names marked *(verify)* should be checked in the first B-042 step.
- Open questions are at the end as **Q-044, Q-044b, …**, each with options and a recommendation. Crown enters them in `docs/DesignDecisions.md`.

---

## Summary

| Area | Ours today | Deckbuilder | GAS | Verdict |
|---|---|---|---|---|
| How long | Instant / N turns / permanent | Instant / N turns / until removed / while in play | Instant / duration (seconds) / infinite | GAS for instant and infinite; **turn counting is custom** |
| Periodic | Start or end of own turn | Turn start, exactly N times | Period in seconds | **Custom tick**, GAS executes it |
| Stacking and refresh | By effect ID; strength gate | By source item; replace + refresh | By GE class, per source ASC or per target | **Custom identity**; GAS stacking can't key on the item |
| Triggers and conditions | Our own dispatcher | Trigger → nested state-checks | No effect triggers (abilities + events) | **Stays custom** in B-042 |
| Tags, immunity, cleanse | Enum checks, scattered | Buff/debuff filter | Tags, tag requirements, immunity, remove-by-query | **GAS does this** |
| Stat changes | Summed by type | Stat rows add | Modifiers on attributes | **GAS does this** |
| Damage maths | `UDamageCalculator` | Fixed order pipeline | Execution calculation | **GAS wrapper**, formula stays ours (B-145, B-146) |
| Absorb (Barrier / Shield / Ward) | None (only "Absorb" to EP) | One mechanism, charges or pool | None | **Custom spending**, GAS holds the effect |
| VFX / UI | Delegates | — | GameplayCues + ASC events | **GAS does this** |
| Descriptions (D-031) | Display names only | Generated text tree | — | **Custom** (B-045); data is nearly ready |

**In one line:** GAS holds the effects, their tags and their stat changes. Our own code keeps counting turns, deciding identity (same source refreshes, other source adds), spending charges and absorbs, firing triggers, and writing the text.

---

## 1. Our effect system today

### 1.1 Shape

- **Authoring:** a `UEffectDefinition` bundle (`Public/Skills/Effects/EffectDefinition.h:27-37`) holds `FSkillEffect`s. Items and skills only reference bundles.
- **`FSkillEffect`** (`Public/Skills/Effects/FSkillEffect.h`):
  - `Conditions[]` (`:43`): the gate. Empty means always.
  - `Payloads[]` (`:48`): each payload is one applied effect.
  - `bStackable` (`:55`), `MaxStacks` (`:60`), `bFiresOncePerMatch` (`:65`).
- **`FSkillCondition`** (`Public/Skills/Effects/FSkillCondition.h:22-42`): `Trigger`, `Threshold` (a %), `Combine` (And / Or with the previous entry), `Subject` (Self / SelfTeam / Target / TargetTeam). `EvaluateConditionGroup` (`:57`) folds the list.
- **`FSkillEffectPayload`** (`Public/Skills/Effects/FSkillEffectPayload.h`): `EffectType` (`:24`), `Element` (`:33`), `Magnitude` (`:38`) or `Value` (`:42`), `Duration` in turns (`:47`), `Charges` (`:55`), `ProcessTiming` (`:62`), `bPermanent` (`:69`), `bDelayFirstExecution` (`:75`), `Target` + `TargetCount` (`:79-83`), `DrainPercent`, `AttackCostScale` (`:99`).
- **`ESkillEffectType`** (`Public/Skills/Effects/ESkillEffectType.h:19`): 110 values. Status-bar results, stat buff/debuff pairs, restores, cleanse, gates (Stun, Silenced, HealBlock), immunities, reflect, Last Stand, push/pull and more. Append-only.
- **`ESkillTrigger`** (`Public/Skills/Effects/ESkillTrigger.h:10-43`): 27 values. Events (On Hit, On Crit, On Guard, On Dodge…), turn start/end, and HP/EP % thresholds.
- **`ESkillEffectTiming`** (`Public/Skills/Effects/ESkillEffectTiming.h:26-45`): Immediate, StartOfOwnTurn, EndOfOwnTurn, OnTrigger, Persistent, Instant. Immediate and Instant overlap; Instant is "vestigial" (`Public/Skills/Effects/ActiveSkillEffect.h:648`).

### 1.2 Runtime

- **`USkillEffectManager`** is a `UGameInstanceSubsystem` (`Public/Skills/Effects/SkillEffectManager.h:69`). It owns all runtime state: `ActiveEffects` per actor (`:425`), `FiredOnceThisMatch` (`:429`), `ArmedConditionals` (`:435`). This is per-fight state in a subsystem, which D-064 says not to add to.
- **`FActiveSkillEffect`** (`Public/Skills/Effects/ActiveSkillEffect.h:27`) is the runtime copy: `EffectID` (`:41`), `RemainingTurns` (`:110`), `bPermanent` (`:118`), `Charges` (`:133`), `EffectValue` (`:145`), `Element` (`:149`), stacking fields (`:170-189`), `SourceActor` (`:196`).
- **Apply** (`Private/Skills/Effects/SkillEffectManager.cpp:84-270`):
  1. Immunity check (`:94`), fires-once gate (`:103`).
  2. Duration 0 and not permanent: run once, never store (`:126-133`).
  3. Same `EffectID` already on the target: stack (`:159-182`), or the **strength gate**: stronger replaces the value and refreshes, equal refreshes, weaker is ignored (`:146-218`).
  4. New: store, then act once at once unless `bDelayFirstExecution` (`:235-240`).
- **Turns:** `ProcessStartOfTurnEffects` (`:955`), `ProcessEndOfTurnEffects` (`:977`), `TickDurations` (`:1102`). Durations go down at the end of the holder's turn and the effect is removed at 0.
- **Triggers:** `ProcessTriggerEffects` (`:1013`) for turn triggers, `OnDamageDealtHandler` (`:1700`), `OnDefenseResolvedHandler` (`:1771`) for defense outcomes. `SkillTriggerUtils::DefenseOutcomeToTriggers` maps an outcome to triggers (`Public/Skills/Effects/SkillTriggerUtils.h:40`).
- **Charges:** `ConsumeCharge` (`:1158`); a trigger fire spends one. Last Stand uses it (`:1213`).
- **What an effect does:** one big `switch` in `ApplyEffectLogic` (`:1254`). Stat types are passive and are summed by `GetTotalStatModifier` (`:1993`).
- **Identity:** `EffectIdentity::PackEffectID(DefID, BundleIndex, PayloadIndex)` (`Public/Skills/Effects/EffectIdentity.h:24`). Full-bar effects use `FMath::Rand()` (`Private/Skills/Effects/SkillEffectManager.cpp:2476`), so they always stack (D-012 fixes this).
- **Status bars:** `UStatusBuildupManager` (`Private/Skills/Effects/StatusBuildupManager.cpp:300` `AddStatusBuildup`, `:564` decay, `:603` cap → effect). Cap is `STATUS_EFFECT_THRESHOLD` 100 (`Public/Combat/CombatConstants.h:360`).

### 1.3 Known problems (from the docs and code)

- Durations don't match D-051 (exactly N ticks, the landing turn counts). A new DoT ticks on landing (`SkillEffectManager.cpp:235-240`) and at each turn end, so a 2-turn DoT ticks 3 times (`docs/Rules/04-effects-and-turns.md` C4-3, C4-4).
- Identity is the definition, not the source item. D-053 says the item is the source.
- Once-per-match is per effect ID, not per character (D-011).
- Cleanse can't tell gear effects apart (D-015).
- Immunity is split: element immunities are only checked in buildup (`StatusBuildupManager.cpp:59`), the rest in `IsImmuneToEffectType` (`SkillEffectManager.cpp:2132`).
- Several types are log-only stubs (`GuaranteedCrit`, `IgnoreDefense`, `DoubleHit`, `RandomSkill`; `docs/Architecture/SkillEffectSystem.md` Known Limitations).
- No shields or element wards (`docs/Rules/04-effects-and-turns.md` ET-38). "Absorb" today turns a % of damage into EP (`Private/Combat/Actions/ActionExecutor.cpp:3002-3005`).
- All state lives in a GameInstance subsystem, which never replicates (D-064).

---

## 2. The deckbuilder's effect model

Sources: `wor-deckbuilder/docs/reference/04-effects.md`, `05-triggers-conditions.md`, `07-text-grammar.md`; match rulings in `wor-deckbuilder/docs/deferrals.md` (C9, C11) and `wor-deckbuilder/docs/design/WoR_TCG_Pivot_Design.md` §7 "Damage, turns and death".

### 2.1 The row

One authored effect is a **row**: verb + numbers + target + condition.

| Part | What it is | Our name |
|---|---|---|
| **Verb** | What happens: `Stat`, `DoT`, `Heal`, `Barrier`, `Shield`, `Ward`, `Reflect`, `IgnoreDefense`, `InvertDamage`, `InvertHeal`, `AddTurn`, `GainEnergy`, `RemoveEffect`, … (29 in all, `04-effects.md` "The verb matrix") | `ESkillEffectType` |
| **Magnitude** | Always a quantity, never a delay or a flag (`04-effects.md` "`Magnitude` — always a quantity") | `Magnitude` / `Value` |
| **Lasts** | One choice: Instant, N turns, Until removed, While in play (`04-effects.md` "`Lasts`") | `Duration` + `bPermanent` |
| **Delay** | Turns before it happens; separate from how long it lasts | — (rituals are a skill feature, not an effect field) |
| **MaxStacks / UnlimitedStacks** | Default 1 = doesn't stack | `bStackable` + `MaxStacks` |
| **Charges / ChargeMode** | How a number is spent: Uses (N charges) or Pool (a budget) | `Charges` (count only) |
| **Target + count** | Self, Ally, Enemy; one, two, all | `Target` + `TargetCount` |
| **Condition** | One trigger, then nested state-checks | `Conditions[]` |
| **Instead** | Replace the row above when its condition holds | — |

**The verb is the meaning, never the sign.** A heal that harms is a different verb. Only `Stat` allows a negative magnitude (`04-effects.md` "Rules encoded in the matrix"). This matches our buff/debuff pairs.

### 2.2 Triggers and conditions

- **Every row says when it fires** (`05-triggers-conditions.md` intro). Trigger groups: Immediate, combat events (On Hit, On Damage, On Damage Received, On Guard, On Heal Received), turn start/end, numeric turns, While equipped, When placed / attached, card events, death and revive, buff/debuff events, damage over time.
- **A combat trigger can carry an element:** "On Fire Hit". The element is the incoming event's, not the item's.
- **`EventWho`** (whose event) is separate from **`Who`** (whose state is checked). Our `Subject` per entry already gives both.
- **Nested conditions** (`05-triggers-conditions.md` "Nested conditions"): state-checks only, one list, **every one must hold**. There is no "or". Several triggers on one item are several rows.
- **HP checks are absolute** ("below 30 HP"); percentages were retired twice (`05-triggers-conditions.md` "The retired percentage comparison"). **Ours are percentages.** This is a real difference (Q-044h).
- **Order:** triggered rows queue first in, first out; same moment → the one applied first fires first, by a match-wide arrival number; chain depth cap 5 (`05-triggers-conditions.md` "Order and depth").

### 2.3 Lasts, ticks and counting

- **"For N turns" counts the holder's turns.** Applied during the holder's own turn, that turn is turn 1. It ends when the holder's next turn after those starts (`04-effects.md` "`Lasts`"). Crown adopted this for every timed effect (D-030, D-051), every turn the holder takes counts (D-055).
- **A DoT ticks exactly N times**, and so does every payout (heal, Energy), at the holder's next N turn starts (`04-effects.md` "`Lasts`"; Pivot §7 "Durations": "DoT/HoT tick at the start of that player's turn"). Our DoTs tick at the end of the turn (Q-044c).
- **A DoT snapshots its source's boost** when it lands or is refreshed (`04-effects.md` DoT row).
- **A span outlives its item:** once applied, it runs its duration even if the item is gone (`04-effects.md` "A span outlives its card"). A **bonus** (always-on gear stat) ends with its item.

### 2.4 Re-apply and stacks

From `04-effects.md` `Stat` row and the resolver (`wor-deckbuilder/wor.core/Match/EffectResolver.cs:143-185`):
- **Buffs and debuffs (Enhancement / Impairment):** the same source item used again **replaces** the held one, with a fresh duration. Boosts from **different** items add up, even two copies of one item. This is D-053.
- **Other rows:** the same row again on the same holder refreshes the duration. If it stacks (cap 2 or more) it also gains a stack, up to the cap. A stack is the held row's magnitude once more.
- **No strength comparison.** Ours compares strength (stronger / equal / weaker, ET-39). Q-044.

### 2.5 The absorbing family: Barrier, Shield, Ward (D-092)

From `04-effects.md` "The charge family", `02-enums.md` `EChargeMode`, `07-text-grammar.md` "The three absorbing verbs share one builder", and the resolver (`wor-deckbuilder/wor.core/Match/SkillCommands.cs:332-470, 600-650`).

**One mechanism, three names. They differ only in what they block:**

| Verb | Blocks | Where the scope comes from |
|---|---|---|
| **Barrier** | any damage | nothing (no scope word) |
| **Shield** | physical damage | the verb |
| **Ward** | one element (required) | the row |

- A "Fire Shield" can't be authored: that's a Ward. A Ward with no element is reported; it would just be a Barrier.
- A Ward keyed "All" also blocks physical. "Elemental" blocks every element but not physical.

**How the number is spent (`ChargeMode`):**

| Mode | Meaning | Text |
|---|---|---|
| **Uses**, N charges | Each damage instance it catches spends one charge; each takes **up to** the magnitude | "Negate the next 5 damage you take." / "…the next 2 attacks" |
| **Uses**, unlimited | Applies to every damage instance, spends nothing | "Whenever you take Fire damage, negate up to 5 of it." |
| **Pool** | One budget drawn down across hits until spent | "Negate 5 damage you take in total." |

**Rules (ported as written, per D-092):**
1. Defender charges spend on **every damage instance, DoT ticks included** (`deferrals.md` C9 ruling 2).
2. A charge that catches nothing isn't spent (a Fire Ward against a Water hit).
3. With duration 0 it stands until spent. With a duration, it ends at whichever comes first (`WoR_TCG_Pivot_Design.md` §8 item 183).
4. A counted, timed charge **refills at the holder's turn start** (`wor-deckbuilder/wor.core/Match/MatchState.cs:476`). So "for 3 of your turns, negate the next 5 damage" blocks one hit per turn.
5. Charges of one verb spend **in the order they were gained**. Order across verbs: **Ward, then Shield, then Barrier** (`SkillCommands.cs:444-446`).
6. Damage order in the deckbuilder: attacker boosts → attacker's Ignore Defense → Invert → Reflect → **Ward → Shield → Barrier** → Defense → Guard halves → minimum 1, unless the charges stopped it all (`SkillCommands.cs:332-345`).
7. What the absorbers and Guard stop is what "On Guard" (deckbuilder "On Block") reports (`05-triggers-conditions.md` "The events").
8. A hit stopped entirely is still a hit (On Hit fires), but not damage (On Damage doesn't).

**Where our game needs its own answer** (D-092 asks for a `Q-<job>`):
- **Order against our Guard and Defense.** Ours is the reverse: Guard reduces the hit first (`Private/Combat/Defense/DefenseSystem.cpp:514`), then Defense as a % (`Private/Combat/Damage/DamageCalculator.cpp:256-263`), then today's Absorb-to-EP and Reflect (`Private/Combat/Actions/ActionExecutor.cpp:2995-3044`). The deckbuilder's Defense is a flat number, ours a percentage. Q-044e.
- **Multi-hit skills.** Our skills can have several impacts. Q-044f.
- **What "physical" is.** Until the damage type of D-037 exists, physical = no element + a physical type (Slash / Pierce / Impact). Ice (D-029) is a Ward element.
- **Name clash.** Our `AbsorbDamage` ("Absorb", % of damage to EP, `ESkillEffectType.h:166`) is a different effect. Keep it; the new verbs print "negate", as the deckbuilder's text does (`07-text-grammar.md` verb bodies).

### 2.6 Text (D-031)

From `07-text-grammar.md` "The card-text tree" and "The short form":
- **Level 1 is the trigger heading** ("On Hit", "At the start of your turn", "Activate (2 EN)"). Headings with the same words merge.
- **Passive** holds only always-on rows and bonuses.
- **Each indented level is one state-check**, and all must hold ("While your HP is below 30").
- **Leaves are the effects**, with numbers, duration and charges. Each leaf names its source list and index so a screen can light it.
- **Short form:** one line per heading, "On Hit*: Heal 3, gain 1 Energy"; `*` means conditions below.

---

## 3. GAS GameplayEffects — the standard

A GameplayEffect (GE) is a data object. It is applied as a **spec** (GE class + level + caller-set values + context) to an **AbilitySystemComponent** (ASC). An applied effect lives as an **active effect** with a handle.

### 3.1 What GAS gives

| Feature | GAS | Fits turn-based? |
|---|---|---|
| **Duration policy** | `Instant` (changes base values, not stored), `HasDuration` (seconds), `Infinite` (until removed) | Instant and Infinite: yes. Seconds: no |
| **Periodic** | `Period` in seconds; `bExecutePeriodicEffectOnApplication`; periodic GEs re-run their modifiers and executions | No (seconds). The execution part is reusable |
| **Stacking** | `StackingType`: None, AggregateBySource (one stack per instigator ASC), AggregateByTarget (one stack per target). `StackLimitCount` | Keys on **GE class + instigator ASC**, not on our item (see 3.2) |
| **Refresh / expiry** | `StackDurationRefreshPolicy` (refresh on apply, never), `StackPeriodResetPolicy`, `StackExpirationPolicy` (clear all, remove one and refresh, refresh), overflow effects, deny overflow | Seconds-based; our tracker does the turn version |
| **Tags** | GE components (UE 5.3+): grant tags to the target (`UTargetTagsGameplayEffectComponent`), tag the GE itself (`UAssetTagsGameplayEffectComponent`), tag requirements to apply / stay active / be removed (`UTargetTagRequirementsGameplayEffectComponent`), immunity (`UImmunityGameplayEffectComponent`), remove other effects (`URemoveOtherGameplayEffectComponent`), block ability tags, grant abilities, chance to apply, custom can-apply *(verify names in 5.8)* | Yes |
| **Modifiers** | On an attribute: add, multiply, divide, override (newer engines add a final add and a compound multiply *(verify)*). Magnitude from a scalable float, another attribute, a custom magnitude class, or **SetByCaller** (a value the code puts on the spec under a tag) | Yes |
| **Execution calculations** | `UGameplayEffectExecutionCalculation`: C++ that reads captured attributes (snapshot or live) and outputs changes. Only on Instant or periodic GEs | Yes (damage, buildup, DoT tick) |
| **Custom application requirement** | `UGameplayEffectCustomApplicationRequirement`: C++ veto at apply time | Yes |
| **Context** | `FGameplayEffectContext`: instigator, effect causer, **source object**; can be subclassed (Lyra does) | Yes (carries our source item) |
| **Removal** | By handle (optionally N stacks), by tag, by `FGameplayEffectQuery` | Yes (cleanse) |
| **Events** | Effect added / removed, stack changed, tag count changed | Yes (UI, tracker) |
| **GameplayCues** | `GameplayCue.*` tags on a GE trigger VFX/SFX: execute (burst) for instants and ticks, add/remove (looping) for lasting effects. Cosmetic only; not for game logic | Yes |
| **Debug** | `showdebug abilitysystem` | Free |

### 3.2 Where GAS stops

1. **No turns.** Durations and periods are seconds. Turn-based games keep effects `Infinite` and remove them from their own turn code. `Design/GASMigration.md` §2 already plans this (the turn tracker).
2. **Stacking can't key on our item.** GAS stacks by GE class, then by instigator ASC (AggregateBySource) or target. Our family GE classes (one class, many definitions) and D-053 (the item is the source; a wearer's ring and sword both have the wearer as instigator) both break that. So family GEs use **`StackingType` None**, and our tracker decides refresh / add / stack. A stack count is shown by updating the SetByCaller magnitude on the active effect (`UpdateActiveGameplayEffectSetByCallerMagnitude` *(verify)*).
3. **No strength rule.** GAS never compares magnitudes. Fine if Q-044 picks the deckbuilder rule (replace), which needs none.
4. **No charges or pools.** A GE has no "uses left". Custom.
5. **No event triggers on effects.** The GAS way is a passive GameplayAbility triggered by a GameplayEvent or a tag. That is stage 4 (B-043) work.
6. **No numeric state-check gates.** Tag requirements cover "while Burning". "While HP below 30%" needs our own check (or a custom application requirement at apply time).
7. **No text.** Descriptions come from our authoring data, not from GEs.

---

## 4. Turn-based durations: the custom piece

This is the one real gap Crown named. It should be **one small, tested piece**, used by everything.

### 4.1 The counting rule (D-030, D-051, D-055, deckbuilder `04-effects.md` "`Lasts`")

- A span of N counts **the holder's turns**. Every turn the holder takes counts, bonus and Execution turns included (D-055).
- If it lands **during the holder's own turn**, that turn is turn 1.
- It **ends when the holder's next turn after those N starts**, before that turn's start-of-turn payouts.
- A payout (DoT, heal over time, Energy over time) happens **exactly N times**, at the holder's next N turn starts (Q-044c), and the effect ends with its last payout.

Worked examples (the deckbuilder's, `09-fixtures.md` `TestHandedRows`):
- "+1 Defense for 1 turn" used on your turn: covers the enemy's turn, ends as your next turn starts. This is also Defend (D-050).
- A debuff "for 1 turn" put on an enemy during your turn: covers their next turn, ends as their following turn starts.

### 4.2 Shape

- **`FTurnSpan`** (a plain struct + pure functions, no GAS needed): `TurnsLeft`, `bLandingTurnCounted`. `OnHolderTurnStart()` → expire? / pay out? `OnHolderTurnEnd()` → count down. Spec-tested on its own. It can ship **before** GAS, inside B-062/B-063 (D-030 already asks for "one shared helper").
- **`UTurnEffectTrackerComponent`** (on each combatant, per `Design/GASMigration.md` §2): `ActiveHandle → { Identity, FTurnSpan, ChargesLeft, PoolLeft, ArrivalNumber, TickSpec }`. Called by the orchestrator at the holder's turn start and end. Removes the GE by handle at expiry. Listens to the ASC's "effect removed" event so a cleanse or death keeps its map clean.
- **The tracker also holds the per-fight sets** that sit in the subsystem today: fires-once (`SkillEffectManager.h:429`) and armed conditionals (`:435`). That follows D-064.

### 4.3 Periodic ticks without seconds

- The lasting GE is `Infinite` with **no Period**. It carries tags (`Status.Burn`, cue) and nothing else.
- At the tick moment, the tracker applies the **tick spec**, an Instant GE with the damage or heal execution. The spec is built **once when the DoT lands** (or is refreshed), so the source's boost is snapshotted then (deckbuilder DoT row, D-012 "re-snapshot"). Re-applying a stored spec is stock GAS.
- No custom period code, no timers, no Tick.

---

## 5. Identity, refresh and stacks

### 5.1 Rulings that apply

| Ruling | What it says | Effect on the design |
|---|---|---|
| **D-011** | Once per match = once **per character** | Fires-once key = (holder, identity), cleared at fight start |
| **D-012** | Full-bar effects: same source refreshes (and re-snapshots), another source adds; identity = (status, element, source) | Replaces `FMath::Rand()` (`SkillEffectManager.cpp:2476`) |
| **D-053** | The **item (or skill) is the source**. Same item again refreshes; a different item adds. A `bDoesNotStack` flag on the definition for things that break if doubled | Identity = (holder, definition, effect index, payload index, **source**). Source = the item's `PersistentID`, or the skill, or the actor |
| **D-015** | Cleanse only removes what **someone else** applied; gear effects are immune | Gear GEs carry an origin tag (`Effect.Origin.Gear`); cleanse queries exclude it |
| **D-051 / D-055** | One counting rule, exactly N ticks, every holder turn counts | Section 4 |
| **D-088** | Discussion only. Latest: one utility + one battle action per turn; Turn Speed stays | **No change to effects.** Durations still count the holder's turns (its own notes say so) |

### 5.2 The identity key

```
EffectIdentity = { Holder, DefinitionId, EffectIndex, PayloadIndex, SourceId }
SourceId       = item PersistentID | skill asset | source actor (for full bars: + status + element)
```

- The source travels in a **custom `FGameplayEffectContext`** (standard GAS extension, as Lyra does) so cleanse, UI and the tracker can read it from any active effect.
- `PackEffectID` (`EffectIdentity.h:24`) and its ID windows retire with `FActiveSkillEffect`. No more ID collisions, because identity is a struct, not a packed int.
- **Re-apply** (if Q-044 = A): same identity → remove-and-reapply (or update magnitude) with a fresh span. **Stack** (if stackable, under the cap): stack count + 1, magnitude updated, span refreshed (Q-044b).
- **`bDoesNotStack`** (D-053): a second source with the same definition adds nothing.

---

## 6. Triggers and conditions under GAS

- **B-042 keeps our trigger dispatcher** (`ProcessTriggerEffects`, `OnDefenseResolvedHandler`, the armed-conditionals list). It already works with the defense windows and turn events, which stay ours (`Design/GASMigration.md` "What stays ours").
- What changes: when a trigger fires, the payload goes through the **spec builder** to the ASC, not into `FActiveSkillEffect`.
- **Add GameplayEvents now** (`Event.Combat.Hit`, `Event.Combat.DefenseResolved`, `Event.Combat.TurnStart`/`TurnEnd`, per `Design/GASMigration.md` §4). The dispatcher listens to them, so B-043 can move triggers onto passive abilities later without touching senders. Q-044j.
- **Tag-based conditions** ("if the target is Burning") use GAS tag queries. **Numeric conditions** (HP/EP %) stay in `EvaluateConditionGroup` (`FSkillCondition.h:57`).

---

## 7. What D-031's nested descriptions need from the data

D-031: item and skill text is generated from the data as **trigger → nested conditions → effects**, with a short form and live lighting in fights.

### 7.1 What we already have

| Text part | Our data | Status |
|---|---|---|
| Trigger heading | The event trigger in `Conditions[]` (`FSkillCondition.h:22`) | Have it, but mixed in with state-checks |
| Whose event | `Subject` on the trigger entry (`:37`) | Have it (= deckbuilder `EventWho`) |
| Nested conditions | Threshold entries in `Conditions[]` + their `Subject` | Have it |
| Leaves | `Payloads[]`: type, magnitude/value, element, duration, charges, target, count | Have it |
| Stacks | `bStackable`, `MaxStacks` → "stacks up to N times" | Have it |
| Passive | Gear effects with no event trigger (`FSkillEffect::IsConditionalEffect`, `FSkillEffect.h:178`) | Have it |
| Leaf address for lighting | `FGatheredEffect` = definition + bundle index (`Public/Skills/Effects/FGatheredEffect.h`), + payload index | Have it |
| "Holds now" for lighting | `IsSingleTriggerMet` (`SkillEffectManager.cpp:2287`) + `ResolveSubjectActors` (`:2354`) | Logic exists; needs a const public query |

### 7.2 What's missing or doesn't fit

1. **Or.** Our `Combine` allows Or (`FSkillCondition.h:32`). The tree says "all must hold". Q-044h covers it with the % question; recommended: retire Or for new data.
2. **Trigger and state-checks in one list.** The deckbuilder has one trigger per row, then state-checks. Ours mixes them. **No schema change needed:** a validator rule ("at most one event trigger per effect; the rest are state-checks") lets the generator split them. An effect wanting two triggers is two effects (or one leaf printed under two headings).
3. **Event element** ("On Fire Hit"). Ours has none. Not needed now; add a field only when an item needs it.
4. **Delay, Instead.** None in ours. Not needed now.
5. **Player-facing words.** `SkillEffectDisplayNames::GetDisplayName` returns `FString` (`Public/Skills/Effects/SkillEffectDisplayNames.h:27`); D-061/D-062 need `FText` and `LOCTEXT` name tables. B-045's job.
6. **Old text.** `FSkillEffect::GetDescription` (`FSkillEffect.h:191`) is a debug string. B-045 replaces it for players and keeps it for logs.

**GAS doesn't change any of this.** The text reads the authoring data (`UEffectDefinition`), which stays the source of truth (`Design/GASMigration.md` §3). Live lighting reads attributes through the same getters after B-041.

---

## 8. Status buildup under GAS

Already planned in `Design/GASMigration.md` stage 3 step 4. What fits where:

| Piece | Today | Under GAS |
|---|---|---|
| The bar | `FStatusBarState.CurrentBuildup` (`Public/Skills/Effects/StatusBuildupManager.h:194-204`) | `Buildup` / `MaxBuildup` attributes (B-041) |
| Adding buildup | `AddStatusBuildup` with amplify × (1 − resistance) (`StatusBuildupManager.cpp:300`) | Instant GE with an execution calculation (B-145, B-146) |
| Immunity | Enum checks in buildup and in the effect manager | `Immunity.*` tags; one tag query |
| Cap → effect | `TriggerSkillEffectFromBuildup` (`:603`) → `ApplyTriggeredSkillEffect` | Attribute set's post-execute sees the cap → spec builder applies the status-break GE (D-090) with identity (status, element, source) per D-012 |
| Decay | `ProcessStatusBarDecay` at turn start (`:564`) | Tracker's turn-start step applies an instant GE |
| Pending element | Field on the bar state | Stays a small custom field (not a number, not an attribute) |

---

## 9. Feature table

"Ours" cites `Source/world_of_refraction/`. "Gap" says what custom code is needed after GAS.

| Feature | Ours | Deckbuilder | GAS | Gap → custom code |
|---|---|---|---|---|
| **Instant** | Duration 0, not stored (`Private/Skills/Effects/SkillEffectManager.cpp:126-133`) | Lasts Instant | Instant GE | None |
| **Duration in turns** | `RemainingTurns`, down at own turn end (`Public/Skills/Effects/ActiveSkillEffect.h:110`; `SkillEffectManager.cpp:1102`) | Holder's turns, landing turn = 1, ends at next start (`04-effects.md` "Lasts") | Seconds only | **`FTurnSpan` + tracker** (section 4) |
| **Permanent / while equipped** | `bPermanent` (`Public/Skills/Effects/FSkillEffectPayload.h:69`) | While in play / until removed | Infinite GE | Removal on unequip (B-081) and fight end: small |
| **Periodic tick** | Start/End-of-own-turn timing (`Public/Skills/Effects/ESkillEffectTiming.h:26-45`; `SkillEffectManager.cpp:955, 977`) | Turn start, exactly N (`04-effects.md` "Lasts") | Period in seconds | **Tracker applies the stored tick spec** |
| **Tick on landing** | Yes, unless `bDelayFirstExecution` (`SkillEffectManager.cpp:235-240`) | No | `bExecutePeriodicEffectOnApplication` | None (D-051: exactly N, so no landing tick) |
| **Snapshot of source boost** | Value fixed at build | DoT snapshots at landing/refresh (`04-effects.md` DoT row) | Snapshot attribute capture in the spec | None (store the spec) |
| **Stacking** | `bStackable`, `MaxStacks`, value × stacks, strongest kept (`Public/Skills/Effects/FSkillEffect.h:55-60`; `SkillEffectManager.cpp:159-182`) | Default 1; stack = held magnitude again; refresh each time | Per GE class, by source ASC or target | **Tracker decides; GE stacking None**; magnitude via SetByCaller update |
| **Re-apply / refresh** | Strength gate: stronger / equal / weaker (`SkillEffectManager.cpp:146-218`) | Same source replaces + refreshes | Refresh policy (seconds) | **Tracker** (Q-044) |
| **Source identity** | `PackEffectID(Def, Bundle, Payload)` (`Public/Skills/Effects/EffectIdentity.h:24`); random for full bars (`SkillEffectManager.cpp:2476`) | Per source card (`EffectResolver.cs:151-153`) | Instigator ASC + source object | **Identity struct**; source in a custom effect context (D-053) |
| **Once per match** | Set of IDs in the subsystem (`Public/Skills/Effects/SkillEffectManager.h:429`) | Per card | — | Tracker set keyed (holder, identity) (D-011) |
| **Charges** | Count; a trigger fire spends one (`SkillEffectManager.cpp:1158`) | Uses (N or unlimited), Pool; timed counted refill per turn | — | **Tracker** `ChargesLeft` / `PoolLeft` |
| **Triggers** | 27 `ESkillTrigger`s; dispatcher (`SkillEffectManager.cpp:1013, 1700, 1771`; `Public/Skills/Effects/SkillTriggerUtils.h:40`) | One trigger per row; event element; `EventWho` | Abilities triggered by events/tags | **Keep dispatcher** (B-042); emit GameplayEvents (Q-044j) |
| **Conditions** | `Conditions[]` And/Or, % thresholds, subject (`Public/Skills/Effects/FSkillCondition.h:22-42, 57`) | Nested state-checks, all must hold, absolute HP | Tag requirements; custom application requirement | **Keep `EvaluateConditionGroup`**; tag queries for tag checks (Q-044h) |
| **Targets** | Per payload `Target` + `TargetCount` (`FSkillEffectPayload.h:79-83`); `UActionExecutor::GetEffectTargets` (`Private/Combat/Actions/ActionExecutor.cpp:6108`) | Target + count | Target data (abilities) | Keep ours |
| **Absorb / pool** | None; `AbsorbDamage` = % to EP (`Private/Combat/Actions/ActionExecutor.cpp:3002-3005`) | Barrier / Shield / Ward, Uses or Pool (`SkillCommands.cs:444-446, 600-650`) | — (often a shield attribute) | **New `UGE_Absorb` + spend function in the damage path** (D-092, section 2.5) |
| **Reflect** | % back as pure damage (`ActionExecutor.cpp:3016-3044`) | Up to N, charge modes | — | Keep ours; move onto the GE + tracker |
| **Invert damage / heal** | None | `InvertDamage`, `InvertHeal` | — | Not asked for; skip |
| **Tags / immunity** | `IsImmuneToEffectType` (`SkillEffectManager.cpp:2132`); element immunity only in buildup (`Private/Skills/Effects/StatusBuildupManager.cpp:59`) | — | Grant tags, tag requirements, immunity component | **None** (one tag tree, B-040) |
| **Stun / Silence / HealBlock gates** | `IsStunned` / `IsSilenced` (`SkillEffectManager.cpp:2104, 2109`); HealBlock in immunity check | — | Granted tags; heal GE requires "not HealBlock" | None |
| **Cleanse / dispel** | `RemoveAllDebuffs` (`SkillEffectManager.cpp:749`), CleanseSelf/Allies (`:1564-1588`) | `RemoveEffect`: buffs/debuffs/either, N or all, newest first, a stack goes whole (`04-effects.md` "The uses verbs and Remove effect") | Remove by query / tags | **Tiny:** "up to N, newest first" over `GetActiveEffects(query)` (Q-044i); gear exempt by tag (D-015) |
| **Buff / debuff class** | `SkillEffectClassification::IsBuff/IsDebuff` (`Public/Skills/Effects/ESkillEffectType.h:284, 358`) | Verb + sign; cost cheaper = buff | Asset tags | Spec builder adds `Effect.Buff` / `Effect.Debuff` |
| **Stat modifiers** | Passive types summed (`SkillEffectManager.cpp:1993`, `:2035`) | `Stat` rows add | Modifiers on attributes | None (feeds the transient-% attributes; Pattern P stays C++) |
| **Damage / heal maths** | `UDamageCalculator` (`Private/Combat/Damage/DamageCalculator.cpp:256-263`) | Fixed pipeline | Execution calculation | Wrapper only (B-145, B-146) |
| **Last Stand** | Death intercept + charge (`SkillEffectManager.cpp:1213`; `Private/Character/CharacterDataComponent.cpp:437`) | `Revive` (1 HP) is different | Attribute set pre-change / post-execute hook | Small custom hook on HP |
| **Turn effects (extra turn, push/pull)** | `ApplyEffectLogic` cases (`SkillEffectManager.cpp:1594`) | `AddTurn` | — | Keep ours (turn and grid stay ours) |
| **Status buildup** | `UStatusBuildupManager` (`StatusBuildupManager.cpp:300, 564, 603`) | Status bars (D-030 counting) | Attribute + execution + cap GE | Pending-element field; decay step in tracker |
| **VFX / SFX** | Delegates + per-skill VFX | — | GameplayCues on GEs | None (soft refs per D-057) |
| **UI events** | `OnEffectApplied` etc. (`SkillEffectManager.h:23-35`) | Status panel | ASC added / removed / stack events | Facade keeps today's delegates during transition |
| **Descriptions** | Display names (`Public/Skills/Effects/SkillEffectDisplayNames.h:27`), debug text (`FSkillEffect.h:191`) | Text tree + short form (`07-text-grammar.md`) | — | **B-045 generator** (section 7) |
| **Trigger order** | Array order | Arrival number, first applied fires first; depth cap 5 (`05-triggers-conditions.md`) | Undefined for our dispatcher | Tracker arrival counter (small) |
| **Per-fight state / co-op** | GameInstance subsystem (`SkillEffectManager.h:425-438`) | Match state | ASC on the character, replicated | Tracker component on the combatant (D-064) |
| **Debug** | `DebugPrintEffects`, `WOR_StartingEffects` | Status panel | `showdebug abilitysystem` | `wor.CompareEffects` (B-042) |

---

## 10. Plan for B-042

**B-042:** "GAS stage 3: turn tracker, effect-family GE classes + spec builder, SkillEffectManager → ASC facade; `wor.CompareEffects`".

### 10.1 Before it starts

- **B-004** (plugin on), **B-040** (tags), **B-041** (attributes + ASC on `ACombatCharacter`).
- **B-134**: the `WoR.Effects` baseline spec. Without it, "identical before/after" can't be shown.
- **Q-044k answered**: whether B-062's identity rules land first on today's code (recommended).
- **B-145 / B-146** plans approved, so the damage and buildup executions have one agreed shape.

### 10.2 Steps (each a cluster of ≤3 files, game playable after each)

| # | Step | Stock GAS | Custom |
|---|---|---|---|
| 1 | **Spike:** one native GE class (Stun: Infinite, grants `Status.Stun`, cue tag) built in C++ with GE components; prove the constructor pattern, `showdebug abilitysystem`, apply/remove by handle | GE, components, tags, cue | — |
| 2 | **`FTurnSpan` + spec** (if not already shipped with B-062): the counting rule of section 4.1, the deckbuilder's turn-count fixtures as tests | — | Pure function |
| 3 | **`UTurnEffectTrackerComponent`**: handle → identity, span, charges, pool, arrival number, tick spec; hooks at holder turn start/end (incl. bonus and Execution turns, D-055); fires-once and armed conditionals move here (D-064) | ASC events (removed) | Tracker |
| 4 | **Custom effect context + identity struct** (source item / skill / actor, origin gear or not) | `FGameplayEffectContext` subclass, globals hook | Identity rules (D-011, D-012, D-053) |
| 5 | **Family GE classes, first batch:** stat modifier per attribute (SetByCaller magnitude, sign = buff/debuff), Stun / Silence / HealBlock, element immunity, DoT + HoT (lasting tag GE + instant tick GE with execution) | Modifiers, tags, immunity, executions | — |
| 6 | **`FEffectSpecBuilder`**: payload → GE class + SetByCaller (magnitude, turns, charges, element) + dynamic tags (buff/debuff, origin, element) + context | Specs, SetByCaller | The mapping table (`ESkillEffectType` → GE class) |
| 7 | **Facade, apply path:** `USkillEffectManager::ApplyEffect` routes ported families to the ASC + tracker; unported families stay on the old path. A console switch (`wor.Effects.UseGAS 0/1`) flips back during the transition | Apply spec | Facade |
| 8 | **Facade, queries and removal:** `IsStunned`/`IsSilenced` → tags; `GetTotalStatModifier` → attributes; cleanse → remove by query, gear exempt (D-015), "up to N" helper | Tag queries, remove by query | Small helper |
| 9 | **Triggers:** dispatcher fires through the spec builder; emit `Event.Combat.*` GameplayEvents | GameplayEvents | Dispatcher (kept) |
| 10 | **Absorb family (D-092):** `UGE_Absorb` (Barrier / Shield / Ward by tag + element), spend function in the damage path per section 2.5 and Q-044e / Q-044f; Reflect moved onto the same shape | GE + tags + cue | Spend order, uses / pool / refill |
| 11 | **Remaining families** (~35 total per `Design/GASMigration.md` §3): reflect, Last Stand (HP hook), extra action, push/pull (call our turn/grid code from the builder), restore %, … | — | Per-family glue |
| 12 | **Status buildup** → attribute + execution + cap GE (section 8) | Attribute, execution | Pending element, decay step |
| 13 | **Stones and item effects** through the same builder | — | — |
| 14 | **`wor.CompareEffects`** + B-134 spec green on both paths; Crown's PIE pass | — | Debug command |
| 15 | **Retire** `FActiveSkillEffect` storage, `PackEffectID`, Space-B IDs, `ESkillEffectTiming` once no reader remains (after PIE, per CLAUDE.md) | — | — |

### 10.3 What maps to stock GAS

- Instant and infinite effects; adding and removing them by handle, tag or query.
- Tags for statuses, immunities, families, buff/debuff, origin; tag checks for Stun, Silence, HealBlock.
- Stat changes as modifiers with SetByCaller magnitudes.
- Damage, heal, DoT tick and buildup maths as execution calculations (formulas stay ours).
- Source and instigator in the effect context.
- VFX/SFX as GameplayCues; UI from ASC events.
- `showdebug abilitysystem`.

### 10.4 What stays custom (and why)

| Custom piece | Why GAS can't |
|---|---|
| Turn counting + periodic ticks | GAS counts seconds |
| Identity, refresh, stack decisions | GAS stacks by GE class + instigator, not by our source item |
| Charges and pools | GAS has no "uses left" |
| Absorb spending and its order | No absorb in GAS; the order is our rule |
| Trigger dispatcher + numeric conditions | GAS's trigger model is abilities (stage 4); % checks aren't tags |
| Description generator (B-045) | GAS has no text |
| Turn and grid effects (extra turn, push/pull) | Turn order and grid stay ours |

### 10.5 Risks

- **Two paths at once.** The facade must route each family to exactly one path. Keep the console switch until `wor.CompareEffects` is clean.
- **Native GE classes.** Epic intends GEs to be data. C++ subclasses with a configured default object work, but GE components must be created in the constructor the supported way *(verify in step 1)*.
- **Missing SetByCaller.** GAS logs an error and uses 0 when a SetByCaller value is missing. The builder must always set every key its class reads; add a spec test per family.
- **Rule changes mixed with the port.** D-051 and D-053 change behaviour. If B-062 lands them first (Q-044k), B-042 only has to match, which keeps "identical before/after" meaningful.

---

## 11. Open questions for Crown

### Q-044 — Re-applying the same effect: strength rule or replace?
- **Today:** stronger replaces the value and refreshes; equal refreshes; weaker is ignored (`Private/Skills/Effects/SkillEffectManager.cpp:146-218`; `docs/Rules/04-effects-and-turns.md` ET-39).
- **Deckbuilder:** the same source again **replaces** the held one, with a fresh duration and today's value (`wor-deckbuilder/docs/reference/04-effects.md`, `Stat` duration row; D-012 already says "re-snapshot").
- **Options:** **A** replace (deckbuilder) / **B** keep the strength rule.
- **Recommendation: A.** It is the deckbuilder's rule, it matches D-012 and D-053, and it needs no strength comparison (GAS has none). Defend and on-hit Burn still refresh, because they re-apply the same value.

### Q-044b — What a stack is
- **Today:** value × stacks, the strongest per-stack value is kept, a weaker application adds a stack but doesn't refresh (`SkillEffectManager.cpp:159-182`; ET-40).
- **Deckbuilder:** a stack is the held effect's magnitude once more; every arrival refreshes the duration; an effect doesn't stack unless its cap is 2 or more (`wor-deckbuilder/docs/reference/04-effects.md` `MaxStacks`; `wor-deckbuilder/wor.core/Match/EffectResolver.cs:161-172`).
- **Options:** **A** deckbuilder stacks / **B** keep ours.
- **Recommendation: A.** One simple rule, and "what a stack adds is exactly what its expiry takes off".

### Q-044c — When damage and heals over time tick
- **Today:** DoTs tick at the end of the holder's turn; heals and EP over time at the start (`Public/Skills/Effects/ESkillEffectTiming.h:26-45`; auto timing at `Public/Skills/Effects/ActiveSkillEffect.h:309`).
- **Deckbuilder:** every payout (DoT, heal, Energy) happens at the holder's next N **turn starts** (`wor-deckbuilder/docs/reference/04-effects.md` "Lasts"; `WoR_TCG_Pivot_Design.md` §7 "Durations").
- **Options:** **A** all payouts at turn start / **B** DoT at turn end, heals at turn start (today).
- **Recommendation: A.** One moment for everything fits D-051's single counting rule, and it gives exactly N ticks with no special case. A DoT that kills at turn start ends that turn with no action (ET-17 already handles this).

### Q-044d — Execution turns and turn-start effects
- **Today:** an Execution turn (a ritual firing) skips start-of-turn effects (`Private/Combat/CombatOrchestrator.cpp:882`; ET-16). D-055 says every holder turn counts toward durations.
- **Deckbuilder:** no Execution turns, so no rule. D-051 asks for exactly N ticks.
- **Options:** **A** an Execution turn runs the turn-start step for effects (expiry and ticks) but still gives no choice / **B** it counts down durations but skips ticks.
- **Recommendation: A.** B would give fewer than N ticks and break D-051.

### Q-044e — Where Barrier, Shield and Ward sit in a hit
- **Deckbuilder:** Ward → Shield → Barrier come **before** Defense and Guard (`wor-deckbuilder/wor.core/Match/SkillCommands.cs:332-345`; `WoR_TCG_Pivot_Design.md` §7 "Damage order"). Its Defense is a flat number.
- **Ours:** Guard reduces first (`Private/Combat/Defense/DefenseSystem.cpp:514`), then Defense as a % (`Private/Combat/Damage/DamageCalculator.cpp:256-263`), then today's Absorb and Reflect (`Private/Combat/Actions/ActionExecutor.cpp:2995-3044`). D-092's note proposed "Guard first, then absorb".
- **Options:** **A** deckbuilder order (absorbers before Defense and Guard) / **B** absorbers last, on the damage left after Guard and Defense, where Absorb and Reflect sit today.
- **Recommendation: B.** Our Defense and Guard are percentages, so the order changes the numbers. With B, "negate the next 5 damage you take" means exactly 5 HP you would have lost, a Perfect Guard keeps your shield longer, and it is the smallest code change. Inside the absorbers, keep the deckbuilder's order (Ward → Shield → Barrier, oldest first).

### Q-044f — Multi-hit skills and absorb charges
- **Deckbuilder:** defender charges spend on every damage instance, DoT ticks included (`wor-deckbuilder/docs/deferrals.md` C9 ruling 2). A card hit is one instance.
- **Ours:** one skill can land several impacts.
- **Options:** **A** each impact is one damage instance (spends one charge) / **B** one charge per skill, whatever its impacts.
- **Recommendation: A.** It is the deckbuilder's rule, and it needs no per-skill bookkeeping. Multi-hit skills become good at breaking shields, which is a clear tactical role.

### Q-044g — "Doesn't stack" on effect definitions (D-053) for existing gear
- **Context:** D-053 makes a different item add its own copy. Some existing effects would break if doubled (Last Stand, fires-once, extra action).
- **Deckbuilder:** no such flag; bonuses never stack and effects stack only when they say so.
- **Options:** **A** default `bDoesNotStack = false`; CC lists every definition with Last Stand, Extra Action or fires-once for Crown to tick / **B** default true for those types in code.
- **Recommendation: A.** Data decides (the deckbuilder's "authoritative data" rule, `wor-deckbuilder/docs/reference/00-index.md`), and Crown sees the list once.

### Q-044h — Conditions: "or", and % versus absolute HP
- **Deckbuilder:** nested conditions are state-checks only, **all must hold** (no "or"); HP checks are **absolute** ("below 30 HP") (`wor-deckbuilder/docs/reference/05-triggers-conditions.md` "Nested conditions", "The retired percentage comparison").
- **Ours:** And/Or per entry (`Public/Skills/Effects/FSkillCondition.h:32`); HP/EP thresholds are % (`:28`).
- **Options:**
  - Or: **A** retire Or for new data (a validator reports it; two effects instead) / **B** keep Or and print "or" in the text.
  - Numbers: **C** keep % / **D** absolute, as the deckbuilder.
- **Recommendation: A and C.** A follows the deckbuilder and keeps D-031's tree honest. C differs from the deckbuilder on purpose: our HP grows with level and gear, so "below 30 HP" would mean different things at different levels; the text prints "below 30% HP".

### Q-044i — Which effects a limited cleanse takes
- **Today:** Iolite removes "up to N" debuffs (or buffs on an enemy) with no stated order (`Private/Inventory/ItemExecutor.cpp:1014-1045`; ET-45).
- **Deckbuilder:** Remove takes N effects that pass its filter; the owner picks; with no pick asked, **the newest go first**; a stack goes whole; standing gear grants are never removed (`wor-deckbuilder/docs/reference/04-effects.md` "The uses verbs and Remove effect").
- **Options:** **A** newest first, a stack whole, gear exempt (D-015) / **B** oldest first.
- **Recommendation: A** (the deckbuilder's rule, no picking UI for now).

### Q-044j — Triggers: our dispatcher or GAS abilities?
- **Standard GAS:** passive effects that react to events are GameplayAbilities triggered by GameplayEvents or tags.
- **Ours:** a dispatcher that already works with defense windows and turn events (`SkillEffectManager.cpp:1013, 1771`).
- **Options:** **A** keep the dispatcher in B-042, send GameplayEvents so B-043 can move triggers to abilities / **B** move triggers to passive abilities in B-042.
- **Recommendation: A.** Abilities are stage 4 (B-043); doing it in stage 3 doubles its size and risk. The events make the later move cheap.

### Q-044k — Land B-062 (identity rules) before GAS, or inside it?
- **Context:** D-011, D-012, D-051, D-053, D-055 are all answered, so B-062 can start. B-042 is far off (waits on B-004 and B-041). D-092 says the absorb family is built inside the GAS work.
- **Options:** **A** build B-062 now on today's code, including the shared `FTurnSpan` helper; B-042 then ports the same tests / **B** fold B-062 into B-042.
- **Recommendation: A.** Players get the new rules sooner, B-134's baseline then records the *new* behaviour, and B-042 becomes a pure port ("identical before/after"). Absorb stays in B-042, as D-092 says.

---

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Initial review: ours vs deckbuilder vs GAS; absorb family (D-092); D-031 data needs; feature table; B-042 plan; Q-044 to Q-044k | chore/B-044-effects-overhaul-review |
