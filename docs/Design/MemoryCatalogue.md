# Memory and Hidden Quest Catalogue

**Status:** structure agreed with Crown (2026-11-08); story content to come (Crown). It covers **structure only**: what kinds of memories and quests exist, who gets each, how it is found and what it can give. **Which** memories exist and what they say is lore, and Crown's alone (`HubAndStory.md` §5). Every "Example" slot is a placeholder for Crown to fill or delete.

Builds on: `HiddenQuests.md` (D-100, the Shangri-La Frontier model), `RunStructure.md` §5, `HubAndStory.md` §4, `docs/Architecture/MemorySystem.md`. Where this page and a ruling differ, the ruling wins: flag it, don't fix it.

## 1. Rules every kind follows (D-100, Q-235d)

1. **All conditions met → it unlocks.** A kind may list **alternative condition sets**: any one set opens it (SLF's Akane / Rabituza).
2. **No hints, ever.** No rumours, no "?" slots, no "you're close" cue. You learn what it is when it plays (Crown, 2026-10-27).
3. **Never missed for good.** Conditions are checked again every run. Failing one means "not this run".
4. **One-time unlock.** Once a player has it, it never unlocks again for that player. It can be reread in the journal.
5. **Rewards are different, not stronger** (pillars 2, 3).
6. **Private.** Only the player who got it is told, on their own screen.
7. **Who gets it** (Q-235d, Crown 2026-11-08, Shangri-La Frontier's unique scenarios):
   - the **player whose character found it** (met its conditions);
   - another player **only if they play the same character and met its conditions themselves**;
   - **everyone**, when the memory is for every character.
   - Conditions about the character or its gear are read against each player's own character. Shared ones (where, what was beaten) count for everyone present (D-100).
8. **The main story never depends on a rare condition or on co-op.** Rare and co-op content is extra.

## 2. Who a memory is for (its scope)

| Scope | Who can get it | Data today | Example |
|---|---|---|---|
| **Personal** | one named character (its player) | `CharacterDepth`: one character at Minor or Deep | *(Crown)* |
| **Shared by depth** | several named characters, each at its own depth: deep for one, minor for another (`RunStructure.md` §5) | `CharacterDepth` with several entries | *(Crown)* |
| **Every character** | everyone who meets the shared conditions | `CharacterDepth` empty | *(Crown)* |
| **Pair** (co-op or party) | only when two named characters are both present | a "party has" condition, to add | *(Crown)* |

Depth decides what the memory gives that character: **Deep** opens its scenes, conversations and rewards; **Minor** is a journal entry only (**Crown, 2026-11-08**; Hades' Codex entries vs. its relationship scenes).

**Any memory can start a quest** (Crown, 2026-11-08), at either depth: the memory names a hidden quest (3.6), which starts for each player who got the memory. Set per depth: a memory can start one quest for the character it is Deep for and none, or another, for a Minor one.

## 3. The kinds

### 3.1 Memory fragment (found on the map)
- **What:** an object on a trial map (`AMemoryFragment`). Interacting plays the memory: a scene, lore or dialogue.
- **How found:** it is on the map; its conditions decide whether it **appears** this run.
- **Typical conditions:** who is in the party, the trial and zone, the story phase.
- **Gives:** a journal entry; can open conversations and V lines; can start a hidden quest (3.6).
- **Co-op:** goes by §1.7. The fragment shows for a player whose character qualifies.
- **Example:** *(Crown)*

### 3.2 Hidden memory (no object)
- **What:** unlocks the moment its conditions are met, with nothing to pick up. SLF's "it activates when it activates".
- **How found:** by playing. A fight ends, a boss falls, a skill is used in a certain way.
- **Typical conditions:** what you beat (optionally how: "killed by Light"), how you played ("no damage taken", "turns under 5"), equip conditions (D-100: a character with a certain skill, spell, adapter, weapon or evolution, optionally under its requirements, at an upgrade level), weather, hidden counters.
- **Checked:** at the event it listens to (fight end, boss defeated, run end), every run.
- **Gives:** as 3.1, and can carry a reward (§4).
- **Example:** *(Crown)*

### 3.3 Lore object
- **What:** a readable thing on a map (`ATrialLoreObject`): a book, a carving, an echo. **No conditions:** anyone can read it.
- **Gives:** a journal lore entry (once per object). It is the place where the world hints at boss weaknesses (SLF's holy water and Setsuna's grave; `HiddenQuests.md` §3). That is world text, not a hint about a memory's conditions.
- **Co-op:** everyone who reads it.
- **Example:** *(Crown)*

### 3.4 Bound gear and reaction memory
- **What:** gear that belongs to a character (D-033), or a set of items worn **while facing a character**, unlocks something (`RunStructure.md` §5 "reaction triggers").
- **How found:** wear it to the right fight. Its owner's bound bonus stays hidden unless its owner wears it.
- **Gives:** a memory, a line in the fight, a quest start, a boss trigger (the boss's own rule, D-100).
- **Example:** *(Crown)*

### 3.5 Chain memory
- **What:** a memory whose conditions include **earlier memories**. Chains carry a character's arc.
- **Rule:** a chain never needs a memory from a character the player can no longer get (`HiddenQuests.md` §3).
- **Chains cross characters** (**Crown, 2026-11-08**): a memory of Orthos's can need one of Garnet's, as long as both can be reached solo (Persona and Expedition 33 camp scenes unlock from other characters' progress).
- **The team's progress counts in co-op** (Crown, 2026-11-08): an "earlier memory / quest done" condition is met when **any player present** has it ("my Garnet quest is done, your Orthos quest is done" → the memory needing both can unlock). Solo, the player needs them all. A memory unlocked this way follows §1.7 for who receives it.
- **Example:** *(Crown)*

### 3.6 Hidden quest (unique scenario)
- **What:** a multi-step quest found like a hidden memory: a **start** (any memory of any kind and depth, §2, or a reaction), **steps** (each one a condition set, done in order), and an **end** (a memory and a reward). This is SLF's Unique Scenario.
- **Progress:** each step done is saved per player and kept across runs. Only the step in progress resets at run end. This keeps the "never missed for good" rule in a 30 to 45 minute run game. **Crown, 2026-11-08** (Monster Hunter's research quests and Hades' prophecies, both kept across runs).
- **The player is told** a quest started and when a step is done, privately, with **no** text about what the next step is (the no-hints rule).
- **Co-op:** the player who started it owns it. Others join the progress only if they started it too (§1.7).
- **Gives:** rewards (§4); can open a new trial variation, an NPC or a boss trigger.
- **Example:** *(Crown)*

### 3.7 Pair memory (co-op or party)
- **What:** needs two named characters **both present** (in one party solo, or across two players in co-op).
- **Rule:** never on the main story; never co-op-only for a main chain (`HiddenQuests.md` §3). In solo, both can be in your party.
- **Gives:** a shared scene or conversation between the two.
- **Example:** *(Crown)*

### 3.8 Curse
- **What:** a reward that is also a penalty, and a key: it weakens something and opens other secrets (SLF's curses, D-100).
- **Lasts:** until removed by its own quest or memory. It stays across runs.
- **Example:** *(Crown)*

### 3.9 Counter milestone
- **What:** a hidden running total on the save ("parried 50 times as Sapphire", SLF's hidden stats). Reaching it unlocks a memory or reward.
- **Rule:** counters keep counting across runs. Only "how you played" conditions stay inside one fight or one run.
- **Example:** *(Crown)*

### 3.10 Story-phase memory (the main line)
- **What:** the memories that move the main story theme by theme (`HubAndStory.md` §4: lost Present memories first, then the Past...). The bar night is one of the first.
- **Rule:** plain conditions only (finish a trial, reach a phase), solo-reachable, never rare, never co-op-only.
- **Open (lore, Crown's):** how one phase unlocks the next.
- **Example:** *(Crown)*

## 4. What an unlock can give

| Reward | Notes |
|---|---|
| Journal entry | always |
| Scene / dialogue | plays once, then rereadable |
| Character conversation | camp-style, between characters (`HubAndStory.md` §4) |
| V line | between runs |
| Unique item | different, not stronger |
| Skill or spell | learned by that character (D-111: skills belong to whoever learned them) |
| Title | display only |
| Curse | 3.8 |
| Boss change | through the boss's own rule list (D-100, B-355); the memory is a condition there |
| New place | a trial variation, a hub spot or an NPC |

## 5. Authoring template (one per memory or quest)

| Field | Fill in |
|---|---|
| Id / title | |
| Kind | 3.1 to 3.10 |
| Scope and depth per character | §2 |
| Condition sets (any one opens it; all conditions in a set must hold) | |
| Earlier memories needed | |
| Steps (hidden quest only) | |
| Rewards | §4 |
| What it opens (conversations, V lines, boss rules, places) | |
| Story text | Crown |

## 6. To build later (not decided here)

- A "party has" condition for pair memories (3.7).
- Hidden quest steps and their saved progress (3.6); curses (3.8); counters (3.9). D-059: explicit save structs.
- The checker indexed by event type, and a `MemoryDebug` printing which conditions pass for any memory (`HiddenQuests.md` §3).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | First draft: rules, scopes, ten kinds, rewards, template; defaults marked | `dev-xj6xnt` |
