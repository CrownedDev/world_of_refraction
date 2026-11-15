# Asset overhaul plan (D-223)

**Status:** plan only, no code yet. **Crown approves the cast and gear list (§2) before anything is built.**
**Inputs:** D-220, D-221, D-222, D-223 (the clean gear set, rings named by their socket, a ring's tier); `Audits/CharacterAssetDump.md` (its "Anything that looks odd" list is the checklist); `Design/LoadoutRestructurePlan.md` (this brings its stage 4 forward); `Conventions/ContentNaming.md`.

## In plain words

The characters and their gear are still authored the old way: two fixed slots (primary / secondary), crystals baked into weapons and rings, spell lists on weapons, and a shield that is part of the sword. We make **new, small, clean assets** in the new shape: each character's starting gear is a plain list of pieces, rings are empty, and crystals are separate items you put in. Then we delete the old assets. The code learns to read only the new shape. Nothing converts the old one.

## 1. The new authored shape

### The character (`UCharacterData`, kept)

- **Kept:** the name, class, innate element, the Broken Darkness flag, the portrait, origin, cosmetics, resistance profile, the sub-stats, and `Inventory` (the link to its starting asset below).
- **Gone:** `TurnSpeed` (OBSOLETE since B-170h, still filled and inconsistent).
- **Fixed in data:** the placeholder 3s. The proposal is one stat line per class, copied from the deckbuilder's class baselines where it has one. Crown tunes these in play.
- **The class kit is not a field here.** The class picks it (`DA_Loadout_ClassDefaults`, found by class through `UTrialRunSubsystem::FindClassDefaults`). A per-character link would say the same thing twice.

### The starting asset (`UInventoryData`, kept, cut down)

| Field | New or kept | What it holds |
|---|---|---|
| `StartingGear` | **new**, type `FClassDefaultLoadout` (the kit's own struct, reused) | The starting worn gear as a list of pieces: each `FClassDefaultPiece` has `Piece`, `OffHand` and `SkillPicks`, plus `ActiveIndex` and `Apparel`. It is minted into the pool once, for a new game or a new recruit, and worn like the kit (D-221 (1)). |
| `StartingEvolution` | **new**, `UEvolutionItemData*` | The character's own evolution (it becomes `FWornGear::EvolutionID`). |
| `InnateSpells`, `BDSpellPools` | **moved here** from `FSavedLoadout` | A Refractor's innate spells and Broken Darkness's pools: the character's own, not a loadout's. |
| `CrystalStock` | kept | The starting crystal stock, `{crystal kind, tier}: count`. This is where the test crystals are authored (§2). |
| `Spells`, `Abilities` | kept | The learned skills the Equipment screen picks from. |
| `EvolutionEquipment` | kept | Owned evolutions besides the worn one. |
| `Weapons`, `Rings` | kept, meaning narrowed | Owned spare pieces that are **not** worn. A piece in `StartingGear` is not listed again here. |

**Gone from `UInventoryData`:** `SavedLoadouts`, which takes the whole authored `FSavedLoadout` with it, and `DefaultActiveLoadoutIndex`; also the deprecated `ItemCrystals` and `RefinedCrystals` (already folded into `CrystalStock` on load).

**What goes with the authored `FSavedLoadout`** (the checklist's "Inventories and loadouts" lines):
- `bShowPrimary`.
- `PrimarySlotType`, `PrimaryWeapon`, `PrimaryWeaponInstance`, `PrimaryOffHandWeapon`, `PrimaryOffHandInstance`, `PrimaryRing`, `PrimaryRingInstance`, `PrimaryEvolution`, `PrimaryEvolutionInstance`, `PrimaryWeaponAbilities`.
- `SecondarySlotType`, `SecondaryWeapon`, `SecondaryWeaponInstance`, `SecondaryOffHandWeapon`, `SecondaryOffHandInstance`, `SecondaryWeaponAbilities`.
- `EquippedRings`, with its zero-GUID `RingInstance`s.
- `EquippedItems`, with its quantity-0 entries.
- `bAutoEquipItemsOnCombatStart`, `EvolutionSpells`, `LoadoutName`, `RequiredClass`.

The struct itself stays only as the save and preset shape, until restructure stage 3 replaces it.

### Gear (`UEquipmentDataBase`, `UWeaponData`, `URingData`)

**Gone from every piece:**
- `AttachedItem`: the baked crystal. A piece always starts with empty sockets; crystals are put in. This also clears the leftover "Garnet / F with kind None" defaults.
- `DefaultSpells` on weapons: a weapon's skills are its `WeaponAttack` and the `PresetAbilities` it allows.
- `DefaultAbilities`: already OBSOLETE.

**Rings keep `PresetSpells`:** the spells an adapter offers to pick from. Today these come from `DefaultSpells`, copied at `FRingInventoryEntry.cpp:37`, so one of the two fields goes. **Proposal:** keep `PresetSpells` and delete `DefaultSpells` everywhere.

**Every weapon must have:**
- a valid `WeaponType` (the Gauntlets asset has `EWeaponType_MAX` today);
- a `WeaponAttack`;
- a `WeaponStance` that matches its type (the dual axe has the sword stance today);
- `Handling` and a `DualWieldStance` when it can pair;
- a mesh (`WeaponSkeletalMesh` or `WeaponStaticMesh`) that is **in git** (the dual axe points at `/Game/Imports/...`, which is out of git);
- a name with no typo.

### The shield as its own piece (the smallest correct change)

**Today:** there is no shield item. `EWeaponWieldMode::OffHandShield` on the sword asset bakes the shield mesh into the sword (`LeftHandSkeletalMesh`), and `EWeaponType` has no Shield.

**Change:**
1. Add **`EWeaponType::Shield`**, appended last so the saved values of the other types don't move. A shield is **off-hand only**: `Handling` is `SingleOnly`, it has no `WeaponAttack`, and its mesh is its own `WeaponSkeletalMesh`.
2. The weapon-set rule (`WeaponSetRules.cpp`, D-209 pairs) accepts a Shield as the `OffHand` of a one-handed main hand, alongside today's same-type pairs. The pair attacks with the main hand's attack and stance.
3. The held type comes from the two pieces: a main hand with a Shield off hand reads "Sword and Shield" (`WoRGameNames::GetHeldWeaponTypeName` gets the off hand's type). The joined name, "Flame Sword and Void Shield", already comes from the pieces (B-513b).
4. The weapon mesh component shows the off-hand piece's own mesh in the left hand. **To check:** a D-209 pair may already do this, since `WeaponMeshComponent.cpp:102` picks the socket from `WieldMode`.
5. `EWeaponWieldMode::OffHandShield` and the left-hand mesh fields are flagged OBSOLETE, and deleted once no asset uses them. The ability gate (`UAbilityData::RequiredWieldMode`) then reads "a pair with a shield" from the pieces.

## 2. The asset list (for Crown's OK)

All new assets go under `Content/Data/`, named per ContentNaming. `Content/Testing/` holds dev-only data, but this cast is the game's working cast, so it stays under `Data/`.

### Create: the cast (5 players, 2 enemies)

| Asset | Path | Class / innate | Starting gear (`StartingGear`) |
|---|---|---|---|
| `DA_Character_Generic` + `DA_Inventory_Generic_Start` | `Data/Characters/Generic/` | Generic | The sword with the shield as its off hand; the two axes (a pair); one ring. Active: the sword. |
| `DA_Character_Refractor` + `DA_Inventory_Refractor_Start` | `Data/Characters/Refractor/` | Caster, Fire innate | One ring; innate Fire spells (`DA_Spells_Fire_*`, the fullest set) |
| `DA_Character_Resonator` + `DA_Inventory_Resonator_Start` | `Data/Characters/Resonator/` | Resonator | The sword; three rings |
| `DA_Character_BrokenDarkness` + `DA_Inventory_BrokenDarkness_Start` | `Data/Characters/BrokenDarkness/` | Caster, Darkness, BD flag on | One ring; the Darkness pools |
| `DA_Character_Reality` + `DA_Inventory_Reality_Start` | `Data/Characters/Reality/` | Caster, Reality innate | One ring |
| `DA_Enemy_Generic` + `DA_Inventory_Enemy_Generic` | `Data/Enemy/Generic/` | Generic enemy | The two axes (its own inventory; no longer shared with the player) |
| `DA_Enemy_Darkness` + `DA_Inventory_Enemy_Darkness` | `Data/Enemy/Darkness/` | Darkness enemy | One ring (its own inventory; was `DA_Test_Inventory`) |

The played test character becomes `DA_Character_Generic` (D-221 Q5).

### Create: the clean gear set (Crown: 2 weapon types as 4 pieces, 5 rings, 2 evolutions)

| Asset | Type | Attack / stance | Mesh (exists, in git) | Name | Tier |
|---|---|---|---|---|---|
| `DA_Weapon_Sword` | Sword, `SingleOnly` | `DA_Sword_Attack_LateralCut` / `DA_Sword_Stance_Basic` | `SK_Sword_1H_Fantasy_01` (StylizedWeapons) | "Arming Sword" | F |
| `DA_Weapon_Shield` | **Shield** (new), off hand only | none (the sword attacks) | `SK_Shield_Fantasy_01` (StylizedWeapons) | "Round Shield" | F |
| `DA_Weapon_Axe_Hand` | Axe, `SingleOrDual` | `DA_Axe_Attack_OverheadChop` / dual stance `DA_Stance_DualSword_Basic` until an axe stance exists | an axe mesh from StylizedWeapons (**editor worker picks; the current one is in `Imports/`, out of git**) | "Hand Axe" | F |
| `DA_Weapon_Axe_Bearded` | Axe, `SingleOrDual` | as above | a second axe mesh, so the pair reads as two pieces | "Bearded Axe" | F |
| `DA_Ring_Adapter_01` to `_05` | Ring form, **empty** | `PresetSpells`: see below | `RingStaticMesh`: one ring mesh | see below | F |
| `DA_Evo_Fire_Phoenix` | Garnet (Fire), Balanced | — | — | "Phoenix" (Crown's example) | E |
| `DA_Evo_Wind_Gale` | Emerald (Wind), Balanced | — | — | "Gale" | E |

The two axes are two assets, not one asset owned twice, so the pair's joined name reads "Hand Axe and Bearded Axe" and not "Axe and Axe". The held type reads "Dual Axes".

**What makes the five rings differ (proposal; Crown didn't say):** each ring adds **one sub-stat** through `BaseStatBonus`, and all five are F tier:

| Ring | Bonus |
|---|---|
| `_01` | Spell Damage |
| `_02` | Efficiency |
| `_03` | Spell Speed |
| `_04` | Status Multiplier |
| `_05` | Crit Damage |

The reference is Diablo's and Path of Exile's rings: an empty base that differs by one stat line, with the socket deciding the power. Their `PresetSpells` are the generic pool spells (`DA_Spell_Missile`, `Storm`, `Wave`), so a ring with any crystal has spells to pick.

**Naming catch (D-223):** the shown name comes from the socket ("Fire Ring"; empty "Adapter Ring"), so the five rings look the same by name. They differ on the detail's stat line and tier only. **Question for Crown:** add the ring's own word ("Fire Ring of Focus"), or keep the stat line as the difference?

**What the two evolutions give:**
- Each one's element comes from its crystal kind: Phoenix is Fire, Gale is Wind.
- Today all 27 evolution assets carry the same placeholder field bonuses (sprint +15%, field-skill EP −25%), so a real difference is a design job.
- **Proposal:** Phoenix takes the field-skill EP −25%, and Gale the sprint +15%, so the two read differently in the field.
- In battle each gives its element as a source, with the evolution spells picked on the loadout (`FWornGear::EvolutionSpells`).
- The names "Phoenix" and "Gale" are lore-adjacent: Crown may rename them.

### Create: the crystal stock

Crystals are item kinds, not assets. Each player character's `CrystalStock` holds the following, all at F tier:

| Crystal | Element | Count |
|---|---|---|
| Garnet | Fire | 2 |
| Sapphire | Water | 2 |
| Citrine | Lightning | 2 |
| Amber | Earth | 2 |
| Emerald | Wind | 2 |

That is 10: enough for the five rings and the weapon sockets. More come from `wor.Economy.GiveCrystal <Type> <Tier> [Count]`, and `wor.Debug.TestGear` stays as the quick test.

### Change: the class kits (`DA_Loadout_ClassDefaults`)

| Class | Pieces |
|---|---|
| Generic | `DA_Weapon_Sword` (with `DA_Weapon_Shield` as its off hand), `DA_Ring_Adapter_01` |
| Caster | `DA_Ring_Adapter_01` |
| Resonator | `DA_Weapon_Sword`, `DA_Ring_Adapter_01` to `_03` |

All pieces are F tier with empty sockets.

### Delete (after the new ones are in and the reference check is clean)

**Characters:**
- `DA_Character_Template`, `DA_Character_GenericLord`, `DA_Character_ResonatorLord`, `DA_Character_BrokenDarknessLord`, `DA_Character_DarknessLord`;
- `DA_Character_EarthLord`, `FireLord`, `LightLord`, `LightningLord`, `RealityLord`, `VoidLord`, `WindLord`, `WaterLord`;
- `DA_Enemy_Generic`, `DA_Enemy_Darkness`, `DA_Enemy_Resonator` (the old ones; the two kept enemies are remade).

**Inventories:** `DA_Inventory_Default`, `DA_Test_Inventory`, `DA_Inventory_Generic`, `_Resonator`, `_BD`, `_Darkness`, `_Earth`, `_Fire`, `_Light`, `_Lightning`, `_Reality`, `_Void`, `_Wind`, `_Water`.

**The loadout equipment assets:**
- `DA_Equipment_Generic_SAS`, `DA_Equipment_Generic_DualAxe`;
- `DA_Resonator_Equpment_Sword`, `DA_Equpment_Resonator_Ring_Light`, `_Void`, `_Wind`;
- `DA_BD_Equpment_Eearth`, `DA_Equpment_Darkness_Ecrystal_Fire`, `DA_Equipment_Elightning`, `DA_Equpment_Darkness_ERing_Fire`, `DA_Equipment_Fire_Ring_Reality`.

**Broken:** `/Game/Data/Testing/RefinedCrystals/DA_Test_EvoCrystal_Water`.

**Not deleted yet: the pool's 35 weapons, 42 rings and 27 evolutions.** Merchants, loot and rest spots may reference them, so they get their own clean-up job with a reference check. Deleting `AttachedItem` from the code empties their baked crystals anyway: they become empty rings whose shown name comes from the socket.

## 3. The code side

**Where the old layout is read today:**

- **`InventoryComponent.cpp`**:
  - `InitializeFromInventoryAsset` (`:2687`): adds `Weapons`, `Rings`, `CrystalStock` and `EvolutionEquipment`, then turns `SavedLoadouts` into runtime loadouts through `CreateFromSavedLoadout` (`:2846`).
  - `InitializeFromPool` (`:2967`): still inflates the authored `SavedLoadouts`.
  - `DrawSharedGear` (`:1634-1643`): **the worn-gear seed.** With no record in the pool, it runs `SavedLoadouts[Active].ToWornGear(Class)`.
  - `MakeRunPreset` (`:1467`): builds a run member's gear from that inflated loadout.
- **`FCombatLoadout.cpp`**:
  - `CreateFromSavedLoadout` (`:414-670`): the old-to-new converter.
  - `ForEachReadLegacySlot` (`:680`).
- **`WornGear.cpp`**: `CreateFromWornGear` (`:108-210`) goes back through a temporary `FSavedLoadout`.
- **Copying the baked crystal and spells from the asset:**
  - `FWeaponInventoryEntry.cpp:25/28`, `FRingInventoryEntry.cpp:34/37`;
  - used by `AddWeapon` / `AddRing`, `PoolSubsystem` (`:1094/1106`), `SaveSubsystem` (`:1639/1649`), `VendorScreenModel` (`:184/192`), `BalanceSimRunner` (`:370`).
- **`EconomyService.cpp:988-1005`:** prices read `AttachedItem`, `DefaultSpells` and `PresetAbilities`.
- **`CharacterFormData.cpp:79-102`:** reads the asset's `Weapons` and `Rings`.
- **`LoadoutComponent.cpp:1834` `AutoPopulateLoadout`:** debug only.

**The change (reads the new shape only, no converter):**
1. **Seed from `StartingGear`.** `DrawSharedGear`'s seed mints `UInventoryData::StartingGear` into the pool and wears it. It reuses `PoolClassDefaults.cpp`'s `MintKitPiece` and `WearClassDefaultKit`, generalised from "the class kit" to "a `FClassDefaultLoadout`". `InitializeFromInventoryAsset` stops reading `SavedLoadouts`. `InnateSpells`, `BDSpellPools` and the evolution come from the asset's new fields.
2. **Pieces start empty.** `CreateFromWeapon` and `CreateFromRing` stop copying `AttachedItem` and `DefaultSpells`, and `EconomyService` drops those price terms. The class kit already clears them (`PoolClassDefaults.cpp:71/86`).
3. **The shield** (§1): `EWeaponType::Shield`, the set rule, the held name, the mesh.
4. **Delete the fields** from §1 once nothing reads them. Old `.uasset`s that still carry them load fine: the engine skips a property that no longer exists.
5. **Saves:** raise the minimum save version, so older saves are wiped with a warning (D-060).

**Not in this plan (restructure stage 2 and 4):**
- the runtime `FCombatLoadout` primary / secondary fields;
- the round trip in `CreateFromWornGear`;
- `ApplyDraft` writing the old runtime fields.

Those are the readers' job. This plan only stops the **authored** side.

**A gap on the Equipment screen (the add-an-adapter flow):**
- Adding an adapter copies the piece's own sockets (`LoadoutV2ScreenWidget.cpp:2398`), and while it is a new draft its socket slots can't be filled ("Comes with the piece… A Edit on it changes its sockets", `:2333`).
- With empty rings, Crown's "pick the empty ring, then the crystal" therefore takes **two visits**: add, then Edit.
- **Proposal:** a small job that lets the add window's socket slots list `BuildSocketChoices`, as the Edit window does.

## 4. A ring's tier (D-223): where it is read today, and a question for Crown

| Use | Where | Which tier |
|---|---|---|
| Spell slots | `FRingLoadoutEntry.cpp:32/56` (`SlotsForContainerTier`) | the owned piece's tier |
| Combat tier / spell power | `CombatTier.cpp:51` | the owned piece's tier |
| Sell, convert, duplicate gold | `VendorServices.cpp:121/183`, `InventoryVerbs.cpp:72`, `DuplicateGearWidget.cpp:169` | the owned piece's tier |
| Merchant buy price | `EconomyService.cpp:988/1001` | the asset's tier |
| Equipment screen detail | `LoadoutV2Model.cpp:312-314` | the **asset's** tier: a bug, since a levelled piece shows its base tier; this gets fixed either way |
| The trial mirror's price | not built yet | — |

**Question for Crown (Q-223a):** the average of the ring and its crystal, rounded down, is the shown tier. Does it also drive the mechanics?

- **(a)** Display only: the tier letter shown everywhere.
- **(b)** Display and combat (spell slots, spell power).
- **(c)** Everything, the prices and the mirror too.

**Recommendation: (b).** The tier shown should be the tier that plays, so what the player reads is what they get. Monster Hunter's decorations and Path of Exile's gems both change what a piece does, and players read the piece as a whole. Prices stay on the ring's own tier, so swapping a crystal can't be used to farm gold.

Not built until this plan is approved.

## 5. The order (each step ≤3 files)

1. **Code: the shield.**
   - `EWeaponType.h`, `WeaponSetRules.cpp`, `WoRGameNames.cpp`, then a spec.
   - Then `WeaponMeshComponent.cpp` if it's needed.
2. **Code: the new fields.**
   - `UInventoryData::StartingGear`, `StartingEvolution`, `InnateSpells`, `BDSpellPools` (`InventoryData.h`).
   - Generalise the kit minting (`PoolClassDefaults.cpp`, `PoolSubsystem.h`).
3. **Code: seed from `StartingGear`.** `InventoryComponent.cpp` stops reading `SavedLoadouts` and the asset's loadouts, then a spec.
4. **Code: pieces start empty.**
   - `FWeaponInventoryEntry.cpp`, `FRingInventoryEntry.cpp`, `EconomyService.cpp`.
   - Then the spec fixes that this breaks (fixtures that relied on a baked crystal give one instead).
5. **Code: delete the old authored fields.**
   - `InventoryData.h`, `EquipmentDataBase.h`, `CharacterData.h`, with their remaining readers.
   - Raise the minimum save version.
6. **Code (small): sockets in the add window** (§3 gap).
7. **Editor worker:**
   - Make §2's assets.
   - Point `DefaultGame.ini` and the hub's played character at `DA_Character_Generic`.
   - Fill `DA_Loadout_ClassDefaults`.
   - Run the reference check, then delete the §2 delete list.
8. **PIE sheet:**
   - A new game: each character wears its starting gear, and the rings are empty.
   - The crystal stock is in the pouch, and a crystal goes into a ring, which then reads "Fire Ring".
   - The sword and shield read "Sword and Shield", and the axes "Dual Axes".
   - The trial starts on the kit.

Each code step builds, passes the suite and goes through `wor-reviewer` before the next. The editor step comes after step 5, so the new assets are authored against the final fields.

## Questions for Crown

- **The cast and gear list (§2):** OK as listed? Includes the item names "Arming Sword", "Round Shield", "Hand Axe", "Bearded Axe", "Phoenix" and "Gale".
- **The five rings:** they differ by one sub-stat each. Keep that, and keep the shown name socket-only, or add the ring's own word?
- **Q-223a, a ring's tier:** display only, display and combat (recommended), or everything?
