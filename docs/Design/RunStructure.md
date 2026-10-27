# Run Structure — what a trial is and how a run plays

**Status:** decided by Crown, 2026-10-26 (`DesignDecisions.md` D-032, with D-002, D-006, D-021, D-026, D-033).
Supersedes `Resources_Design.md` §7.1–7.2 (the escalating climb, and "leave early = forfeit everything").
Values marked *tune* are placeholders: named constants with `// TODO: tune` in code.

> **Naming (Crown, 2026-10-26):** "lord" / "Elemental Lord" in this doc is a **placeholder**. The playable characters are **not lords**; each will have a **real character name and lore**. Read "lord" as "the character tied to that element". (V is the one true Void Lord: `DesignDecisions.md` D-079.)

## 1. The story frame

- The **Elemental Lords** are characters from Crown's story who have **lost their memories**.
- **Trials are how they get them back.** In a trial a lord faces **versions of themselves** (or someone tied to them), and finds gear, items and dialogue about who they were.
- **Everything is cross-linked.** Every character has some connection to the others, so any lord in any trial gets lore about how they relate to that trial's lord.

## 2. A run, start to finish

1. **Hub:** build the party from your roster (gear presets drawn from the one shared pool, D-020) and buy the run loadout with the starting Gold budget (D-021).
2. **Gate:** choose a lord's trial, its **variation** and the **difficulty** (§3). The gate shows what it represents before you enter.
3. **Trial:** an **open map**, Nightreign style (§4). No timer. Choose your fights, explore, use abilities and movement out in the world.
4. **End of run:** beat the boss or die (full payout), or leave at a rest spot (reduced payout) (§6).

Any lord can enter any trial (D-032 part 8). The trial reacts to who's in the party.

## 3. Gates: variation and difficulty

- **Variation** (examples only: Past, Present, Future, Anger…) changes **the boss**: a **variant of the lord** (e.g. a younger self) or **someone related to them**, with that story.
- Variations change **boss and story only: no rule twists.**
- **Difficulty is picked at the door.** Harder pays more (D-006).

## 4. The map

- **One unified map per variation**, shared by every lord's trial, **tailored to the trial's element** with weather, sky, lighting, effects and props (`TrialData.Element`).
- On every map:

| Feature | What it does |
|---|---|
| Enemies | Optional fights: Gold, loot, fragments |
| Mini-bosses | Tougher, better rewards, may guard fragments; beating one creates a rest spot |
| Memory fragments | Unlock memories (§5) |
| Characters to recruit | Join for this run only, ungeared; geared from the shared run inventory (D-026) |
| Vendors | Spend Gold |
| Chests | Loot |
| Lore objects | Logged dialogue and notes |
| Rest spots | Full heal (HP, EP); where you sort inventory and loadout; **exit points** |
| Boss arena | Face the boss whenever you're ready |

- **Resting does not reset enemies.** Beaten enemies stay beaten.
- **Mini-boss revival (opt-in farming)** (Crown, 2026-10-26): a beaten **mini-boss** can be revived **stronger**, for a **better chance at good gear**. The trigger is at the **rest spot it creates** (Dark Souls II's Bonfire Ascetic). **Repeatable, one step at a time:** each revival raises its strength and loot by one tier, up to a cap (`// TODO: tune`). **No cost:** the risk is the price (losing ends the run). The **main boss** can't be revived: beating it **ends the run**.
- **Carry-over inside a run:** HP, EP, absorption and durability carry between fights; **status bars reset every fight** (D-013).

## 5. Memories, bound gear and reactions

- **Memory fragments** unlock a memory: scene, lore, dialogue, maybe a quest.
  - **Memories unlock by conditions** (D-100): all of a memory's conditions must be met (character, gear and level, place, what you beat, how you played, hidden); never told to the player; can chain. Boss changes are the **boss's own triggers**, not memories.
  - **Character-specific:** the same fragment or item can be deep for one lord and minor for another.
  - **No hint:** you learn what a fragment does when it plays.
- **Bound gear** (D-033): an item can belong to a lord. Anyone can equip it; only its lord gets the **bound bonus**, which is **hidden unless equipped by its owner**.
- **Reaction triggers**, one structure for all of the above: **if these items (a list, all equipped) are equipped while facing this character → trigger effects / a quest / dialogue.** Built on the same condition model as effects and descriptions (B-044, D-031), not a separate system.

## 6. Ending a run and rewards

| How it ends | Payout |
|---|---|
| Beat the boss | Full |
| Die | Full |
| Leave at a rest spot | Reduced (*tune*); the confirm screen says "full rewards only for beating the boss or falling in battle" |

- **Gold:** earned per fight won, spent at vendors, lost at run end.
- **Prisms and in-run upgrade materials:** tallied during the run, **paid at run end** (D-006). A payout multiplier exists for future passives (B-096).
- **Kept after a run:** items found in the run (to home storage, D-097), the pool with its upgrade levels, Prisms, head-start allocation. **Lost:** run world-stat levels, Gold, gear found in the run (but its upgrade progress is remembered: find it again in a later run and it returns at that level, D-097), run-only recruits, slotted crystals (D-002, D-026).

## 7. Open questions

- How a hub-built party's own gear enters a run beyond the leader's loadout (D-026 note).
- Data shapes for memory fragments and reaction triggers: proposed when built (D-032 notes).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created from D-032 and related decisions | docs/run-structure |
