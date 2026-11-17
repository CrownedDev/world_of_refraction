# Old asset removal list (read-only audit)

**Date:** 2026-11-16 · **Branch read:** `dev` · **Nothing was deleted, moved, renamed, edited or saved.**

**What this is:** every old character, inventory, loadout piece, ring and Blueprint that the new format (D-221 to D-236) replaces, who still points at it, and when it is safe to remove.

**Counts**

| Group | Meaning | Assets |
|---|---|---|
| 1 | Safe to delete now | **34** |
| 2 | After the one-player work (D-228) | **43** |
| 3 | After the trial pool (D-225) | **4** |
| Keep | Still the only source of something | listed in §5 |
| Ask Crown | Could not be placed | 2 (§6) |

## How it was checked, and the limits

| Check | How | Limit |
|---|---|---|
| Other assets and levels | The editor's asset registry (`get_referencers` / `get_dependencies`) for every asset below. A level that places an asset shows up as a referencer. | The registry sees saved packages only. It does not see a path typed as text inside a Blueprint graph. **Unverified.** |
| Config | Grep of `Config/*.ini` for `/Game/Data` and `/Game/Blueprints`. | Complete. |
| C++ | Grep of `Source/` for hard-coded paths and for each old asset name. | Complete. No old asset is named in C++. |
| Loot by tag | `TrialLoot::GatherPool` (`TrialLoot.cpp:63`) finds every gear asset by its `SourceTags`, with no reference. I read `sourceTags` on every old gear piece: **all empty**, so none can drop. | Read through the editor; nothing was marked dirty. |
| Placed actors | `TestLevel_Nav` was **not** open (the editor had `L_FrontEnd` open) and I did not load it. The level's package references all 12 `BP_CombatCharacter_*`. | **What each placed Lord is for is from the docs, not read per actor.** `PartySystem.md:147` says every placed `BP_CombatCharacter_*` is a walk-up recruit. No external-actor packages exist. |
| Save files | Not read. A save stores each party member's pawn class as a path (`SaveSubsystem.cpp:1301`, `:1664`). | **Unverified:** a save made with a Lord recruited would point at a deleted Blueprint. D-060 wipes a save below the minimum version, so raise the version with the group 2 delete. |
| Redirectors | 9 found under `/Game/Data` (§2). Their targets were read. | Two point at assets that no longer exist. Use the editor's **Fix Up Redirectors**, not a plain delete. |
| The log | `Saved/Logs` was **blocked for this session** (a permission rule), so the file was not read. The warnings in §7 come from the editor's own output log for the running session (started 22:08, two hub loads at 22:09). | One session only. |

## 1. Group 1: safe to delete now (34)

Nothing points at these: no asset, level, config line or C++ path.

| # | Asset | What it was | Note |
|---|---|---|---|
| 1 | `Characters/Data/Caster/Darkness/Inventory/Loadouts/Loadout0/DA_Equpment_Darkness_ERing_Fire` | Old baked ring (3 built-in spells) | Delete **before** group 2's `DA_Equpment_Darkness_Ecrystal_Fire`: it points at it |
| 2 | `Characters/Data/Caster/Fire/Inventory/Loadouts/Loadout0/DA_Equipment_Fire_Ring_Reality` | Old ring with a baked Iolite | |
| 3 | `Characters/Data/Generic/Inventory/Loadouts/Loadout0/DA_Generic_FireEvo` | Old item (`ItemData`) | Not in the plan's delete list; found here |
| 4 | `Characters/Data/DA_Inventory_Default` | Empty inventory | |
| 5 | `Enemy/DA_Enemy_Resonator` | Old Resonator enemy | Nothing spawns it. The plan keeps only the Generic and Darkness enemies. Deleting it frees `DA_Inventory_Resonator` for group 2 |
| 6-10 | `Skills/Weapons/DA_Weapon_SAS`, `DA_GreatSword`, `DA_Weapon_Dagger`, `DA_Weapon_DualAxe`, `Gauntlets/DA_Weapon_Gauntlets` | The first weapon set, with built-in spells and abilities | Delete the 5 weapon assets only. **Keep the folders' attacks, abilities, stances and montages:** the new start inventories use them |
| 11-13 | `/Game/Testing/Characters/DA_Test_Char_DualCaster`, `DA_Test_Generic`, `DA_Test_Resonator` | Old test characters | Remove their 3 lines from `Scripts/ue/validation_baseline.txt` in the same change |
| 14-20 | `/Game/Testing/Rings/DA_Test_Ring_Fire`, `_Fire_Evolved`, `_Light`, `_Lightning`, `_Water`, `_Water_Evolved`, `_Wind` | Old test rings | |
| 21-24 | `/Game/Testing/Weapons/DA_Test_Weapon_Dagger`, `_Spear`, `_Spear_Evolved`, `_Sword_Evolved` | Old test weapons | |
| 25 | `Content/Testing/Weapons/DA_Test_Weapon_Sword.uasset` | A broken file | The editor cannot load it (error at every start, §7). Delete the file on disk; it is not in the registry |
| 26-34 | The 9 redirectors in §2 | Leftovers of old moves | Fix Up Redirectors |

**Order (no dangling reference at any step):**
1. Rows 1 to 5, in that order.
2. Rows 6 to 10 (the 5 weapon assets).
3. Rows 11 to 25.
4. The redirectors, in the order in §2.
5. Remove the folders that are now empty. **Do not delete `Characters/Data/` itself** (§5, `DA_Cosmetics_Default`).

**To re-point first:** nothing.

## 2. The redirectors (part of group 1)

| # | Redirector | Points at | Referenced by |
|---|---|---|---|
| 1 | `Testing/RefinedCrystals/DA_Test_EvoCrystal_Water` | `/Game/Testing/RefinedCrystals/DA_Test_EvoCrystal_Wind` (**does not exist**) | nothing |
| 2 | `Merchant/DA_Merchant_Test` | `Pool/Merchant/DA_Merchant_Blacksmith` | nothing |
| 3 | `Characters/Models/BP_CombatCharacterBase_Fire` | `BP_CombatCharacter_Fire` | nothing |
| 4 | `Characters/Models/BP_CombatCharacterBase_Water` | `BP_CombatCharacter_Water` | nothing |
| 5 | `Characters/Models/BP_TestCombatCharacter_Water` | `BP_TestCombatCharacter_Water_past` (**does not exist**) | nothing |
| 6 | `Characters/Models/BP_TestCombatCharacter_Fire` | redirector 7 | nothing |
| 7 | `Characters/Models/BP_CombatCharacterBase` | redirector 8 | redirector 6 |
| 8 | `Characters/Models/CharacterBase/BP_CombatCharacterBase` | redirector 9 | redirector 7 |
| 9 | `Characters/Models/CharacterBase/BP_CharacterBase` | `BP_TestCharacterBase` | redirector 8 |

Delete 6, then 7, 8, 9. `Characters/Models/CharacterBase/` is in `DirectoriesToAlwaysCook` (`DefaultGame.ini:107`) and holds live assets (meshes, input, `BP_CombatPlayerController`): only the redirectors go.

## 3. Group 2: after the one-player work, D-228 (43)

These are the old player characters. They are still on the new-game list or stand in the hub.

| Asset | What it was | Who points at it |
|---|---|---|
| `Characters/DA_Character_Template` | The player's default character | `BP_TestCharacterBase` (the default pawn: `DefaultGame.ini:76`; the hub, trial and battle game modes) |
| 12 Lords under `Characters/Data/`: Generic, Resonator, BrokenDarkness, Darkness, Earth, Fire, Light, Lightning, Reality, Void, Wind, Water | One player character per class and element | Each by its own `BP_CombatCharacter_*`. **7 are also on the new-game list** (`DefaultGame.ini:81-87`: Water, Earth, Wind, Light, Darkness, Lightning, Void) |
| 12 `/Game/Blueprints/Characters/BP_CombatCharacter_*` | The Lords' bodies | **`TestLevel_Nav` places all 12** (hub recruits, per `PartySystem.md:147`; not read per actor). `_Fire` is also the default pawn of the old `BP_Combat_GameMode`. `_Fire` and `_Water` are redirector targets (§2) |
| 10 caster inventories: `DA_Inventory_Darkness`, `_Earth`, `_Light`, `_Lightning`, `_Reality`, `_Void`, `_Wind`, `_BD`, `_Fire`, `_Water` | Old starting gear | Each by its Lord only |
| `DA_BD_Equpment_Eearth`, `DA_Equpment_Darkness_Ecrystal_Fire`, `DA_Equipment_Elightning` | Old evolution items | `DA_Inventory_BD` (the first two), `DA_Inventory_Water` |
| `DA_Inventory_Resonator`, `DA_Resonator_Equpment_Sword`, `DA_Equpment_Resonator_Ring_Light`, `_Void`, `_Wind` | The old Resonator's gear (rings with baked A-tier crystals) | The inventory: `DA_Character_ResonatorLord` and `DA_Enemy_Resonator`. **In group 2 only once `DA_Enemy_Resonator` is deleted (group 1, row 5); otherwise group 3** |

**To re-point first (listed, not done):**
- `DefaultGame.ini:81-87`: take the 7 Lords off `NewGameCharacters` (D-228 removes the list).
- `BP_TestCharacterBase`: its character data off `DA_Character_Template`.
- `TestLevel_Nav`: remove or replace the 12 placed Lords.
- `BP_Combat_GameMode`: its default pawn off `BP_CombatCharacter_Fire` (or delete it: only a redirector at `/Game/Level/BP_Combat_GameMode` points at it).
- Raise the save version, so old saves holding a Lord are wiped (D-060).

**Order:** level actors and config, then the 12 Blueprints, then the 12 Lords and Template, then the inventories, then the gear and evolution items.

## 4. Group 3: after the trial pool, D-225 (4)

| Asset | What it was | Who points at it |
|---|---|---|
| `Characters/Data/Generic/Inventory/DA_Inventory_Generic` | The old Generic inventory, old loadout shape | `DA_Enemy_Generic` (**the live trial enemy**), `DA_Character_Template`, `DA_Character_GenericLord`. It already holds new pieces too (the adapter, sword, shield, both axes) |
| `Characters/Data/DA_Test_Inventory` | A test inventory, old loadout shape | `DA_Enemy_Darkness` |
| `DA_Equipment_Generic_SAS` | Old sword-and-shield with a baked Garnet and 3 Fire spells | `DA_Inventory_Generic`, `DA_Test_Inventory` |
| `DA_Equipment_Generic_DualAxe` | Old dual axe with a baked Sapphire | `DA_Inventory_Generic`, `DA_Test_Inventory` |

**The old loadout shape** (two fixed slots, `loadout0`, baked crystals) lives in the two inventories above. It goes when enemies get their own inventories (`DA_Inventory_Enemy_Generic`, `DA_Inventory_Enemy_Darkness` in the plan; neither exists yet).

**To re-point first:** `DA_Enemy_Generic` and `DA_Enemy_Darkness` onto their own new inventories. `DA_Inventory_Generic` also waits for group 2.

**Order:** the two inventories, then the two weapons. Then remove their 2 lines from `validation_baseline.txt`.

## 5. Keep

| Asset | Why |
|---|---|
| **`Characters/Data/DA_Cosmetics_Default`** | **Sits inside the old folder.** All 5 new characters, all 3 enemies and every Lord use it. It is the only cosmetics asset |
| `Enemy/DA_Enemy_Generic` | The live enemy (`BP_Enemy_Generic`, placed in `TestLevel_Trial_01`, used by `BP_BattleGameMode`) |
| `Enemy/DA_Enemy_Darkness` | Nothing spawns it today, but the plan keeps a Darkness enemy. Crown may move it to group 1 |
| `BP_TestCharacterBase`, `BP_EnemyBase`, `BP_Enemy_Generic` | The player's and the enemy's bodies |
| The 5 new characters and their `_Start` inventories | The new format. No asset points at the characters: 4 are on `NewGameCharacters` (`DefaultGame.ini:78-80, 88`); `DA_Character_BrokenDarkness` is on no list |
| 36 pool rings with baked crystals, and `DA_Ring_Generic` | **The Jeweller's only stock** (`DA_Merchant_Jewler`, placed in the hub). The old caster inventories also hold them. `DA_Ring_Generic` is also hard-coded (`TestGearDebug.cpp:29`). They go when the Jeweller sells adapters and prebuilt finds instead |
| 18 pool weapons | The Blacksmith's stock. `DA_Weapon_ArmingSword` is hard-coded (`TestGearDebug.cpp:28`, `RunEndDebug.cpp:19`) |
| 27 pool evolutions | The Evolutionist's stock |
| `Skills/Weapons/**` attacks, abilities, stances, montages | Used by the new start inventories and by `LightHitMontageBuilder.cpp` |
| `/Game/Testing/Weapons/Abilities/DA_Test_Ability` | Hard-coded (`CombatOrchestratorDebug.cpp:495`, `:1036`) |
| `/Game/Testing/BP_Test_TurnManager`, `BP_Test_StatusEffectManager`, `Actor/BP_TestCombatActor` (and its `DA_Test_Caster`) | Legacy test actors: retired only by D-067 (a matching spec and Crown's PIE pass) |

## 6. Ask Crown (could not be placed)

| Asset | What I found |
|---|---|
| `Pool/Generic/Rings/DA_Ring_Adapter_C` | Only `DA_Inventory_Generic` holds it. `docs/PIE/B-527.md:16` treats it as a live adapter. If it is a leftover copy of `DA_Ring_Adapter`, it goes with group 3 |
| 9 pool weapons with no attack, stance or abilities: `DA_Weapon_TwinHandaxes`, `TwinTomahawks`, `SaiPair`, `TwinDaggers`, `TwinKris`, `TwinKatanas`, `TwinSabres`, `TwinShortswords`, `CombatGauntlets` | Nothing points at them and they cannot drop. They are unfinished, not old-format, so they are not counted in group 1 |

## 7. Warnings a hub load prints today

Read from the editor's output log (this session; `Saved/Logs` could not be read).

**From old assets**

| Warning | How often | Cause | Goes away with |
|---|---|---|---|
| `[InventoryData] 'DA_Inventory_Generic' validation: Loadout[0] 'loadout0': Too many S-tier crystals equipped: 3 (max 1)` | Once per hub load | The old `loadout0` in `DA_Inventory_Generic` | **Group 2 in the hub** (the player's pawn and the Generic Lord are its hub users; which of the two prints it is unverified). **Group 3 everywhere** (the Generic enemy still loads it in a trial) |
| `LogWoRLoadout: Cleared invalid loadout 0 slot: EquippedSTierCrystal ... S Emerald x2` | Once per hub load | The same loadout | The same |
| `[CombatAnimInstance] NativeInit ran, notify bound for ABP_TestCombatCharacter_C_0` | 13 per hub load | One per body in the level: 12 Lords and the player. A code log at Warning level, not a fault | 12 of 13 with **group 2**. The last one needs the log level lowered in code |
| `LogAssetRegistry: Error: Package is unloadable: .../Content/Testing/Weapons/DA_Test_Weapon_Sword.uasset` | Once per editor start | A broken file | **Group 1** (row 25) |

**Also in `Scripts/ue/validation_baseline.txt`** (shown by `test.sh`, not by a hub load): `DA_Test_Inventory` and `DA_Inventory_Generic` (group 3), 3 test characters (group 1), `DA_Test_Caster` and `DA_Test_Ability` (kept).

**Not from old assets (not this job):**
- `Failed to find object ... DA_CombatTuning_Default` and `DA_ResistanceProfile_Class`: optional default assets that were never made.
- `PoseAsset ... is out-of-date` (16 lines): a vendor pack.
- `No GameplayCueNotifyPaths were specified`.
- `[TrialDoor] Tint` and `SetAutoActivate ... FX_WindLines`.
- `BP_HubPlayerController: Accessed None ... GetPawn`.
- The five merchants' empty stock rows (baseline only).

## 8. The three riskiest items

1. **`DA_Cosmetics_Default` lives in the old `Characters/Data/` folder.** Deleting that folder whole breaks all 5 new characters and the enemies. Move it out first, or delete asset by asset.
2. **`DA_Inventory_Generic` has three owners:** the player's default pawn, the Generic Lord and the live Generic enemy. It is the source of the S-tier warning, and it already carries new pieces. It can only go last (group 3), after the enemy has its own inventory.
3. **Paths the registry cannot see.** Saves store a member's pawn class as text, and `NewGameCharacters` names 7 Lords. Deleting the Lords without the config edit and a save-version bump leaves broken paths. Blueprint graphs were not searched for typed paths, and the 12 placed Lords were not read actor by actor.

## Noticed, outside this job

- `/Game/Level/BP_Combat_GameMode` and `/Game/Core/Hub/BP_Hub_GameMode` are redirectors; `/Game/Level/NewBlueprint` points at the first.
- `Data/Characters/BP_CharacterDataTester` and `Data/Items/BP_ItemDataTester`: nothing points at them (D-067 applies).
- `LevelFireLord` is on `MapsToCook` (`DefaultGame.ini:105`) and references no game data.
