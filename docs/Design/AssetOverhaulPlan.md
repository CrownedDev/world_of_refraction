# Asset overhaul plan (D-223, D-224)

**Status:** plan; stages 1 to 4 and the skills track S1 to S3 are built (B-515 to B-526, in dev). Stage 5 needs no code (checked 2026-11-16, §6). S4 is folded into the trial pool stage (§7). **Crown approves the cast and gear list (§2) before the asset stages are built.**

**Revision 6 (2026-11-15) takes in Crown's last answers (D-224, "More answers" and "The Broken Darkness pool"). Every question is now answered, apart from tuning and the three follow-ons on found skills (questions 10 to 12):**
- **Q8:** a sword with a shield counts as **one weapon** for the D-211 slots: the shield adds nothing to the slot average. It still adds its stats. This is a one-line rule change, its own small job right after stage 2 (§1, "The shield as its own piece").
- **Q9:** the starting bolt is **in the inventory**: the player picks it into a slot. Nothing is pre-attached, and the kit's sockets start empty (§5.3).
- **The class kits, first shape** (tuned later): a Refractor starts with nothing (it has its innate), a Generic with one weapon, a Resonator with two rings (§2).
- **Q6:** innate spells stay. "No skills at the start" means no learned or bought ones.
- **(b) yes:** a character keeps the skills found in a run, "similar to their other equipment", under the found-gear rules of D-219 and D-221 (§5.3).
- **Broken Darkness's pools are capacity, not content:** empty places to learn spells, never pre-filled. The authored field goes, and the pool is a rule in code that the screens draw as empty places (§5.4).

**Revision 6 (2026-11-16) takes in Crown's last answers (D-223, D-224):**
- **Only the elemental side averages (§4.1):** the weapon-and-crystal average drives the elemental (infused) strength and the elemental status build-up. Physical attacks and physical build-up stay on the weapon's own tier. Revision 5 moved the whole channel; this splits it.
- **Found skills follow the equipment rules (§5.3, questions 10 to 12):** only slotted skills are kept, the rest go back; on a lost run they follow D-219; a kept skill is for the hub and its presets, not the next run's start.
- **Two same-named pieces:** the tier tells them apart first, a number only when the tier matches too (D-223; built by B-516b; question 2).

**Revision 5 (2026-11-15) takes in Crown's answers on revision 4 (D-224):**
- **Q3:** the averaged tier does not lower the ring's own stat lines; they stay on the ring's own tier. The average drives how strong the skills cast through the ring are (§4).
- **Q4:** weapons average with their crystal too, and it affects the status build-up (§4.1).
- **Q7:** conjured (locked) rings become ordinary empty rings; found rings may come prebuilt, with a crystal or an evolution already in the socket (§3, "Found pieces").
- **Stage 2 is built** (B-516): the empty adapter, socketing in the add window, two same-named rings.

**Revision 4 (2026-11-15) takes in D-224:**
- **A ring's tier is the built average** of the ring and its crystal: a rules change, with its own stage (§4).
- **No built-in skills on any gear.** Weapons, adapters and evolutions carry no skill lists; skills sit in what is worn and in the preset (§5.1).
- **A skill states what it needs** (a weapon type, an adapter, an evolution, an element) and works when the worn gear meets it (§5.2).
- **A run starts with no skills**, apart from the kit's starting skill (one generic bolt for now). More are bought or won from enemies (§5.3).
- **Still open:** (b) whether a character keeps a permanent list of known skills in the hub.

**Inputs:**
- D-220 to D-224.
- `Audits/CharacterAssetDump.md`: its "Anything that looks odd" list is the checklist.
- `Design/LoadoutRestructurePlan.md`: this plan brings its stage 4 forward.
- `Conventions/ContentNaming.md`.

## In plain words

The characters and their gear are still authored the old way:
- two fixed slots (primary and secondary);
- crystals baked into weapons and rings;
- skill lists on weapons and rings;
- a shield that is part of the sword.

We make **new, small, clean assets** in the new shape:
- Each character's starting gear is a plain list of pieces, and each piece says which crystal sits in its socket.
- Gear carries stats and effects but **no skills**. Skills are chosen into what is worn, and each skill says what gear it needs.

Then we delete the old assets. The code learns to read only the new shape; nothing converts the old one. It is built in **small stages**, and each ends in a short PIE sheet Crown checks before the next starts (§6).

## 1. The new authored shape

### The character (`UCharacterData`, kept)

- **Kept:**
  - the name, class, innate element and the Broken Darkness flag;
  - the portrait, origin, cosmetics, resistance profile and sub-stats;
  - `Inventory`, its link to the starting asset below.
- **Gone:** `TurnSpeed` (OBSOLETE since B-170h, still filled and inconsistent).
- **Fixed in data:** the placeholder 3s. Each class gets one stat line, copied from the deckbuilder's class baselines where it has one. Crown tunes them in play.
- **The class kit is not a field here.** The class picks it (`DA_Loadout_ClassDefaults`, found by class through `UTrialRunSubsystem::FindClassDefaults`).

### The starting asset (`UInventoryData`, kept, cut down)

| Field | New or kept | What it holds |
|---|---|---|
| `StartingGear` | **new**, type `FClassDefaultLoadout` (the kit's own struct, reused) | The starting worn gear as a list of pieces. Each `FClassDefaultPiece` has `Piece`, `OffHand`, `SkillPicks` (D-224: **the worn entry's skills**, not the piece's) and the new `Crystal`, plus `ActiveIndex` and `Apparel`. It is minted into the pool once (a new game or a new recruit) and worn like the kit (D-221 (1)). |
| `StartingEvolution` | **new**, `UEvolutionItemData*` | The character's own evolution (it becomes `FWornGear::EvolutionID`). |
| `InnateSpells` | **moved here** from `FSavedLoadout` | The character's own spells (§5.1). |
| ~~`BDSpellPools`~~ | **not moved: deleted** (stage 6) | Broken Darkness's pools are empty places the player fills, a rule in code (§5.4), so nothing is authored. |
| `CrystalStock` | kept | The starting crystal stock, `{crystal kind, tier}: count` (§2). |
| `Spells`, `Abilities` | kept until (b) is answered | The character's known skills in the hub (§5.3). |
| `EvolutionEquipment` | kept | Owned evolutions besides the worn one. |
| `Weapons`, `Rings` | kept, meaning narrowed | Owned **spare** pieces; a piece in `StartingGear` is not listed again. |

**New on `FClassDefaultPiece`: `Crystal`** (`FCrystalId`, the same key `CrystalStock` uses; empty means an empty socket).
- It is the crystal minted into the piece's socket. This is how the clean set starts with "Fire Ring", "Water Ring" and so on, without baking a crystal into the ring asset.
- The class kit can use it too, or leave it empty.

**New on `FClassDefaultLoadout`: `StartingSkills`** (`TArray<TObjectPtr<USkillDataBase>>`).
- These are the skills a run starts with (D-224 (2), (a)).
- The default is one generic bolt per class kit, marked `// TODO: tune the starting skills (D-224)` (§5.3).

**Gone from `UInventoryData`:**
- `SavedLoadouts`, which takes the authored `FSavedLoadout` with it;
- `DefaultActiveLoadoutIndex`;
- the deprecated `ItemCrystals` and `RefinedCrystals`, already folded into `CrystalStock` on load.

**The authored `FSavedLoadout` fields that go** (the checklist's "Inventories and loadouts" lines):
- `bShowPrimary`;
- `PrimarySlotType`, `PrimaryWeapon`, `PrimaryWeaponInstance`, `PrimaryOffHandWeapon`, `PrimaryOffHandInstance`;
- `PrimaryRing`, `PrimaryRingInstance`, `PrimaryEvolution`, `PrimaryEvolutionInstance`, `PrimaryWeaponAbilities`;
- `SecondarySlotType`, `SecondaryWeapon`, `SecondaryWeaponInstance`, `SecondaryOffHandWeapon`, `SecondaryOffHandInstance`, `SecondaryWeaponAbilities`;
- `EquippedRings`, with its zero-GUID `RingInstance`s;
- `EquippedItems`, with its quantity-0 entries;
- `bAutoEquipItemsOnCombatStart`, `EvolutionSpells`, `LoadoutName`, `RequiredClass`.

The struct itself stays only as the save and preset shape, until restructure stage 3 replaces it.

### Gear (`UEquipmentDataBase`, `UWeaponData`, `URingData`, `UEvolutionItemData`)

**Gone from every piece** (D-223, D-224 "no built-in skills on any gear"):
- `AttachedItem`, the baked crystal. A piece's socket is filled by `StartingGear`'s `Crystal`, or in play.
- **Every skill list:**
  - a weapon's `PresetAbilities` and `bAbilitiesLocked`;
  - `DefaultSpells` (weapons and rings);
  - a ring's `PresetSpells` and `bSpellsLocked`;
  - `DefaultAbilities`, already OBSOLETE.
  - An evolution asset has no spell list today (`EvolutionItemData.h`), so it has nothing to remove.

**Kept: the weapon's `WeaponAttack`. It is not a skill.**
- It is the weapon's basic attack: a `UAbilityData` with `bIsAttack` (`WeaponData.h:57-60`, `AbilityData.h:71-72`).
- It is never learned, slotted or counted in Combat Tier (`FAbilityCollection.cpp:17-19`, `CombatTier.cpp:59-72`).
- The battle's Attack button plays it (`LoadoutComponent.cpp:2459-2479` `GetCurrentAttack`), and the field swing plays its `FieldMontage` (`FieldMovementAttack.cpp:98-113`). So **the basic Attack works unchanged with no weapon skills.**
- With no weapon there is no Attack button, and the field gives a plain swing, as today (`FCombatCapabilities.cpp:58`).
- **The rule for a pair (Crown, after B-515):** any held pair's basic attack is the **main hand's** `WeaponAttack`, whatever is in the off hand: a shield, a second sword or a second axe. **The code already does this everywhere:**
  - the battle's `GetCurrentAttack` (`LoadoutComponent.cpp:2459-2479`);
  - the attack list `GetAllWeaponAttacks` (`:1402-1446`);
  - the field swing (`FieldMovementAttack.cpp:106`).
  - Each one reads the entry's main-hand weapon, and an off hand's `WeaponAttack` is never read. The spec `WoR.Loadout.WeaponSets` "attacks with the main hand's own attack whatever is in the off hand" covers a dual pair and a shield pair (B-515b), so stage 3 (the axes) needs no attack job.
  - The worn entry's `OverrideAttack` (`FLoadoutSlot`), where set, still comes first, as today.

**Every weapon must have:**
- a valid `WeaponType` (the Gauntlets asset has `EWeaponType_MAX` today);
- a `WeaponAttack` (except a shield);
- a `WeaponStance` that matches its type;
- `Handling` and a `DualWieldStance` when it can pair;
- a mesh that is **in git**;
- a name with no typo.

### What an adapter (ring) can carry: its own stats and effects, like a weapon

An adapter is a full gear piece, and all of this **stays** in the new shape. Only the baked crystal and the spell lists go.

**From `UEquipmentDataBase`, shared with weapons:**
- `BaseStatBonus` (`FEquipmentStatBonus`): Raw Damage, Spell Damage, Efficiency, Status Multiplier, Crit Damage, Spell Speed, Defense, Action Speed, Reflex, Max HP and the rest.
  - It is scaled by a tier (today the ring's own, `LoadoutComponent.cpp:3931-3988`; with D-224 the adapter tier, §4).
  - Rolled pieces add `GeneratedStatBonus` (`bRandomGenerateOnPickup`).
- `BaseResistance` and `GeneratedResistance`.
- `ReferencedEffects`: the effects tree. The Equipment screen shows it (`AddEffects`, `LoadoutV2Model.cpp:213`).
- The bound data: `BoundOwner`, `BoundEffects`, `BoundMemories`, `BoundQuests`, `Memory`, `Conditions`.
- `Requirements`.
- The infusion settings: `bImmuneToInfusion`, `InfusionStatusMultiplier`.
- `bLockSkills`, `SourceTags`, `Tier`, `Name` and `Description`.

**From `URingData` itself:**
- `Form` (Ring, Pistol, Trainer), `FormMesh` and `FormCastMontage`;
- `InfusionMode`;
- the ring mesh and its rotation.

### The shield as its own piece (built: B-515)

`EWeaponType::Shield` is appended last.
- A shield is held only in the off hand of a **pairable** (one-handed) weapon of any type, and never alone or in the main hand.
- The pair reads "Sword and Shield" and `OffHandShield`, and stands in the main hand's own stance.
- The left hand shows the shield piece's own mesh. A D-209 pair now shows its second piece's mesh too.
- A shield asset is `DualOnly` with no attack, and the sword is `SingleOrDual`.
- `EWeaponWieldMode::OffHandShield` on a sword (the baked-in shield) and the left-hand mesh fields are OBSOLETE, deleted in stage 6.

**A sword and shield count as one weapon for the slots (Crown, Q8: "as if it was one weapon, i.e. like a sword").**
- **Today:** `WeaponSetRules::GetSetTierSlots` (`WeaponSetRules.cpp:84-87`) averages the two hands' slots for any valid pair (D-211), a shield included. An F shield lowers an S sword's slots.
- **The change:** a shield pair (`IsShieldPair`) takes the main hand's own slots, as a sword alone does. A D-209 pair of two weapons keeps the average.
- **Its stats stay:** the off hand's stat bonus (`LoadoutComponent.cpp:4015`, scaled by its own tier) and its resistances (`:4107`) still count. A shield is worn for its defence. This is the plan's recommendation, and Crown's answer only changed the slots.
- **The job:** one rule line plus a `WoR.Loadout.WeaponSets` case (an S sword with an F shield keeps the S slots; two axes still average). It is its own small job right after stage 2 (B-515c), as it touches neither the empty adapter nor the tier work.

## 2. The asset list (for Crown's OK)

All new assets go under `Content/Data/`, named per ContentNaming.

### Create: the cast (5 players, 2 enemies)

| Asset | Path | Class / innate | Starting gear (`StartingGear`; F tier, crystals at F) | Evolution |
|---|---|---|---|---|
| `DA_Character_Generic` + `DA_Inventory_Generic_Start` | `Data/Characters/Generic/` | Generic | Sword with the shield as its off hand (active). The two axes as a pair. **Earth Ring** (adapter + Amber). | Gale Force |
| `DA_Character_Refractor` + `DA_Inventory_Refractor_Start` | `Data/Characters/Refractor/` | Caster, Fire innate | **Fire Ring** (adapter + Garnet). Innate Fire spells (`DA_Spells_Fire_*`). | Phoenix Fire |
| `DA_Character_Resonator` + `DA_Inventory_Resonator_Start` | `Data/Characters/Resonator/` | Resonator | Sword. **Water Ring** (Sapphire), **Lightning Ring** (Citrine), **Wind Ring** (Emerald). | — |
| `DA_Character_BrokenDarkness` + `DA_Inventory_BrokenDarkness_Start` | `Data/Characters/BrokenDarkness/` | Caster, Darkness, BD flag on | Fire Ring. The Darkness pools. | — |
| `DA_Character_Reality` + `DA_Inventory_Reality_Start` | `Data/Characters/Reality/` | Caster, Reality innate | Wind Ring | — |
| `DA_Enemy_Generic` + `DA_Inventory_Enemy_Generic` | `Data/Enemy/Generic/` | Generic enemy | The two axes (its own inventory) | — |
| `DA_Enemy_Darkness` + `DA_Inventory_Enemy_Darkness` | `Data/Enemy/Darkness/` | Darkness enemy | Earth Ring (its own inventory; was `DA_Test_Inventory`) | — |

**The five test rings** are the Generic's Earth Ring, the Refractor's Fire Ring and the Resonator's Water, Lightning and Wind Rings: one of each element in the stock. BD, Reality and the Darkness enemy hold further copies of the same adapter.

The played test character becomes `DA_Character_Generic` (D-221 Q5).

### Create: the clean gear set

**Crown's set:** two weapon types as four pieces, five rings, two evolutions.

| Asset | Type | Attack / stance | Mesh (in git) | Name | Tier |
|---|---|---|---|---|---|
| `DA_Weapon_Sword` | Sword, **`SingleOrDual`** (pairable, so it takes the shield) | `DA_Sword_Attack_LateralCut` / `DA_Sword_Stance_Basic` | `SK_Sword_1H_Fantasy_01` (StylizedWeapons) | "Arming Sword" | F |
| `DA_Weapon_Shield` | **Shield**, `DualOnly` | none (the sword attacks) | `SK_Shield_Fantasy_01` (StylizedWeapons), `LeftMeshRotation` set | "Round Shield" | F |
| `DA_Weapon_Axe_Hand` | Axe, `SingleOrDual` | `DA_Axe_Attack_OverheadChop` / dual stance `DA_Stance_DualSword_Basic` until an axe stance exists | an axe mesh from StylizedWeapons (the editor worker picks; the current one is in `Imports/`, out of git) | "Hand Axe" | F |
| `DA_Weapon_Axe_Bearded` | Axe, `SingleOrDual` | as above | a second axe mesh | "Bearded Axe" | F |
| `DA_Ring_Adapter` | Ring form, **empty**, no spells | — | one ring mesh | (shown by socket) | F |
| `DA_Evo_Fire_Phoenix` | Garnet (Fire), Balanced | — | — | "Phoenix Fire" (in a ring: "Phoenix Fire Ring") | E |
| `DA_Evo_Wind_Gale` | Emerald (Wind), Balanced | — | — | "Gale Force" (in a ring: "Gale Force Ring") | E |

No piece has a skill list (D-224).

**One adapter asset, five pieces.**
- The five rings are told apart by their crystal's element (Crown), so they are five owned pieces of **one** empty adapter, each with a different crystal in `StartingGear`'s `Crystal`.
- They read "Fire Ring", "Water Ring", "Lightning Ring", "Earth Ring" and "Wind Ring" (D-223, B-514b). The empty adapter reads "Adapter Ring". No ring has an element in its own name.

**Every ring is its own piece (Crown: "each one is unique, i.e. we can have 2 Lightning adapters").**
- Each owned ring is a separate instance with its own `PersistentID`.
- Two rings may carry the same element, and both must work as separate entries everywhere.
- They are keyed by the piece id, or by their place in the list; never by element, name or asset.

Today:

| Where | Keyed by | Two Lightning rings |
|---|---|---|
| Worn gear | `FWornGear::Equipment[].GearID`, the piece id (`WornGear.h`, `GetPieceIDs`) | separate |
| Saved presets | `FSavedCustomLoadout::AdapterInstances`, piece ids matched index for index with `Adapters` (`WoRSaveTypes.h:624`) | separate |
| Equipment list | `FLoadoutV2Entry::AdapterIndex`, the place in the ring loadout (`LoadoutV2Model.cpp:61`) | separate |
| Quick strip | the place in the list (`FWheelPick::FieldAttackIndex`, Q-503 (3)) | separate |
| Gear mirroring | by `PersistentID`; a second copy of the same asset only raises the duplicate notice (`InventoryComponent.cpp:2291`) | separate |
| **Crystal wear: which ring a cast wears** | **the ring asset** (`LoadoutComponent.cpp:3034-3048`). B-310 picks the adapter in use first; any other match falls to the first copy in the list. | **to check in stage 2**; if wear lands on the wrong copy, key it by piece id |
| "Owns this ring" checks | the asset (`InventoryComponent.cpp:1736-1739`) | fine: these only ask "has one" |

**Telling two same-named rings apart (default, still open with Crown):** a small number, shown only where two entries in the same list would read the same: "Lightning Ring", then "Lightning Ring 2".
- It is worked out at display time from the list's order, and nothing is stored.
- One helper in `WoRGameNames` does it, used by the quick strip and the Equipment list (stage 2).
- Diablo's and Path of Exile's lists show identical names and let the slot tell them apart. A number is the shortest form of that on a strip with no slot labels.

**The adapter's stat line (shared by all five):**
- One `BaseStatBonus` line, `BonusSpellDamage`, at the value the pool's existing F rings use. The editor worker reads it off one of them; no new number.
- Effects (`ReferencedEffects`) stay empty in the clean set.

**The two axes are two assets**, so the pair reads "Hand Axe and Bearded Axe" (not "Axe and Axe"), and the held type reads "Dual Axes".

**What the two evolutions give.** Their element comes from their crystal kind: Phoenix Fire is Fire, Gale Force is Wind (names: Crown, D-223).
- **Proposal:** Phoenix Fire takes the field-skill EP −25% and Gale Force the sprint +15%, so the two read differently in the field. Today all 27 evolutions carry both placeholder bonuses.
- In battle, each gives its element as a source.
- The names are lore-adjacent, so Crown may rename them.

### Create: the crystal stock

Crystals are item kinds, not assets. Each player's `CrystalStock` holds the following, all at F tier:

| Crystal | Element | Count |
|---|---|---|
| Garnet | Fire | 2 |
| Sapphire | Water | 2 |
| Citrine | Lightning | 2 |
| Amber | Earth | 2 |
| Emerald | Wind | 2 |

These are spares on top of the crystals in the rings. More come from `wor.Economy.GiveCrystal <Type> <Tier> [Count]`.

### Change: the class kits (`DA_Loadout_ClassDefaults`)

**The first shape (Crown, 2026-11-15; `TODO` tune later):**

| Class | Pieces | `StartingSkills` (in the inventory, not picked) |
|---|---|---|
| Generic | one weapon: `DA_Weapon_Sword` | one generic bolt |
| Caster (Refractor) | **nothing** ("since they have innate") | none: its innate spells are its start |
| Resonator | two `DA_Ring_Adapter` | one generic bolt |

- Every kit is F tier, and its `Crystal` is left empty: the trial's kit starts empty (D-221), and its stock fills it.
- **The bolt is in the inventory (Crown, Q9):** `StartingSkills` go into the character's run skills, not into a slot. The player picks the bolt into a slot themself, and it can be cast once a crystal is in.
- **Today's kit asset** also gives the Generic the shield and a ring, and the Resonator a sword and three rings; the editor worker cuts it down to the first shape in stage 5.
- **The generic bolt:** no spell asset is referenced in code. The docs name `DA_Spell_Bolt` and `DA_Spell_Missile` (`Audits/CharacterAssetDump.md:22`). The editor worker uses whichever exists, or makes `DA_Spell_Bolt` as a Generic-element bolt (it takes the source's element).

### Delete (after the new ones are in and the reference check is clean)

**Characters:**
- `DA_Character_Template`, `DA_Character_GenericLord`, `DA_Character_ResonatorLord`, `DA_Character_BrokenDarknessLord`, `DA_Character_DarknessLord`;
- `DA_Character_EarthLord`, `FireLord`, `LightLord`, `LightningLord`, `RealityLord`, `VoidLord`, `WindLord`, `WaterLord`;
- the old `DA_Enemy_Generic`, `DA_Enemy_Darkness` and `DA_Enemy_Resonator` (the two kept enemies are remade).

**Inventories:** `DA_Inventory_Default`, `DA_Test_Inventory`, `DA_Inventory_Generic`, `_Resonator`, `_BD`, `_Darkness`, `_Earth`, `_Fire`, `_Light`, `_Lightning`, `_Reality`, `_Void`, `_Wind`, `_Water`.

**The loadout equipment:**
- `DA_Equipment_Generic_SAS`, `DA_Equipment_Generic_DualAxe`;
- `DA_Resonator_Equpment_Sword`;
- `DA_Equpment_Resonator_Ring_Light`, `_Void`, `_Wind`;
- `DA_BD_Equpment_Eearth`, `DA_Equpment_Darkness_Ecrystal_Fire`, `DA_Equipment_Elightning`, `DA_Equpment_Darkness_ERing_Fire`, `DA_Equipment_Fire_Ring_Reality`.

**Broken:** `/Game/Data/Testing/RefinedCrystals/DA_Test_EvoCrystal_Water`.

**Not deleted yet: the pool's 35 weapons, 42 rings and 27 evolutions.** Merchants, loot and rest spots may reference them, so they get their own clean-up job with a reference check. Deleting the baked crystal and the skill lists empties them anyway.

## 3. The code side (the authored layout)

**Where the old layout is read today:**
- **`InventoryComponent.cpp`:**
  - `InitializeFromInventoryAsset` (`:2687`) turns `SavedLoadouts` into runtime loadouts through `CreateFromSavedLoadout` (`:2846`).
  - `InitializeFromPool` (`:2967`) still inflates them.
  - `DrawSharedGear` (`:1634-1643`) is the worn-gear seed: with no record in the pool, it runs `SavedLoadouts[Active].ToWornGear(Class)`.
  - `MakeRunPreset` (`:1467`).
- **`FCombatLoadout.cpp`:** `CreateFromSavedLoadout` (`:414-670`, the old-to-new converter) and `ForEachReadLegacySlot` (`:680`).
- **`WornGear.cpp`:** `CreateFromWornGear` (`:108-210`) goes back through a temporary `FSavedLoadout`.
- **The baked crystal and spells copied from the asset:**
  - `FWeaponInventoryEntry.cpp:25/28` and `FRingInventoryEntry.cpp:34/37`;
  - used by `AddWeapon` / `AddRing`, `PoolSubsystem` (`:1094/1106`), `SaveSubsystem` (`:1639/1649`), `VendorScreenModel` (`:184/192`) and `BalanceSimRunner` (`:370`).
- **Prices:** `EconomyService.cpp:988-1005` reads `AttachedItem`, `DefaultSpells` and `PresetAbilities`.
- **`CharacterFormData.cpp:79-102`:** reads the asset's `Weapons` and `Rings`.

**The change (it reads the new shape only, with no converter):**
1. **Seed from `StartingGear`.**
   - `DrawSharedGear`'s seed mints `UInventoryData::StartingGear` into the pool and wears it, reusing `PoolClassDefaults.cpp`'s `MintKitPiece` and `WearClassDefaultKit`, generalised to "a `FClassDefaultLoadout`".
   - Each piece's `Crystal` is minted into its socket.
2. **Pieces start empty.** `CreateFromWeapon` and `CreateFromRing` stop copying `AttachedItem`, and `EconomyService` drops that price term.
3. **The shield:** built (B-515).
4. **Delete the fields** in §1 once nothing reads them. Old `.uasset`s that still carry them load fine: the engine skips a property that no longer exists.
5. **Saves:** raise the minimum save version, so older saves are wiped with a warning (D-060).

**Found pieces: a drop or a chest may come prebuilt (Crown, Q7).** "You can find prebuilt rings, i.e. built with an evolution crystal or a normal crystal." So a piece starts empty (step 2) unless the find says what is in its socket.
- **Today:** a drop picks an asset by its source tag (`TrialMapFeature.cpp:201`, `TrialLoot::PickFromPool`) and gives it with `TrialLoot::GrantTo` → `AddWeapon` / `AddRing`. That calls `CreateFromRing`, which copies the asset's baked `AttachedItem`, the shape step 2 removes. Chest gear is pool gear marked as in a chest (B-324), minted by the same factories.
- **The change:** a find names its piece and, optionally, its socket. It is the same shape as a kit piece's `FClassDefaultPiece::Crystal` (stage 4): a gem (`FCrystalId`) or an evolution asset.
  - **Minting** reuses the kit's path (`PoolClassDefaults.cpp`'s `MintKitPiece`): the empty piece with a new `PersistentID`, then its socket filled.
  - **A gem** goes in at full durability, as a socketed gem does.
  - **An evolution** is minted as its own instance in the finder's evolutions and attached (`AttachEvolutionToRing`), so it can be taken off and used elsewhere like any owned evolution.
  - **The loot list** (today a list of assets with source tags) gains a small entry for a prebuilt find: the piece, its source tags, and its gem or evolution. A plain find stays a plain asset and arrives empty.
- **A merchant's stock** stays empty (`VendorScreenModel.cpp:184/192`).
- **Where:** stage 3, beside "pieces start empty", so a find never arrives with an old baked crystal. The kit minting is generalised in stage 4; stage 3 takes the gem / evolution fill from it early, or stage 4 adds prebuilt finds. One small spec: a prebuilt Phoenix Ring drop arrives with its evolution in the socket and owned; a plain drop arrives empty.

**Not in this plan (restructure stages 2 and 4):** the runtime `FCombatLoadout` primary and secondary fields, the round trip in `CreateFromWornGear`, and `ApplyDraft` writing the old runtime fields.

**A gap on the Equipment screen (the add-an-adapter flow):** adding an adapter copies the piece's own sockets (`LoadoutV2ScreenWidget.cpp:2398`), and a new draft's socket slots can't be filled (`:2333`). **Fix (stage 2):** the add window's socket slots list `BuildSocketChoices`, as the Edit window does.

## 4. A ring's tier is the built average (D-224 (1))

**The rule.** An adapter's tier is the average of the ring's tier and its socket's tier, a half rounded down, as D-211 rounds a pair of weapons. The tiers are counted F0, E1, D2, C3, B4, A5, S6, so an F ring with an S gem is C, and a D ring with a B gem is C.
- **A gem crystal:** its own tier.
- **A fusion:** its gem half's tier. An augmented fusion with no gem half counts as empty.
- **An evolution:** its instance tier.
- **An empty socket**, or one with no spell source: the ring's own tier (Crown: "an empty one shows its own tier").
- The stone socket (D-141) doesn't count.

One helper works this out (`CrystalEffectTable::GetAdapterTier(const FRingInventoryEntry&)`), and every reader below uses it.

**Each mechanic that reads a tier today, and what it should read:**

| Mechanic | Today | With D-224 | What breaks or needs a rule |
|---|---|---|---|
| **Spell slots** | the crystal's tier, which replaces the ring's (`ResolveSpellSlotCap`, `FRingLoadoutEntry.cpp:30-32/54-56`; CL-25, Q-147) | **the adapter tier**: `SlotsForContainerTier(adapter tier)` | CL-25 and Q-147 change for rings: their rule rows and specs are updated. An empty ring shows its own tier's slots but still can't cast (no source), as today. |
| **The tier-gap output** (spell damage) | the crystal's tier as the channel; spell tier − channel (`ActionExecutor.cpp:429-500`; ED-11) | **the adapter tier** as the channel for a ring's spells | The ED-11 specs for rings change. An empty socket still means no channel (×1.00). A weapon crystal's spells stay on the crystal until the weapon question below. |
| **Crystal Amp** | the crystal's tier (`LoadoutComponent.cpp:3970`; ED-50d) | **keep the crystal's tier** | Recommended: it is the crystal's own bonus, and the ring already counts once through the channel; averaging it too would count the ring twice. |
| **Max durability** | the crystal's tier, F30 … S100 (ER-34) | **keep the crystal's tier** | Durability is the crystal's own state. It moves with the crystal, so a ring can't change it. |
| **Wear per cast** | 7% per tier gap, spell tier − crystal tier (`CrystalManager.cpp:105-121`) | **the same gap as the output**: spell tier − adapter tier | It is part of casting the skill (Q3: the average drives the skills), so one gap per cast keeps wear and damage reading the same. A better ring slows a weaker crystal's wear. |
| **The adapter's stat lines** | the ring's own instance tier (`GetTierPowerMultiplier` on `RingTier`, `LoadoutComponent.cpp:3991`) | **keep the ring's own tier** (Crown, Q3) | You always get the ring's stats; only the skills cast through it follow the average. |
| **Combat Tier** | the ring's tier (`CombatTier.cpp:51`; D-118, Q-325) | **the adapter tier** | It is the overall number, so it should match. Weapons' crystals still aren't counted (question 4). |
| **Dismantle** | the ring's own tier (`EconomyService.cpp:475`) | **keep the ring's tier** | You dismantle the ring piece; its crystal comes out as today. |
| **Buy / sell price** | the sum of the ring's and the attachment's prices (ER-22, `EconomyService.cpp:920-934`) | **keep the sum** | The sum already prices both parts; an average would under-price the pair. |
| **Repair** | the crystal's tier price (`GearRepair.cpp:35-45`) | **keep** | Repair fixes the crystal. |
| **Lost-run tier drop** | the ring drops 3 tiers, its crystal doesn't (ER-65g, D-219) | **keep** | The adapter tier falls with the ring through the average. |
| **A broken crystal** (D-083), **the S-crystal cap** (CL-13b) | the spell's tier; the crystal's tier | **keep** | — |
| **The Equipment detail** | the owned piece's tier (fixed in B-516; it read the asset's) | **the adapter tier**, with the parts in the detail line | — |

**What the logic does today, and what Q3 keeps (Crown: "you would still get the ring's stats, the skills you use just wouldn't be as powerful").**
- **Today the two halves already read different tiers.** The ring's stat lines read the ring's own tier. The skills cast through it read the crystal's tier alone: the spell slots, the channel (damage) and the wear gap.
- **With D-224** only the skill half moves to the average. The stat half stays on the ring's own tier, as it is today.
- So an S ring with an F gem keeps its S stats, and its skills read C (today they read F). An F ring with an S gem keeps its F stats, and its skills read C (today they read S). That is Crown's "the skills just wouldn't be as powerful" in the second case.

**The size of the change:**
- **Code:** one helper plus four ring readers (the stat-line multiplier is not one, Q3):
  - the ring slot cap;
  - the channel tier;
  - the wear gap;
  - Combat Tier.
- **Weapons:** their own readers (§4.1).
- **Equipment detail:** fixed as above.
- **Specs:** a new `WoR.Loadout.AdapterTier` spec; the CL-25, ED-11 and ER-34 ring specs are updated.
- **Rules pages:** the `docs/Rules/` rows change with it.
- **Commits:** about three, of at most three files each.
- **Stage:** it has its own stage (2b, rings and weapons) and its own PIE sheet.

### 4.1 A weapon's tier is the built average too (D-224, Q4)

**Crown:** weapons average with their crystal too, "and it would affect the status build-up". **Only the elemental side** (Crown, D-224): the average drives the elemental (infused) strength and the **elemental** status build-up; physical attacks and physical build-up stay on the weapon's own tier. The rule is the ring's: the weapon's own tier and its socket's tier, a half rounded down, counted from F0 to S6. An empty socket, or one with no spell source, gives the weapon's own tier. The stone socket doesn't count. The sockets are the main hand's (D-210), so a pair averages its main hand with that socket.

**Each weapon mechanic that reads a tier today, and what it should read:**

| Mechanic | Today | With D-224 | Note |
|---|---|---|---|
| **The channel for attacks and abilities** (the tier gap) | the weapon's own instance tier (`ResolveChannelTier`, `ActionExecutor.cpp:431-447`) | **two channels:** the physical one on the weapon's own tier, the elemental one on the average | Today there is one gap per action; it is split (below). |
| **Status build-up** of an ability or attack | one number, authored × the charge multiplier × own-tier power × **the tier gap** (`ActionExecutor.cpp:1633`), sent to both the physical bar and the element bar | **the physical bar** on the weapon's own gap, **the element bar** on the average's gap | Two numbers instead of one, from the same authored build-up. An uninfused action fills only the physical bar, as today. |
| **Damage** of an ability or attack | the same tier gap (ED-11), one number | **the physical part** on the weapon's own gap, **the elemental part** on the average's gap | An uninfused hit is all physical: nothing changes for it. |
| **The action's own tier fallback** (a skill that can't resolve) | the weapon's instance tier (`ActionExecutor.cpp:414-424`) | **the weapon tier** | Safety path only. |
| **Combat Tier** | the weapon entry's tier (`CombatTier.cpp:47`) | **the weapon tier** | Matches the rings' row. |
| **Weapon power** (base damage) | `Power` × the instance tier's power (`WeaponPower.cpp:50`) | **keep the weapon's own tier** | The piece's own stats, as Q3 keeps a ring's stat lines. |
| **Skill slots** | the weapon's own tier (`SlotsForContainerTier`, `FWeaponLoadoutEntry.cpp:41/217/420`), a pair the average of its two hands (D-211), **plus** the crystal's own slots (CL-20) | **keep** | The crystal already adds its slots (CL-20). Averaging as well would count it twice. |
| **Crystal infusion wear, max durability, Crystal Amp** | the crystal's tier | **keep** | The crystal's own state, as for rings. |
| **Dismantle, price, repair, the lost-run drop** | as for rings | **keep** | As for rings. |

**Splitting a hit with both sides.** An infused physical hit (a Fire Sword's slash) is both. Today it is one number with one gap.
- **The physical part** is what the hit deals uninfused: the base × the gap against the weapon's own tier.
- **The elemental part** is what the infusion adds: the charge multiplier's share above ×1 × the gap against the average. A level-0 infusion (×1.00) adds no elemental damage, so its damage stays on the weapon's tier; it still takes the element and fills the element bar.
- **A plain elemental action** (a pulse, a spell through the weapon's crystal) is all elemental: the whole of it reads the average.
- **Build-up** splits the same way: the physical bar's share reads the weapon's gap, the element bar's share the average's.
- Resistances and the element's own rules still apply to the whole hit as today; only the tier gap is split.

**How the average feeds the status build-up.** The gap is the skill's tier against the channel. What follows is the **elemental** side; the physical side reads the weapon's own tier, as today. A skill above the channel is cut, and one below it is boosted (`TierGapDamage`, `TierGapScalingSpec`). The build-up uses the same multiplier as the damage.
- **A gem above the weapon** raises the channel. A skill above the weapon is cut less, and its build-up and damage go up. An F sword with a B Garnet reads D: a D ability on it was 2 tiers above the channel and is now matched (×1.00).
- **A gem below the weapon** lowers the channel. Its skills gain the "below the channel" boost less, or are cut, so build-up and damage go down. An S sword with an F gem reads C.
- **A weapon with no crystal** keeps its own tier: nothing changes.

**The size:** one helper (`CrystalEffectTable::GetWeaponTier(const FWeaponInventoryEntry&)`); the channel split into a physical and an elemental gap (`ResolveChannelTier` gives both), read by the damage and the build-up at `ActionExecutor.cpp:737` / `:1633`; the fallback and Combat Tier. Add weapon cases to `WoR.Loadout.AdapterTier`: an F sword with a B gem reads D on the elemental side and F on the physical; empty reads its own tier on both; an uninfused hit is unchanged; an infused D ability's element build-up rises, its physical build-up doesn't. Update the ED-11 weapon specs and the rules rows. It joins stage 2b, two or three commits more.

## 5. Skills under D-224

### 5.1 Where each skill lives now: no skills on gear

| Skill | Today | With D-224 | Cost |
|---|---|---|---|
| **Weapon built-ins** (`PresetAbilities`, `bAbilitiesLocked`) | on the asset; they pre-fill the weapon's slot list (`WeaponSetRules::GetSetBuiltIns`, D-210) and are offered on the Equipment screen (`LoadoutSkillsModel.cpp:241-252`) | **gone** | `GetSetBuiltIns` and its pre-fill go. The Equipment screen offers only the known skills. The price term goes (`EconomyService.cpp:988-1005`). |
| **Weapon picks** | **on the owned piece**: `FWeaponInventoryEntry::SkillPicks` / `bSkillSlotsActive` (`:101/106`), mirrored on `FGearInstance` (`:72/75`), saved in `FSavedGearDetail` (`WoRSaveTypes.h:480`); written back from the loadout (`InventoryComponent.cpp:1104-1117`, `:1356`) | **on the worn entry**: `FLoadoutSlot::AssignedAbilities` (exists, `FLoadoutSlot.h:51`) in `FWornGear::Equipment`. `FGearPreset` holds a `FWornGear`, so **presets carry the skills automatically**. | The piece's pick fields, the mirror and the save fields go, and so does the write-back. `GiveGear` stops carrying picks (`PoolSubsystem.cpp:520-534`). `FSavedWornGear` must save each entry's skills; if it doesn't, a field is added. This reverses D-145 / CL-20c (Q-085 A stands). |
| **Ring built-ins** (`PresetSpells`, `bSpellsLocked`, `DefaultSpells`) | on the asset; they pre-fill the ring's slots (`FRingLoadoutEntry.cpp:121-135`) | **gone** | The ring's spell list is only the picks. A "locked" (conjured) ring becomes an ordinary empty ring (Crown, Q7); `bSpellsLocked` is deleted in stage 6. |
| **Ring picks** | on the owned piece: `FRingInventoryEntry::SpellPicks` / `bSpellSlotsActive` / `AssignedSpells` (`:88-98`); dropped when the crystal comes off (`FRingInventoryEntry.h:196`) | **on the worn entry**: `FLoadoutSlot::SpellOverrides` (exists; today "weapon only"), the same list for any piece | The same as weapon picks. The drop on unsocketing stays. |
| **Evolution spells** | not on the asset. They are the worn seat's picks: `FWornGear::EvolutionSpells` (`WornGear.h:68`), sized by the evolution's tier (`LoadoutRules.cpp:550`) | **no change**: already part of what is worn. An evolution in an adapter uses the adapter entry's list. | None. |
| **Innate spells** | `FSavedLoadout`; runtime `FWornGear` | moved to `UInventoryData` (§1), runtime unchanged | Crown (Q6): they stay, and a run starts with them. |
| **Broken Darkness pools** | `FSavedLoadout::BDSpellPools` (authored, empty everywhere); runtime `FCombatLoadout` / `FWornGear` | the authored field is deleted; the places are a rule (§5.4) | Small: one copy line goes. |
| **Known skills** | `UInventoryData::Spells` / `Abilities`; runtime `UInventoryComponent` and the pool's `CharacterSkills`; saved in `FSavedCharacterSkills` | **depends on (b)** (§5.3) | — |
| **Field skills** | derived from the worn pieces (`GetFieldAttackEntries`) | **no change** | None. |

**Slot counts (D-211, CL-20):**
- The formulas don't change. They now size **the worn entry's list** instead of the piece's.
- **A weapon entry:** the piece's tier slots, or a pair's average of the two tiers' slots rounded down (D-211), plus its crystal's slots (CL-20; at most 12).
- **An adapter entry:** the adapter tier's slots (§4).
- **The evolution seat:** the evolution's tier.
- **A shield** counts in its pair's D-211 average today, and adds its stats (B-515) (question 8).

### 5.2 Skills name what they need

**What exists today:**
- **`UAbilityData`:**
  - `RequiredWeaponType` defaults to Sword, and `EWeaponType` has no "any".
  - `RequiredWieldMode` and `Element` (`AbilityData.h:41-58`).
- **`USpellData`:** `Element`, and `RequiredEvolutionCrystal` (`SpellData.h:84-86`), **which nothing reads**.
- **`USkillDataBase::Requirements`:** the pillar requirement (Mind / Body / Spirit). An unmet one plays at ×0.6 (`SkillDataBase.h:364-366`, D-119).
- **The checks:**
  - `LoadoutRules.cpp:185-253`: learned, weapon type, wield mode and the gem element. Primary slot only.
  - `FWeaponLoadoutEntry.cpp:372-381`: weapon type and the set's wield mode (B-515).
  - `ActionExecutor::GetSkillRequirementFactor`: pillars.
  - The Equipment screen offers every known skill with no type filter (`LoadoutSkillsModel.cpp:241-252`).
  - The battle's skill list checks no requirement (`FCombatCapabilities.cpp:53-127`).
- **Conditions** (`ConditionSystem.md`, `WoRCondition.h`):
  - `GearEquipped` and `EvolutionEquipped` name one specific asset.
  - `WornItemState` (socket element, evolution, minimum tier) covers only the bound item.
  - No condition reads "wearing any weapon of type X", "any adapter", "any evolution", or "an element in a worn socket".

**What is missing:**
1. An "any weapon" value (abilities are pinned to one type, Sword by default).
2. A "needs an adapter" requirement (optionally a form: Ring, Pistol, Trainer).
3. An "evolution" requirement that is read (`RequiredEvolutionCrystal` exists unread).
4. A worn-gear element requirement (a socket of element X in the worn gear).
5. **One gate** that checks a skill's requirement against what is worn, used by the Equipment screen's choices, the loadout rules and the battle list. Today each place checks something different, and only against a slot on a piece.

**Proposal (the smallest):**
- **A struct:** `FSkillGearRequirement` on `USkillDataBase`: `Kind` (None, Weapon, Adapter, Evolution), `WeaponType` (for Weapon; unset means any weapon), `Form` (for Adapter, optional) and `Element` (None means any).
- **One rule:** `SkillRequirementRules::IsMet(Skill, FWornGear)`.
- **What folds in:**
  - An ability's `RequiredWeaponType` becomes `Kind = Weapon` plus that type.
  - `RequiredWieldMode` stays as a second gate.
  - A spell's `RequiredEvolutionCrystal` becomes `Kind = Evolution`.
  - A spell with nothing set needs an adapter or an evolution with a spell source, as today.
- **Why this shape:** Path of Exile's skill gems ("requires a bow") and Monster Hunter's weapon-type moves gate the skill, not the gear, and players read the requirement on the skill.

**The size:**
- **Code:** the struct and the rule (2 files), then the four readers (the loadout rules, the weapon entry gate, the Equipment choices, the battle list). About three commits.
- **Data:** every skill asset gets its requirement. About 224 weapon skills already have `RequiredWeaponType`, so the editor worker sets `Kind` from it in one scripted pass, and the spells get `Kind = Adapter` or `Evolution`.
- **Stage:** one code stage and one editor pass, each with a sheet.

### 5.3 A run's skills: bought or won (D-224 (2), (a))

**What exists today:**
- **Chests:** `ATrialChest` → `TrialLoot::GrantTo` learns a spell or ability from the chest's source-tag pool, falling back to the zone's loot pool (`TrialLoot.cpp:43-68`).
- **Merchants:** `CombatMaster` sells abilities and `SpellShop` spells (`MerchantData.h:30-31`), learned on purchase (`EconomyService.cpp:1626-1655`).
- **Mentor Tokens:** teach a skill copy (`GearGive.cpp:460`).
- **Rest spots** (Blacksmith, Spiritualist, Jeweler, shrine, Extract): no skills.
- **D-156 / D-183:** pillar points drop from enemies and are bought at trial NPCs. These are pillars, not skills.

**What is missing:**
- **Enemy skill drops:** no enemy or loot table gives a skill.
- **A run-only skill list:** learning writes the character's permanent list (the pool's `CharacterSkills`), so a skill bought in a run stays forever. That contradicts "a run starts with no skills".
- **The reset at trial start**, and the kit's `StartingSkills` (§1).
- **A trial mirror** that builds presets (planned in the restructure, not in code).

**The start (D-224, tune later):**
- A Generic's and a Resonator's kit give one generic bolt in `StartingSkills` (§2), with `// TODO: tune the starting skills (D-224)`. A Refractor's gives none: it starts on its innate spells (Q6).
- **The bolt is in the inventory (Crown, Q9):** it goes into the run's skills, not into a slot. Nothing is pre-attached. The player picks it into a slot, and it casts once a crystal from the stock is in a ring.
- Innate spells stay (Q6). "No skills at the start" means no learned or bought ones.

**(b) yes: a character keeps the skills found in a run, "similar to their other equipment" (Crown).** The hub keeps the character's known list (`CharacterSkills`, as today), and presets are built from it. A run keeps its own list beside the run-start snapshot (B-508): the kit's `StartingSkills`, plus purchases, chests and enemy drops. At the end, the run list is settled by the found-gear rules (D-219, D-221).

**What D-219's found-gear rules mean for a skill:**

| The run ends by | A found gear piece (D-219) | A found skill |
|---|---|---|
| **a boss win (finish)** | kept if worn at the end | kept if **slotted** at the end: it joins the hub's known list; the rest go back |
| **an extract** (leaving early) | kept if worn | kept if slotted |
| **out of wipes** | kept if worn, and every worn piece drops tier ranks (3, tunable) | kept if slotted, and every slotted skill (found or owned) drops the same ranks |
| **a quit, or the game closed mid-run** | the run-start loadout is put back; found gear goes back; the pieces worn then that the player still owns drop tier (D-219, D-221 Q7) | **found ones are lost**: nothing joins the hub list; the owned skills slotted then drop tier, as owned gear does |

- **Today it is not built:** learning writes the permanent list at once (`PoolSkills.cpp`, `CharacterSkills`), so a skill bought in a run stays even after a quit. The run list and its settling at the end are the S3 code.
- **A kept skill is for the hub and its presets** (Crown, question 11): it can be put in any preset in the hub. It does **not** start the next run: a trial starts on the class kit with no skills, and the trial mirror is where a preset's skills are bought back (D-221).
- **"Slotted"** means in a slot of a worn entry at the end (`FLoadoutSlot::AssignedAbilities` / `SpellOverrides`, B-517). A found skill only in the run list goes back to the pool, as found gear left in the inventory does.

**What a skill's tier is today, and what the drop does.**
- **A skill has a tier:** the learned copy's instance tier (`FAbilityInstance::Tier` / `FSpellInstance::Tier`), set from the asset's tier when it is learned. `UInventoryComponent::ResolveAbilityTier` / `ResolveSpellTier` (`InventoryComponent.cpp:742`) read it; nothing raises it yet (leveling is planned).
- **What reads it:** only the tier gap: the copy's tier is the action's tier against the channel, for damage, build-up and cost (`ActionExecutor.cpp:402-410`). The skill's **own-tier power** (×1.00 at F to ×4.80 at S) reads the **asset's** tier, not the copy's (`ActionExecutor.cpp:737`, `:1437`, `:1633`).
- **So the drop, as the code stands, would backfire.** `DropLostRunTier` (`PoolRunLayer.cpp:64`, 3 ranks, not below F) on the copy lowers it against the channel, and the gap **boosts** a skill below the channel (up to ×1.30, and cheaper). The skill would come out stronger.
- **The fix with it (S3, the plan's default):** the own-tier power reads the copy's tier as well. Then the drop costs, as it does for gear: a C skill dropped to F goes from ×2.20 power to ×1.00, with at most the ×1.20 gap boost, so about half its damage and build-up. The copy is the character's one copy, so the drop shows in every preset that holds it, as a dropped piece does.

**Answered (Crown, D-224 "Found skills follow the equipment rules", questions 10 to 12):**
- **(10)** Only the skills that are **slotted** at the end are kept; "everything else returns to the pool, like gear".
- **(12)** On a lost run a kept skill "would follow the same rules as equipment": D-219's worn-gear rules apply to slotted skills (found ones aren't kept out of a quit; what is kept or owned takes the tier loss). The drop needs the own-tier power on the copy's tier, above.
- **(11, agreed)** A kept skill is for the hub and its presets; a trial still starts on the class kit with no skills.

### 5.4 Broken Darkness's pools: a rule, not data (Crown, 2026-11-15)

**Crown:** "the pool is just saying we have these empty spaces to learn spells", "we wouldn't add presets to it", and "we might even just have to remove the data asset side of it and keep it as logic for UI".

**What a pool is in code today:**
- **The struct:** `FBDElementSpellPool` (`FCombatLoadout.h:48`): an `Element` and its `Spells` (at most 6, each of that element). One pool per absorbable element other than Darkness, so 7. Darkness's own spells are the character's `InnateSpells`, not a pool.
- **Where it is authored:** `FSavedLoadout::BDSpellPools` (`FSavedLoadout.h:169`, as `FSavedBDElementSpellPool`) on `UInventoryData`'s loadouts. The asset dump shows it empty on every inventory.
- **The places already come from code:** `ULoadoutComponent::InitializeBDPools` (`LoadoutComponent.cpp:1960`, run by `ApplyBDPoolsIfBroken`) adds an empty pool for each absorbable element the loadout lacks. The limits are named constants: `MAX_BD_ELEMENT_POOLS` 7 (`LoadoutConstants.h:19`), `MAX_EQUIPPED_SLOT_POOL` 6 places a pool (`SpellPoolConstants.h:23`), `BD_SPELL_BUDGET` 48 (`:28`).
- **The runtime pools** hold the spells the player has put in. They are kept on the loadout and on what is worn (`FCombatLoadout::BDSpellPools`, `FWornGear::BDSpellPools`) and saved with them (`FSavedCustomBDPool` in `WoRSaveTypes.h:268` / `:635`).

**Every reader of the pools today:**

| Reader | What it reads | After the change |
|---|---|---|
| `MakeRunPreset` (`InventoryComponent.cpp:1475`) | copies the **authored** pools into a run preset | **goes**: the run preset starts with the places from the rule |
| `InventoryData.cpp:255` | empties an authored loadout's pools for a non-Caster | **goes** with the field |
| `FCombatLoadout::CreateFromSavedLoadout` / `ToSavedLoadout` (`FCombatLoadout.cpp`) | the authored and preset round trip | the authored part goes. A preset keeps the spells the player put in (a preset is a copy of what is worn, D-221) |
| `WornGear.cpp` (compare, `CreateFromWornGear`, `ToSavedLoadout`) | the worn pools | kept: they hold the player's spells |
| `SaveSubsystem.cpp:306-420` | saves and loads the preset and worn pools | kept |
| `GetAvailableSpells` (`LoadoutComponent.cpp:1499`) | the active pool's spells in a fight | kept |
| `ResolveSpellSource`, `FindSpellCatalystHolder`, the broken-source checks (`LoadoutComponent.cpp:1547`, `:1630`, `:2930`, `:3003`) | "is this spell a pool spell" | kept |
| `FLoadoutRules` (`LoadoutRules.cpp:269`) | the pool size and budget check | kept: it already checks against the constants |
| `FCombatCapabilities.cpp:98`, `ActionExecutor.cpp:4802` | the castable list, the AI | kept |
| `LoadoutSkillsModel.cpp:98` | a `TODO` (B-440d): the Skills tab doesn't draw the pools yet | the tab draws each absorbed element's 6 places, empty until filled |
| `InventoryDebug.cpp`, `GenericSpellResolveDebug.cpp` | debug prints | kept |

**The rule that replaces the authored field:** a Broken Darkness character has one pool per absorbable element other than Darkness (`MAX_BD_ELEMENT_POOLS`), each with `MAX_EQUIPPED_SLOT_POOL` empty places, within `BD_SPELL_BUDGET`. That is `InitializeBDPools` as it stands. No reader needs content the rule can't give: none of them reads authored spells (there are none), so there is nothing to ask Crown.

**Where the spells the player puts in are kept:** on what is worn, as today, and therefore in presets. A spell learned into a pool in a run is a found skill (§5.3): it follows the same keep-or-lose rule at the run's end.

**The stage:** stage 6 deletes `FSavedLoadout::BDSpellPools` (and `FSavedBDElementSpellPool` if nothing else uses it) and the two "goes" readers above. The Skills tab's empty places are UI work in the restructure's Skills screen, not this plan.

**Trial skills (a future stage, not designed now):** a trial-side track beside the starting-gear track (D-221), with unlocks such as "start with this spell in a trial" and "more starting spells". They are read at run start alongside the kit's `StartingSkills`.

## 6. The stages (each ends in a PIE check)

**Each stage runs the same way:**
1. Code commits of at most 3 files each.
2. Build, the full suite and `wor-reviewer`.
3. Merge to dev.
4. The editor worker makes the stage's assets.
5. A short sheet in `docs/PIE/`.
6. **Crown's PIE pass before the next stage starts.**

Until stage 4, the new pieces are added as spares to the current played character's inventory.

### The asset track

| Stage | Code | Editor | Crown checks in PIE (its own sheet) |
|---|---|---|---|
| **1. Sword and shield** ✅ B-515, in dev | `EWeaponType::Shield`, the set rule, the held name, the off-hand mesh | `DA_Weapon_Sword` (SingleOrDual), `DA_Weapon_Shield` | `PIE/B-515.md`: "Arming Sword and Round Shield", "Sword and Shield", the shield in the left hand, the sword's attack and stance. |
| **2. The empty adapter** ✅ B-516, in dev | sockets in the add window (§3 gap); the detail shows the owned piece's tier; the repeat-number helper (+ specs, including two Lightning rings) | `DA_Ring_Adapter` (no spells, the shared stat line); **three** copies as spares | Add it: "Adapter Ring", F, its stat line. Socket a Garnet in the same window: "Fire Ring", one spell slot, empty until you pick a known spell. **Two Lightning rings:** "Lightning Ring" and "Lightning Ring 2" as separate entries, each picked and cast on its own, picks separate, wear only on the cast ring, both kept through a preset save and reload. |
| **2b. The built tier: rings and weapons** (§4, §4.1) | `GetAdapterTier` and its four ring readers; `GetWeaponTier` and its three readers; the detail line (+ `WoR.Loadout.AdapterTier` with weapon cases, updated ring and ED-11 weapon specs, rules rows) | none (use the stage 2 rings, the sword and the stock) | An F ring with a B Citrine reads **D**: 3 spell slots, its spells' damage and wear against D, **its stat line still at F**. Unsocketed it reads F again. An F sword with a B Garnet reads D: a D ability's damage and status build-up go up. Combat Tier moves with both. |
| **3. Pieces start empty; the axes** | stop copying `AttachedItem`; the price term; a found piece may be prebuilt (§3, "Found pieces") (+ spec fixes) | `DA_Weapon_Axe_Hand`, `DA_Weapon_Axe_Bearded` | The axes read "Hand Axe and Bearded Axe", "Dual Axes", each axe's own mesh in its hand. A ring bought from the merchant arrives empty. |
| **4. The Generic starts from `StartingGear`** | the `UInventoryData` fields, `FClassDefaultPiece::Crystal`, `FClassDefaultLoadout::StartingSkills` (field only); generalise the kit minting; the `DrawSharedGear` seed (+ spec) | `DA_Character_Generic`, `DA_Inventory_Generic_Start`, Gale; point the played character at it | A new game: the Generic wears the sword and shield, the axes are spare, the **Earth Ring** is socketed, Gale is in its seat, and the stock is in the pouch. |
| **5. The rest of the cast** | **none (checked 2026-11-16).** A player's `StartingGear` seeds any party member (B-522); an empty kit row (the Refractor's) mints and wears nothing and keeps the innate. Two things the editor worker must know: innate spells are still read from the start asset's one old loadout (B-522, until stage 6), and **an enemy reads only the old `SavedLoadouts`** (`InitializeFromInventoryAsset`), not `StartingGear`. So the two enemies keep the old loadout shape, with a ring whose asset has a baked crystal, until §7 job (c) builds an enemy's loadout from a kit. | the other characters, Phoenix Fire, the two enemies, `DA_Loadout_ClassDefaults` cut to the first shape (§2: the Generic's sword, the Resonator's two rings, nothing for the Refractor; the bolt) | Each character's start, the Refractor's Fire innate and Phoenix, BD's empty pool places, a trial on the kit (the bolt in the inventory, not in a slot), a fight against each enemy. |
| **6. Delete the old** | delete the §1 fields and their readers, and `FSavedLoadout::BDSpellPools` with its two authored readers (§5.4); raise the minimum save version | the reference check, then the §2 delete list | A new game works, an old save is wiped with a warning, validation has no new errors, a battle plays. |

### The skills track (D-224; (b) is answered, so none of it waits)

| Stage | Code | Editor | Crown checks in PIE |
|---|---|---|---|
| **S1. No skills on gear** (§5.1); part 1 ✅ B-517 (the picks on the worn entry), part 2 B-518 (the built-ins go) | picks move from the piece to the worn entry (`FLoadoutSlot`), with the save; built-ins and the write-back go; the Equipment screen offers only known skills | clear the clean set's skill lists (none authored) | A sword has no skills until you put one in. The skills stay when you change crystals. A preset keeps them. Giving the sword away doesn't take them along. |
| **S2. Skills name what they need** (§5.2) | `FSkillGearRequirement`, `SkillRequirementRules::IsMet`, the four readers | one scripted pass setting every skill's requirement | An axe skill is offered only with an axe worn. A bolt needs an adapter with a crystal. An evolution skill needs an evolution. A skill whose gear you take off greys out. |
| **S3. A run's skills** (§5.3) | the kit's `StartingSkills` into the run list at trial start (not into a slot); purchases and chests go into it; at the end the list is settled by D-219: the **slotted** ones are kept on a finish, an extract or out of wipes, the rest go back; found ones are dropped on a quit; slotted skills drop tier on a lost run or a quit, and the own-tier power reads the copy's tier | the bolt in the Generic's and the Resonator's kits | A trial starts with the bolt in the inventory, and you pick it into a slot. Buying a skill at the merchant adds it. After an extract the slotted ones are in the hub list and an unslotted one is gone; after a quit the found ones are gone. Out of wipes a slotted skill reads three tiers lower and hits softer. The next trial starts with no skills again. |
| **S4. Enemies drop skills: folded into the trial pool stage (§7, job (d))** | not built on its own (brain, 2026-11-16): D-225 changes how enemies get and drop skills, so S4 on today's stopgap enemy fill would be thrown away | — | in §7's PIE sheet: defeating an enemy can give a skill it used, shown in the reward. |
| **Later: trial skills** | the trial-side starting-skill track | — | — |

**The order:** stages 2, 2b and 3 don't depend on the skills track. S1 is best before stage 4, so the new assets never carry picks on pieces. The trial pool (§7) comes after stage 5.

## 7. A trial's pool: what it gives is what its enemies use (D-225)

**In plain words:** each trial lists what can be earned in it. Its enemies are rolled from that same list when the run starts, sized by the enemy's own pillar points. A boss also has a few skills it always has. What an enemy uses is more likely to drop.

### 7.1 What exists today and is reused

| Need | Already there | Gap |
|---|---|---|
| A pool | `TrialLoot::GatherPool(tag)`: every weapon, ring, spell and ability whose `SourceTags` match a tag (B-188). The zone's `FEncounterZoneInfo::LootPool` and `ATrialChest::GetLootPool` already name one. | Evolutions and apparel have `SourceTags` but `GatherPool` doesn't read them yet. Crystals aren't assets (`FCrystalId` is a type and a tier), so they need a short list. |
| A seeded roll | `TrialLoot::PickFromPool(candidates, pool, seed)`; `UTrialRunSubsystem::GetRunSeed`; `UBattleRandomSubsystem` for the fights | the roll is unweighted (its TODO already asks for weights) |
| The enemies | `UEncounterData::Enemies` (character templates) and `MiniBossForm`; `EEncounterRank` (Normal, MiniBoss, Boss, Elite); the zone's `Tier` (0 to 4) | nothing says what an enemy may roll |
| An enemy's size | the template's `WorldMindLevel` / `WorldBodyLevel` / `WorldSpiritLevel` (0 to 7 each) | no table from them to a kit |
| Hard limits | `LoadoutClassTable` (`FLoadoutClassLimits`: budget, max items, weapons, adapters) per class | — |
| Slots | D-211: `SlotsForContainerTier` F1 E2 D3 C4 B5 A6 S6, the pair average, the crystal's slots (CL-20) | — |
| A skill's fit | `FWorldStatRequirements` on a skill (`RequiredWorldMind` etc.); a gap lowers its power (`GetRequirementGapPillarPercents`); S2's gear requirement (§5.2) | — |
| Merchant and chest | `UMerchantData::Stock` and `AvailabilityTags`; the chest's pool | the merchant doesn't read the trial pool |
| Drops | `TrialGate::RollLevelDrops` (the level-drop ladder); S4's skill drop entry | no "what it used" weight |
| Today's stand-in | Q-518's `FCombatLoadout::FillEmptyPicksFromKnown` (built in B-518; D-225 calls it B-519's): an enemy's empty piece fills from its known skills | goes when §7 is built |

### 7.2 The new data

**The pool stays a tag** (no new pool asset): a trial's items opt in by `SourceTags`, as chests do today. The trial gets one new field:
- `UTrialData::Pool` (a `FGameplayTag`): the trial's pool, read by its zones, chests, merchant and enemies. A zone's own `LootPool` stays as an override.
- `UTrialData::PoolCrystals` (a list of `FCrystalId`): the crystals the trial can give, since crystals aren't tagged assets.

**On the enemy's template** (`UCharacterData`, enemies only), one struct `FEnemyRollLimits`:
- `WeaponTypes`: the weapon types it may roll (empty means its class's usual ones).
- `Elements`: the crystal elements it may roll (empty means its innate element only).
- `AlwaysHas`: skills it always has, slotted first (a mini boss's or boss's signature; Persona and Expedition 33 keep fixed boss signatures).
- `bRollsExtras`: whether a boss rolls more beyond `AlwaysHas` (default yes, inside the table).

Its class (`LoadoutClassTable`) stays the hard cap on pieces.

### 7.3 The pillar table (`EnemyKitTable`, a named table in C++, like `LoadoutClassTable`)

The enemy's **total** pillar points (Mind + Body + Spirit, 0 to 21) pick a row. Total reads closer to Crown's example and stays one table. Each pillar is used **per skill** instead (below), which matches how skill requirements already work.

| Total pillar points | Pieces | Skills (at most) | Tier band |
|---|---|---|---|
| 3 (e.g. 1 1 1, Crown's example) | 1 // TODO | 3 // TODO | D // TODO |
| 0 to 2 | 1 // TODO | 1 // TODO | F // TODO |
| 4 to 8 | // TODO | // TODO | // TODO |
| 9 to 14 | // TODO | // TODO | // TODO |
| 15 to 21 | // TODO | // TODO | // TODO |

**How it fits D-211's slots:**
- The table's skills number is a **cap**, not a target: an enemy holds at most the smaller of that number and the slots its rolled pieces give. Slots need not be filled (D-225 (3)).
- The tier band sets the pieces' tier, and the slots follow from D-211. So the band must give enough slots for the skills number. One F piece has one slot, so Crown's "1 piece with 3 skills" needs a **D** piece (3 slots), or an E piece with a crystal (2 + the crystal's). That is why the example row says D.
- A pair or a shield set follows the D-211 rules as worn by players.

**How it fits the skills' pillar requirements:**
- The roll prefers skills whose `FWorldStatRequirements` the enemy meets, pillar by pillar. A skill it falls short on may still be rolled, but plays weaker by the existing gap rule. So a Body-heavy enemy leans to Body skills without a new rule.
- Gear is rolled first, then only skills that gear can use (D-224, §5.2's gate).

### 7.4 When and how it rolls

1. **Run start** (`UTrialRunSubsystem`, with the run seed): for each enemy template in the trial's encounters, roll its kit once and keep it for the run (D-225 (5)). Every fight against it, a retry and a co-op partner all see the same kit.
2. **The roll:** pieces from the pool inside `FEnemyRollLimits` and the class cap, at the band's tier. Then `AlwaysHas`, then skills from the pool up to the cap.
3. **The fight:** the enemy's loadout is built from the rolled kit instead of its authored loadout. The Q-518 fill and its `// TODO` go. (The TODO's "stage 5" means this stage.)
4. **Drops:** `PickFromPool` gets weights, and what the defeated enemy used weighs more (D-225 (4); Diablo-style "the monster drops what it wields"). S4's skill drops fold into this.
5. **Seeing a kit:** a field skill (or an evolution's field skill) shows a rolled enemy's kit (D-225 (6)). It is a later job, since it needs its UI.

### 7.5 Where it sits and how big it is

**Placement:** after stage 5 (the cast, including the two enemies), as its own stage; stage 6's deletes don't touch what it reads. S4 (enemies drop skills) merges into its drop step.

**Size: about four code jobs, plus editor work.**
- (a) The two `UTrialData` fields, `FEnemyRollLimits`, `EnemyKitTable` and its debug print, plus specs.
- (b) The run-start roll and its run-kept kits (`wor.Trial.PrintEnemyKits`), plus specs with a fixed seed.
- (c) The enemy loadout from the kit; the fill goes. It also reads an enemy's own `StartingGear` (with each piece's `Crystal`) where no kit is rolled, so stage 6 can delete `SavedLoadouts`; **(c) comes before stage 6's deletes**. Plus PIE.
- (d) The weighted drops.

Editor: tag the clean set into the first trial's pool, and author the two enemies' limits and pillar points. Later: (e) the field skill.

## Questions for Crown

1. **The cast and gear list (§2):** OK as listed? It includes the item names, who holds which ring and evolution, and the adapter's one shared stat line.
2. ~~**Two same-named rings.**~~ **Answered (Crown, D-223):** the tier tells them apart first (the tier badge; the strip's tier letter); a number only when the name and the tier both match (built: B-516b).
3. ~~**The built ring tier (§4).**~~ **Answered (Crown):** the ring's stat lines stay on its own tier; the average drives the skills cast through it (slots, damage, wear) and Combat Tier.
4. ~~**Weapons with a crystal.**~~ **Answered (Crown):** they average too, and it affects the status build-up (§4.1). By the plan's default, weapon power and skill slots keep the weapon's own tier (its own stats; CL-20 already adds the crystal's slots).
5. ~~**(b).**~~ **Answered (Crown):** yes. A character keeps the skills found in a run, like its other equipment, under D-219 / D-221 (§5.3). Three follow-ons are questions 10 to 12.
6. ~~**Innate spells and BD pools.**~~ **Answered (Crown):** innate spells stay; a BD pool is empty places, not spells, and becomes a rule (§5.4).
7. ~~**Locked (conjured) rings.**~~ **Answered (Crown):** they become ordinary empty rings; found rings may come prebuilt, with a crystal or an evolution in the socket (§3, "Found pieces").
8. ~~**A shield in a pair.**~~ **Answered (Crown):** it counts as one weapon, like a sword; the shield adds nothing to the slot average. By the plan's default it still adds its stats (§1).
9. ~~**The starting bolt.**~~ **Answered (Crown):** it's in the inventory and the player attaches it; nothing is pre-attached, and the kit's sockets start empty (§5.3).
10. ~~**A found skill: picked or carried?**~~ **Answered (Crown):** only the slotted ones are kept; the rest go back to the pool, like gear (§5.3).
11. ~~**A kept skill and the next run.**~~ **Answered (agreed by Crown):** the hub list and its presets only; a trial starts on the kit with no skills (§5.3).
12. ~~**A kept skill out of wipes.**~~ **Answered (Crown):** it follows the equipment rules: slotted skills, owned or found, take the tier loss. By the plan's default the skill's own-tier power then reads the copy's tier, or the drop would make it stronger (§5.3).
13. **The pillar table (§7.3):** the **total** of the three pillars picks the row, and each pillar steers which skills are rolled. OK, or a row per pillar?
14. **Crown's example row:** 1 1 1 gives one piece with up to 3 skills, so the piece must be D tier (or E with a crystal) to have 3 slots. OK, or fewer skills at that size?
15. **Elites (§7.4):** roll like normal enemies one row up, or have `AlwaysHas` like bosses?
16. **A boss's extras:** `AlwaysHas` plus a roll inside the table (default), or `AlwaysHas` only?
17. **Drop weight:** how much likelier is what the enemy used (default 3×, // TODO)? And can a boss's `AlwaysHas` skills drop?
18. **Apparel on enemies:** rolled from the pool like weapons (default), or drop-only?
19. **The merchant:** does it sell from the trial's pool too (default yes, as Hades' shops follow the region), or keep its own stock?
