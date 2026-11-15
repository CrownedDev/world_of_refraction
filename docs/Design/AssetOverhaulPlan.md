# Asset overhaul plan (D-223)

**Status:** plan only, no code yet. **Crown approves the cast and gear list (§2) before anything is built.** Revision 2 (2026-11-15) takes in Crown's answers:
- the rings are told apart by their crystal's element;
- a ring's tier reuses the existing logic;
- the work is built and tested in stages;
- the skill-ownership assessment (§5).

**Inputs:**
- D-220, D-221, D-222 and D-223: the clean gear set, rings named by their socket, a ring's tier.
- `Audits/CharacterAssetDump.md`: its "Anything that looks odd" list is the checklist.
- `Design/LoadoutRestructurePlan.md`: this plan brings its stage 4 forward.
- `Conventions/ContentNaming.md`.

## In plain words

The characters and their gear are still authored the old way:
- two fixed slots (primary and secondary);
- crystals baked into weapons and rings;
- spell lists on weapons;
- a shield that is part of the sword.

We make **new, small, clean assets** in the new shape: each character's starting gear is a plain list of pieces, and each piece says which crystal sits in its socket. Then we delete the old assets. The code learns to read only the new shape; nothing converts the old one. It is built in **small stages**, and each one ends in a short PIE sheet Crown checks before the next starts (§6).

## 1. The new authored shape

### The character (`UCharacterData`, kept)

- **Kept:**
  - the name, class, innate element, the Broken Darkness flag;
  - the portrait, origin, cosmetics, resistance profile, the sub-stats;
  - `Inventory`, its link to the starting asset below.
- **Gone:** `TurnSpeed` (OBSOLETE since B-170h, still filled and inconsistent).
- **Fixed in data:** the placeholder 3s. Each class gets one stat line, copied from the deckbuilder's class baselines where it has one. Crown tunes them in play.
- **The class kit is not a field here.** The class picks it (`DA_Loadout_ClassDefaults`, found by class through `UTrialRunSubsystem::FindClassDefaults`).

### The starting asset (`UInventoryData`, kept, cut down)

| Field | New or kept | What it holds |
|---|---|---|
| `StartingGear` | **new**, type `FClassDefaultLoadout` (the kit's own struct, reused) | The starting worn gear as a list of pieces: each `FClassDefaultPiece` has `Piece`, `OffHand`, `SkillPicks` and the new `Crystal` (below), plus `ActiveIndex` and `Apparel`. It is minted into the pool once (a new game or a new recruit) and worn like the kit (D-221 (1)). |
| `StartingEvolution` | **new**, `UEvolutionItemData*` | The character's own evolution (it becomes `FWornGear::EvolutionID`). |
| `InnateSpells`, `BDSpellPools` | **moved here** from `FSavedLoadout` | The character's own spells (§5). |
| `CrystalStock` | kept | The starting crystal stock, `{crystal kind, tier}: count` (§2). |
| `Spells`, `Abilities` | kept | The learned skills (§5). |
| `EvolutionEquipment` | kept | Owned evolutions besides the worn one. |
| `Weapons`, `Rings` | kept, meaning narrowed | Owned **spare** pieces; a piece in `StartingGear` is not listed again. |

**New on `FClassDefaultPiece`: `Crystal` (`FCrystalId`, the same key `CrystalStock` uses; empty = an empty socket).**
- The crystal minted into the piece's socket. This is how the clean set starts with "Fire Ring", "Water Ring" and so on, without baking a crystal into the ring asset.
- The class kit can use it too, or leave it empty.

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

### Gear (`UEquipmentDataBase`, `UWeaponData`, `URingData`)

**Gone from every piece:**
- `AttachedItem`, the baked crystal. A piece's socket is filled by `StartingGear`'s `Crystal`, or in play.
- `DefaultSpells` on weapons. A weapon's skills are its `WeaponAttack` and the `PresetAbilities` it allows.
- `DefaultAbilities`, already OBSOLETE.

**Rings keep `PresetSpells`.**
- These are the ring's built-in spells, filling its spell slots until a pick replaces one (`FRingLoadoutEntry.cpp:121-135`, D-145). On a locked ring they are the only spells.
- Today they are copied from `DefaultSpells` (`FRingInventoryEntry.cpp:37`), so `DefaultSpells` goes and `PresetSpells` is the one list.

**Every weapon must have:**
- a valid `WeaponType` (the Gauntlets asset has `EWeaponType_MAX` today);
- a `WeaponAttack`;
- a `WeaponStance` that matches its type;
- `Handling` and a `DualWieldStance` when it can pair;
- a mesh that is **in git**;
- a name with no typo.

### What an adapter (ring) can carry: it keeps its own stats and effects, like a weapon

An adapter is a full gear piece, and all of this **stays** in the new shape. Only the baked crystal (`AttachedItem`) and `DefaultSpells` go.

**From `UEquipmentDataBase`, shared with weapons:**
- `BaseStatBonus` (`FEquipmentStatBonus`): Raw Damage, Spell Damage, Efficiency, Status Multiplier, Crit Damage, Spell Speed, Defense, Action Speed, Reflex, Max HP and the rest.
  - It is scaled by the ring's own tier (`LoadoutComponent.cpp:3931-3988`).
  - Rolled pieces add `GeneratedStatBonus` (`bRandomGenerateOnPickup`).
- `BaseResistance` and `GeneratedResistance`.
- `ReferencedEffects`: the effects tree. The Equipment screen shows it through `AddEffects` (`LoadoutV2Model.cpp:213`), using `GetStartingEffects` and `GetConditionalEffects`, for weapons and adapters alike.
- The bound data: `BoundOwner`, `BoundEffects`, `BoundMemories`, `BoundQuests`, `Memory`, `Conditions`.
- `Requirements`.
- The infusion settings: `bImmuneToInfusion`, `InfusionStatusMultiplier`.
- `bLockSkills`, `SourceTags`, `Tier`, `Name` and `Description`.

**From `URingData` itself:**
- `Form` (Ring, Pistol, Trainer), `FormMesh` and `FormCastMontage`;
- `PresetSpells` and `bSpellsLocked`;
- `InfusionMode`;
- the ring mesh and its rotation.

### The shield as its own piece (the smallest correct change)

**Today:** there is no shield item. `EWeaponWieldMode::OffHandShield` on the sword asset bakes the shield mesh into the sword (`LeftHandSkeletalMesh`), and `EWeaponType` has no Shield.

**The change:**
1. **Add `EWeaponType::Shield`**, appended last so the saved values of the other types don't move.
   - A shield is **off hand only**: `Handling` is `SingleOnly` and it has no `WeaponAttack`.
   - Its mesh is its own `WeaponSkeletalMesh`.
2. **The weapon-set rule** (`WeaponSetRules.cpp`, D-209 pairs) accepts a Shield as the `OffHand` of a one-handed main hand. The pair attacks with the main hand's attack and stance.
3. **The held type:** a main hand with a Shield off hand reads "Sword and Shield" (`WoRGameNames::GetHeldWeaponTypeName` gets the off hand's type).
4. **The mesh:** the weapon mesh component shows the off-hand piece's own mesh in the left hand. To check: a D-209 pair may already do this (`WeaponMeshComponent.cpp:102`).
5. **Retire the old setup:** `EWeaponWieldMode::OffHandShield` and the left-hand mesh fields are flagged OBSOLETE, then deleted once no asset uses them.

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
| `DA_Weapon_Sword` | Sword, `SingleOnly` | `DA_Sword_Attack_LateralCut` / `DA_Sword_Stance_Basic` | `SK_Sword_1H_Fantasy_01` (StylizedWeapons) | "Arming Sword" | F |
| `DA_Weapon_Shield` | **Shield** (new), off hand only | none (the sword attacks) | `SK_Shield_Fantasy_01` (StylizedWeapons) | "Round Shield" | F |
| `DA_Weapon_Axe_Hand` | Axe, `SingleOrDual` | `DA_Axe_Attack_OverheadChop` / dual stance `DA_Stance_DualSword_Basic` until an axe stance exists | an axe mesh from StylizedWeapons (the editor worker picks; the current one is in `Imports/`, out of git) | "Hand Axe" | F |
| `DA_Weapon_Axe_Bearded` | Axe, `SingleOrDual` | as above | a second axe mesh | "Bearded Axe" | F |
| `DA_Ring_Adapter` | Ring form, **empty**, unlocked | `PresetSpells`: `DA_Spell_Missile`, `DA_Spell_Storm`, `DA_Spell_Wave` | one ring mesh | (shown by socket) | F |
| `DA_Evo_Fire_Phoenix` | Garnet (Fire), Balanced | — | — | "Phoenix" | E |
| `DA_Evo_Wind_Gale` | Emerald (Wind), Balanced | — | — | "Gale" | E |

**One adapter asset, five pieces.**
- The five rings are told apart by their crystal's element (Crown), so they are five owned pieces of **one** empty adapter, each with a different crystal in `StartingGear`'s `Crystal`. They read "Fire Ring", "Water Ring", "Lightning Ring", "Earth Ring" and "Wind Ring" (D-223, B-514b).
- The empty adapter reads "Adapter Ring". No ring has an element in its own name.
- The earlier idea of one sub-stat per ring is dropped.

**Every ring is its own piece (Crown: "each one is unique, i.e. we can have 2 Lightning adapters").** Each owned ring is a separate instance with its own `PersistentID`. Two rings may carry the same element, and both must work as separate entries everywhere. They are keyed by the piece id, or by their place in the list, and never by element, name or asset. Today:

| Where | Keyed by | Two Lightning rings |
|---|---|---|
| Worn gear | `FWornGear::Equipment[].GearID`, the piece id (`WornGear.h`, `GetPieceIDs`) | separate |
| Saved presets | `FSavedCustomLoadout::AdapterInstances`, piece ids matched index for index with `Adapters` (`WoRSaveTypes.h:624`) | separate |
| Equipment list | `FLoadoutV2Entry::AdapterIndex`, the place in the ring loadout (`LoadoutV2Model.cpp:61`) | separate |
| Quick strip | the place in the list (`FWheelPick::FieldAttackIndex`, Q-503 (3)); the ring slices are built per ring loadout entry | separate |
| Gear mirroring | by `PersistentID`; a second copy of the same asset arriving only raises the duplicate notice (`InventoryComponent.cpp:2291`) | separate |
| **Crystal wear: which ring a cast wears** | **the ring asset** (`LoadoutComponent.cpp:3034-3048`). B-310 picks the adapter in use first, so the cast's ring is worn, but any other match falls to the first copy in the list. | **to check in stage 2**; if wear lands on the wrong copy, key it by piece id |
| "Owns this ring" checks | the asset (`InventoryComponent.cpp:1736-1739`) | fine: these only ask "has one" |

**Telling two same-named rings apart in the UI (the simplest):** a small number, shown only where two entries in the same list would read the same. The first keeps its name and the next reads "Lightning Ring 2".
- It is worked out at display time from the list's order, and nothing is stored.
- The tier badge doesn't work alone, since two F rings look the same.
- The stat line doesn't work alone, since the clean set's rings share one, and it is only in the details, not on the strip tile.
- This is how Diablo's and Path of Exile's stash and equipment lists handle duplicates: identical item names, with the slot telling them apart. A number is the shortest form of that on a strip with no slot labels.
- **Proposal:** one helper in `WoRGameNames` that numbers repeats in a list of names, used by the quick strip and the Equipment list (stage 2).

**The adapter's stat line (shared by all five):**
- One `BaseStatBonus` line, `BonusSpellDamage`, at the value the pool's existing F rings use. The editor worker reads it off one of them; no new number.
- This exercises the adapter's own stat on the Equipment detail, the tier scaling and the combat sum.
- Its built-in spells are generic pool spells, which take the socket's element, so every ring has a spell in its one F slot.
- Effects (`ReferencedEffects`) are left empty in the clean set; the evolutions test effects instead.

**The two axes are two assets**, so the pair reads "Hand Axe and Bearded Axe" (not "Axe and Axe"), and the held type reads "Dual Axes".

**What the two evolutions give.** Their element comes from their crystal kind: Phoenix is Fire, Gale is Wind.
- Today all 27 evolution assets carry the same placeholder field bonuses (sprint +15% and field-skill EP −25%).
- **Proposal:** Phoenix takes the EP −25% and Gale the sprint +15%, so the two read differently in the field.
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

These are spares on top of the crystals already in the rings, so a ring can be re-socketed and a weapon socket filled in play. More come from `wor.Economy.GiveCrystal <Type> <Tier> [Count]`.

### Change: the class kits (`DA_Loadout_ClassDefaults`)

| Class | Kit |
|---|---|
| Generic | `DA_Weapon_Sword` (with `DA_Weapon_Shield` as its off hand) and one `DA_Ring_Adapter` |
| Caster | one `DA_Ring_Adapter` |
| Resonator | `DA_Weapon_Sword` and three `DA_Ring_Adapter` |

Every kit is F tier, and the kit's `Crystal` is left empty: the trial's kit starts empty (D-221), and its stock fills it.

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

**Not deleted yet: the pool's 35 weapons, 42 rings and 27 evolutions.** Merchants, loot and rest spots may reference them, so they get their own clean-up job with a reference check. Deleting `AttachedItem` empties their baked crystals anyway: they become empty rings named by their socket.

## 3. The code side

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
- **Debug only:** `LoadoutComponent.cpp:1834` (`AutoPopulateLoadout`).

**The change (it reads the new shape only, with no converter):**
1. **Seed from `StartingGear`.**
   - `DrawSharedGear`'s seed mints `UInventoryData::StartingGear` into the pool and wears it. It reuses `PoolClassDefaults.cpp`'s `MintKitPiece` and `WearClassDefaultKit`, generalised from "the class kit" to "a `FClassDefaultLoadout`".
   - Each piece's `Crystal` is minted into its socket. The crystal comes with the piece; it is not taken from the stock.
   - `InitializeFromInventoryAsset` stops reading `SavedLoadouts`.
2. **Pieces start empty.** `CreateFromWeapon` and `CreateFromRing` stop copying `AttachedItem` and `DefaultSpells`, and `EconomyService` drops those price terms. The class kit already clears them (`PoolClassDefaults.cpp:71/86`).
3. **The shield** (§1).
4. **Delete the fields** in §1 once nothing reads them. Old `.uasset`s that still carry them load fine: the engine skips a property that no longer exists.
5. **Saves:** raise the minimum save version, so older saves are wiped with a warning (D-060).

**Not in this plan (restructure stages 2 and 4):** the runtime `FCombatLoadout` primary and secondary fields, the round trip in `CreateFromWornGear`, and `ApplyDraft` writing the old runtime fields.

**A gap on the Equipment screen: the add-an-adapter flow.**
- Adding an adapter copies the piece's own sockets (`LoadoutV2ScreenWidget.cpp:2398`).
- While it is a new draft, its socket slots can't be filled (`:2333`, "A Edit on it changes its sockets").
- So "pick the empty ring, then the crystal" takes two visits.
- **Fix (stage 2):** the add window's socket slots list `BuildSocketChoices`, as the Edit window does.

## 4. A ring's tier (D-223, Q-223a): the logic that already exists

**What the code does today: the ring's tier and its crystal's tier are never combined.** Each mechanic reads one or the other. The split is by role: **the crystal decides the spell side, and the ring decides the piece side.**

| Use | Reads | Where |
|---|---|---|
| Spell slots | **the crystal's tier**, which replaces the ring's (F1 E2 D3 C4 B5 A6 S6). An evolution uses its own tier; only an empty or non-gem socket falls back to the ring's tier. | `CrystalEffectTable.h:778-799` `ResolveSpellSlotCap`, called by `FRingLoadoutEntry.cpp:30-32/54-56`. CL-25, Q-147, D-039 |
| Output: the tier-gap damage multiplier | **the crystal's tier** as the channel; the gap is the spell's tier − the channel's. An empty socket means no channel, ×1.00. The ring's tier is never read. | `ActionExecutor.cpp:429-500` `ResolveChannelTier`. ED-11 |
| Output: Crystal Amp | **the crystal's tier** | `LoadoutComponent.cpp:3970` `GetCrystalTierAmp`. ED-50d |
| Output: the adapter's stat lines | **the ring's tier** (`GetTierPowerMultiplier`) | `LoadoutComponent.cpp:3931-3988` |
| Durability and wear | **the crystal** against the action: max durability by crystal tier (F30 … S100), wear 7% per tier gap for spells. A ring has no durability of its own. | `CrystalManager.cpp:105-121`. ER-34 |
| A broken crystal | leaves F-tier spells only, keyed on the spell's tier | `SourceBreakRules.h:13-17`. D-083 |
| Combat Tier | **the ring's tier** (socketed crystals aren't counted, for weapons either), averaged with the other gear, the evolution and the skills, rounded down | `CombatTier.cpp:44-55`, `:16-31`. D-118, Q-325 |
| Buy price | **the sum** of the ring's tier price, the attachment's price and the bundled spells | `EconomyService.cpp:920-934, 1002`. ER-22 |
| Repair | the crystal's tier price | `GearRepair.cpp:35-45` |
| Dismantle; the lost-run tier drop | the ring's tier; the ring drops 3 tiers, its crystal doesn't | `EconomyService.cpp:475`. ER-65g, D-219 |

**There is no existing ring-plus-crystal rule.** There is no "crystal capped by its ring" and no "effective tier = lower of the two"; CL-13b caps S crystals per loadout, not per ring. The only average in the code is D-211's pair of weapons, which is weapon with weapon.

**What D-223's shown tier would have to match, and where it disagrees.** The average of the ring and its crystal, rounded down, matches the mechanics only when the two tiers are equal, or the ring is empty:
- **Spell slots and spell output follow the crystal alone.** An F ring with an S gem would show C and have 6 slots; an S ring with an F gem would show C and have 1.
- **The adapter's stat lines, Combat Tier and dismantle follow the ring alone.**
- **The price is a sum, not an average.**
- **An empty ring agrees:** it shows its own tier, its slots come from its own tier, and it has no channel.

**Recommendation for Q-223a: (a), the shown tier is display only, and every mechanic keeps the tier it reads today.**
- This reuses the existing logic as it stands. The code already splits the job sensibly: the crystal is the spell's power and wear, and the ring is the piece's stats and value.
- Making one averaged number drive both would change balance rules that are already ruled (CL-25, ED-11, ER-34, D-118) and tested.
- To keep the shown tier honest, the Equipment detail shows the parts next to it, "C (ring F · crystal S)". The spell-slot line already counts from the crystal.
- This is how Monster Hunter decorations work: a decoration's level is its own, the armour's rarity is the armour's, and the screen shows both. Players read the slot from the decoration.
- **The one place that could reasonably adopt the average is Combat Tier**, since it is itself an "overall" number: one line at `CombatTier.cpp:51`. But it ignores weapon crystals too, so changing only rings would make the two inconsistent. **Recommendation: leave it.**
- The Equipment detail shows the asset's tier, not the owned piece's (`LoadoutV2Model.cpp:312-314`). That is a bug, fixed in stage 2 either way.

## 5. Should skills stay on the character? (Crown's question)

The worry: if the loadout goes, wouldn't every skill move? **Mostly no.** Most skills already live on their owner, and only the authored home changes, because the authored `FSavedLoadout` goes. The runtime homes stay.

| Skill | Authored today | Lives at runtime | What the new design implies | Recommendation | Cost of moving it |
|---|---|---|---|---|---|
| **Innate spells** | `FSavedLoadout::InnateSpells` (`FSavedLoadout.h:162`), one copy per loadout; must also be listed in `UInventoryData::Spells` | `FWornGear::InnateSpells` (`WornGear.h:76`), `FCombatLoadout::InnateSpells`; saved in `FSavedWornGear` | They belong to the character's class and element, always on (D-040), and the restructure plan already says "the character's, not the kit's". | **Keep on the character:** authored on `UInventoryData::InnateSpells`, runtime on the worn-gear record (which is per character) as today. | Small: one authored field moved, the runtime unchanged. |
| **Learned spells and abilities** | `UInventoryData::Spells` / `Abilities` (`InventoryData.h:99/105`) | `UInventoryComponent::Spells` / `Abilities`; the pool's `CharacterSkills` (`PoolSubsystem.h:786`); saved in `FSavedCharacterSkills` | Nothing changes: they are what the character knows, the list the Equipment screen picks from. | **Keep on the character** (D-111: "learned spells and abilities stay per character"; Q-091). | None. Moving them to pieces would undo D-111 and rework the pool, the saves and the Equipment screen: large, and no gain. |
| **Weapon built-ins** | `UWeaponData::WeaponAttack`, `PresetAbilities`, `bAbilitiesLocked` (`WeaponData.h:60-68`) | read from the asset | They are what the weapon *is*. | **Keep on the piece asset.** | None. |
| **Weapon skill picks** | old: `FSavedLoadout::PrimaryWeaponAbilities` / `SecondaryWeaponAbilities` (Resonator, Test); new: `FClassDefaultPiece::SkillPicks` | already **on the owned piece**: `FWeaponInventoryEntry::SkillPicks` (`:101`), saved in `FSavedGearDetail`; old copies in `FWeaponLoadoutEntry::AssignedAbilities` and `FLoadoutSlot::AssignedAbilities` | Picks are a choice made on that piece (D-145, CL-20c: "lives on the item"). | **Keep on the piece** (the owned entry). Author them in `StartingGear`'s `SkillPicks`. The old loadout copies go with restructure stage 2. | Small for the authored side. The loadout copies are the readers' job (the LA-5 / CS-12 duplication). |
| **Adapter (ring) spells** | built-ins: `URingData::PresetSpells` (`RingData.h:51`) and the duplicate `DefaultSpells`; old picks: `FResonatorRingSlot::AssignedSpells` | on the owned piece: `FRingInventoryEntry::SpellPicks` (`:93`) and `AssignedSpells` (`:88`); dropped when the crystal comes off (`FRingInventoryEntry.h:196`) | Same as weapons: built-ins on the asset, picks on the piece (CL-20c, D-145). | **Built-ins on the asset** (`PresetSpells` only; `DefaultSpells` deleted). **Picks on the piece.** The clean set authors no picks: the built-ins fill the F slot. | Small. |
| **Evolution spells** | `FSavedLoadout::EvolutionSpells` (`:252`), empty on every asset | `FWornGear::EvolutionSpells` (`WornGear.h:68`) for the character's evolution seat; an evolution in an adapter uses the ring's `SpellPicks` | Picked in play. Nothing is authored today, so there is nothing to move. | **The seat's picks stay on the worn-gear record** (the character's). An evolution in an adapter keeps using the ring's picks. **No authored field.** | None. |
| **Broken Darkness spell pools** | `FSavedLoadout::BDSpellPools` (`:169`), empty on every asset | `FWornGear::BDSpellPools` (`WornGear.h:79`), `FCombatLoadout::BDSpellPools` | Broken Darkness is a character trait (the BD flag), so its pools are the character's (CL-08, CL-35). | **Keep on the character:** authored on `UInventoryData::BDSpellPools`, runtime on the worn-gear record. | Small: one authored field moved. |
| **Field skills** | nothing per character: `FFieldSkillChoice` defaults to Innate / Dash (`FieldSkillTypes.h:217-230`); unlocks come from `USpellData::FieldAura`; evolution field bonuses are on `EvolutionItemData.h:109-118` | `FWornGear::WheelPick`, `FPartyMember::FieldSkill` / `FieldAttackIndex`; the field attack entries are rebuilt from the worn pieces on every read (`GetFieldAttackEntries`) | Already derived from what is worn (D-215, D-217, D-222). | **No change.** | None. |

**So the moves are only from the authored loadout to the character's starting asset.** Innate spells and BD pools go to `UInventoryData`; weapon picks go to `StartingGear`'s `SkillPicks`. Nothing moves at runtime in this plan.

**Design choices for Crown (not technical):**
- **Who owns a piece's picks?**
  - The older Q-085 A says the loadout's picks are the loadout's own. D-145 and CL-20c say a pick lives on the item, and the code follows D-145: `GiveGear` keeps the picks when a piece changes hands (`PoolSubsystem.cpp:520-534`).
  - **Recommendation:** D-145 stands and Q-085 A is noted as superseded. This is the Diablo / Path of Exile way: a socketed gem's setup goes with the item.
- **A given piece carrying a pick the new owner hasn't learned.** It is not checked today. Two options:
  - (a) the pick plays, because it is the item's;
  - (b) it shows but is greyed until learned.
  - **Recommendation: (a)** for weapon `PresetAbilities` picks, since they come from the weapon itself, and **(b)** for a pick from the old owner's learned list.
- **Innate spells: authored list, or derived from the element?** Today they are a hand list per character. They could instead be "every spell of the innate element up to the character's tier".
  - **Recommendation:** keep the authored list for now (the smallest change), and revisit with the skill tree.

## 6. The stages (each ends in a PIE check)

**Each stage runs the same way:**
1. Code commits of at most 3 files each.
2. Build, the full suite and `wor-reviewer`.
3. Merge to dev.
4. The editor worker makes the stage's assets.
5. A short sheet in `docs/PIE/`.
6. **Crown's PIE pass before the next stage starts.**

Until stage 4, the new pieces are added as spares to the current played character's inventory, so they can be tried on today's character.

| Stage | Code | Editor | Crown checks in PIE (its own sheet) |
|---|---|---|---|
| **1. Sword and shield** | `EWeaponType::Shield`, the set rule, the held name (+ spec); the left-hand mesh if needed | `DA_Weapon_Sword`, `DA_Weapon_Shield` | Wear the sword with the shield in the off hand: it reads "Arming Sword and Round Shield", the type "Sword and Shield", the shield is in the left hand, and the battle attack is the sword's. |
| **2. The empty adapter** | sockets in the add window (§3 gap); the detail shows the owned piece's tier and the shown tier per Q-223a; the repeat-number helper for same-named entries (+ specs, including two Lightning rings) | `DA_Ring_Adapter` (the stat line and built-ins as in §2); **three** copies as spares on the played character | Add the adapter: it reads "Adapter Ring", F, with its stat line. Socket a Garnet in the same window: "Fire Ring", one spell slot, its built-in Missile. The quick strip shows "Fire Ring". **Two Lightning rings:** add two more adapters and socket a Citrine in each. The list and the strip show "Lightning Ring" and "Lightning Ring 2" as separate entries. Each one can be picked on the strip and casts on its own. A spell pick on one doesn't change the other. A cast wears only the ring it went through. Save a preset, reload, and both are still there. |
| **3. Pieces start empty; the axes** | stop copying `AttachedItem` and `DefaultSpells`; prices (+ spec fixes) | `DA_Weapon_Axe_Hand`, `DA_Weapon_Axe_Bearded` | The axes pair reads "Hand Axe and Bearded Axe", "Dual Axes". A ring bought from the merchant arrives empty and is priced without a crystal. |
| **4. The Generic starts from `StartingGear`** | the `UInventoryData` fields and `FClassDefaultPiece::Crystal`; generalise the kit minting; the `DrawSharedGear` seed (+ spec) | `DA_Character_Generic`, `DA_Inventory_Generic_Start`, the Gale evolution; point the played character at it | A new game: the Generic wears the sword and shield, the axes are spare, it has an **Earth Ring** socketed and Gale in its seat, and the crystal stock is in the pouch. |
| **5. The rest of the cast** | none expected (the fields exist); small fixes if PIE finds any | the Refractor, Resonator, BD and Reality characters with their inventories, the Phoenix evolution, the two enemies, `DA_Loadout_ClassDefaults` | Each character's start. The Resonator's Water, Lightning and Wind Rings. The Refractor's Fire innate and Phoenix. BD's pools. A trial starts on the kit (empty sockets). A fight against each enemy. |
| **6. Delete the old** | delete the §1 fields and their readers; raise the minimum save version | the reference check, then delete the §2 delete list | A new game still works, an old save is wiped with a warning, data validation shows no new errors, and a battle plays. |

## Questions for Crown

1. **The cast and gear list (§2):** OK as listed? It includes:
   - the item names "Arming Sword", "Round Shield", "Hand Axe", "Bearded Axe", "Phoenix" and "Gale";
   - which character holds which ring and evolution;
   - the adapter's one shared stat line (Spell Damage at the existing F value).
2. **Q-223a, a ring's tier (§4):** the shown tier is display only, and the mechanics keep reading the crystal for spells and wear and the ring for stats and Combat Tier (recommended)? Or should one of them follow the shown tier?
3. **Two same-named rings:** a repeat number ("Lightning Ring 2"), shown only where a list has the same name twice (recommended, §2)?
4. **§5's design choices:** a pick stays with its item (D-145 over Q-085 A); a given piece's picks from the old owner's learned list are greyed until learned; innate spells stay a hand list for now.
