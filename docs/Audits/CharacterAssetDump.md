# Character asset dump (read-only)

Dumped through the editor connection on 2026-11-15: every property of each asset, read with `list_properties` + `get_properties` (real property names, camelCase as the editor reports them). Nothing was changed or saved. Reference values show the asset path without the `.Name` suffix; `None` = empty.

Coverage: 16 character templates (13 `DA_Character_*` incl. the "Character Template" Crown plays and "Generic Lord", plus 3 `DA_Enemy_*`), the cosmetics asset, 13 inventories, all 11 loadout/evolution equipment assets under `Data/Characters`, 35 weapons (all `DA_Weapon_*`, the old `Skills/Weapons` ones, the baked loadout weapons), 42 rings, 27 evolutions, `DA_SAS_Attack_LightHit` and the two stance assets. Spells and abilities are only listed as references.

**Mesh and anim class are not on the character template** (`UCharacterData` has no such field); the only visual data is `cosmetics` (all 16 point to the same `DA_Cosmetics_Default`). **Crystals are not assets**: Garnet, Sapphire, Citrine etc. exist as enum values in `attachedItem`, `crystalStock` and `equippedItems`, and as the crystal rings `DA_Ring_Garnet`, `DA_Ring_Sapphire`, `DA_Ring_Citrine` (and the other six). **Missing-asset references could not be detected**: a broken reference reads back as `None`, so empty reference fields below may hide one.

## Anything that looks odd

**Characters**
- `DA_Character_Template` and `DA_Character_GenericLord` are identical (same stats, same inventory `DA_Inventory_Generic`); `DA_Enemy_Generic` is the same again with `origin: Enemy`. The enemy and player Generic share one inventory asset.
- All characters have `maxHealth` 3, `maxEnergy` 3 and the other substats at 3 or 0: placeholder numbers. Only Generic has `worldBodyLevel` 7 and `actionSpeed` 40; the Earth, Fire, Light, Lightning, Reality, Void, Wind and Water casters are 3/3/3; Darkness, Broken Darkness and Resonator are 1/1/1.
- `turnSpeed` is marked OBSOLETE (B-170h) but is still filled: 5 on Generic/enemy-Generic, 3 on Resonator, Earth, Fire, Light, Lightning, Reality, Void, Wind; 0 on Darkness, BD, Water. Inconsistent across casters.
- `DA_Enemy_Darkness` uses `DA_Test_Inventory` (a test asset), not a Darkness inventory. `DA_Inventory_Default` is empty (no weapons, rings, loadouts) and no character here points to it.
- `DA_Character_BrokenDarknessLord`: `innateElement` Darkness + `bBrokenDarknessInnate` true; its inventory has 6 innate spells against 12 on `DA_Inventory_Darkness`.

**Inventories and loadouts**
- Two slot styles side by side: Generic and Test use `primarySlotType` / `secondarySlotType` Weapon with `primaryWeapon` + `secondaryWeapon`; casters use `primarySlotType` Ring + `primaryRing` and secondary None; Resonator uses `primaryWeapon` + a filled `equippedRings` list (3 entries, the Light ring twice, `ringInstance` all zero GUID). Casters' `equippedRings` are empty.
- Old-looking slot fields are filled: `primary*` / `secondary*` on every loadout; `primaryWeaponAbilities` (6) on the Resonator loadout and `primaryWeaponAbilities` 2 / `secondaryWeaponAbilities` 3 on the Test loadout but empty on the Generic loadout, while all three inventories hold the same kind of `abilities` list (Generic 5, Test 5, Resonator 6 = AssignedAbilities).
- `bDSpellPools` empty everywhere; `apparel` empty everywhere; `evolutionSpells` empty everywhere.
- Casters other than Fire and Water use the generic pool spells (`DA_Spell_Missile`, `Storm`, `Wave`...) as `innateSpells`; Fire and Water use their own element spells (`DA_Spells_Fire_*`, `DA_Spells_Water_*`) and have more of them (Fire 16, Water 17 vs 12).
- `DA_Inventory_Darkness.evolutionEquipment` holds one empty (`None`) entry; BD holds two evolution items, Water one.
- Loadout names differ: `loadout0`, `Loadout0`, `Unnamed Loadout` (Resonator).
- `equippedItems` on the Generic loadout has three entries with `quantity` 0 (Sapphire F, Garnet B, Amber F); `crystalStock` differs between `DA_Inventory_Generic` (5 entries, includes Iolite C) and `DA_Test_Inventory` (4 entries).

**Weapons, rings, evolutions**
- Baked crystals (`attachedItem`): `DA_Equipment_Generic_SAS` Garnet F, `DA_Equipment_Generic_DualAxe` Sapphire F, the three Resonator rings Opal / Amethyst / Emerald A_Tier, `DA_Equipment_Fire_Ring_Reality` Iolite F; every crystal ring in the pool bakes its own crystal at E_Tier. Every other weapon and ring shows `crystalType` Garnet / `crystalTier` F_Tier even with `kind` None (a leftover default, not a real crystal).
- Weapon spell lists: `DA_Equipment_Generic_SAS` and `..._DualAxe` carry `defaultSpells` (3, the SAS ones are Fire spells BlazingSpeed / FireBall / EmberShot); the old `Skills/Weapons` set (`DA_Weapon_SAS`, `DA_GreatSword`, `DA_Weapon_Dagger`, `DA_Weapon_DualAxe`) has 3 `defaultSpells` + 6 `presetAbilities`; `DA_Weapon_Gauntlets` has 4 spells and `bAbilitiesLocked` true. `DA_Equpment_Darkness_ERing_Fire` has `defaultSpells` 3 although `attachedItem.kind` is None.
- `DA_Weapon_Gauntlets` has `weaponType: EWeaponType_MAX` (invalid) while `DA_Weapon_CombatGauntlets` is Gauntlets. `DA_Weapon_Dagger` is typed Sword with the sword LateralCut attack.
- Nine pool weapons have no `weaponAttack`, no stance and no abilities: `TwinHandaxes`, `TwinTomahawks`, `SaiPair`, `TwinDaggers`, `TwinKris`, `TwinKatanas`, `TwinSabres`, `TwinShortswords`, `CombatGauntlets`. The other 18 pool weapons have 2 `presetAbilities` but most have `weaponStance` None (only `DA_Weapon_BattleAxe` has one, and it is the sword stance).
- `DA_Equipment_Generic_DualAxe` is `weaponType` Axe with the sword stance `DA_Stance_DualSword_Basic`. `DA_Equipment_Generic_SAS` is `weaponType` Sword with `wieldMode` OffHandShield; there is no sword-and-shield weapon type.
- Evolution items live under misleading "equipment" names: `DA_BD_Equpment_Eearth` is Citrine / Lightning A_Tier; `DA_Equipment_Elightning` has `itemName` "Garnet (A)" but `crystalType` Citrine; `DA_Equpment_Darkness_Ecrystal_Fire` is Garnet / Fire. Typos in names: `Equpment` (x5), `Eearth`, `Guantlets`.
- Test gear (`wor.Debug.TestGear`): `DA_Weapon_ArmingSword` E_Tier, `weaponAttack` OverheadCut, no stance, 2 preset abilities, no baked crystal; `DA_Ring_Generic` E_Tier, no baked crystal, no spells.
- `/Game/Data/Testing/RefinedCrystals/DA_Test_EvoCrystal_Water` is in the asset registry but cannot be loaded (stale or broken asset).
- Cosmetics (`DA_Cosmetics_Default`, shared by all): `ringSwitchMontage` = the parry montage; both item-use montages = `AS_HittingTheBall2_Montage` (a sports animation); `perfectGuardSound` = an engine editor sound (`CompileSuccess_Cue`); `perfectGuardFlashVFX` = the engine `RadialBurst` template.

---

## DA_Character_Template

`/Game/Data/Characters/DA_Character_Template`

- name: Character Template
- characterClass: Generic
- innateElement: Generic
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: None
- origin: Player
- inventory: /Game/Data/Characters/Data/Generic/Inventory/DA_Inventory_Generic
- worldMindLevel: 1
- worldBodyLevel: 7
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 40
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 5
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: -5, water: -5, earth: -5, wind: -5, light: -5, darkness: -5, lightning: -5, void: -5, reality: -5, slash: 15, pierce: 15, impact: 15, ice: 0}

## DA_Character_GenericLord

`/Game/Data/Characters/Data/Generic/DA_Character_GenericLord`

- name: Generic Lord
- characterClass: Generic
- innateElement: Generic
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_090_Plank
- origin: Player
- inventory: /Game/Data/Characters/Data/Generic/Inventory/DA_Inventory_Generic
- worldMindLevel: 1
- worldBodyLevel: 7
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 40
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 5
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: -5, water: -5, earth: -5, wind: -5, light: -5, darkness: -5, lightning: -5, void: -5, reality: -5, slash: 15, pierce: 15, impact: 15, ice: 0}

## DA_Character_ResonatorLord

`/Game/Data/Characters/Data/Resonator/DA_Character_ResonatorLord`

- name: Resonator Lord
- characterClass: Resonator
- innateElement: Generic
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Tools/T_064_HeavyConveyorPart
- origin: Player
- inventory: /Game/Data/Characters/Data/Resonator/Inventory/DA_Inventory_Resonator
- worldMindLevel: 1
- worldBodyLevel: 1
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: -15, pierce: -15, impact: -15, ice: 0}

## DA_Character_BrokenDarknessLord

`/Game/Data/Characters/Data/Caster/BrokenDarkness/DA_Character_BrokenDarknessLord`

- name: Broken Darkness Lord
- characterClass: Caster
- innateElement: Darkness
- bBrokenDarknessInnate: true
- infusionMode: Balanced
- description: 
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Tools/T_056_EnergyBattery
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/BrokenDarkness/Inventory/DA_Inventory_BD
- worldMindLevel: 1
- worldBodyLevel: 1
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 0
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: -5, water: -5, earth: -5, wind: -5, light: -5, darkness: -5, lightning: -5, void: -5, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_DarknessLord

`/Game/Data/Characters/Data/Caster/Darkness/DA_Character_DarknessLord`

- name: Darkness Lord
- characterClass: Caster
- innateElement: Darkness
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Tools/T_062_TurboEngine
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Darkness/Inventory/DA_Inventory_Darkness
- worldMindLevel: 1
- worldBodyLevel: 1
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 0
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: -15, darkness: 15, lightning: 0, void: -10, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_EarthLord

`/Game/Data/Characters/Data/Caster/Earth/DA_Character_EarthLord`

- name: Earth Lord
- characterClass: Caster
- innateElement: Earth
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Earth Lord does not dodge. The mountain waits, endures, and buries whatever is still standing when the dust settles.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_076_GoldOre
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Earth/Inventory/DA_Inventory_Earth
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: -5, earth: 15, wind: 0, light: 0, darkness: 0, lightning: -15, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_FireLord

`/Game/Data/Characters/Data/Caster/Fire/DA_Character_FireLord`

- name: Fire Lord
- characterClass: Caster
- innateElement: Fire
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Fire Lord's mastery lies not in the strength of each flame, but in casting twice as often for half the cost. Overwhelms through volume and speed, never giving enemies a moment to breathe. Glass cannon? No. Death by a thousand cuts? Absolutely.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_079_Ruby
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Fire/Inventory/DA_Inventory_Fire
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 15, water: -15, earth: 0, wind: -5, light: 0, darkness: 0, lightning: 0, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_LightLord

`/Game/Data/Characters/Data/Caster/Light/DA_Character_LightLord`

- name: Light Lord
- characterClass: Caster
- innateElement: Light
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Light Lord blinds before it burns. Every spell lands a heartbeat before you believe it was cast.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_094_CrystalShard
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Light/Inventory/DA_Inventory_Light
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: 15, darkness: -15, lightning: 0, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_LightningLord

`/Game/Data/Characters/Data/Caster/Lightning/DA_Character_LightningLord`

- name: Lightning Lord
- characterClass: Caster
- innateElement: Lightning
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Lightning Lord strikes once, precisely, and rarely needs a second strike.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_073_CopperOre
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Lightning/Inventory/DA_Inventory_Lightning
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 15, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 15, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_RealityLord

`/Game/Data/Characters/Data/Caster/Reality/DA_Character_RealityLord`

- name: Reality Lord
- characterClass: Caster
- innateElement: Reality
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Reality Lord rewrites the rules mid-fight. The battlefield you entered is never the one you leave.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Currencies/T_151_PremiumToken
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Reality/Inventory/DA_Inventory_Reality
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 10, water: 10, earth: 10, wind: 10, light: 10, darkness: 10, lightning: 10, void: -10, reality: 0, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_VoidLord

`/Game/Data/Characters/Data/Caster/Void/DA_Character_VoidLord`

- name: Void Lord
- characterClass: Caster
- innateElement: Void
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Void Lord unmakes. Armor, wards, certainty — everything is negotiable at the edge of nothing.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_082_Amethyst
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Void/Inventory/DA_Inventory_Void
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: -10, darkness: -10, lightning: 0, void: 0, reality: -10, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_WindLord

`/Game/Data/Characters/Data/Caster/Wind/DA_Character_WindLord`

- name: Wind Lord
- characterClass: Caster
- innateElement: Wind
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Wind Lord is never where the blow lands. Strike after strike arrives from angles that should not exist.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_080_Emerald
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Wind/Inventory/DA_Inventory_Wind
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 2
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: -15, wind: 15, light: 0, darkness: 0, lightning: 0, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Character_WaterLord

`/Game/Data/Characters/Data/Caster/Water/DA_Character_WaterLord`

- name: Water Lord
- characterClass: Caster
- innateElement: Water
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: Master of control and adaptation. Water Lord flows between offense and defense, using superior durability and tactical positioning to outlast opponents. Excels at disruption and sustained pressure through balanced stats and versatile spell selection.
- portrait: /Game/Imports/IdleTycoonUpgradeIconsMegaPack/Textures/Resources/T_081_Sapphire
- origin: Player
- inventory: /Game/Data/Characters/Data/Caster/Water/Inventory/DA_Inventory_Water
- worldMindLevel: 3
- worldBodyLevel: 3
- worldSpiritLevel: 3
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 0
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: -15, water: 15, earth: 0, wind: 0, light: 0, darkness: 0, lightning: -15, void: 0, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Enemy_Generic

`/Game/Data/Enemy/DA_Enemy_Generic`

- name: Generic Lord
- characterClass: Generic
- innateElement: Generic
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: None
- origin: Enemy
- inventory: /Game/Data/Characters/Data/Generic/Inventory/DA_Inventory_Generic
- worldMindLevel: 1
- worldBodyLevel: 7
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 40
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 5
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: -5, water: -5, earth: -5, wind: -5, light: -5, darkness: -5, lightning: -5, void: -5, reality: -5, slash: 15, pierce: 15, impact: 15, ice: 0}

## DA_Enemy_Darkness

`/Game/Data/Enemy/DA_Enemy_Darkness`

- name: Darkness Lord
- characterClass: Caster
- innateElement: Darkness
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: None
- origin: Enemy
- inventory: /Game/Data/Characters/Data/DA_Test_Inventory
- worldMindLevel: 1
- worldBodyLevel: 1
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 0
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: -15, darkness: 15, lightning: 0, void: -10, reality: -15, slash: 0, pierce: 0, impact: 0, ice: 0}

## DA_Enemy_Resonator

`/Game/Data/Enemy/DA_Enemy_Resonator`

- name: Resonator Lord
- characterClass: Resonator
- innateElement: Generic
- bBrokenDarknessInnate: false
- infusionMode: Balanced
- description: 
- portrait: None
- origin: Enemy
- inventory: /Game/Data/Characters/Data/Resonator/Inventory/DA_Inventory_Resonator
- worldMindLevel: 1
- worldBodyLevel: 1
- worldSpiritLevel: 1
- efficiency: 3
- spellDamage: 0
- critDamage: 3
- spellSpeed: 3
- defense: 3
- actionSpeed: 0
- rawDamage: 3
- maxHealth: 3
- enemyTier: F_Tier
- enemyCameraHeight: ByRank
- reflex: 0
- maxEnergy: 3
- resistance: 3
- turnSpeed: 3
- luck: 0
- statusMultiplier: 0
- cosmetics: /Game/Data/Characters/Data/DA_Cosmetics_Default
- resistanceProfile: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: -15, pierce: -15, impact: -15, ice: 0}

## DA_Cosmetics_Default

`/Game/Data/Characters/Data/DA_Cosmetics_Default`

- dodgeLeftMontage: /Game/Imports/Animations/Dodge/AN_Dodge_L_4_Montage
- dodgeRightMontage: /Game/Imports/Animations/Dodge/AN_Dodge_R_4_Montage
- blockMontage: /Game/Imports/Animations/Dodge/AN_Block_F_8_Montage
- parryMontage: /Game/Imports/Animations/Dodge/AN_Parry_F_1_Montage
- perfectGuardFlashVFX: /Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst
- perfectGuardSound: /Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue
- guardSparkVFX: None
- weaponGuardParryEffectVFX: None
- unarmedGuardParryEffectVFX: None
- dodgeWhiffVFX: None
- unarmedStance: /Game/Data/Characters/Stances/DA_MF_Idle_Default
- ringSwitchMontage: /Game/Imports/Animations/Dodge/AN_Parry_F_1_Montage
- infusionDisplay: /Game/Data/Characters/InfusionDisplays/Body/DA_Infusion_Body_1
- itemUseSelfMontage: /Game/Imports/Animations/SportAnimations/Animations/AS_HittingTheBall2_Montage
- itemUseTargetMontage: /Game/Imports/Animations/SportAnimations/Animations/AS_HittingTheBall2_Montage
- equippedWeatherVariant: /Game/SoStylized/Environment/Sky/Weather/DA_Weather_Rain_Light

## DA_Inventory_Default

`/Game/Data/Characters/Data/DA_Inventory_Default`

- weapons: []
- rings: []
- crystalStock: {}
- evolutionEquipment: []
- spells: []
- abilities: []
- savedLoadouts: []
- defaultActiveLoadoutIndex: 0

## DA_Test_Inventory

`/Game/Data/Characters/Data/DA_Test_Inventory`

- weapons: [/Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS, /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DualAxe/DA_Equipment_Generic_DualAxe]
- rings: []
- crystalStock: {(Type=Garnet,Tier=B_Tier,StoneStat=None): 5, (Type=Sapphire,StoneStat=None): 5, (Type=Emerald,Tier=A_Tier,StoneStat=None): 3, (Type=Emerald,Tier=S_Tier,StoneStat=None): 3}
- evolutionEquipment: []
- spells: []
- abilities: [/Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_CrossCut, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_smash, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_Whirlwind, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder]
- savedLoadouts: [{loadoutName: loadout0, requiredClass: Generic, bShowPrimary: true, innateSpells: [], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Weapon, primaryWeapon: /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: None, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder], evolutionSpells: [], secondarySlotType: Weapon, secondaryWeapon: /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DualAxe/DA_Equipment_Generic_DualAxe, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [/Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_CrossCut, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_smash, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_Whirlwind], equippedItems: [{crystalId: {type: Sapphire, tier: F_Tier, stoneStat: None}, quantity: 0}, {crystalId: {type: Garnet, tier: B_Tier, stoneStat: None}, quantity: 0}, {crystalId: {type: Emerald, tier: S_Tier, stoneStat: None}, quantity: 3}, {crystalId: {type: Amber, tier: F_Tier, stoneStat: None}, quantity: 0}], bAutoEquipItemsOnCombatStart: true}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Generic

`/Game/Data/Characters/Data/Generic/Inventory/DA_Inventory_Generic`

- weapons: [/Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS, /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DualAxe/DA_Equipment_Generic_DualAxe]
- rings: []
- crystalStock: {(Type=Garnet,Tier=B_Tier,StoneStat=None): 5, (Type=Sapphire,StoneStat=None): 5, (Type=Emerald,Tier=A_Tier,StoneStat=None): 3, (Type=Emerald,Tier=S_Tier,StoneStat=None): 3, (Type=Iolite,Tier=C_Tier,StoneStat=None): 3}
- evolutionEquipment: []
- spells: []
- abilities: [/Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_CrossCut, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_smash, /Game/Data/Skills/Weapons/DualAxe/Abilities/DA_DualAxe_Abilities_Whirlwind, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder]
- savedLoadouts: [{loadoutName: loadout0, requiredClass: Generic, bShowPrimary: true, innateSpells: [], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Weapon, primaryWeapon: /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: None, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: Weapon, secondaryWeapon: /Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DualAxe/DA_Equipment_Generic_DualAxe, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [{crystalId: {type: Sapphire, tier: F_Tier, stoneStat: None}, quantity: 0}, {crystalId: {type: Garnet, tier: B_Tier, stoneStat: None}, quantity: 0}, {crystalId: {type: Emerald, tier: S_Tier, stoneStat: None}, quantity: 3}, {crystalId: {type: Amber, tier: F_Tier, stoneStat: None}, quantity: 0}, {crystalId: {type: Iolite, tier: C_Tier, stoneStat: None}, quantity: 3}], bAutoEquipItemsOnCombatStart: true}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Resonator

`/Game/Data/Characters/Data/Resonator/Inventory/DA_Inventory_Resonator`

- weapons: [/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Sword/DA_Resonator_Equpment_Sword]
- rings: [/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Light, /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Void, /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Wind]
- crystalStock: {}
- evolutionEquipment: []
- spells: []
- abilities: [/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_HeavyStrike, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_QuickStrike, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Thrust, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Whirlwind]
- savedLoadouts: [{loadoutName: Unnamed Loadout, requiredClass: Resonator, bShowPrimary: true, innateSpells: [], bDSpellPools: [], equippedRings: [{ring: /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Light, ringInstance: 00000000-0000-0000-0000-000000000000, assignedSpells: []}, {ring: /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Light, ringInstance: 00000000-0000-0000-0000-000000000000, assignedSpells: []}, {ring: /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Wind, ringInstance: 00000000-0000-0000-0000-000000000000, assignedSpells: []}], apparel: [], apparelInstances: [], primarySlotType: Weapon, primaryWeapon: /Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Sword/DA_Resonator_Equpment_Sword, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: None, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_HeavyStrike, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_QuickStrike, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Thrust, /Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Whirlwind], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_BD

`/Game/Data/Characters/Data/Caster/BrokenDarkness/Inventory/DA_Inventory_BD`

- weapons: []
- rings: [/Game/Data/Pool/Darkness/Rings/DA_Ring_Onyx, /Game/Data/Pool/Darkness/Rings/DA_Ring_Shadowveil, /Game/Data/Pool/Darkness/Rings/DA_Ring_Voidtouch, /Game/Data/Pool/Darkness/Rings/DA_Ring_Nightbrand]
- crystalStock: {}
- evolutionEquipment: [/Game/Data/Characters/Data/Caster/BrokenDarkness/Inventory/Loadouts/Loadout0/DA_BD_Equpment_Eearth, /Game/Data/Characters/Data/Caster/Darkness/Inventory/Loadouts/Loadout0/DA_Equpment_Darkness_Ecrystal_Fire]
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Darkness/Rings/DA_Ring_Onyx, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Darkness

`/Game/Data/Characters/Data/Caster/Darkness/Inventory/DA_Inventory_Darkness`

- weapons: []
- rings: [/Game/Data/Pool/Darkness/Rings/DA_Ring_Onyx, /Game/Data/Pool/Darkness/Rings/DA_Ring_Shadowveil, /Game/Data/Pool/Darkness/Rings/DA_Ring_Voidtouch, /Game/Data/Pool/Darkness/Rings/DA_Ring_Nightbrand]
- crystalStock: {}
- evolutionEquipment: [None]
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Darkness/Rings/DA_Ring_Onyx, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Earth

`/Game/Data/Characters/Data/Caster/Earth/Inventory/DA_Inventory_Earth`

- weapons: []
- rings: [/Game/Data/Pool/Earth/Rings/DA_Ring_Amber, /Game/Data/Pool/Earth/Rings/DA_Ring_Boulder, /Game/Data/Pool/Earth/Rings/DA_Ring_Mountainheart, /Game/Data/Pool/Earth/Rings/DA_Ring_Stoneskin]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Earth/Rings/DA_Ring_Amber, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Fire

`/Game/Data/Characters/Data/Caster/Fire/Inventory/DA_Inventory_Fire`

- weapons: []
- rings: [/Game/Data/Pool/Fire/Rings/DA_Ring_Garnet, /Game/Data/Pool/Fire/Rings/DA_Ring_Emberflame, /Game/Data/Pool/Fire/Rings/DA_Ring_Wildfire, /Game/Data/Pool/Fire/Rings/DA_Ring_Phoenix]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_BurningRage, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_CometStrike, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_EmberShot, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FlameWave, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_Inferno, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_Orb, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_SearingStrike, /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_HeartOfFire_, /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_Immolation, /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_SearingWeakness, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Cauterize, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_LifeSpark, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_OverHeat, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_PhoenixRebirth, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_ResilientFlame, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Warmth]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_BurningRage, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_CometStrike, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_EmberShot, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FlameWave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_Orb, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_HeartOfFire_, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_Immolation, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_SearingWeakness, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Cauterize, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_LifeSpark, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_OverHeat, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_PhoenixRebirth, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_ResilientFlame, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Warmth, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Fire/Rings/DA_Ring_Garnet, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Light

`/Game/Data/Characters/Data/Caster/Light/Inventory/DA_Inventory_Light`

- weapons: []
- rings: [/Game/Data/Pool/Light/Rings/DA_Ring_Opal, /Game/Data/Pool/Light/Rings/DA_Ring_Sunspear, /Game/Data/Pool/Light/Rings/DA_Ring_Radiance, /Game/Data/Pool/Light/Rings/DA_Ring_Ascendant]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Light/Rings/DA_Ring_Opal, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Lightning

`/Game/Data/Characters/Data/Caster/Lightning/Inventory/DA_Inventory_Lightning`

- weapons: []
- rings: [/Game/Data/Pool/Lightning/Rings/DA_Ring_Citrine, /Game/Data/Pool/Lightning/Rings/DA_Ring_Thunderbolt, /Game/Data/Pool/Lightning/Rings/DA_Ring_Sparklight, /Game/Data/Pool/Lightning/Rings/DA_Ring_Chainstorm]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Lightning/Rings/DA_Ring_Citrine, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Reality

`/Game/Data/Characters/Data/Caster/Reality/Inventory/DA_Inventory_Reality`

- weapons: []
- rings: [/Game/Data/Pool/Reality/Rings/DA_Ring_Iolite, /Game/Data/Pool/Reality/Rings/DA_Ring_Sundered, /Game/Data/Pool/Reality/Rings/DA_Ring_Truthforge, /Game/Data/Pool/Reality/Rings/DA_Ring_Anchor]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Reality/Rings/DA_Ring_Iolite, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Void

`/Game/Data/Characters/Data/Caster/Void/Inventory/DA_Inventory_Void`

- weapons: []
- rings: [/Game/Data/Pool/Void/Rings/DA_Ring_Amethyst, /Game/Data/Pool/Void/Rings/DA_Ring_Hollowbrand, /Game/Data/Pool/Void/Rings/DA_Ring_Rift, /Game/Data/Pool/Void/Rings/DA_Ring_Nullforge]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Void/Rings/DA_Ring_Amethyst, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Wind

`/Game/Data/Characters/Data/Caster/Wind/Inventory/DA_Inventory_Wind`

- weapons: []
- rings: [/Game/Data/Pool/Wind/Rings/DA_Ring_Emerald, /Game/Data/Pool/Wind/Rings/DA_Ring_Cyclone, /Game/Data/Pool/Wind/Rings/DA_Ring_Zephyr, /Game/Data/Pool/Wind/Rings/DA_Ring_Gale]
- crystalStock: {}
- evolutionEquipment: []
- spells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Wind/Rings/DA_Ring_Emerald, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Inventory_Water

`/Game/Data/Characters/Data/Caster/Water/Inventory/DA_Inventory_Water`

- weapons: []
- rings: [/Game/Data/Pool/Water/Rings/DA_Ring_Sapphire, /Game/Data/Pool/Water/Rings/DA_Ring_Tidecaller, /Game/Data/Pool/Water/Rings/DA_Ring_Highwater, /Game/Data/Pool/Water/Rings/DA_Ring_Deepcurrent]
- crystalStock: {}
- evolutionEquipment: [/Game/Data/Characters/Data/Caster/Water/Inventory/Loadouts/Loadout0/DA_Equipment_Elightning]
- spells: [/Game/Data/Spells/Water/Destruction/DA_Spells_Water_AbsoluteZero, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_Avalanche, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_Blizzard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceLance, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave, /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_AdaptiveForm, /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_FlowingGrace, /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_GlacialMastery, /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_Pressure, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_AquaticRebirth, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Cleanse, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_HealingSpring, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Hydration, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Refresh, /Game/Data/Spells/Water/Restoration/DA_Spells_Water_TidalRestoration]
- abilities: []
- savedLoadouts: [{loadoutName: Loadout0, requiredClass: Caster, bShowPrimary: true, innateSpells: [{spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_AbsoluteZero, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_Avalanche, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_Blizzard, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceLance, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_AdaptiveForm, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_FlowingGrace, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_GlacialMastery, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Enhancement/DA_Spells_Water_Pressure, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_AquaticRebirth, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Cleanse, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_HealingSpring, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Hydration, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_Refresh, instanceId: 00000000-0000-0000-0000-000000000000}, {spell: /Game/Data/Spells/Water/Restoration/DA_Spells_Water_TidalRestoration, instanceId: 00000000-0000-0000-0000-000000000000}], bDSpellPools: [], equippedRings: [], apparel: [], apparelInstances: [], primarySlotType: Ring, primaryWeapon: None, primaryWeaponInstance: 00000000-0000-0000-0000-000000000000, primaryOffHandWeapon: None, primaryOffHandInstance: 00000000-0000-0000-0000-000000000000, primaryRing: /Game/Data/Pool/Water/Rings/DA_Ring_Sapphire, primaryRingInstance: 00000000-0000-0000-0000-000000000000, primaryEvolution: None, primaryEvolutionInstance: 00000000-0000-0000-0000-000000000000, primaryWeaponAbilities: [], evolutionSpells: [], secondarySlotType: None, secondaryWeapon: None, secondaryWeaponInstance: 00000000-0000-0000-0000-000000000000, secondaryOffHandWeapon: None, secondaryOffHandInstance: 00000000-0000-0000-0000-000000000000, secondaryWeaponAbilities: [], equippedItems: [], bAutoEquipItemsOnCombatStart: false}]
- defaultActiveLoadoutIndex: 0

## DA_Equpment_Resonator_Ring_Light

`/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Light`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Evolved Fire Ring
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Opal, crystalTier: A_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Equpment_Resonator_Ring_Void

`/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Void`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Evolved Fire Ring
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amethyst, crystalTier: A_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Equpment_Resonator_Ring_Wind

`/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Rings/DA_Equpment_Resonator_Ring_Wind`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Evolved Fire Ring
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Emerald, crystalTier: A_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Resonator_Equpment_Sword

`/Game/Data/Characters/Data/Resonator/Inventory/Loadout/Loadout0/Sword/DA_Resonator_Equpment_Sword`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_LateralCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WrathCut, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WhirlwindSlash, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HundredCuts, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_Riposte, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_FellCrescent, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HeadsmansCut]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Sword_Stance_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/CombatMasterAnimBundle/Weapon/Meshes/SM_Sword_DSAS
- weaponSkeletalMesh: None
- meshRotation: {pitch: 180, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Primary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Equipment_Generic_SAS

`/Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Skills/Weapons/SwordAndShield/LightHit/DA_SAS_Attack_LightHit
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Sword_Stance_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: None
- weaponSkeletalMesh: /Game/StylizedWeapons/WeaponPack1/Meshes/Item/Weapons/Sword/SK_Sword_1H_Fantasy_01
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: /Game/StylizedWeapons/WeaponPack1/Meshes/Item/Weapons/Shield/SK_Shield_Fantasy_01
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: /Game/StylizedWeapons/WeaponPack1/Materials/Instances/Item/Weapon/MI_Sword_1H_Fantasy_01_Bl
- leftHandMaterialOverride: /Game/StylizedWeapons/WeaponPack1/Materials/Instances/Item/Weapon/MI_Shield_Fantasy_01_Bl
- sourceTags: {gameplayTags: []}
- name: Generic Primary Weapon
- tier: F_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_EmberShot]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Equipment_Generic_DualAxe

`/Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DualAxe/DA_Equipment_Generic_DualAxe`

- weaponType: Axe
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_OverheadChop
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Stance_DualSword_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Secondary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Sapphire, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 1, bonusStatusMultiplier: 1, bonusCritDamage: 0, bonusSpellSpeed: 1, bonusDefense: 1, bonusActionSpeed: 1, bonusReflex: 0, bonusMaxHP: 1, bonusMaxEnergy: 1, bonusResistance: 1, bonusTurnSpeed: 1, bonusLuck: 0, bonusMindModifierPercent: 2, bonusBodyModifierPercent: 2, bonusSpiritModifierPercent: -0.07916629314422607, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_BD_Equpment_Eearth

`/Game/Data/Characters/Data/Caster/BrokenDarkness/Inventory/Loadouts/Loadout0/DA_BD_Equpment_Eearth`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Citrine
- displayElement: Lightning
- tier: A_Tier
- itemName: Citrine (A)
- description: A masterwork citrine crystal. Restores 85% of the target's max energy; overloads the user with Lightning status buildup.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 80
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 10, bonusBodyModifierPercent: 5, bonusSpiritModifierPercent: -5, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Equpment_Darkness_Ecrystal_Fire

`/Game/Data/Characters/Data/Caster/Darkness/Inventory/Loadouts/Loadout0/DA_Equpment_Darkness_Ecrystal_Fire`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Garnet
- displayElement: Fire
- tier: A_Tier
- itemName: Garnet (A)
- description: A masterwork garnet crystal. Deals 180 fire damage.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 80
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 10, bonusBodyModifierPercent: 5, bonusSpiritModifierPercent: -5, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Equipment_Elightning

`/Game/Data/Characters/Data/Caster/Water/Inventory/Loadouts/Loadout0/DA_Equipment_Elightning`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Citrine
- displayElement: Lightning
- tier: A_Tier
- itemName: Garnet (A)
- description: A masterwork garnet crystal. Deals 180 fire damage.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 80
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 10, bonusBodyModifierPercent: 5, bonusSpiritModifierPercent: -5, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Equpment_Darkness_ERing_Fire

`/Game/Data/Characters/Data/Caster/Darkness/Inventory/Loadouts/Loadout0/DA_Equpment_Darkness_ERing_Fire`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Evolved Fire Ring
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Cauterize, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_CometStrike]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Equipment_Fire_Ring_Reality

`/Game/Data/Characters/Data/Caster/Fire/Inventory/Loadouts/Loadout0/DA_Equipment_Fire_Ring_Reality`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Evolved Fire Ring
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: Crystal, crystalType: Iolite, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, /Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Cauterize, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_CometStrike]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_ArmingSword

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_ArmingSword`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_OverheadCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_CrossCut, /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_GuardPiercer]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Knight_s_Straight_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Arming Sword
- tier: E_Tier
- description: Classic knightly one-handed sword, balanced for cutting and thrusting.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Generic

`/Game/Data/Pool/Generic/Rings/DA_Ring_Generic`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Generic Ring
- tier: E_Tier
- description: A basic ring.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Garnet

`/Game/Data/Pool/Fire/Rings/DA_Ring_Garnet`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Garnet Ring
- tier: E_Tier
- description: A basic fire ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Garnet, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Sapphire

`/Game/Data/Pool/Water/Rings/DA_Ring_Sapphire`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Sapphire Ring
- tier: E_Tier
- description: A basic water ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Sapphire, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Citrine

`/Game/Data/Pool/Lightning/Rings/DA_Ring_Citrine`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Citrine Ring
- tier: E_Tier
- description: A basic lightning ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Citrine, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_SAS

`/Game/Data/Skills/Weapons/DA_Weapon_SAS`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_LateralCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WrathCut, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WhirlwindSlash, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HundredCuts, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_Riposte, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_FellCrescent, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HeadsmansCut]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Sword_Stance_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/CombatMasterAnimBundle/Weapon/Meshes/SM_Sword_DSAS
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 180}
- leftHandStaticMesh: /Game/CombatMasterAnimBundle/Weapon/Meshes/SM_Shield_DSAS
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 15, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Primary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Fire/Enhancement/DA_Spells_Fire_BlazingSpeed, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_EmberShot]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_GreatSword

`/Game/Data/Skills/Weapons/DA_GreatSword`

- weaponType: Greatsword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Attacks/DA_Greatsword_Attack_GreatCleave
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_ColossalCleave, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_BladeCyclone, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_RelentlessHews, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_WardingSweep, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_ChargingCleave, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_DecapitatingCleave]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Stance_DualSword_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/CombatMasterAnimBundle/Weapon/Meshes/SM_GreatSword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 100, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 180, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Secondary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 1, bonusStatusMultiplier: 1, bonusCritDamage: 0, bonusSpellSpeed: 1, bonusDefense: 1, bonusActionSpeed: 1, bonusReflex: 0, bonusMaxHP: 1, bonusMaxEnergy: 1, bonusResistance: 1, bonusTurnSpeed: 1, bonusLuck: 0, bonusMindModifierPercent: 2, bonusBodyModifierPercent: 2, bonusSpiritModifierPercent: -0.07916629314422607, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Dagger

`/Game/Data/Skills/Weapons/DA_Weapon_Dagger`

- weaponType: Sword
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_LateralCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WrathCut, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WhirlwindSlash, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HundredCuts, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_Riposte, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_FellCrescent, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HeadsmansCut]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Stance_DualSword_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Greatsword_of_the_Conqueror
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Greatsword_of_the_Conqueror
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Secondary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 1, bonusStatusMultiplier: 1, bonusCritDamage: 0, bonusSpellSpeed: 1, bonusDefense: 1, bonusActionSpeed: 1, bonusReflex: 0, bonusMaxHP: 1, bonusMaxEnergy: 1, bonusResistance: 1, bonusTurnSpeed: 1, bonusLuck: 0, bonusMindModifierPercent: 2, bonusBodyModifierPercent: 2, bonusSpiritModifierPercent: -0.07916629314422607, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_DualAxe

`/Game/Data/Skills/Weapons/DA_Weapon_DualAxe`

- weaponType: Axe
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_OverheadChop
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_MightyChop, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_WhirlingAxe, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_RainOfChops, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_WardingChop, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_ChargingChop, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_BeheadingChop]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Stance_DualSword_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- weaponSkeletalMesh: None
- meshRotation: {pitch: 180, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 180, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Generic Secondary Weapon
- tier: E_Tier
- description: 
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Spells/Water/Enhancement/DA_Spells_Water_ChillingTouch, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_IceShard, /Game/Data/Spells/Water/Destruction/DA_Spells_Water_TidalWave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 1, bonusStatusMultiplier: 1, bonusCritDamage: 0, bonusSpellSpeed: 1, bonusDefense: 1, bonusActionSpeed: 1, bonusReflex: 0, bonusMaxHP: 1, bonusMaxEnergy: 1, bonusResistance: 1, bonusTurnSpeed: 1, bonusLuck: 0, bonusMindModifierPercent: 2, bonusBodyModifierPercent: 2, bonusSpiritModifierPercent: -0.07916629314422607, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Gauntlets

`/Game/Data/Skills/Weapons/Gauntlets/DA_Weapon_Gauntlets`

- weaponType: EWeaponType_MAX
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Skills/Weapons/Gauntlets/Attacks/DA_Attack_Guantlets_Jab
- presetAbilities: [/Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_Lunge, /Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_OneTwo, /Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_QuestionKick, /Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_SideKick, /Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_Uppercut, /Game/Data/Skills/Weapons/Gauntlets/Abilities/DA_Gauntlets_Abilities_RoundHouse]
- bAbilitiesLocked: true
- weaponStance: /Game/Data/Skills/Weapons/Gauntlets/Stances/DA_Stance_Gauntlets_Default
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: None
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Gauntlets
- tier: E_Tier
- description: Fight with your bare hands, enhanced with combat training
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [None, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_EmberShot, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall, /Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FlameWave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 1, requiredWorldBody: 1, requiredWorldSpirit: 1}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Katana

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Katana`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_RisingCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_MastersWrath, /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_Impale]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/CombatMasterAnimBundle/Weapon/Meshes/SM_Katana
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Katana
- tier: E_Tier
- description: Curved single-edged blade of Eastern origin.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_DaneAxe

`/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_DaneAxe`

- weaponType: Axe
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_ReapingSwing
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_WhirlingAxe, /Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_BeheadingChop]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Giant_Great_Ax_of_the_Fallen
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Dane Axe
- tier: E_Tier
- description: Heavy two-handed war axe — devastating cleaving blows.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Gladius

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Gladius`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Attacks/DA_Sword_Attack_InsideThrust
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_Puncture, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_GuardBreaker]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Short_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Gladius
- tier: E_Tier
- description: Short Roman blade — close-quarters stabbing and cutting.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_ArmingSword_Shield

`/Game/Data/Pool/Generic/Weapons/SwordAndShield/DA_Weapon_ArmingSword_Shield`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_LateralCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_Riposte, /Game/Data/Pool/Generic/Weapons/Sword/Impact/Abilities/DA_Sword_Ability_ConcussiveBlow]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Knight_s_Straight_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Shield_1
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Arming Sword & Kite Shield
- tier: E_Tier
- description: Arming sword paired with a kite shield — balanced sword-and-board defence.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Gladius_Scutum

`/Game/Data/Pool/Generic/Weapons/SwordAndShield/DA_Weapon_Gladius_Scutum`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Attacks/DA_Sword_Attack_RisingThrust
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_Pin, /Game/Data/Pool/Generic/Weapons/Sword/Impact/Abilities/DA_Sword_Ability_BullRush]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Short_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Shield_3
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Gladius & Scutum
- tier: E_Tier
- description: Gladius paired with a scutum — legionary shield-wall fighting.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Claymore

`/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Claymore`

- weaponType: Greatsword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Attacks/DA_Greatsword_Attack_GreatCleave
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_DecapitatingCleave, /Game/Data/Pool/Generic/Weapons/Greatsword/Impact/Abilities/DA_Greatsword_Ability_Skullbreaker]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Claymore
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Claymore
- tier: E_Tier
- description: Scottish two-hander with sloping quillons — reach and cutting power.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_SAS_Attack_LightHit

`/Game/Data/Skills/Weapons/SwordAndShield/LightHit/DA_SAS_Attack_LightHit`

- requiredWeaponType: Sword
- requiredWieldMode: Single
- element: Generic
- bIsAttack: true
- physicalDamageType: Slash
- executionMontage: None
- sourceTags: {gameplayTags: []}
- name: Light Hit
- description: Hit your opponent with a basic slash
- icon: None
- cameraSequence: None
- cameraTuning: {style: Default, distanceScale: 1, side: Right, shakeScale: 1, bFollowProjectile: true, bNamedAttack: false}
- executionType: Melee
- executionRange: 100
- hitCount: 1
- damageSplit: []
- defaultDifficulty: {parry: Inherit, dodge: Inherit, block: Inherit}
- statusBuildup: 5
- bImmuneToInfusion: false
- referencedEffects: []
- tier: F_Tier
- targetType: Enemy
- targetCount: Single
- baseDamage: 10
- statScaling: [{stat: None, tier: C}]
- ignoreDefensePercent: 0
- baseEnergyCost: 0
- actionCost: 1
- activationDelay: 0
- deliveryType: Projectile
- projectileSpeed: 1500
- castArray: [{label: , deliveryType: Projectile, projectileClass: /Script/world_of_refraction, projectileSpeed: 1500, size: 1, trail: /Game/Imports/Big_Pack_Magic_VFX/Pack_Effects_1/Air_Magic/VFX_Niagara/NS_Air_Magic_Slash2.NS_Air_Magic_Slash2, visualScale: 1, count: 1, burstInterval: 0.15000000596046448, bIgnoreDamage: false, bIgnoreStatus: false, bInterruptable: false, difficulty: {parry: Inherit, dodge: Inherit, block: Inherit}}]
- skillMontage: /Game/Data/Skills/Weapons/SwordAndShield/LightHit/AM_SAS_LightHit
- fieldMontage: /Game/Data/Skills/Weapons/SwordAndShield/LightHit/AM_SAS_LightHit_Field.AM_SAS_LightHit_Field
- ritualCastMontage: None
- returnMontage: /Game/Data/Skills/Weapons/SwordAndShield/LightHit/AM_SAS_LightHit_Return
- baseAnimSpeed: 1
- vFXArray: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}

## DA_Sword_Stance_Basic

`/Game/Data/Skills/Weapons/Sword/Stances/DA_Sword_Stance_Basic`

- stanceName: Sword Default
- description: Default long sword stance stance
- idleAnimMontage: /Game/CombatMasterAnimBundle/Animations/HandsomeSwordV1/Manny_UE5/RootMotion/Idle/Anim_Sword_1H_Attack_Idle
- icon: None

## DA_MF_Idle_Default

`/Game/Data/Characters/Stances/DA_MF_Idle_Default`

- stanceName: MF Idle
- description: Ready for battle.
- idleAnimMontage: /Game/Data/Characters/Stances/AM_MF_Idle_Default
- icon: None

## DA_Ring_Onyx

`/Game/Data/Pool/Darkness/Rings/DA_Ring_Onyx`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Onyx Ring
- tier: E_Tier
- description: A basic darkness ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Onyx, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Amber

`/Game/Data/Pool/Earth/Rings/DA_Ring_Amber`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Amber Ring
- tier: E_Tier
- description: A basic earth ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amber, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Opal

`/Game/Data/Pool/Light/Rings/DA_Ring_Opal`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Opal Ring
- tier: E_Tier
- description: A basic light ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Opal, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Iolite

`/Game/Data/Pool/Reality/Rings/DA_Ring_Iolite`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Iolite Ring
- tier: E_Tier
- description: A basic reality ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Iolite, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Amethyst

`/Game/Data/Pool/Void/Rings/DA_Ring_Amethyst`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Amethyst Ring
- tier: E_Tier
- description: A basic void ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amethyst, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Emerald

`/Game/Data/Pool/Wind/Rings/DA_Ring_Emerald`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Emerald Ring
- tier: E_Tier
- description: A basic wind ring.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Emerald, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Emberflame

`/Game/Data/Pool/Fire/Rings/DA_Ring_Emberflame`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Emberflame Ring
- tier: E_Tier
- description: A ring wreathed in low-burning coals.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Garnet, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Sunspear

`/Game/Data/Pool/Light/Rings/DA_Ring_Sunspear`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Sunspear Ring
- tier: E_Tier
- description: A shaft of pure light.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Opal, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Thunderbolt

`/Game/Data/Pool/Lightning/Rings/DA_Ring_Thunderbolt`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Thunderbolt Ring
- tier: E_Tier
- description: One strike, one clear sky.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Citrine, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Shadowveil

`/Game/Data/Pool/Darkness/Rings/DA_Ring_Shadowveil`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Shadowveil Ring
- tier: E_Tier
- description: The strike comes from where light does not reach.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Onyx, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Hollowbrand

`/Game/Data/Pool/Void/Rings/DA_Ring_Hollowbrand`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Hollowbrand Ring
- tier: E_Tier
- description: The void bends around a chosen point.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amethyst, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Cyclone

`/Game/Data/Pool/Wind/Rings/DA_Ring_Cyclone`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Cyclone Ring
- tier: E_Tier
- description: The wind gathers into fury.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Emerald, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Tidecaller

`/Game/Data/Pool/Water/Rings/DA_Ring_Tidecaller`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Tidecaller Ring
- tier: E_Tier
- description: The tide answers the wearer.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Sapphire, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Voidtouch

`/Game/Data/Pool/Darkness/Rings/DA_Ring_Voidtouch`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Voidtouch Ring
- tier: E_Tier
- description: The shadow lingers on what it touches.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Onyx, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Sundered

`/Game/Data/Pool/Reality/Rings/DA_Ring_Sundered`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Sundered Ring
- tier: E_Tier
- description: Cut cleanly through what should not be.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Iolite, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Rift

`/Game/Data/Pool/Void/Rings/DA_Ring_Rift`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Rift Ring
- tier: E_Tier
- description: A tear that answers when opened.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amethyst, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Truthforge

`/Game/Data/Pool/Reality/Rings/DA_Ring_Truthforge`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Truthforge Ring
- tier: E_Tier
- description: The truth cuts through the lie.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Iolite, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Highwater

`/Game/Data/Pool/Water/Rings/DA_Ring_Highwater`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Highwater Ring
- tier: E_Tier
- description: Water rises to mend what is broken.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Sapphire, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Mend, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Cleanse]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Radiance

`/Game/Data/Pool/Light/Rings/DA_Ring_Radiance`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Radiance Ring
- tier: E_Tier
- description: Light gathers around the wearer, and their kin rise with it.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Opal, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Aura, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Ascendant

`/Game/Data/Pool/Light/Rings/DA_Ring_Ascendant`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Ascendant Ring
- tier: E_Tier
- description: Light lifts the weary — none fall while it burns.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Opal, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_EnhancementAura, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Zephyr

`/Game/Data/Pool/Wind/Rings/DA_Ring_Zephyr`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Zephyr Ring
- tier: E_Tier
- description: The wind's own quickness.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Emerald, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Sparklight

`/Game/Data/Pool/Lightning/Rings/DA_Ring_Sparklight`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Sparklight Ring
- tier: E_Tier
- description: Small strikes, swift and endless.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Citrine, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Deepcurrent

`/Game/Data/Pool/Water/Rings/DA_Ring_Deepcurrent`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Deepcurrent Ring
- tier: E_Tier
- description: The current pulls swift and true.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Sapphire, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Missile, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Gale

`/Game/Data/Pool/Wind/Rings/DA_Ring_Gale`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Gale Ring
- tier: E_Tier
- description: The wind sweeps everything before it.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Emerald, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Grace]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Nightbrand

`/Game/Data/Pool/Darkness/Rings/DA_Ring_Nightbrand`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Nightbrand Ring
- tier: E_Tier
- description: Shadow sharpened into a weapon.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Onyx, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Bolt, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Boulder

`/Game/Data/Pool/Earth/Rings/DA_Ring_Boulder`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Boulder Ring
- tier: E_Tier
- description: One blow, the weight of the earth.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amber, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Lance, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Chainstorm

`/Game/Data/Pool/Lightning/Rings/DA_Ring_Chainstorm`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Chainstorm Ring
- tier: E_Tier
- description: Lightning arcs from one to the next.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Citrine, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Storm, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Wildfire

`/Game/Data/Pool/Fire/Rings/DA_Ring_Wildfire`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Wildfire Ring
- tier: E_Tier
- description: The flame spreads where it wills.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Garnet, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Mountainheart

`/Game/Data/Pool/Earth/Rings/DA_Ring_Mountainheart`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Mountainheart Ring
- tier: E_Tier
- description: The mountain endures, the wearer endures.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amber, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Restore]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Stoneskin

`/Game/Data/Pool/Earth/Rings/DA_Ring_Stoneskin`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Stoneskin Ring
- tier: E_Tier
- description: Skin as stone, will as bedrock.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amber, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Nullforge

`/Game/Data/Pool/Void/Rings/DA_Ring_Nullforge`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Nullforge Ring
- tier: E_Tier
- description: Void as forge, void as shield.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Amethyst, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Conjuration/DA_Spell_Wave, /Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Phoenix

`/Game/Data/Pool/Fire/Rings/DA_Ring_Phoenix`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Phoenix Ring
- tier: E_Tier
- description: From ash, rise again.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Garnet, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Rally]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Ring_Anchor

`/Game/Data/Pool/Reality/Rings/DA_Ring_Anchor`

- bSpellsLocked: false
- infusionMode: Balanced
- presetSpells: []
- form: Ring
- formMesh: None
- formCastMontage: None
- ringStaticMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- sourceTags: {gameplayTags: []}
- name: Anchor Ring
- tier: E_Tier
- description: The truth holds when all else gives way.
- icon: None
- attachedItem: {kind: Crystal, crystalType: Iolite, crystalTier: E_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: [/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Warding, /Game/Data/Pool/Generic/Spells/Restoration/DA_Spell_Renewal]
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Evo_Darkness_CloakedFist

`/Game/Data/Pool/Darkness/Evolutions/DA_Evo_Darkness_CloakedFist`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Onyx
- displayElement: Darkness
- tier: E_Tier
- itemName: Cloaked Fist
- description: The arm cloaked in shadow — element surges through flesh.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 15, bonusEfficiency: -5, bonusStatusMultiplier: 15, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 5, bonusSpiritModifierPercent: 12, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Darkness_FrenziedFist

`/Game/Data/Pool/Darkness/Evolutions/DA_Evo_Darkness_FrenziedFist`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Onyx
- displayElement: Darkness
- tier: E_Tier
- itemName: Frenzied Fist
- description: The shadow devours the wielder — raw curse.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 21, bonusEfficiency: -20, bonusStatusMultiplier: 21, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: -15, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 8, bonusSpiritModifierPercent: 15, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Darkness_ShadowedFist

`/Game/Data/Pool/Darkness/Evolutions/DA_Evo_Darkness_ShadowedFist`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Onyx
- displayElement: Darkness
- tier: E_Tier
- itemName: Shadowed Fist
- description: Shadow flickers along the arm — controlled draw.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 10, bonusEfficiency: -10, bonusStatusMultiplier: 10, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 3, bonusSpiritModifierPercent: 8, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Earth_LongMile

`/Game/Data/Pool/Earth/Evolutions/DA_Evo_Earth_LongMile`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amber
- displayElement: Earth
- tier: E_Tier
- itemName: Long Mile
- description: Every step doubled — endurance turned burden.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: -15, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Earth_Pace

`/Game/Data/Pool/Earth/Evolutions/DA_Evo_Earth_Pace`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amber
- displayElement: Earth
- tier: E_Tier
- itemName: Pace
- description: Measured stride — every step half its worth.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 12, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Earth_SecondWind

`/Game/Data/Pool/Earth/Evolutions/DA_Evo_Earth_SecondWind`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amber
- displayElement: Earth
- tier: E_Tier
- itemName: Second Wind
- description: The runner's boundless breath — no cost too high.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 15, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Fire_EmberVow

`/Game/Data/Pool/Fire/Evolutions/DA_Evo_Fire_EmberVow`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Garnet
- displayElement: Fire
- tier: E_Tier
- itemName: Ember Vow
- description: Rebirth from the ash — the weapon pays the price.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 8, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Fire_PyreCycle

`/Game/Data/Pool/Fire/Evolutions/DA_Evo_Fire_PyreCycle`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Garnet
- displayElement: Fire
- tier: E_Tier
- itemName: Pyre Cycle
- description: The flame reignites, and those nearby feel the pyre.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: -10, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 15, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Fire_RisingAsh

`/Game/Data/Pool/Fire/Evolutions/DA_Evo_Fire_RisingAsh`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Garnet
- displayElement: Fire
- tier: E_Tier
- itemName: Rising Ash
- description: Ash rises from the fallen — the ember refuses to die.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 15, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Light_AllForOne

`/Game/Data/Pool/Light/Evolutions/DA_Evo_Light_AllForOne`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Opal
- displayElement: Light
- tier: E_Tier
- itemName: All-for-One
- description: Physical immortality — unyielding vessel.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 15, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 15, bonusSpellSpeed: 0, bonusDefense: 15, bonusActionSpeed: 0, bonusReflex: 10, bonusMaxHP: 21, bonusMaxEnergy: 0, bonusResistance: 15, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 15, bonusSpiritModifierPercent: 10, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Light_KingsCrown

`/Game/Data/Pool/Light/Evolutions/DA_Evo_Light_KingsCrown`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Opal
- displayElement: Light
- tier: E_Tier
- itemName: King's Crown
- description: The crown's weight — power fills the vessel until it slows.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 21, bonusSpellDamage: -15, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 21, bonusSpellSpeed: 0, bonusDefense: 21, bonusActionSpeed: -20, bonusReflex: 15, bonusMaxHP: 21, bonusMaxEnergy: 0, bonusResistance: 21, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 15, bonusSpiritModifierPercent: 15, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Light_Legacy

`/Game/Data/Pool/Light/Evolutions/DA_Evo_Light_Legacy`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Opal
- displayElement: Light
- tier: E_Tier
- itemName: Legacy
- description: The inheritance carried forward — measured strength.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 10, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 10, bonusSpellSpeed: 0, bonusDefense: 10, bonusActionSpeed: 0, bonusReflex: 5, bonusMaxHP: 15, bonusMaxEnergy: 0, bonusResistance: 10, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 8, bonusSpiritModifierPercent: 5, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Lightning_GenerationalIdea

`/Game/Data/Pool/Lightning/Evolutions/DA_Evo_Lightning_GenerationalIdea`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Citrine
- displayElement: Lightning
- tier: E_Tier
- itemName: Generational Idea
- description: Thought quickens — body pays a small tax.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 5, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 5, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: -8, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 8, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Lightning_GenerationalProcessing

`/Game/Data/Pool/Lightning/Evolutions/DA_Evo_Lightning_GenerationalProcessing`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Citrine
- displayElement: Lightning
- tier: E_Tier
- itemName: Generational Processing
- description: Thought spans generations — harnessed foresight.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 10, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 10, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 15, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Lightning_GenerationalThought

`/Game/Data/Pool/Lightning/Evolutions/DA_Evo_Lightning_GenerationalThought`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Citrine
- displayElement: Lightning
- tier: E_Tier
- itemName: Generational Thought
- description: Mind races centuries ahead — body left behind.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 15, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 15, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: -20, bonusReflex: -15, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 15, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Reality_BareTruth

`/Game/Data/Pool/Reality/Evolutions/DA_Evo_Reality_BareTruth`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Iolite
- displayElement: Reality
- tier: E_Tier
- itemName: Bare Truth
- description: Truth cuts both ways — no illusion of strength.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 15, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Reality_HeldGround

`/Game/Data/Pool/Reality/Evolutions/DA_Evo_Reality_HeldGround`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Iolite
- displayElement: Reality
- tier: E_Tier
- itemName: Held Ground
- description: Truth is heavy — no flourish, only what is.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: -8, bonusSpellDamage: -8, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 12, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Reality_Unshaken

`/Game/Data/Pool/Reality/Evolutions/DA_Evo_Reality_Unshaken`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Iolite
- displayElement: Reality
- tier: E_Tier
- itemName: Unshaken
- description: Truth anchors the vessel — status cannot land.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 15, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Void_QuietVoid

`/Game/Data/Pool/Void/Evolutions/DA_Evo_Void_QuietVoid`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amethyst
- displayElement: Void
- tier: E_Tier
- itemName: Quiet Void
- description: A small opening — the void offers what it will.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 8, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 5, bonusMindModifierPercent: 10, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 8, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Void_VoidShaper

`/Game/Data/Pool/Void/Evolutions/DA_Evo_Void_VoidShaper`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amethyst
- displayElement: Void
- tier: E_Tier
- itemName: Void Shaper
- description: Form from nothing — the void answers the call.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 10, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 10, bonusMindModifierPercent: 15, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 10, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Void_WildVoid

`/Game/Data/Pool/Void/Evolutions/DA_Evo_Void_WildVoid`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Amethyst
- displayElement: Void
- tier: E_Tier
- itemName: Wild Void
- description: The void answers, and it does not ask permission.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 21, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: -15, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 21, bonusMindModifierPercent: 15, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 5, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Water_DeepSleep

`/Game/Data/Pool/Water/Evolutions/DA_Evo_Water_DeepSleep`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Sapphire
- displayElement: Water
- tier: E_Tier
- itemName: Deep Sleep
- description: Sleep lands where willed — mastered stillness.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 10, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Water_Drifting

`/Game/Data/Pool/Water/Evolutions/DA_Evo_Water_Drifting`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Sapphire
- displayElement: Water
- tier: E_Tier
- itemName: Drifting
- description: The sleep drifts inward — self more often than not.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 12, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Water_RestlessDepths

`/Game/Data/Pool/Water/Evolutions/DA_Evo_Water_RestlessDepths`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Sapphire
- displayElement: Water
- tier: E_Tier
- itemName: Restless Depths
- description: The sleep spills wide, catching allies in its wake.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: -10, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 15, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Wind_WindRider

`/Game/Data/Pool/Wind/Evolutions/DA_Evo_Wind_WindRider`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Emerald
- displayElement: Wind
- tier: E_Tier
- itemName: Wind Rider
- description: The wind carries them — twice as fast.
- revealedDescription: 
- evolutionType: Positive
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 20, bonusReflex: 10, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Wind_WindRunner

`/Game/Data/Pool/Wind/Evolutions/DA_Evo_Wind_WindRunner`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Emerald
- displayElement: Wind
- tier: E_Tier
- itemName: Wind Runner
- description: The wind fights them — every step drags.
- revealedDescription: 
- evolutionType: Cursed
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: -20, bonusReflex: -10, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Evo_Wind_WindWalker

`/Game/Data/Pool/Wind/Evolutions/DA_Evo_Wind_WindWalker`

- boundQuests: []
- runPayoutBonusPercent: 0
- sourceTags: {gameplayTags: []}
- crystalType: Emerald
- displayElement: Wind
- tier: E_Tier
- itemName: Wind Walker
- description: The wind at their side — quickened but grounded.
- revealedDescription: 
- evolutionType: Balanced
- infusionMode: Balanced
- fieldSprintBonusPercent: 15
- fieldSkillEPDiscountPercent: 25
- fieldFlightLimits: {bUnlimitedEP: false, bNoCeiling: false, bIgnoreNoFly: false, extraCeiling: 0}
- maxDurability: 40
- breakability: Breakable
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 10, bonusReflex: 5, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- icon: None
- tierColor: {r: 0, g: 0, b: 0, a: 0}
- useEffect: None
- useSound: None

## DA_Weapon_Tomahawk

`/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_Tomahawk`

- weaponType: Axe
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_RisingChop
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_ChargingChop, /Game/Data/Pool/Generic/Weapons/Axe/Pierce/Abilities/DA_Axe_Ability_LungingPick]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- weaponSkeletalMesh: None
- meshRotation: {pitch: 180, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Tomahawk
- tier: E_Tier
- description: Light hand axe — throwing and melee.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_BattleAxe

`/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_BattleAxe`

- weaponType: Axe
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_CleavingSwing
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_MightyChop, /Game/Data/Pool/Generic/Weapons/Axe/Impact/Abilities/DA_Axe_Ability_CrushingBlow]
- bAbilitiesLocked: false
- weaponStance: /Game/Data/Skills/Weapons/Sword/Stances/DA_Sword_Stance_Basic
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Steel_Ax
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Battle Axe
- tier: E_Tier
- description: One-handed fighting axe — brutal chopping strikes.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_BeardedAxe

`/Game/Data/Pool/Generic/Weapons/Axe/DA_Weapon_BeardedAxe`

- weaponType: Axe
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Axe/Slash/Attacks/DA_Axe_Attack_OverheadChop
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Axe/Slash/Abilities/DA_Axe_Ability_HookingPull, /Game/Data/Pool/Generic/Weapons/Axe/Pierce/Abilities/DA_Axe_Ability_HookedPin]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Rough_Axe
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Bearded Axe
- tier: E_Tier
- description: Viking-style axe with hooked beard — snags shields and limbs.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_BastardSword

`/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_BastardSword`

- weaponType: Greatsword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Attacks/DA_Greatsword_Attack_RisingSlash
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Greatsword/Pierce/Abilities/DA_Greatsword_Ability_GuttingThrust, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_SunderGuard]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Silver_Greatsword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Bastard Sword
- tier: E_Tier
- description: Hand-and-a-half sword — one or two hands as needed.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Longsword

`/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Longsword`

- weaponType: Greatsword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Attacks/DA_Greatsword_Attack_DiagonalHew
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Greatsword/Pierce/Abilities/DA_Greatsword_Ability_ArmorPiercer, /Game/Data/Pool/Generic/Weapons/Greatsword/Impact/Abilities/DA_Greatsword_Ability_MurderBlow]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Lonsword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Longsword
- tier: E_Tier
- description: Versatile two-handed blade — powerful cuts and precise thrusts.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Zweihander

`/Game/Data/Pool/Generic/Weapons/Greatsword/DA_Weapon_Zweihander`

- weaponType: Greatsword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Greatsword/Impact/Attacks/DA_Greatsword_Attack_MordhauSwing
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_ColossalCleave, /Game/Data/Pool/Generic/Weapons/Greatsword/Slash/Abilities/DA_Greatsword_Ability_StaggeringSweep]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Heavy_Steel_Greatsword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Zweihänder
- tier: E_Tier
- description: Massive two-handed sword — sweeping strikes.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Rapier

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Rapier`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Attacks/DA_Sword_Attack_StraightThrust
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_CounterThrust, /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Abilities/DA_Sword_Ability_Heartseeker]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Crystal_Rapier
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Rapier
- tier: E_Tier
- description: Narrow thrusting blade — precise, long reach.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Falchion

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Falchion`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_DescendingCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WrathCut, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_SeverArtery]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Wide_sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Falchion
- tier: E_Tier
- description: Wide single-edged blade — heavy slashing cleaver.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Sabre

`/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_Sabre`

- weaponType: Sword
- wieldMode: Single
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Slash/Attacks/DA_Sword_Attack_BackhandCut
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WhirlwindSlash, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_FellCrescent]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curved_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Sabre
- tier: E_Tier
- description: Curved slashing blade, fast and agile.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Sabre_Buckler

`/Game/Data/Pool/Generic/Weapons/SwordAndShield/DA_Weapon_Sabre_Buckler`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Pierce/Attacks/DA_Sword_Attack_DescendingThrust
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_FeintingSlash, /Game/Data/Pool/Generic/Weapons/Sword/Impact/Abilities/DA_Sword_Ability_CounterBash]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curved_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Shield_4
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Sabre & Buckler
- tier: E_Tier
- description: Sabre paired with a buckler — agile parrying duellist.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_Broadsword_Heater

`/Game/Data/Pool/Generic/Weapons/SwordAndShield/DA_Weapon_Broadsword_Heater`

- weaponType: Sword
- wieldMode: OffHandShield
- infusionMode: Balanced
- weaponAttack: /Game/Data/Pool/Generic/Weapons/Sword/Impact/Attacks/DA_Sword_Attack_HiltSmash
- presetAbilities: [/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HeadsmansCut, /Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_HundredCuts]
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Straight_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Shield_2
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Broadsword & Heater Shield
- tier: E_Tier
- description: Broadsword paired with a heater shield — steady frontline guard.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinHandaxes

`/Game/Data/Pool/Generic/Weapons/DualAxe/DA_Weapon_TwinHandaxes`

- weaponType: Axe
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Steel_Ax
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Steel_Ax
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Hand Axes
- tier: E_Tier
- description: Matched one-handed axes — relentless dual chopping.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinTomahawks

`/Game/Data/Pool/Generic/Weapons/DualAxe/DA_Weapon_TwinTomahawks`

- weaponType: Axe
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- weaponSkeletalMesh: None
- meshRotation: {pitch: 180, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 180, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Tomahawks
- tier: E_Tier
- description: Paired light hatchets — fast strikes and throws.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_SaiPair

`/Game/Data/Pool/Generic/Weapons/DualDagger/DA_Weapon_SaiPair`

- weaponType: Dagger
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Sai
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Sai
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Pair of Sai
- tier: E_Tier
- description: Twin pronged truncheons — trapping and jabbing.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinDaggers

`/Game/Data/Pool/Generic/Weapons/DualDagger/DA_Weapon_TwinDaggers`

- weaponType: Dagger
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Standard_Dagger
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Standard_Dagger
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Daggers
- tier: E_Tier
- description: Matched pair of straight daggers — rapid dual-wield strikes.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinKris

`/Game/Data/Pool/Generic/Weapons/DualDagger/DA_Weapon_TwinKris`

- weaponType: Dagger
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curv_Dagger
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curv_Dagger
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Kris
- tier: E_Tier
- description: Paired wavy-bladed daggers — vicious slashing and stabbing.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinKatanas

`/Game/Data/Pool/Generic/Weapons/DualSword/DA_Weapon_TwinKatanas`

- weaponType: Sword
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/ShortKatana
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/ShortKatana
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Katanas
- tier: E_Tier
- description: Two-sword style — flowing paired cuts.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinSabres

`/Game/Data/Pool/Generic/Weapons/DualSword/DA_Weapon_TwinSabres`

- weaponType: Sword
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curved_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Curved_Sword
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Sabres
- tier: E_Tier
- description: Matched curved blades — whirling dual-wield offence.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_TwinShortswords

`/Game/Data/Pool/Generic/Weapons/DualSword/DA_Weapon_TwinShortswords`

- weaponType: Sword
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Short_Sword
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: /Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Short_Sword
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Twin Shortswords
- tier: E_Tier
- description: Paired short blades — balanced dual-wield fighting.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0

## DA_Weapon_CombatGauntlets

`/Game/Data/Pool/Generic/Weapons/Gauntlets/DA_Weapon_CombatGauntlets`

- weaponType: Gauntlets
- wieldMode: Dual
- infusionMode: Balanced
- weaponAttack: None
- presetAbilities: []
- bAbilitiesLocked: false
- weaponStance: None
- handling: SingleOnly
- dualWieldStance: None
- power: 0
- statScaling: []
- drawMontage: None
- sheatheMontage: None
- parryMontage: None
- castVFX: None
- weaponStaticMesh: None
- weaponSkeletalMesh: None
- meshRotation: {pitch: 0, yaw: 0, roll: 0}
- leftHandStaticMesh: None
- leftHandSkeletalMesh: None
- leftMeshRotation: {pitch: 0, yaw: 0, roll: 0}
- weaponMaterialOverride: None
- leftHandMaterialOverride: None
- sourceTags: {gameplayTags: []}
- name: Combat Gauntlets
- tier: E_Tier
- description: Paired armoured fists — close-range dual strikes.
- icon: None
- attachedItem: {kind: None, crystalType: Garnet, crystalTier: F_Tier, evolution: None, fusionHalfAType: DamageStone, fusionHalfATier: F_Tier, bFusionHalfBIsCrystal: true, fusionHalfBType: Garnet, fusionHalfBTier: F_Tier, fusionBonusStat: None}
- defaultSpells: []
- defaultAbilities: []
- bImmuneToInfusion: false
- bLockSkills: false
- infusionStatusMultiplier: 1
- baseStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- generatedStatBonus: {bonusRawDamage: 0, bonusSpellDamage: 0, bonusEfficiency: 0, bonusStatusMultiplier: 0, bonusCritDamage: 0, bonusSpellSpeed: 0, bonusDefense: 0, bonusActionSpeed: 0, bonusReflex: 0, bonusMaxHP: 0, bonusMaxEnergy: 0, bonusResistance: 0, bonusTurnSpeed: 0, bonusLuck: 0, bonusMindModifierPercent: 0, bonusBodyModifierPercent: 0, bonusSpiritModifierPercent: 0, amp: {all: 0, physical: 0, spell: 0, fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0}}
- baseResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- generatedResistance: {fire: 0, water: 0, earth: 0, wind: 0, light: 0, darkness: 0, lightning: 0, void: 0, reality: 0, slash: 0, pierce: 0, impact: 0, physicalDamage: 0, spellDamage: 0, ice: 0}
- bRandomGenerateOnPickup: false
- statMaxPoolOverride: 0
- resistanceMaxPoolOverride: 0
- referencedEffects: []
- boundOwner: None
- boundEffects: []
- boundMemories: []
- boundQuests: []
- requirements: {requiredWorldMind: 0, requiredWorldBody: 0, requiredWorldSpirit: 0}
- substatPoints: 0
- pillarPoints: 0
