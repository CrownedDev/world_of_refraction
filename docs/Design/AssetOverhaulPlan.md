# Asset overhaul plan (D-223, D-224)

**Status:** plan; stage 1 is built (B-515, in dev). **Crown approves the cast and gear list (§2) before the asset stages are built.**

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
| `InnateSpells`, `BDSpellPools` | **moved here** from `FSavedLoadout` | The character's own spells (§5.1). |
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

## 2. The asset list (for Crown's OK)

All new assets go under `Content/Data/`, named per ContentNaming.

### Create: the cast (5 players, 2 enemies)

| Asset | Path | Class / innate | Starting gear (`StartingGear`; F tier, crystals at F) | Evolution |
|---|---|---|---|---|
| `DA_Character_Generic` + `DA_Inventory_Generic_Start` | `Data/Characters/Generic/` | Generic | Sword with the shield as its off hand (active). The two axes as a pair. **Earth Ring** (adapter + Amber). | Gale |
| `DA_Character_Refractor` + `DA_Inventory_Refractor_Start` | `Data/Characters/Refractor/` | Caster, Fire innate | **Fire Ring** (adapter + Garnet). Innate Fire spells (`DA_Spells_Fire_*`). | Phoenix |
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
| `DA_Evo_Fire_Phoenix` | Garnet (Fire), Balanced | — | — | "Phoenix" | E |
| `DA_Evo_Wind_Gale` | Emerald (Wind), Balanced | — | — | "Gale" | E |

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

**What the two evolutions give.** Their element comes from their crystal kind: Phoenix is Fire, Gale is Wind.
- **Proposal:** Phoenix takes the field-skill EP −25% and Gale the sprint +15%, so the two read differently in the field. Today all 27 evolutions carry both placeholder bonuses.
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

| Class | Pieces | `StartingSkills` (D-224 default, `TODO` tune) |
|---|---|---|
| Generic | `DA_Weapon_Sword` (with `DA_Weapon_Shield` as its off hand), one `DA_Ring_Adapter` | one generic bolt |
| Caster | one `DA_Ring_Adapter` | one generic bolt |
| Resonator | `DA_Weapon_Sword`, three `DA_Ring_Adapter` | one generic bolt |

- Every kit is F tier, and its `Crystal` is left empty: the trial's kit starts empty (D-221), and its stock fills it.
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
| **Wear per cast** | 7% per tier gap, spell tier − crystal tier (`CrystalManager.cpp:105-121`) | **the same gap as the output**: spell tier − adapter tier | **A new consequence:** a better ring slows a weaker crystal's wear. One gap per cast keeps wear and damage reading the same. Crown's OK needed (question 3). |
| **The adapter's stat lines** | the ring's tier (`GetTierPowerMultiplier`, `LoadoutComponent.cpp:3931-3988`) | **the adapter tier** | **A new consequence:** socketing a lower crystal lowers an S ring's stats (S with F reads C). An empty ring uses its own tier. Crown's OK needed (question 3). |
| **Combat Tier** | the ring's tier (`CombatTier.cpp:51`; D-118, Q-325) | **the adapter tier** | It is the overall number, so it should match. Weapons' crystals still aren't counted (question 4). |
| **Dismantle** | the ring's own tier (`EconomyService.cpp:475`) | **keep the ring's tier** | You dismantle the ring piece; its crystal comes out as today. |
| **Buy / sell price** | the sum of the ring's and the attachment's prices (ER-22, `EconomyService.cpp:920-934`) | **keep the sum** | The sum already prices both parts; an average would under-price the pair. |
| **Repair** | the crystal's tier price (`GearRepair.cpp:35-45`) | **keep** | Repair fixes the crystal. |
| **Lost-run tier drop** | the ring drops 3 tiers, its crystal doesn't (ER-65g, D-219) | **keep** | The adapter tier falls with the ring through the average. |
| **A broken crystal** (D-083), **the S-crystal cap** (CL-13b) | the spell's tier; the crystal's tier | **keep** | — |
| **The Equipment detail** | the asset's tier, a bug (`LoadoutV2Model.cpp:312-314`) | **the adapter tier**, with the parts in the detail line | — |

**The size of the change:**
- **Code:** one helper plus five readers:
  - the ring slot cap;
  - the channel tier;
  - the wear gap;
  - the stat-line multiplier;
  - Combat Tier.
- **Equipment detail:** fixed as above.
- **Specs:** a new `WoR.Loadout.AdapterTier` spec; the CL-25, ED-11 and ER-34 ring specs are updated.
- **Rules pages:** the `docs/Rules/` rows change with it.
- **Commits:** about three, of at most three files each.
- **Stage:** it has its own stage (2b) and its own PIE sheet.

## 5. Skills under D-224

### 5.1 Where each skill lives now: no skills on gear

| Skill | Today | With D-224 | Cost |
|---|---|---|---|
| **Weapon built-ins** (`PresetAbilities`, `bAbilitiesLocked`) | on the asset; they pre-fill the weapon's slot list (`WeaponSetRules::GetSetBuiltIns`, D-210) and are offered on the Equipment screen (`LoadoutSkillsModel.cpp:241-252`) | **gone** | `GetSetBuiltIns` and its pre-fill go. The Equipment screen offers only the known skills. The price term goes (`EconomyService.cpp:988-1005`). |
| **Weapon picks** | **on the owned piece**: `FWeaponInventoryEntry::SkillPicks` / `bSkillSlotsActive` (`:101/106`), mirrored on `FGearInstance` (`:72/75`), saved in `FSavedGearDetail` (`WoRSaveTypes.h:480`); written back from the loadout (`InventoryComponent.cpp:1104-1117`, `:1356`) | **on the worn entry**: `FLoadoutSlot::AssignedAbilities` (exists, `FLoadoutSlot.h:51`) in `FWornGear::Equipment`. `FGearPreset` holds a `FWornGear`, so **presets carry the skills automatically**. | The piece's pick fields, the mirror and the save fields go, and so does the write-back. `GiveGear` stops carrying picks (`PoolSubsystem.cpp:520-534`). `FSavedWornGear` must save each entry's skills; if it doesn't, a field is added. This reverses D-145 / CL-20c (Q-085 A stands). |
| **Ring built-ins** (`PresetSpells`, `bSpellsLocked`, `DefaultSpells`) | on the asset; they pre-fill the ring's slots (`FRingLoadoutEntry.cpp:121-135`) | **gone** | The ring's spell list is only the picks. A "locked" (conjured) ring loses its fixed spells (question 7). |
| **Ring picks** | on the owned piece: `FRingInventoryEntry::SpellPicks` / `bSpellSlotsActive` / `AssignedSpells` (`:88-98`); dropped when the crystal comes off (`FRingInventoryEntry.h:196`) | **on the worn entry**: `FLoadoutSlot::SpellOverrides` (exists; today "weapon only"), the same list for any piece | The same as weapon picks. The drop on unsocketing stays. |
| **Evolution spells** | not on the asset. They are the worn seat's picks: `FWornGear::EvolutionSpells` (`WornGear.h:68`), sized by the evolution's tier (`LoadoutRules.cpp:550`) | **no change**: already part of what is worn. An evolution in an adapter uses the adapter entry's list. | None. |
| **Innate spells, BD pools** | `FSavedLoadout`; runtime `FWornGear` | moved to `UInventoryData` (§1), runtime unchanged | Small. Do they count as "skills" a run starts without? (question 6). |
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

**The default for now (D-224, tune later):**
- Each class kit's `StartingSkills` is one generic bolt (§2), with `// TODO: tune the starting skills (D-224)`.
- A Generic's or Resonator's bolt needs an adapter with a crystal. The kit's sockets start empty, so the bolt can't be cast until a crystal from the stock goes in (question 9).

**What depends on (b), a permanent known-skills list in the hub:**
- **If yes:**
  - The hub keeps the character's known list (`CharacterSkills`, as today), and presets are built from it.
  - A run keeps its own list beside the run-start snapshot (B-508). It starts as the kit's `StartingSkills`, and purchases, chests and enemy drops go into it.
  - At the run's end, the run list is dropped. Whether a won skill joins the hub list is a follow-on question.
- **If no:** one list per character, reset to the kit's `StartingSkills` at each trial start.

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
| **2. The empty adapter** | sockets in the add window (§3 gap); the detail shows the owned piece's tier; the repeat-number helper (+ specs, including two Lightning rings) | `DA_Ring_Adapter` (no spells, the shared stat line); **three** copies as spares | Add it: "Adapter Ring", F, its stat line. Socket a Garnet in the same window: "Fire Ring", one spell slot, empty until you pick a known spell. **Two Lightning rings:** "Lightning Ring" and "Lightning Ring 2" as separate entries, each picked and cast on its own, picks separate, wear only on the cast ring, both kept through a preset save and reload. |
| **2b. A ring's built tier** (§4) | `GetAdapterTier` and its five readers; the detail line (+ `WoR.Loadout.AdapterTier`, updated ring specs, rules rows) | none (use the stage 2 rings and the stock) | An F ring with a B Citrine reads **D**: 3 spell slots, the stat line at D, its spells' damage and wear against D. Unsocketed it reads F again. Combat Tier moves with it. |
| **3. Pieces start empty; the axes** | stop copying `AttachedItem`; the price term (+ spec fixes) | `DA_Weapon_Axe_Hand`, `DA_Weapon_Axe_Bearded` | The axes read "Hand Axe and Bearded Axe", "Dual Axes", each axe's own mesh in its hand. A ring bought from the merchant arrives empty. |
| **4. The Generic starts from `StartingGear`** | the `UInventoryData` fields, `FClassDefaultPiece::Crystal`, `FClassDefaultLoadout::StartingSkills` (field only); generalise the kit minting; the `DrawSharedGear` seed (+ spec) | `DA_Character_Generic`, `DA_Inventory_Generic_Start`, Gale; point the played character at it | A new game: the Generic wears the sword and shield, the axes are spare, the **Earth Ring** is socketed, Gale is in its seat, and the stock is in the pouch. |
| **5. The rest of the cast** | none expected | the other characters, Phoenix, the two enemies, `DA_Loadout_ClassDefaults` (pieces and the bolt) | Each character's start, the Resonator's three rings, the Refractor's Fire innate and Phoenix, BD's pools, a trial on the kit, a fight against each enemy. |
| **6. Delete the old** | delete the §1 fields and their readers; raise the minimum save version | the reference check, then the §2 delete list | A new game works, an old save is wiped with a warning, validation has no new errors, a battle plays. |

### The skills track (D-224; S1 and S2 don't wait for (b), S3 does)

| Stage | Code | Editor | Crown checks in PIE |
|---|---|---|---|
| **S1. No skills on gear** (§5.1) | picks move from the piece to the worn entry (`FLoadoutSlot`), with the save; built-ins and the write-back go; the Equipment screen offers only known skills | clear the clean set's skill lists (none authored) | A sword has no skills until you put one in. The skills stay when you change crystals. A preset keeps them. Giving the sword away doesn't take them along. |
| **S2. Skills name what they need** (§5.2) | `FSkillGearRequirement`, `SkillRequirementRules::IsMet`, the four readers | one scripted pass setting every skill's requirement | An axe skill is offered only with an axe worn. A bolt needs an adapter with a crystal. An evolution skill needs an evolution. A skill whose gear you take off greys out. |
| **S3. A run's skills** (§5.3, after (b)) | the kit's `StartingSkills` given at trial start; the run list (or the reset); purchases and chests go into it | the bolt in each kit | A trial starts with only the bolt. Buying a skill at the merchant adds it for the run. After the run it's gone (or kept, per (b)). |
| **S4. Enemies drop skills** | a skill entry in enemy loot | a few drop entries | Defeating a tagged enemy can give a skill, shown in the reward. |
| **Later: trial skills** | the trial-side starting-skill track | — | — |

**The order:** stages 2, 2b and 3 don't depend on the skills track. S1 is best before stage 4, so the new assets never carry picks on pieces.

## Questions for Crown

1. **The cast and gear list (§2):** OK as listed? It includes the item names, who holds which ring and evolution, and the adapter's one shared stat line.
2. **Two same-named rings:** a repeat number ("Lightning Ring 2"), shown only where a list has the same name twice (the default for now)?
3. **The built ring tier (§4).** The proposed reads:
   - spell slots, spell damage, the wear gap, the stat lines and Combat Tier use the adapter tier;
   - Crystal Amp, max durability, dismantle, price and repair stay as they are.
   - Two consequences need your OK: **a better ring slows a weaker crystal's wear**, and **socketing a low crystal lowers an S ring's stats**.
4. **Weapons with a crystal:** should they average too? Recommendation: not now, rings first. A weapon's ability slots already add its crystal's (CL-20), and its spells' channel stays the crystal's.
5. **(b), still open:** does a character keep a permanent list of known skills in the hub?
6. **Do innate spells and Broken Darkness's pools count as skills a run starts without?** Recommendation: no. They are the class's identity and always on (D-040).
7. **Locked (conjured) rings:** today their spells are fixed. With no spells on gear, do they become ordinary empty rings? Recommendation: yes, and their spells become skills that require that ring's form.
8. **A shield in a pair:** today it counts in the D-211 slot average (an F shield lowers an S sword's slots) and adds its own stats. Recommendation: the shield adds its stats but **doesn't** count in the slot average; the main hand's slots stand.
9. **The starting bolt** needs an adapter with a crystal, but the kit's sockets start empty, so a crystal must go in before the bolt can be cast. OK, or should the kit's ring start socketed?
