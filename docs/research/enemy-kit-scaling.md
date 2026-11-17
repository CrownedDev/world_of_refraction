# Enemy kits from the trial's pool: how much does 1–7 carry?

**Researcher, 2026-11-17.** Web research for Crown's question on D-225 ("build a bear"). It builds on `docs/Design/TrialPoolDesign.md` and does not repeat it: the pool, always-has plus may-grab, the roll at run start, the 3× drop weight, class caps and table A or B (§4) are already there. This page answers **what each step from 1 to 7 should carry**. Suggestions only; Crown decides.

## 1. Does anything like this exist? Yes, in pieces

| Game / system | What it does | What worked | What players disliked |
|---|---|---|---|
| **Path of Exile 1 and 2: Rogue Exiles** | Enemies built like players: real player skills and items, and they **drop all the gear they carried** ([PoE2 wiki](https://www.poe2wiki.net/wiki/Rogue_exile), [PoE1 wiki](https://www.poewiki.net/wiki/Rogue_exile)) | Feels like fighting another player; the reward is obvious and exciting ([Mobalytics](https://mobalytics.gg/poe-2/guides/rogue-exiles)) | Spikes: a well-geared exile is far harder than the area around it |
| **NetHack** | Monsters pick up, wear and use almost the same items as the player (wands, armour, potions), and **drop their inventory on death** ([wiki: Monster](https://nethackwiki.com/wiki/Monster), [Death drop](https://nethackwiki.com/wiki/Death_drop)) | Classic "what it used, you win"; you learn to fear a monster holding a wand | Surprise deaths from an item you didn't see it pick up |
| **Dicey Dungeons** | Enemies fight with the **same equipment cards** as the player, and their cards are shown on screen ([Wikipedia](https://en.wikipedia.org/wiki/Dicey_Dungeons)) | Fully readable: you see every card before you act | Enemy use of the cards can look dumb ([fan wiki](https://bloxodes.com/wiki/dicey-dungeons/enemies)) |
| **Backpack Battles** | Each round's opponent is **another player's saved build** of the same round ([Steam](https://store.steampowered.com/app/2427700/Backpack_Battles/), [PC Gamer](https://pcgamer.com/games/arrange-your-items-to-optimize-your-fights-in-asynchronous-autobattler-backpack-battles)) | The round number is the budget, so the opponent's power fits the moment | Meta builds repeat |
| **Elden Ring NPC invaders** | Named enemies wear player armour sets and weapons and **drop that set** ([Fextralife](https://eldenring.wiki.fextralife.com/Invaders), [Leontiel](https://nerdschalk.com/how-to-get-leontiels-greatsword-and-armor-in-the-elden-ring-tarnished-pack/)) | "Beat it, wear its gear" is a much-loved hook | Only for named, hand-made enemies |
| **The deckbuilder** (our own) | PvE enemy deck = the Event's shared cards + the character's bonded cards | Already ruled into D-225 | — |

## 2. Strength from a budget or from separate tracks

| System | How it works | Lesson |
|---|---|---|
| **Pathfinder 2e "Building Creatures"** | **Each stat gets its own grade** (terrible, low, moderate, high, extreme) per level. One extreme stat should be paid for with low ones; higher levels allow more extremes ([AoN rules](https://2e.aonprd.com/Rules.aspx?ID=2874), [Paizo PDF](https://downloads.paizo.com/PZOCUP024E_PathfinderGameMasteryGuide_MonsterHazardCreation_PF2E.pdf)) | Per-stat tracks give enemies a clear shape (a brute, a caster); it is the best-liked tabletop monster maker |
| **Pathfinder 2e encounter XP** | A fight's budget is spent by level difference (level −1 = 30 XP, same level = 40, +1 = 60); weak and elite add −1 or +1 ([calculator](https://west-games.com/pathfinder-2e-experience-calculator/), [Foundry builder](https://foundryvtt.com/packages/pf2e-encounter-builder)) | A total works for **how much**; weak and elite work poorly at the very bottom ([Paizo forum](https://paizo.com/threads/rzs2vbq3)) |
| **D&D 5e challenge rating** | One number per monster plus an XP budget | The most criticised: one number misreads a solo monster against a party, and the "action economy" decides fights ([EN World](https://www.enworld.org/goto/post?id=6794183), [analysis](https://dragnacarta.substack.com/p/d-and-d-combat-building-is-fundamentally)). Legendary actions are the patch |
| **Lancer NPCs** | A base NPC plus templates (Grunt, Elite, Ultra), with optional systems added per template; community budgets: grunt 0.25, base 1, elite 2 ([guide](https://owacsender.substack.com/p/the-gms-guide-to-building-lancer), [data wiki](https://github-wiki-see.page/m/massif-press/lancer-data/wiki/NPC-Classes-and-Templates)) | "Add N optional systems" is the closest to "carries N pieces"; masses of grunts can one-shot a player |
| **Warhammer 40k / Kill Team** | Points per model **and per wargear**; Kill Team dropped points for fixed team lists ([Warhammer Community](https://www.warhammer-community.com/2021/07/22/new-kill-team-replaces-points-with-a-fast-finely-balanced-list-building-system/)) | Pricing every item separately was hard to balance; players complained when upgrades were priced wrong ([Bolter & Chainsword](https://bolterandchainsword.com/topic/379048-munitorum-field-manual-points/page/6)) |

## 3. Readability: can the player tell what it carries?

- **Slay the Spire's intent icons** are the most praised: every enemy shows its next move and exact damage, so fights are about planning, not guessing ([summary](https://summerengine.com/blog/make-a-deckbuilder-like-slay-the-spire), [design note](https://saschb2b.github.io/game-mechanics/concepts/enemy-intent-telegraph)).
- **Dicey Dungeons** shows the enemy's equipment cards in full.
- **Path of Exile rares** list their modifiers by name over the enemy. The complaint is hidden effects (on-death explosions) and too many at once ([PoE forum](https://www.pathofexile.com/forum/view-thread/3374314)).
- **Diablo 4** elites show affix names; players still say elites are "more dangerous dead than alive" ([Icy Veins](https://www.icy-veins.com/d4/news/diablo-4-on-death-effects-make-elites-more-dangerous-dead-than-alive/)).
- **Our case:** a field skill reveals the full kit (ruled, D-225). Evidence says also show **something free in fight**: the gear the enemy holds is visible on its model and the element of its ring is coloured, and its skills appear the first time it uses them (Slay the Spire's lesson: readable beats hidden). The field skill then shows the full list ahead of time.

## 4. The low end and the top end

- **Low end:** the weakest is **one piece, one move** in every good example (Slay the Spire act 1, Lancer grunts, PoE normal monsters). Weak and elite tweaks break at the bottom (PF2e), so author the bottom rows by hand rather than subtracting.
- **Top end, what makes a wall:** stacked random abilities, not raw size. Diablo 3 bans some affix pairs ([Blizzard Watch](https://blizzardwatch.com/2015/07/11/combat-elite-monster-affixes-diablo-3/)); Diablo 4 patched out frustrating affixes ([Sportskeeda](https://sportskeeda.com/mmo/diablo-4-patch-1-1-1-set-remove-three-nightmare-dungeon-affixes)); Path of Exile cut how often hard rares appear after Archnemesis ([dev post](https://devtrackers.gg/pathofexile/p/e6c0fef3-what-happened-with-items)) and blocks some mod combinations ([PoE2 wiki](https://www.poe2wiki.net/wiki/Rare)); its forums ask that "one mod shouldn't stonewall a build" ([forum](https://www.pathofexile.com/forum/view-thread/3712521)). D&D's fix for strong solos is more turns, not just more numbers.
- **What stops our 7 7 7 being a wall** (already in place): the class cap (at most 3 pieces), the trial's pool (it can't grab what the trial lacks) and "slots need not be filled". **To add:** a cap on the number of skills, and a short **never-together list** (for example two hard control skills on one enemy).

## 5. Three ladders for 1 to 7

All use table **A** from §4 (the tier sets the skill slots) and keep the class cap. "≤ E" means the highest tier it may grab. Five examples each: **1 1 1, 3 3 3, 7 1 1, 1 7 1 (today's Generic enemy), 7 7 6.**

### Ladder 1: each pillar runs its own part (Pathfinder style)
Body = how many pieces (1–2: 1, 3–5: 2, 6–7: 3). Spirit = the highest tier (1 F, 2 E, 3 E, 4 D, 5 C, 6 B, 7 A). Mind = how many skills (equal to Mind). *Which pillar runs which part is a proposal; the pillars have no such meaning today.*

| Enemy | Carries |
|---|---|
| 1 1 1 | 1 piece, F, 1 skill |
| 3 3 3 | 2 pieces, ≤ E, 3 skills |
| 7 1 1 | 1 piece, F, **7 skills wanted but only 1 slot**: 1 skill (wasted points) |
| 1 7 1 | 3 pieces, F, 1 skill (lots of gear, nothing to do with it) |
| 7 7 6 | 3 pieces, ≤ B, 7 skills |

Clear shapes, but lopsided enemies waste points or come out odd, which matches Pathfinder's own warning that an extreme stat must be paid for elsewhere.

### Ladder 2: the total is a point budget (D&D / 40k style)
Budget = total − 1. A piece costs its tier (F 1, E 2, D 3, C 4, B 5, A 6), a skill costs 1. It spends on gear first, then skills.

| Enemy (budget) | Carries |
|---|---|
| 1 1 1 (2) | F piece + 1 skill |
| 3 3 3 (8) | E piece + D piece + 3 skills |
| 7 1 1 (8) | the same as 3 3 3 |
| 1 7 1 (8) | the same again |
| 7 7 6 (19) | B, C and D pieces + 7 skills |

Fine-grained, but 7 1 1, 1 7 1 and 3 3 3 come out identical, and every price must be tuned (40k's lesson).

### Ladder 3 (favoured): the average sets the row, the highest pillar sets the lean
Row = the **average of the three, rounded down** (so only 7 7 7 is row 7). The row sets **how much**; the highest pillar sets **what kind**: its gear is grabbed first, and its skills are preferred (the skill rule in §4 already leans this way).

| Row | Pieces (class cap) | Tier ≤ | Skills ≤ |
|---|---|---|---|
| 1 | 1 | F | 1 |
| 2 | 1 | E | 2 |
| 3 | 2 | E | 3 |
| 4 | 2 | D | 4 |
| 5 | 3 | C | 5 |
| 6 | 3 | B | 6 |
| 7 | all it can wear | A (S if the pool has it) | all its slots ("anything and everything") |

| Enemy | Row | Carries |
|---|---|---|
| 1 1 1 | 1 | 1 F piece, 1 skill |
| 3 3 3 | 3 | 2 pieces ≤ E, 3 skills, no lean (a mix) |
| 7 1 1 | 3 | 2 pieces ≤ E, 3 skills, all leaning to its 7 (e.g. Body: weapons and Body skills) |
| 1 7 1 | 3 | the same amount as 3 3 3, shaped by its 7 |
| 7 7 6 | 6 | 3 pieces ≤ B, 6 skills, two leans |

**Why the evidence favours ladder 3:**
- It matches D-225 and §10 question 13 ("the total picks the row, each pillar steers which skills").
- It gives shape without waste (Pathfinder's strength, without its lopsided cases).
- It avoids pricing every item (40k's problem).
- The bottom row is authored by hand rather than subtracted (PF2e's warning).
- Seven rows map one-to-one onto 1–7, which Crown can read at a glance.
- Lopsided enemies are cheaper than even ones (7 1 1 = row 3), so a "glass cannon" doesn't get top-row gear for free.

**Add to any ladder:** a skill cap per row, a never-together list (the Diablo and PoE lesson), and the free in-fight readability in section 3.

**One more choice for Crown:** with ladder 3, should a 7 1 1 enemy get one extra skill of its lean, as a reward for being extreme? Pathfinder lets one extreme stat stand out; default **no**, to keep it simple.

Not checked: official rulebook pages for D&D, Lancer and 40k (community sources used); no exact data found for how players rate Rogue Exiles.
