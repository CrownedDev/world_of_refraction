# One player character (D-228): the plan

**Status:** plan for Crown to read; nothing built. **Rules:** D-228, with D-227 (class locked; test starts stay full), D-221 (the trial kit), D-224 (class kits). **Branch:** `docs/D-228-one-player-character-plan`.

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
| **The class table** | One row per class with what the class changes: world levels, action speed, resistances, portrait, the trial kit's base (apparel, `StartingSkills`), and **what the new-game pick offers**: a Generic's weapon list, a Resonator's prebuilt-ring list. The Refractor's row has **one entry per element choice**, Broken Darkness and Reality included. Each entry holds that element's `InnateElement`, BD flag, innate spells, resistances and world levels. | Added to `UClassDefaultLoadoutData` (`Loadout/ClassDefaultLoadoutData.h:97`) beside `Defaults`; the kit pieces stay as `FClassDefaultLoadout` |
| **The player's picks** | One small saved struct: body, class, the Refractor's element choice (an element, BD or Reality), a Generic's two weapons, a Resonator's two prebuilt rings. Locked for the game (D-227). | New struct, saved with the leader |
| **Building the player** | A function that makes the player's `UCharacterData` from the shared asset, the class row and the picks: a copy of `DA_Character_Player` with the class, element and numbers filled in. It gets a fixed name, so the pool's key stays the same across loads. | New function |
| **Starting gear** | The picked pieces are the starting gear: minted once and worn, through B-529's one seed (`UPoolSubsystem::SeedStartingGear`). | Reused; it reads the picks instead of an inventory asset |

**Why build a character, not change every reader:** about 47 places read `InnateElement` / `bBrokenDarknessInnate`, and many more read `CharacterClass` (`CharacterData.h:129-144`, helpers `:274-302`). A built character feeds all of them with no change. Elden Ring and Dark Souls work this way: a new game takes one base character, applies the starting class and gift, and saves the result.

## 3. The pick sets the trial kit (D-228 a)

Today a trial start wears the class's fixed row: `FindClassDefaults(Class)` (`TrialRunSubsystem.cpp:1243`, read at `:241`). The Generic's row is sword and shield, the Caster's is empty, the Resonator's is two adapters.

**New:** the trial kit is **the class row's base plus the picked pieces**:
- a Generic gets its two picked weapons;
- a Resonator gets its two picked prebuilt rings;
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

## 5. What does not change

- **Enemies** keep their own `DA_Enemy_*` assets and inventories, and the D-225 trial pool.
- **Named party members** (recruits, story characters) keep their own assets and starting gear.
- The body choice (forms), the worn-gear record, the pool, the kit's reset rules (D-221), and how skills are settled at a run's end (D-224).
- **Test starts stay full (D-227).** Today's `_Start` inventories become a dev-only "test start" per class. The finished game uses the picks.

## 6. Stages (each small, ending in Crown's PIE check)

UI-first order: put the UI in, Crown looks, then the code and data are aligned to it and the real path is tested.

| # | Stage | Crown checks |
|---|---|---|
| 1 | **New-game screen shows the picks** (plain placeholder UI). Body, class, the Refractor's element list with BD and Reality, the Generic's two weapon choices, the Resonator's two ring choices. Lists come from a stub class table; Begin still starts the old way. | The look and the choices |
| 2 | **The class table and `DA_Character_Player`**: the new fields and the editor step that fills them from today's assets. The test start keeps working. | The numbers per class and element |
| 3 | **Build the player from the picks**: the saved picks struct, the build function, `StartFreshGame`, save and load (version up). The hub pawn is the built character (B-529's rule). | New game: the hub shows the picked class and element; a reload keeps it |
| 4 | **The pick sets the starting gear and the trial kit** (§3). | The hub wears the picks; a trial starts on them; a quit and an extract behave as now |
| 5 | **Co-op**: a second player picks and joins as their own built character. | 2-player PIE |
| 6 | **Delete the old**: the 13 Lords, their inventories and equipment, `NewGameCharacters`, `FindStartCharacter`. This is part of `AssetOverhaulPlan.md` stage 6, after stage 5 here passes. The `_Start` inventories stay as test starts until Crown retires them. | A new game, and an old save wiped with the warning |

## 7. Open questions for Crown (defaults are what gets built unless Crown changes them)

1. **Which weapons and prebuilt rings are offered?**
   - **Default:** every weapon and one prebuilt ring of each element that the clean gear set has.
   - **Reference:** Dark Souls' and Elden Ring's starting gifts offer a short fixed list; a short list keeps the choice readable.
2. **Is a sword with its shield one weapon pick?**
   - **Default:** yes. A pair counts as one held piece (D-224 Q8), so a Generic picks two held pieces: for example sword and shield plus the axes pair.
   - **Reference:** Elden Ring treats a weapon and shield as two hands of one stance.
3. **Do class numbers differ only by class, or also by element?**
   - **Default:** class sets the world levels and action speed; the Refractor's element sets its resistances (own element strong, its opposite weak), as the Lords do now. This keeps one table row per element choice.
   - **Reference:** Pokémon's type chart, and Elden Ring's classes differing only in starting stats.
4. **Does the Refractor also start with an evolution** (the cast gives the Fire Refractor Phoenix Fire)?
   - **Default:** no. D-224 says a Refractor "starts with nothing" beyond its innate; evolutions are found. Phoenix Fire stays in the test start.
5. **Is the hub's starting gear the same as the trial kit's pieces?**
   - **Default:** yes: one pick sets both (D-228 a). The hub gets its own pieces once; each trial gets fresh kit copies.
   - **Reference:** Hades: the weapon you pick is the one you start every run with.
6. **The player's name and portrait.**
   - Name: open (lore-adjacent), no outside recommendation. Options: a name typed at the new-game screen; the class name ("Generic"); a fixed story name.
   - Portrait: by class, with the element's colour (default).
