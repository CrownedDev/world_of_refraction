# One player character (D-228): the plan

**Status:** plan updated for Crown's answers (D-228 plan questions, D-229 title and save list, D-234 Starting spirit); nothing built. **Rules:** D-228, D-229, D-234, with D-227 (class locked; test starts stay full), D-221 (the trial kit), D-224 (class kits), D-232 / B-537 (the menu level). **Branch:** `docs/D-228-one-player-character-plan`, then `docs/D-228-plan-rulings`.

**In one line:** the player becomes one shared character. The body, the class and the class pick are choices saved with the game, not separate data assets. The game builds the player's character data from those choices when a game starts or loads. Enemies and named party members keep their own assets.

## 1. What a player asset holds today, and what really differs

A player character is a `UCharacterData` (`Character/CharacterData.h`) plus a `UInventoryData` (`Inventory/InventoryData.h`). The 13 old player assets (the "Lords") and the 5 new cast assets (`DA_Character_Generic`, `_Refractor`, `_Resonator`, `_BrokenDarkness`, `_Reality`) all have the same shape. Sources: `Audits/CharacterAssetDump.md` (2026-11-15) and `AssetOverhaulPlan.md` §2.

**The same on every player:** the cosmetics asset (all point to `DA_Cosmetics_Default`), infusion mode (Balanced), the placeholder stats (max HP 3, max EP 3, and the other stats at 3 or 0), and origin (Player). The body (Manny / Quinn) is already a separate choice: it is saved as the base form (`SaveSubsystem.cpp:541`), not stored on the character.

**What differs by class:**
| Field | Generic | Resonator | Refractor (Caster) |
|---|---|---|---|
| `CharacterClass` | Generic | Resonator | Caster |
| World levels (Mind / Body / Spirit) | 1 / 7 / 1 | 1 / 1 / 1 | 3 / 3 / 3 (Darkness and BD: 1 / 1 / 1) |
| `ActionSpeed` | 40 | 0 | 0 |
| Resistances | physical +15, every element -5 | physical -15, elements 0 | by element (below) |
| Starting gear | weapons | a weapon and rings | rings, or nothing |

**What the element Lords differ in, beyond innate spells:**
- `InnateElement`. Broken Darkness is Darkness with `bBrokenDarknessInnate` on.
- **The resistance profile.** The own element is +15, a weak element -15, and Reality -15 on almost all. Reality's own is +10 to every element and -10 Void. BD is -5 everywhere and Reality -15.
- **World levels.** 3 / 3 / 3, except Darkness and BD at 1 / 1 / 1.
- **Their inventory's four rings** of that element.
- **Evolutions.** BD has two, Water one, the rest none.
- **Innate spells.** Fire and Water have their own element spells (16 and 17). The others use the shared pool spells (12).
- **Portrait.** Each has its own.
- `TurnSpeed` also differs, but it is obsolete (B-170h) and nothing reads it.

**So the real per-player differences are small:** the class, the element (or BD / Reality), a few numbers (world levels, action speed, resistances), the innate spells, and the starting pieces. Everything else is shared.

## 2. The new shape

| Piece | What it is | New or reused |
|---|---|---|
| **`DA_Character_Player`** | One `UCharacterData` holding what every player shares: cosmetics, base stats, origin Player. | New asset; no new class |
| **The class table** | One row per class with what the class changes: world levels, action speed, resistances, portrait, the trial kit's base (apparel, `StartingSkills`), and **what the new-game pick offers**: a Generic's weapon list, a Resonator's crystal-type list (it starts with two empty adapters). The Refractor's row has **one entry per element choice**, Broken Darkness and Reality included. Each entry holds that element's `InnateElement`, BD flag, innate spells, resistances and world levels. | Added to `UClassDefaultLoadoutData` (`Loadout/ClassDefaultLoadoutData.h:97`) beside `Defaults`; the kit pieces stay as `FClassDefaultLoadout` |
| **The player's picks** | One small saved struct: the **typed name** (D-228 Q6), the **Starting spirit** (Male Nobody / Female Nobody, D-234), class, the Refractor's element choice (an element, BD or Reality), a Generic's **two weapons** (a sword and a shield are two separate picks, D-228 Q2), a Resonator's **two crystal types**. Locked for the game (D-227). | New struct, saved with the leader |
| **Building the player** | A function that makes the player's `UCharacterData` from the shared asset, the class row and the picks: a copy of `DA_Character_Player` with the class, element and numbers filled in. It gets a fixed name, so the pool's key stays the same across loads. | New function |
| **Starting gear** | The picked pieces are the starting gear, all at **F tier**: minted once and worn, through B-529's one seed (`UPoolSubsystem::SeedStartingGear`). A Resonator gets **two empty adapters** and **3 crystals of each picked type** (F tier; the count is one named tunable, D-228) and sockets them itself. | Reused; it reads the picks instead of an inventory asset |

**Why build a character, not change every reader:** about 47 places read `InnateElement` / `bBrokenDarknessInnate`, and many more read `CharacterClass` (`CharacterData.h:129-144`, helpers `:274-302`). A built character feeds all of them with no change. Elden Ring and Dark Souls work this way: a new game takes one base character, applies the starting class and gift, and saves the result.

## 3. The pick sets the trial kit (D-228 a)

Today a trial start wears the class's fixed row: `FindClassDefaults(Class)` (`TrialRunSubsystem.cpp:1243`, read at `:241`). The Generic's row is sword and shield, the Caster's is empty, the Resonator's is two adapters.

**New:** the trial kit is **the class row's base plus the picked pieces**:
- a Generic gets its two picked weapons;
- a Resonator gets its two empty adapters and 3 F-tier crystals of each picked type;
- a Refractor gets nothing beyond its innate.

These are kit copies, minted once and reset at each trial start, as today (D-221). They are not the player's own starting pieces. `FindClassDefaults` takes the member's picks; `MintClassDefaultKit`, `ResetClassDefaultKit` and `WearClassDefaultKit` stay as they are.

## 4. What reads character data today and would change

| Reader | Where | Change |
|---|---|---|
| New-game screen: finds an asset by class and element | `UI/Menus/NewGameScreenModel.cpp:127` (`FindStartCharacter`), `:160` (`Begin`); the list `PartySessionSubsystem.h:68` (`NewGameCharacters`, `DefaultGame.ini`) | Shows the picks instead (BD and Reality inside the Refractor's choice; weapon and ring lists from the class table). `Begin` passes the picks |
| New game | `Save/SaveSubsystem.cpp:1024` (`StartFreshGame`) | Takes the picks, builds the player, makes it the leader |
| The leader rule | `Player/WoRGameModeBase.cpp:22` (`StandAsPartyLeader`) | No change: it already copies the leader's character data onto the pawn. That is now the built character, so Character Template is never the player's |
| Starting gear | `Inventory/InventoryComponent.cpp:1589-1601`; `Trial/TrialRunSubsystem.cpp` (`StartRunGearAndGold`, the seed) | Reads the picked pieces, not `Inventory->StartingGear` |
| Save and load | `SaveSubsystem.cpp:1293` (saves the member's asset path), `:1656` (loads it) | The leader saves its **picks** and is rebuilt on load. The save version goes up; an older save is wiped with a warning (D-060) |
| The pool's key | `Pool/PoolSubsystem.cpp:341` (key = the asset's path) | The built character's fixed name keeps one key per player |
| Co-op second player | `Party/PartySessionSubsystem.cpp:821-832` (a joiner bringing a character already in the party joins as a copy, keyed by member); `WoRPlayerState.h:251` (`ClaimedCharacters`) | Each player's character is built from their own picks. Two players with the same class get separate keys from their member ids (as the copy rule does today) |

## 4b. The title and the save list (D-229)

- **Title:** **Continue** (the last played character) and **Load**. No New Game entry, no 2-second hold, and **no Join a friend** (joining is reached from the in-game menu only).
- **Load** lists the characters, **one save file each**. **New game sits under the last character.** With the list full, New game is not offered (default). Deleting a character is a row action with a yes / no confirm.
- **The most save files** is **one configurable value**. Its default is the number of class choices: Generic, Resonator and each Refractor choice.
- **The menu level exists** (B-537, D-232): the title and the new-game screen run there; Begin and Continue travel to the hub.
- **Readers that change:** the title screen (Continue / Load), `USaveSubsystem` (one slot per character instead of one save), and `StartFreshGame` (writes a new slot).

## 4c. Run levels kept in the hub (D-237, built in stage 3)

What it touches today:
- **Where run levels end:** `UTrialRunSubsystem::RunLevels` is reset in `EnterTrial` (the run starts empty) and in `FinishRun` (every run end: boss win, extract, out of wipes; a quit settles through `SettleRunAsQuit`, which calls it too). Nothing outside a run holds them. D-237: `FinishRun` copies them to the kept levels before the reset (a quit keeps the pre-run ones).
- **The save (CS-5):** `FSavedRun::RunLevels` (`FSavedRunLevels`: Mind, Body, Spirit, purchases, over-cap, unspent) is written only while a run is open, so a resumed or settled run has them and the hub does not. D-237 adds a kept copy per character outside the run (stage 3's per-character save; until then on `FSavedPartyMember`).
- **The live level:** `UCharacterDataComponent` adds the asset's world level, the member's head start (`FPartyMember::HeadStart*`, B-348b) and, in a run only, the run's earned levels (`FindRunLevelStore` is null outside a run). The hub would add the kept levels; a trial still starts without them.
- **The trial start:** `EnterTrial` resets the run levels, so the next trial starts from the asset's levels plus the head start. That matches "the next trial starts from the class's base levels" if the head start counts as part of the base (it is the perk-raised floor, kept across runs).
- **A death (Crown, 2026-11-16: in the trial, at the death):** in code a death is a **party wipe**: `WoRDefeat::ApplyWipe` (`DefeatRules.cpp`) calls `UTrialRunSubsystem::RecordWipe` when the whole party falls in a fight; one member down is not counted (they can be revived in the fight). D-237 takes the points there, for each member, from the run levels straight away (a named tunable, 2 now, each point rolled on its own on the run's seed, `GetRunSeed`), so the rest of the run is played with fewer. The old wipe loss (`LoseRunPoints`, `DefeatRules::PickPillarToLose`, OBSOLETE since D-160) is the hook, but it picks the pillar with the most points, not a random one, so the pick is new. Only the run's own points can go: the class base and the head start are never touched, and a member with no run points loses nothing.
- **What a lost point does mid-run:**
  - **HP / EP:** max HP follows Body and max EP follows Spirit (`RecomputeMaxPools`). A wipe that does not end the run revives the whole party at full at the rest spot (`ReviveFallenMembers`), and the pawns spawn again (`SeedWorldStats`), so they come back full at the new, lower max; nothing needs clamping.
  - **Weapon and skill requirements:** a pillar falling under a requirement doesn't unequip anything or make it uncastable: the weapon or skill works at `REQUIREMENT_UNMET_DAMAGE_FACTOR` with no pillar scaling (D-119, `FWorldStatRequirements::GetRequirementFactor`) until the level is back.
  - **Vendor level purchases:** the purchase count (the rising price, D-102) stays; a lost point is not refunded.
  - **Over-cap points (B-364):** they exist only at level 7; one lost from a pillar at 7 leaves its over-cap points in place under the current rules (the old TODO took over-cap points first). This is a decision for the build: default, over-cap points go first.
  - **Waiting (unspent) levels:** untouched.

## 4d. The hub's recruits are built like the player (D-238)

The 12 walk-up recruits in the hub stay, but each is **the shared character plus fixed picks**, made by the same build function as the player (stage 3) and starting on its class's F-tier kit (stage 4), not its own `UCharacterData` and `UInventoryData`.

**A recruit's row** (one list, e.g. on the class table asset): the same picks struct as the player, fixed instead of chosen: name, class, the Refractor's element (BD and Reality included), a Generic's two weapons or a Resonator's two crystal types, the Starting spirit (body), and a colour for its box or body (D-238 default). The placed hub actor points at its row.

**Today the 12 are** `BP_CombatCharacter_*` (Generic, Resonator, BrokenDarkness, Darkness, Earth, Fire, Light, Lightning, Reality, Void, Wind, Water), all placed in `TestLevel_Nav` with `UPartyInteractionComponent`, each with its own Lord asset and (casters, Resonator) inventory. Beyond class and element they differ in (section 1, `Audits/CharacterAssetDump.md`):
- **Moves to the class table** (kept, same numbers): world levels (3 / 3 / 3; Darkness and BD 1 / 1 / 1; Generic 1 / 7 / 1), action speed, the resistance profile per element, innate spells (Fire and Water their own, the rest the shared pool), the portrait (each its own placeholder icon).
- **Lost unless the row or class table carries it:** the hand-authored **evolutions** (BD two, Water one, the rest none), the caster inventories' **four element rings** and the old evolution items (replaced by the F-tier kit), the names ("Fire Lord" and so on: the row's name instead).
- **Not read (an editor step):** each Blueprint's own mesh, materials and anim class. `UCharacterData` holds no look (all point at `DA_Cosmetics_Default`), so any unique look sits on the Blueprint; recruits are not dressed by `ApplyFormLook` (players only). Under D-238 the row's spirit and colour give the look.
- `TurnSpeed` also differs but is obsolete (B-170h).

**When:** stage 4b, after stages 3 and 4 (it needs the build function and the kit), before stage 6 deletes the Lords (removal list group 2). Saves store a recruit's pawn class path (`SaveSubsystem.cpp:1301`), so the swap raises the save version (D-060).

## 5. What does not change

- **Enemies** keep their own `DA_Enemy_*` assets and inventories, and the D-225 trial pool.
- **Named party members** (recruits, story characters) keep their own assets and starting gear.
- The body choice (forms), the worn-gear record, the pool, the kit's reset rules (D-221), and how skills are settled at a run's end (D-224).
- **Test starts stay full (D-227).** Today's `_Start` inventories become a dev-only "test start" per class. The finished game uses the picks.

## 6. Stages (each small, ending in Crown's PIE check)

UI-first order: put the UI in, Crown looks, then the code and data are aligned to it and the real path is tested.

| # | Stage | Crown checks |
|---|---|---|
| 1 | **Look only (UI first):** the title, all centred (D-229 as changed by Crown, 2026-11-16): the game's name, then the character slots (each filled one shows only two things (Crown, 2026-11-16): the character's name and the Mind / Body / Spirit levels of its last run, read from the save's run (`FSavedRun::RunLevels`); a run's levels end with it today (D-102), so with no run in the save each shows a dash; from stage 3 the slot reads the kept levels (D-237); the next empty one a +; their count is `[WoR.Title] MaxCharacterSlots`, default one per class choice), then Settings, Credits and Quit as buttons along the bottom; delete is R / Y on a slot with a yes / no. Until the character preview exists the slots stand in the centre; with it they move left (`TitleScreenStyle::SlotsAlignment`). With today's one save it is slot 1 and the + slot 2. Later, when a character figure is added, it goes on the right and the slots move left (Crown, 2026-11-16; this replaces the preview-on-the-left note). The new-game screen with the typed name, Starting spirit, class, the Refractor's element list (BD and Reality inside it), the Generic's two weapon picks (sword and shield separate) and the Resonator's two crystal types ("3 of each, F tier"). Lists come from a stub class table; Begin and Continue still start the old way. | The look and the choices |
| 2 | **Align the data:** the class table and `DA_Character_Player` (new fields, the editor step that fills them from today's assets). The test start keeps working. **Built (B-549):** `UClassDefaultLoadoutData::Classes` on `DA_Loadout_ClassDefaults`, filled by its **Fill From Sources** button; resistances stay in `ClassInnateResistanceTable`; the pick lists stay on the stage 1 stub until stage 4; Reality has a row too (the desktop fill, read back 2026-11-17). Action speed 0 (the Resonator and every Refractor row) is the normal speed, ×1.0. | The numbers per class and element |
| 3 | **Build the player from the picks and save it per character:** the picks struct (name included), the build function, `StartFreshGame`, one save file per character with the max-saves value, Continue and Load real, save version up (old saves wiped with the warning, D-060). The hub pawn is the built character. **Kept run levels (D-237, section 4c):** each wipe takes points off the run levels at once, in the trial (a tunable, 2, each rolled on its own); the per-character save gains the kept Mind / Body / Spirit levels, which are simply the run levels when the run ended (a quit puts back the pre-run ones); the hub adds them; the title slot reads them. **Built (B-554, the first half):** the picks struct (`FPlayerPicks`), the build function (`WoRPlayerBuild`), the picks saved per party member and the player built again on load, one save file per character, the title's slots and the + real, save version 44. The inventory is the old start character's until stage 4. The kept levels are B-555. | New game: the hub shows the picked class, element, spirit and name; Continue and Load bring it back |
| 3b | **The character figure on the title (D-229 addition, Crown 2026-11-16):** the lit slot's character stands on the right; the slots move left (`TitleScreenStyle::SlotsAlignment`). Needs one save per character (stage 3) and the built body. | The figure matches the lit slot; keys, pad and mouse still work |
| 4 | **The picks set the starting gear and the trial kit** (section 3), all F tier; the Resonator's empty adapters and crystals. | The hub wears the picks; a trial starts on them; a quit and an extract behave as now |
| 4b | **Recruits built like the player (D-238, section 4d):** a recruit row per hub recruit, one recruit Blueprint, the 12 placed actors pointed at their rows; the Lords' evolutions and look decided first. **Built (B-557, the first half):** the recruit rows on the class table (Fill Recruits), each placed recruit built from its row by class and element, its picks kept on the member; one recruit Blueprint is not needed yet. Spirits per recruit and the body colour are B-558. | Each recruit joins with its class, element, kit and look; the party screen and a trial behave as now |
| 5 | **Co-op:** a second player picks and joins (from the in-game menu) as their own built character. | 2-player PIE |
| 6 | **Delete the old:** the 13 Lords, their inventories and equipment, `NewGameCharacters`, `FindStartCharacter`. This is part of `AssetOverhaulPlan.md` stage 6, after stage 5 here passes. The `_Start` inventories stay as test starts until Crown retires them. | A new game, and an old save wiped with the warning |

## 7. Crown's answers (D-228, 2026-11-16)

1. **Weapons and crystals offered:** the default stands: every weapon and every crystal type the clean gear set has (approved by the reference-backed default).
2. **Sword and shield:** **two separate picks.** A Generic's two picks can be sword and shield. For skill slots the pair still counts as one weapon (D-224).
3. **Class numbers:** the default stands: class sets world levels and action speed; the Refractor's element sets its resistances.
4. **A Refractor's evolution:** the default stands: none; evolutions are found. Phoenix Fire stays in the test start.
5. **Hub gear and trial kit:** the default stands: one pick sets both.
6. **Name and portrait:** the player **types their name** at the new-game screen; the portrait is by class with the element's colour (default).
7. **Resonator (changed):** two empty starting adapters and two picked crystal types, **3 of each**, at F tier, instead of two prebuilt rings. **Every pick is F tier.**
