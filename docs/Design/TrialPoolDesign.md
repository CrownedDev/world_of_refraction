# Trial pool design: one pool, and the enemies built from it

**Status:** design for Crown to read (2026-11-17). Nothing here is built. It pulls together D-225 (with its additions), D-224, D-226, D-231, D-219 / D-221 / D-239, D-237 and D-238, and replaces `AssetOverhaulPlan.md` section 7 as the place to read this.
**Marks:** *Ruled* = Crown's words in a D-entry. *Default* = proposed here, listed in section 10 for Crown to agree or change. Every number is a `// TODO` tunable.

## 1. The idea in five lines

1. Each trial has one **pool**: everything that can be earned there.
2. Its **enemies are built from that same pool**, so what you fight is what you can win.
3. An enemy type is "build a bear": **what it always has**, plus **what it may grab** from the pool.
4. Enemies are **rolled once, when the run starts**, from the run's seed, so everyone sees the same enemy.
5. What an enemy used is **3 times likelier to drop**; the merchant sells from the pool and from a small pool of its own.

## 2. A trial's pool

**What it holds (ruled, D-225):** weapons, adapters, crystals, evolutions, skills and apparel.

**How a piece gets in (default):** by tag, as chests work today. A piece carries the trial's pool tag in its `SourceTags`; the trial names that tag. No new pool asset. Crystals are not assets (a type and a tier), so the trial lists them.

| Part | Today | New |
|---|---|---|
| Weapons, adapters, spells, abilities | In: `GatherPool` reads every equipment and skill asset whose `SourceTags` match (`Private/Trial/TrialLoot.cpp:63-88`) | Nothing |
| Prebuilt pieces (D-226, D-231) | In: `UPrebuiltFindData`, a piece plus the gem or evolution in its socket (`Public/Trial/PrebuiltFindData.h:23-43`) | Nothing |
| Apparel | Gathered (it is equipment, `Public/Equipment/Apparel/ApparelData.h:15`) but cannot be handed out (`TrialLoot.cpp:185-221`) | Hand it out |
| Evolutions | Tagged but not read ("INERT", `Public/Equipment/Crystals/EvolutionItemData.h:66-70`) | Read and hand out |
| Crystals | Not in any pool | A list on the trial |
| The trial's own pool | No field: "BANKED", `Public/Trial/TrialData.h:65-70`. Only a zone or a chest names a pool (`Public/Combat/Encounter/EnemyZone.h:27-30`, `Private/Trial/TrialMapFeature.cpp:182-185`) | `UTrialData::Pool` and `PoolCrystals`; a zone's own pool stays as an override |

**The merchant's pool, beside it (ruled, D-225):** the merchant sells from the trial's pool **and** from a pool of its own: things only merchants have, which never drop from enemies. Today a trial vendor shows 8 lines rolled from its own authored list (`Public/Merchant/MerchantStockRoll.h:16,56-59`) and does not read the trial at all. Default: the merchant's authored `Stock` becomes its own pool, and its shelf is rolled from both (how many lines from each: section 10).

**The deckbuilder has this rule** (`wor-deckbuilder/docs/design/WoR_TCG_Series_Event_Pack_Economy.md` §1b; an Event there is a trial here): "an Event's card pool = its shared cards + each of its characters' bonded cards", and "PvE enemy decks = the Event's shared cards + that character's bonded cards". That is this design: the pool, plus each enemy's always-has list. One difference to decide (section 10, question 3): there, every element's crystal at every tier is in **every** pool.

## 3. An enemy type: "build a bear"

**Ruled (D-225):** any enemy, not only a boss, is built in two parts. What the first part does not fill is rolled from the pool. With nothing set at all, the enemy is rolled entirely from the pool.

| Part | What Crown sets | Left empty means |
|---|---|---|
| **Always has** | A list of gear and skills it carries every time ("always has a sword"; "this boss must have these three skills") | Nothing fixed |
| **May grab: more pieces** | How many more pieces from the pool | The fallback table (section 4) |
| **May grab: which kinds** | Weapon types, adapters, apparel, crystal elements | Whatever its class can wear (default) |
| **May grab: skills** | How many more skills | The fallback table |
| **May grab: tier limits** | Lowest and highest tier of what it grabs | The fallback table |

- **Hard cap, always:** the enemy's class limits, the same as a player's (a Generic: 3 pieces, at most 2 weapons and 1 adapter; a Refractor: 1; a Resonator: 1 weapon and 5 adapters; `Public/Loadout/LoadoutRules.h:153-176`).
- **Slots need not be filled** (ruled). The skill number is "at most".
- **An elite needs no rule of its own** (ruled): it is an enemy with a list.
- **Default:** a rolled adapter comes with a crystal from the pool (an empty ring does nothing); a sword and shield count as one weapon (D-224).

**Today:** an enemy's template has none of this. It fights with the old loadouts authored in its inventory asset, and the stopgap fills their empty slots from the skills that inventory lists (`Private/Inventory/InventoryComponent.cpp:3018-3032`; `Private/Loadout/WornGear.cpp:60-62`, marked "remove in stage 5"). An enemy team is a list of templates, three at most (`Public/Combat/Encounter/EncounterData.h:19,30`).

## 4. The fallback table: pillar points into amounts

**Ruled (D-225):** with no parameters, the enemy's **own** Mind + Body + Spirit decide how many pieces and skills it rolls and at what tier. Each is 0 to 7 on its template (`Public/Character/CharacterData.h:180-190`), so the total is 0 to 21. The two enemies today: the Darkness enemy is 1 1 1 (total 3), the Generic enemy 1 7 1 (total 9) (`docs/Audits/CharacterAssetDump.md:508-510,543-545`).

**The clash to settle.** A piece's tier decides its skill slots: F 1, E 2, D 3, C 4, B 5, A 6, S 6 (`Public/Equipment/Crystals/AugmentStoneConstants.h:31-35`). A ring's slots come from its crystal's tier (D-226; `Private/Loadout/Entries/FRingLoadoutEntry.cpp:32-40`); a weapon's from its own tier plus its crystal's (`FWeaponLoadoutEntry.cpp:34-45`). Crown's example, "1 1 1 ... 1 equipment with 3 skills", does not fit an F piece, which has 1 slot. So there are two ways to read the table:

| Total | Example | Pieces (at most) | **A: the tier decides the skills** | **B: the skills are set, the tier follows** |
|---|---|---|---|---|
| 0 to 3 | 1 1 1 | 1 | F piece, **1 skill** | **3 skills**, so a D piece (or an F ring with a D crystal, or an F weapon with an E crystal: 1 + 2) |
| 4 to 8 | 2 3 2 | 2 | F to E, up to 4 | 5 skills, D pieces |
| 9 to 14 | 1 7 1 | 2 | D to C, up to 6 | 6 skills, C pieces |
| 15 to 21 | 6 6 6 | 3 | B to A, up to 8 | 8 skills, B to A |

| | A | B |
|---|---|---|
| A 1 1 1 enemy | One F piece, one skill | One D-grade piece, three skills |
| Early drops | F pieces and one skill at a time: level with the player's F kit (D-221). The first skills arrive slowly | Its piece or crystal is 3 times likelier to drop, so the first fights can hand out D gear, two tiers over the kit, and three skills are weighted at once |
| The first fights | Gentle: the player starts with no skills (D-224) | Harder: three skills against none |
| Fits the slot rules | As they are | Needs the weakest enemies to carry D-grade gear |

**Default: A** (Slay the Spire's first act: weak enemies with one or two moves, and rewards that start small; Hades' first region the same). B keeps Crown's exact example and gives a faster start.

**Which skills (default):** the roll prefers skills whose Mind / Body / Spirit needs the enemy meets (`Public/Skills/Definitions/WorldStatRequirements.h:39`), so a Body-heavy enemy leans to Body skills. One it falls short on may still be rolled, and plays weaker by the existing gap rule.

## 5. The roll

- **When (ruled):** once, when the run starts. Every fight against that enemy in the run uses the same kit.
- **Order (default in D-225):** 1. the always-has list; 2. more pieces from the pool, inside the parameters and the class cap; 3. crystals for them; 4. always-has skills, then only skills the gear can use (a sword skill needs a sword, a Fire spell needs a Fire source: D-224), up to the number.
- **Seeded (default in D-225):** from the **run's seed** and the enemy's own name, the way each merchant's shelf is rolled today (`MerchantStockRoll.h:20-23`; the seed: `Public/Trial/TrialRunSubsystem.h:476,525`, read in co-op through `Public/Player/RunStateComponent.h:120`). The same seed gives the same kit, so a co-op partner, a retried fight and a resumed run all match, and nothing new is saved.
- **Small clash in wording:** D-225 says "the run's seeded random (BattleRandom)". Those are two things. BattleRandom is one stream per fight, seeded when the fight starts and not sent to co-op clients (`docs/Architecture/BattleRandom.md`, Overview and Known Limitations). The kit roll happens before any fight, so it uses the run seed.
- **Check without playing:** a console line that prints every rolled kit (`wor.Trial.PrintEnemyKits`), beside today's `wor.Trial.Pool` (`TrialLoot.cpp:103-112`).

## 6. Drops

**Today:** defeated enemies drop pillar levels only (`Private/Trial/TrialGate.cpp:238-247`). Gear and skills come from chests: one seeded, unweighted pick per player (`TrialMapFeature.cpp:190-201`; the weight is a TODO at `Public/Trial/TrialLoot.h:32-34`).

| Rule | Status |
|---|---|
| Anything in the trial's pool can drop | Ruled |
| What the enemy was using is **3 times likelier** than the rest (one named number) | Ruled |
| Apparel drops like weapons | Ruled |
| Merchant-only things never drop | Ruled |
| Always-has pieces can drop too | Default in D-225 |
| A boss drops one of its always-has skills the first time it is beaten | Default in D-225 |
| A plain piece arrives empty, and its crystal is a separate drop; a piece authored as prebuilt arrives built, crystal in slot 1 (D-231) | Default: this is how finds work today (`PrebuiltFindData.h:2-3`) |
| How many drops a fight gives, and how often | Open (section 10) |

**What happens to a drop** is already ruled: worn at run end, it is kept for the hub on a boss win or an extract; found skills are kept only if slotted; a wipe-out keeps no found gear (D-219, D-221, D-224; rules ER-65f, ER-65h).

## 7. Seeing an enemy's kit

**Ruled (D-225):** it is not free. It is a **field skill**, or an evolution's field skill. No such skill exists today.

Default: used in the field on an enemy in sight; it shows that enemy's pieces and skills on a small card; once seen, it stays known for the run (the kit does not change). It shows no rank word: "Elite", "Mini-boss" and "Boss" are never shown to the player (D-187).

## 8. Worked examples

Names are from the clean gear set (`AssetOverhaulPlan.md` §2). Skills are asset names (`CharacterAssetDump.md:643-696,928`); shown names may differ. The elite and the boss are made up to show the shape, not rulings.

**1. A plain enemy.** The Darkness enemy, 1 1 1, nothing set. Row 0 to 3: one piece.
- A: an Adapter Ring (F) with an F Amber, so "Earth Ring", 1 slot: Bolt.
- B: the same ring with a D Amber, 3 slots: Bolt, Lance, Wave.
- Drops: the ring, the Amber and its skills weigh 3; everything else in the pool weighs 1.

**2. A shaped enemy.** The Generic enemy, 1 7 1. Always has: Arming Sword. May grab: one adapter.
- Every run: the Arming Sword. This run it also grabbed an Adapter Ring with a Citrine ("Lightning Ring").
- Skills (row 9 to 14): Lunge and Heavy Strike on the sword, Missile on the ring. Another run it may be a Fire Ring, or no ring.

**3. An elite.** Always has: Hand Axe and Bearded Axe as a pair, and Whirlwind. May grab: one adapter, two more skills, nothing over D.
- This run: the axes, Whirlwind, Cross Cut, and a "Wind Ring" (Emerald) with Wave.

**4. A boss.** Always has: Arming Sword with Round Shield, a Phoenix Fire Ring (prebuilt), and three skills: Wrath Cut, Storm, Warding. May grab: one more skill.
- This run it added Might.
- First time beaten: one of Wrath Cut, Storm or Warding drops for certain. After that, the 3 times weight like anyone else.

**5. One whole trial.**

| The trial's pool | The merchant's own pool |
|---|---|
| Weapons: Arming Sword, Round Shield, Hand Axe, Bearded Axe | Gale Force (evolution) |
| Adapter: Adapter Ring. Prebuilt: Phoenix Fire Ring | Mend, Restore, Cleanse (spells) |
| Crystals: Garnet, Sapphire, Citrine, Amber, Emerald, F to D | An augment stone |
| Skills: Lunge, Heavy Strike, Wrath Cut, Whirlwind, Cross Cut, Bolt, Missile, Lance, Wave, Storm, Might, Warding | |
| Apparel: none yet (the clean set has no apparel piece) | |

The merchant's shelf this run: 8 lines, 6 rolled from the left column and 2 from the right. Healing spells and Gale Force can only be bought; everything on the left can also be won from the enemies above.

## 9. Today against new, and the stages

| Part | Today | New |
|---|---|---|
| The pool | A tag query for chests and zones | The trial names its pool; evolutions, apparel and crystals join it |
| An enemy's kit | Old loadouts in a shared inventory, plus the stopgap fill | Always-has, parameters, and the fallback table |
| The roll | None | Once per run, from the run seed |
| Enemy drops | Pillar levels only | Gear and skills, weighted 3 to 1 |
| The merchant | 8 lines of its own list | The trial's pool plus its own |
| Seeing a kit | Nothing | A field skill |

**Stages**, each in Crown's order (apply the UI, Crown looks, align code and data, test the real path). Stages with nothing on screen go straight to code and a console print.

| # | Stage | Crown sees | Tested by |
|---|---|---|---|
| 1 | **The look:** the kit card and the "dropped" line in the fight's reward, plain, with made-up contents | Both, in PIE | Crown's eye |
| 2 | **The pool:** the trial's pool tag and crystal list; evolutions and apparel readable; the clean set tagged (editor) | `wor.Trial.Pool` lists the trial's pool | Specs |
| 3 | **Build a bear:** always-has and parameters on the enemy's template; the fallback table | A print button on the enemy | Specs for A or B |
| 4 | **The roll:** kits rolled at run start; the enemy fights with its kit; the two enemies stop reading the old inventories | The same enemy with the same kit on a retry; a different one on a new run | Specs with a fixed seed, then PIE |
| 5 | **Drops:** weighted picks, shown in the reward line from stage 1 | Winning what an enemy used | Spec on the weight, then PIE |
| 6 | **The merchant:** shelf from the trial's pool plus its own | The shelf in a trial | Spec, then PIE |
| 7 | **Seeing a kit:** the field skill, on the card from stage 1 | Using it on an enemy | PIE |

**Where the last old enemy data can go:** after stage 4 passes Crown's PIE check, the stopgap fill is removed and no enemy reads `DA_Inventory_Generic` or `DA_Test_Inventory`. That unblocks removal list group 3 (`docs/Audits/OldAssetRemovalList.md` §4): the two old inventories and the two baked pieces (`DA_Equipment_Generic_SAS`, `DA_Equipment_Generic_DualAxe`). `DA_Inventory_Generic` also waits for group 2 (the old Lords).

## 10. Open questions for Crown

| # | Question | Default, and why |
|---|---|---|
| 1 | **The table: A or B** (section 4)? | **A.** Slay the Spire's and Hades' first areas: weak enemies with one or two moves, small first rewards |
| 2 | **One kit per enemy type, or per enemy standing on the map?** | **Per type.** Slay the Spire and Monster Hunter: one kind of enemy always fights the same way, so players learn it and can hunt the one that carries what they want |
| 3 | **Which crystals are in a pool?** | **Every element in every pool, with the trial setting the tiers.** The deckbuilder's rule (§1b): crystals are staples in every Event's pool |
| 4 | **What may a plain enemy grab when no kinds are set?** | **Anything its class can wear, any element.** Hades: any enemy of a region can show up with that region's tricks. (The other choice: its own element only) |
| 5 | **How many drops, how often?** | **One per won fight, for each player; a boss always drops.** Slay the Spire: one reward after every fight, a sure one from elites and bosses |
| 6 | **In co-op, does everyone get their own drop?** | **Yes, each player their own roll,** as chests already do (D-103); the enemy's kit is the same for all |
| 7 | **The tier of a dropped piece** | **The tier the enemy wore.** Elden Ring and Monster Hunter: you get what it had |
| 8 | **Can a plain enemy roll an evolution?** | **No: only from an always-has list.** Elden Ring: the special piece comes from a named enemy, plain ones drop plain gear |
| 9 | **An always-has piece that drops: built or empty?** | **Empty, unless it is authored as prebuilt** (today's find rule, D-231) |
| 10 | **A dropped skill the player already knows** | **Rolled again once, then paid out as the duplicate's worth** (the existing duplicate rule, ER-63). Slay the Spire lets you skip a reward for nothing; this is kinder |
| 11 | **The merchant's shelf** | **8 lines: 6 from the trial's pool, 2 from its own.** Hades: Charon mostly sells the region's rewards, plus a few things only he has |
| 12 | **The kit field skill** | **Used in the field, costs EP, stays known for the run.** Persona's and Final Fantasy's scan: pay once, the notes stay |
| 13 | **The total, or each pillar?** (old question 13) | **The total picks the row** (Crown's words in D-225); each pillar only steers which skills |
| 14 | **Which tier wins: the table's, or the enemy's own tier?** (clash 3 below) | **The table sets the gear; the enemy's own tier stays for health only.** One source for gear, as D-225 says |
| 15 | **Lore: do enemies carrying the trial's own gear mean something in the story?** | Lore, so options only: (a) say nothing; (b) the trial arms what lives in it; (c) they are earlier challengers. Crown's alone |

## Clashes found

1. **D-225's example against the slot rules (D-211, D-226):** "1 1 1 ... 1 equipment with 3 skills" does not fit an F piece (1 slot). Section 4, question 1.
2. **D-225's wording "the run's seeded random (BattleRandom)":** two different things. The roll should use the run seed (section 5).
3. **Three things can set an enemy's tier:** D-225's table, the template's own `EnemyTier` (`CharacterData.h:224-226`), and the zone's guide, which checks placed enemies against F, D, C, B, A (`EnemyZone.h:63-71`). An enemy's fight tier is the average of its gear and skills, so under B a 1 1 1 enemy in the first zone would read as D where the guide expects F. Question 14.
4. **Crystals:** the deckbuilder puts every crystal in every pool; the earlier plan gave each trial its own short list. Question 3.
5. **Not a clash, a limit:** "what the trial gives is what its enemies use" has one ruled exception, the merchant's own pool.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | Created from D-225 and its additions; replaces `AssetOverhaulPlan.md` §7 as the reading copy | dev (docs only) |
