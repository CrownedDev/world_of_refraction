# Trial pool design: one pool, and the enemies built from it

**Status:** design, in line with D-225 as it stands on 2026-11-17 (every addition, to "Gaps closed after the rewrite"). Every question for Crown is answered; section 10 holds build notes only. Nothing here is built. It replaces `AssetOverhaulPlan.md` section 7 as the place to read this.
**Marks:** *Ruled* = Crown's words in D-225 unless another entry is named. *Default* = proposed here or in D-225, not yet objected to. Every number is a `// TODO` tunable.

## 1. The idea in five lines

1. Each trial has one **pool**: everything that can be earned there.
2. Its **enemies are built from that same pool**, so what you fight is what you can win.
3. An enemy is **what it always has**, plus **whatever it takes from the pool, at any tier, inside its limits**.
4. Enemies **follow the player's rules** (class and slots), so everyone is on equal footing.
5. Each won fight gives **one drop per player**, 3 times likelier to be something the enemies used; the merchant's shelf is half the pool, half its own.

**Why enemies carry gear you can win (Crown):** "we don't need a lore reason for it; the reason is the trials are a simulation".

## 2. A trial's pool

**What it holds (ruled):** weapons, adapters, crystals, evolutions, skills and apparel.

**How a piece gets in (default):** by tag, as chests work today. A piece carries the trial's pool tag in its `SourceTags`; the trial names that tag. No new pool asset.

| Part | Today | New |
|---|---|---|
| Weapons, adapters, spells, abilities | In: `GatherPool` reads every equipment and skill asset whose `SourceTags` match (`Private/Trial/TrialLoot.cpp:63-88`) | Nothing |
| Prebuilt pieces (D-226, D-231) | In: `UPrebuiltFindData`, a piece plus the gem or evolution in its socket (`Public/Trial/PrebuiltFindData.h:23-43`) | Nothing |
| Apparel | Gathered (it is equipment, `Public/Equipment/Apparel/ApparelData.h:15`) but cannot be handed out (`TrialLoot.cpp:185-221`) | Hand it out |
| Evolutions | Tagged but not read ("INERT", `Public/Equipment/Crystals/EvolutionItemData.h:66-70`) | Read and hand out |
| Crystals | Not in any pool | Every element, in every trial (below) |
| The trial's own pool | No field: "BANKED", `Public/Trial/TrialData.h:65-70`. Only a zone or a chest names a pool (`Public/Combat/Encounter/EnemyZone.h:27-30`, `Private/Trial/TrialMapFeature.cpp:182-185`) | `UTrialData::Pool`; a zone's own pool stays as an override |

**Crystals (ruled):** a trial gives **every element's** crystals, but leans to its own element: that element comes up more often and is likelier to come at a higher tier. The trial already has its element (`TrialData.h:40-43`); the two weights are named tunables. This now matches the deckbuilder, where crystals are in every pool (`wor-deckbuilder/docs/design/WoR_TCG_Series_Event_Pack_Economy.md` §1b).

**Each zone has its own tier range (ruled).** The first zone holds only the pool's low tiers and later zones open the higher ones, so enemies and drops get stronger deeper in. The ranges follow the zones' tier guide, which exists today: zones 0 to 4 ask for F, D, C, B, A (`EnemyZone.h:23-25,63-65`). Proposed ranges (tunable): zone 0 F to E, zone 1 F to D, zone 2 F to C, zone 3 F to B, zone 4 F to A. A weak enemy with a strong piece is still possible, inside its zone's range.

**The merchant (ruled):** a merchant has **its own pool** (things that never drop from enemies) and also draws from the trial's pool. The shelf is **half and half** (a named tunable). Today a trial vendor shows 8 lines rolled from its own authored list only (`Public/Merchant/MerchantStockRoll.h:16,56-59`). So: 4 lines from its own list, 4 from the trial's pool.

**The deckbuilder's rule for enemies is the same shape** (§1b, an Event there is a trial here): "PvE enemy decks = the Event's shared cards + that character's bonded cards": the pool, plus what the enemy always has.

## 3. An enemy: what it always has, and its limits

**Ruled.** There is no ladder and no fallback table. An enemy's pillar points do **not** decide what gear or tier it carries: "think of an adventure, you met a low level character with a high level sword".

**Part 1: what it always has.** A list of gear and skills on the enemy's template, for any enemy, not only a boss. A piece on the list has the tier it is given ("provide a low rank enemy with a D class weapon").

**Part 2: what it takes from the pool.** Anything, at any tier its zone's range holds, inside five limits (Crown, 2026-11-17: most weapons, most adapters and evolution allowed merged into most gear and the kinds). Each is optional and **unset means any**, with one exception: skills.

| Limit | Unset means |
|---|---|
| Most gear (its slot count: weapons, adapters and an evolution together; apparel has its own place) | As many as its class carries. The class decides which kinds fit and caps them: a Generic 2 weapons and 1 adapter, a Refractor 1 item, a Resonator 1 weapon and 5 adapters |
| Most skills **per piece** (Crown, 2026-11-17: replaces the total of 3) | As many as each piece holds by the player's slot rules (its tier's slots, a weapon's attached crystal included) |
| Most innate (its own limit, in points; the innate pool is much bigger) | Its innate budget (D-240): 3 points, plus 1 per pillar point of its own levels, at most 24 (Broken Darkness: the same 3, each pillar point worth 2 and a pillar's seventh worth 3, up to 48). Each innate spell costs its tier (F 1 to S 7), less its pillar discount |
| Which kinds (weapon, adapter, evolution, apparel; and which weapon types) | Any kind. An evolution is allowed when its kind is |
| Which elements (its crystals and spells) | Any element |
| A tier range | Any tier in its zone's range |

**Its crystals lean to the trial's element** (ruled), by the same weights as the drops, so a Fire trial's enemies are mostly Fire.

**With nothing set at all,** the enemy wears what it was given and fills the rest of what its class can wear from the pool. So a Resonator enemy can carry five adapters, and every weapon combination a class allows can turn up.

**Shared by the template, rolled per enemy (ruled).** What the template gives is the same on every enemy of that type. What is left open is rolled separately for each enemy: "three bandits share their weapon and first skill and differ in the rest".

**An elite needs no rule of its own** (ruled): it is an enemy with a list.

**Today:** a template has none of this. An enemy fights with the old loadouts authored in its inventory asset, and a stopgap fills their empty slots from the skills that inventory lists (`Private/Inventory/InventoryComponent.cpp:3018-3032`; `Private/Loadout/WornGear.cpp:60-62`, marked "remove in stage 5"). An enemy team is a list of templates, three at most (`Public/Combat/Encounter/EncounterData.h:19,30`).

## 4. The rules an enemy follows

**Ruled: the player's rules, exactly.** "It should just follow our rules in the background ... plus it puts everyone on equal footing."

| Rule | What it is today |
|---|---|
| **Class limits** | A Generic: 3 pieces, at most 2 weapons and 1 adapter. A Refractor: 1. A Resonator: 1 weapon and 5 adapters (`Public/Loadout/LoadoutRules.h:153-176`) |
| **Slots by tier** | F 1, E 2, D 3, C 4, B 5, A 6, S 6 (`Public/Equipment/Crystals/AugmentStoneConstants.h:31-35`) |
| **A ring's slots** | From its crystal's tier (D-226; `Private/Loadout/Entries/FRingLoadoutEntry.cpp:32-40`) |
| **A weapon's slots** | Its own tier's, plus its crystal's (`FWeaponLoadoutEntry.cpp:34-45`) |
| **A skill needs its gear** | A sword skill needs a sword, a Fire spell a Fire source (D-224) |
| **Slots need not be filled** | Ruled: 3 skills unless the enemy sets its own number, and never more than its slots |

- **A dull enemy is fixed by a better piece, not by breaking the slot rule** (ruled). An F sword has one skill; give the enemy an E or D sword and it has two or three.
- **What pillar points still do:** they are the enemy's own strength (`Public/Character/CharacterData.h:180-190`), and a skill whose Mind / Body / Spirit need the enemy falls short on plays weaker, by the existing gap rule (`Public/Skills/Definitions/WorldStatRequirements.h:39`). So a low enemy with a high sword hits less hard with it than a high enemy would. No new rule.

## 5. The roll

- **When (ruled):** once, when the run starts. Each enemy keeps its kit for the run: a retried fight meets the same kit.
- **Per enemy (ruled):** each enemy standing on the map gets its own roll for its open part.
- **Order (default in D-225):** 1. the always-has list; 2. more pieces from the pool, inside the limits and the class cap; 3. crystals for them (an adapter with no crystal does nothing); 4. always-has skills, then only skills the gear can use, up to the slots and the skill limit.
- **Seeded (default in D-225):** from the **run's seed**, the encounter's id and the enemy's place in its team, the way each merchant's shelf is seeded today (`MerchantStockRoll.h:20-23`; the seed: `Public/Trial/TrialRunSubsystem.h:476,525`, read in co-op through `Public/Player/RunStateComponent.h:120-121`). The same seed gives the same kit, so a co-op partner, a retried fight and a resumed run all match, and nothing new is saved.
- **Check without playing:** a console line that prints every rolled kit (`wor.Trial.PrintEnemyKits`), beside today's `wor.Trial.Pool` (`TrialLoot.cpp:103-112`).

## 6. Drops

**Today:** defeated enemies drop pillar levels only (`Private/Trial/TrialGate.cpp:238-247`). Gear and skills come from chests: one seeded, unweighted pick per player (`TrialMapFeature.cpp:190-201`; the weight is a TODO at `Public/Trial/TrialLoot.h:32-34`).

| Rule | Status |
|---|---|
| **One drop per won fight, for each player: one thing of any kind** (a weapon, an adapter, a crystal, a skill, apparel or an evolution) | Ruled |
| A boss always drops | Ruled |
| **A boss's first defeat gives two things:** one thing guaranteed from its presets, of any kind (its always-has list: a skill, a weapon, an adapter and so on), and the normal drop. Later defeats give the one drop | Ruled |
| A preset piece arrives as it was set up: prebuilt if it was, otherwise empty | Ruled |
| A drop comes from the zone's tier range; crystals lean to the trial's element | Ruled |
| What the enemies were using is **3 times likelier** than the rest of the pool (a named number) | Ruled |
| **A dropped piece arrives empty:** no crystal, no skills | Ruled |
| A piece set up as prebuilt drops prebuilt, crystal in slot 1 (D-231) | Ruled |
| It drops **at the tier the enemy wore** | Ruled |
| Apparel drops like weapons; merchant-only things never drop | Ruled |
| Always-has pieces can drop | Default in D-225 (not changed) |

**A duplicate (ruled).** Duplicates are allowed, "since everything is tiered". When a drop is something the player already has, they choose one of three:
1. **Replace** the one they have with it.
2. **Add** it to the inventory ("in case you want to give it to a teammate").
3. **Convert** it to essence.

This covers everything in the inventory (weapons, adapters, evolutions, crystals, skills, apparel) **except utility items**, which stack as now; a utility item can also be **dismantled** for essence. There is no second roll.

**Keeping a drop** is already ruled: worn at run end, it is kept for the hub on a boss win or an extract; found skills are kept only if slotted; a wipe-out keeps no found gear (D-219, D-221, D-224; rules ER-65f, ER-65h). **A co-op guest keeps what they earn** in their own save, by the same run-end rules (Q-561; built as B-563 and B-564).

## 7. Seeing an enemy's kit

**Ruled:** it is not free, and it comes as **two variations of one skill**:

| Variation | How it works |
|---|---|
| **Used in the field** | Used on an enemy before the fight. Costs EP. It shows the full kit of the enemy it is used on, and remembers the type's shared part for the run |
| **Always on** | Once owned, enemies show their gear as the player gets close |

No such skill exists today. Default: the card shows no rank word, since "Elite", "Mini-boss" and "Boss" are never shown to the player (D-187).

## 8. Worked examples

Names are from the clean gear set (`AssetOverhaulPlan.md` §2). Skills are asset names (`docs/Audits/CharacterAssetDump.md:643-696,928`); shown names may differ. The enemies are made up to show the shape, not rulings.

**1. A plain enemy, nothing set.** The Darkness enemy, a Refractor (one piece at most), in zone 2 (F to C).
- This run: an Adapter Ring with a C Emerald, so a "Wind Ring" with 4 slots. It fills its 4 slots: Bolt, Lance, Wave and one more.
- Another like it in the first zone (F to E): a Hand Axe (F), one slot, Cross Cut.

**2. A low enemy with a high sword.** A Generic enemy at 1 1 1, in zone 1 (F to D). Always has: Arming Sword at **D**, the top of its zone. Limits: most gear 1, kinds weapon.
- Three slots, three skills: Lunge, Heavy Strike, Thrust. It is weak (1 1 1), so skills it falls short on hit less hard.
- Beat it and the Arming Sword is 3 times likelier to be the drop. It arrives at D, empty, while the player's kit is F.

**3. Three bandits (one template),** in the first zone. Always has: Hand Axe and Bearded Axe as a pair at E (2 slots), and Cross Cut. Limits: kinds weapon.
- All three: the axes and Cross Cut.
- Second slot, rolled per bandit: Smash, Whirlwind, Smash.

**4. A Resonator enemy with five adapters,** in zone 3 (F to B). Nothing given.
- This run: an Arming Sword (D) and five Adapter Rings: Garnet C, Garnet B, Citrine E, Amber F, Emerald F. Two are Garnet: a Fire trial leans that way.
- Those hold up to 16 skills (3 + 4 + 5 + 2 + 1 + 1). With nothing set it fills every slot its pool's skills can.
- Give it "most skills per piece 1" and each piece carries one: six skills, such as Lunge, Bolt, Missile, Wave, Storm, Might.

**5. One whole trial (Fire), its boss and its merchant.**

| The trial's pool | The merchant's own pool |
|---|---|
| Weapons: Arming Sword, Round Shield, Hand Axe, Bearded Axe, F to A (each zone opens a tier) | Gale Force (evolution) |
| Adapter: Adapter Ring. Prebuilt: Phoenix Fire Ring | Mend, Restore, Cleanse (spells) |
| Crystals: all five, Garnet more often and at higher tiers | An augment stone |
| Skills: Lunge, Heavy Strike, Thrust, Quick Strike, Wrath Cut, Whirlwind, Cross Cut, Smash, Bolt, Missile, Lance, Wave, Storm, Might, Warding | |
| Apparel: none yet (the clean set has no apparel piece) | |

- **The boss,** in zone 4 (F to A). Always has: Arming Sword (A) with Round Shield, a Phoenix Fire Ring (prebuilt), and Wrath Cut, Storm, Warding. Limit: most skills per piece 2. This run it added Might and Lunge.
- **Beating it:** always a drop. The first time, two things: one of its presets for certain (the Arming Sword, the Round Shield, the Phoenix Fire Ring, Wrath Cut, Storm or Warding), and the normal drop. The sword or shield arrives empty, at A; the ring arrives built, Phoenix Fire in slot 1.
- **The merchant's shelf this run:** 8 lines: 4 from the left column, 4 from the right. Healing spells and Gale Force can only be bought.
- **A duplicate:** a second Arming Sword (D) drops while the player holds one at F. They choose: replace, add, or convert.

## 9. Today against new, and the stages

| Part | Today | New |
|---|---|---|
| The pool | A tag query for chests and zones | The trial names its pool; evolutions, apparel and crystals join it |
| An enemy's kit | Old loadouts in a shared inventory, plus the stopgap fill | Always-has and five limits on the template |
| The roll | None | Once per run, per enemy, from the run seed |
| Enemy drops | Pillar levels only | One per fight per player, weighted 3 to 1 |
| A duplicate | Refused in a shop (rule ER-20) or converted (ER-63) | Three choices; utility stacks and can be dismantled |
| The merchant | 8 lines of its own list | Half its own, half the trial's |
| Seeing a kit | Nothing | A field skill, two variations |

**Stages**, each in Crown's order (apply the UI, Crown looks, align code and data, test the real path). Stages with nothing on screen go straight to code and a console print.

| # | Stage | Crown sees | Tested by |
|---|---|---|---|
| 1 | **The look:** the kit card, the "dropped" line in the fight's reward, and the three-choice duplicate prompt; plain, with made-up contents | All three, in PIE | Crown's eye |
| 2 | **The pool:** the trial's pool tag; evolutions and apparel readable; every element's crystals with the lean; each zone's tier range; the clean set tagged (editor) | `wor.Trial.Pool` lists the trial's pool, zone by zone | Specs |
| 3 | **The enemy's parts:** the always-has list and the limits on the template (five since B-567b) | A print button on the enemy | Specs: unset means any (skills: every slot, B-567c); the class and slot rules hold |
| 4 | **The roll:** each enemy's kit from the run seed; the enemy fights with it; the two enemies stop reading the old inventories | The same kit on a retry, another on a new run; three bandits alike in part | Specs with a fixed seed, then PIE |
| 5 | **Drops:** one per fight per player, weighted, at the worn tier, empty unless prebuilt | Winning what an enemy used | Spec on the weight, then PIE |
| 6 | **Duplicates:** the three choices; utility dismantle | The prompt on a real duplicate | Spec, then PIE |
| 7 | **The merchant:** half and half | The shelf in a trial | Spec, then PIE |
| 8 | **Seeing a kit:** the field variation, then the always-on one | Each, on an enemy | PIE |

**Where the last old enemy data can go:** after stage 4 passes Crown's PIE check, the stopgap fill is removed and no enemy reads `DA_Inventory_Generic` or `DA_Test_Inventory`. That unblocks removal list group 3 (`docs/Audits/OldAssetRemovalList.md` §4): the two old inventories and the two baked pieces (`DA_Equipment_Generic_SAS`, `DA_Equipment_Generic_DualAxe`). `DA_Inventory_Generic` also waits for group 2 (the old Lords). `DA_Ring_Adapter_C` goes with it, not before (D-225).

## 10. Build notes

Nothing here waits on Crown. Each is a place where code or a written rule must change to match the rulings, with the fix proposed.

| # | What | The fix |
|---|---|---|
| 1 | **The dropped tier against the pickup grade roll.** A fresh pickup of a flagged weapon, adapter or evolution rolls a random grade today (rule ER-38) | An enemy's drop is given at the tier the enemy wore and skips that roll (stage 5). Chests keep it. Update ER-38's row |
| 2 | **A shop refuses a spell or ability the player already knows** (rule ER-20) | Duplicates are allowed now: the shop sells it, and the buyer gets the three choices like any duplicate (stage 6). Update ER-20's row |
| 3 | **A co-op guest's skill "it already has keeps its own copy"** (Q-561, B-564) | Leave as built: this is the run-end hand-back into the guest's save, where nobody is there to choose. The three choices happen earlier, when the skill drops. Add that sentence to Q-561 |
| 4 | **The seed wording.** D-225 says "the run's seeded random (BattleRandom)". BattleRandom is one stream per fight and is not sent to co-op clients (`docs/Architecture/BattleRandom.md`) | The kit roll and the drop pick use the run seed (section 5). Correct the words in D-225 when its row is next touched |
| 5 | **An always-has piece given a tier outside its zone's range** | The given tier wins (it is authored on purpose), and the zone print warns, as it already does for an enemy placed below its zone's guide (`EnemyZone.h:67-77`) (stage 3) |
| 6 | **Older lines inside D-225:** "based on total pillar points", the fallback table and the ladder are replaced by "Limits, not a ladder" but still in the entry | No code. Read the entry from the bottom up; this document is the reading copy |

## Clashes

None open. The earlier ones are settled by D-225's "Gaps closed after the rewrite": the one drop is one thing of any kind; an unset skill limit is 3; each zone's tier range keeps enemies in line with the zone guide; a boss's first defeat gives two things (one of its presets, of any kind, and the normal drop).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | Created from D-225 and its additions; replaces `AssetOverhaulPlan.md` §7 as the reading copy | dev (docs only) |
| 2026-11-17 | Brought in line with Crown's answers: limits not a ladder, the player's slot rules, shared template and per-enemy roll, one drop per fight, crystals lean to the trial's element, duplicates' three choices, the half-and-half shelf, two kit-skill variations, co-op guests keep what they earn | dev (docs only) |
| 2026-11-17 | Gaps closed: one drop of any kind, 3 skills when unset, a tier range per zone, a boss's first defeat gives two things, the story answer; section 10 is build notes only | dev (docs only) |
| 2026-11-17 | Crown's final wording: a boss's first defeat gives one thing from its presets, of any kind, plus the normal drop; a preset piece arrives as set up | dev (docs only) |
