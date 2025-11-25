# Combat Systems State Documentation
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 2.0  
**Status:** Abilities & Spells Complete - Ready for Items/Ultimates/Evolutions

---

## Table of Contents

1. [Systems Overview](#systems-overview)
2. [Completed Systems](#completed-systems)
3. [Universal Spell Innovation](#universal-spell-innovation)
4. [Current Asset Inventory](#current-asset-inventory)
5. [Remaining Systems](#remaining-systems)
6. [Next Steps](#next-steps)

---

## Systems Overview

### Combat Action Types

**Completed:**
- ✅ **Abilities** - Universal, element-neutral skills (any character can use)
- ✅ **Spells** - Element-locked magical abilities (Fire Lord = Fire spells only)

**In Progress:**
- 🟡 **Items** - Consumable crystals (10 types)

**Planned:**
- ⬜ **Ultimates** - Powerful cinematic abilities
- ⬜ **Evolutions** - Permanent form transformations

### Core Design Pillars

**1. Abilities = Universal Flexibility**
- Any character can attempt any ability
- Requirement penalties guide optimization
- Infusion system adds elemental power (in-combat choice)
- 10 diverse abilities covering all playstyles

**2. Spells = Elemental Specialization**
- Strict element locking (Fire Lord = Fire only, Generic = none)
- Mode toggle system (Elemental vs Raw, loadout choice)
- School-based requirements (Destruction/Enhancement/Restoration/Conjuration)
- Universal spell system reduces redundancy

**3. Strategic Layers**
- **Abilities:** In-combat infusion decision (damage vs status trade-off)
- **Spells:** Pre-battle mode configuration (preparation matters)
- **Requirements:** Soft restrictions guide builds without hard locks

---

## Completed Systems

### Ability System (Complete)

**Technical Implementation:**
- AbilityData.h/cpp: Primary data asset
- AbilityDataDebug.h/cpp: Comprehensive testing
- TargetType.h: Target selection enum
- AbilityEffectType.h: Effect system enum

**10 Abilities Created:**
1. Quick Strike - Basic attack
2. Heavy Strike - Power attack (Body 30)
3. 16 Hit Combo - Multi-hit technical (Mind 20, Spirit 25)
4. Tactical Strike - Precision balanced
5. Focus - Damage buff (Mind 18, Spirit 15)
6. Weaken - Debuff attack (Mind 20)
7. Fortify - Defense buff (Spirit 25)
8. Drain Strike - Energy steal (Mind 18, Spirit 22)
9. Pressure Point - Status specialist (Mind 25)
10. Whirlwind - AOE attack (Body 25, Spirit 28)

**Key Features:**
- Requirement penalty system (sqrt scaling, 60% cap)
- Infusion system (30% damage penalty, +50% energy, status buildup)
- Effect system (buffs, debuffs, utilities)
- Status buildup scales with Spirit stat
- Multi-hit support
- Full debug tools

**Constants:**
```cpp
REQUIREMENT_PENALTY_SCALE = 0.10f
REQUIREMENT_PENALTY_MAX = 0.6f
INFUSION_DAMAGE_PENALTY = 0.30f
INFUSION_ENERGY_MULTIPLIER = 1.5f
BASE_STATUS_BUILDUP_PER_HIT = 5
STATUS_EFFECT_THRESHOLD = 100
```

---

### Spell System (Complete)

**Technical Implementation:**
- SpellData.h/cpp: Primary data asset
- SpellDataDebug.h/cpp: Comprehensive testing
- SpellSchool.h: School enum
- Shares: TargetType.h, AbilityEffectType.h, CombatConstants.h

**Universal Spells (27 created):**

**Destruction (6):**
1. Ember Shot - Basic projectile (toggle)
2. Fireball - Classic damage (toggle)
3. Flame Wave - AOE sweep (toggle)
4. Searing Strike - Focused blast (toggle)
5. Inferno - Pure elemental power (no toggle, high status)
6. Meteor Strike - Ritual 3-turn ultimate (no toggle)

**Enhancement (6):**
7. Flame Shield - Brief defense buff
8. Blazing Speed - Movement buff
9. Searing Weakness - Defense debuff attack
10. Burning Fury - Damage buff
11. Immolation Ritual - 3-turn channel with self-damage
12. Infernal Dominance - Major offensive buff

**Restoration (6):**
13. Cauterize - Instant heal
14. Warmth - Heal over time
15. Dehydration - Energy drain attack (cross-school!)
16. Fire's Resilience - Heal + defense
17. Life Spark - Ally healing
18. Phoenix Rebirth - Master resurrection

**Conjuration (6):**
19. Flame Dart - Multi-projectile (3 hits, toggle)
20. Fire Spear - Weapon conjuration (toggle)
21. Flame Barrier - Defensive wall
22. Ember Clone - Decoy creation
23. Flame Whip - Multi-hit lash (5 hits, toggle)
24. Fire Elemental - Summon companion

**Universal Auras (3):**
25. Enhancement Aura - Body buff (all sub-stats)
26. Elemental Aura - Retaliation damage
27. Manipulation Aura - Mind buff (all sub-stats)

**Key Features:**
- Element locking with Generic exclusion
- Mode toggle (Elemental vs Raw/Construct)
- Turn cost system for rituals
- School-based requirements
- Universal spell system with dynamic naming
- Cross-school effects
- DOT system
- Full debug tools

**New Effect Types Added:**
```cpp
// Pillar buffs/debuffs
MindBuff, MindDebuff
BodyBuff, BodyDebuff
SpiritBuff, SpiritDebuff

// DOT effects
BurnDOT, ChillDOT, PoisonDOT
ElectrifiedDOT, BleedDOT, CorruptDOT

// Special effects
RetaliationDamage (Elemental Aura)
SelfDamage (spell recoil)
```

---

## Universal Spell Innovation

### The Breakthrough

**Original Design:**
- 24 spells per element
- 11 elements
- **264 total spells to create**

**New Design:**
- ~27 universal spells (work for all elements)
- ~10 element-specific spells per element
- **~37 total spells** (87% reduction!)

### How It Works

**Technical Implementation:**

```cpp
// In SpellData.h
bool bIsUniversalSpell = false;  // Can any element cast this?
bool bPrependElementName = false;  // Add element prefix?

// In SpellData.cpp
FString GetDisplayName(UCharacterData* Caster) const
{
    if (bIsUniversalSpell && bPrependElementName && Caster)
    {
        // "Elemental" becomes "Fire Elemental"
        return ElementName + " " + SpellName;
    }
    return SpellName;
}
```

**Examples:**

**With Prepend (bPrependElementName = true):**
- SpellName: "Elemental"
- Fire Lord casts: "Fire Elemental"
- Water Lord casts: "Water Elemental"
- Earth Lord casts: "Earth Elemental"

**Without Prepend (bPrependElementName = false):**
- SpellName: "Enhancement Aura"
- Fire Lord casts: "Enhancement Aura"
- Water Lord casts: "Enhancement Aura"
- (Same name, different visual effects)

### Design Benefits

**Efficiency:**
- Reduce creation workload by 87%
- Easier to balance (change once, affects all)
- Faster iteration

**Consistency:**
- All elements have access to core mechanics
- No element feels incomplete
- Clear baseline for element-specific design

**Flexibility:**
- Element-specific spells stand out as unique
- Can add new elements easily
- Universal spells update retroactively

### When to Use Universal vs Element-Specific

**Universal (most spells):**
- Core mechanics (damage, buffs, healing)
- Common constructs (barriers, clones, projectiles)
- Summons with elemental variants
- Any spell where element = visual flavor

**Element-Specific (unique spells):**
- Thematic abilities (Phoenix Rebirth for Fire)
- Unique mechanics (Chain Lightning multi-target)
- Story/lore significance
- Special interactions

---

## Current Asset Inventory

### Characters
- DA_FireLord (Mind 36, Body 14, Spirit 25)
- DA_WaterLord (Mind 25, Body 33, Spirit 17)

### Abilities (10 Total)
```
Universal (all 10):
├─ DA_QuickStrike
├─ DA_HeavyStrike
├─ DA_16HitCombo
├─ DA_TacticalStrike
├─ DA_Focus
├─ DA_Weaken
├─ DA_Fortify
├─ DA_DrainStrike
├─ DA_PressurePoint
└─ DA_Whirlwind
```

### Spells (27 Total)

```
Universal (27):
├─ Destruction (6)
│  ├─ DA_EmberShot (toggle)
│  ├─ DA_Fireball (toggle)
│  ├─ DA_FlameWave (toggle)
│  ├─ DA_SearingStrike (toggle)
│  ├─ DA_Inferno (pure elemental)
│  └─ DA_MeteorStrike (ritual 3-turn)
├─ Enhancement (6)
│  ├─ DA_FlameShield
│  ├─ DA_BlazingSpeed
│  ├─ DA_SearingWeakness
│  ├─ DA_BurningFury
│  ├─ DA_ImmolationRitual (3-turn + self-damage)
│  └─ DA_InfernalDominance
├─ Restoration (6)
│  ├─ DA_Cauterize
│  ├─ DA_Warmth
│  ├─ DA_Dehydration (cross-school)
│  ├─ DA_FiresResilience
│  ├─ DA_LifeSpark
│  └─ DA_PhoenixRebirth
├─ Conjuration (6)
│  ├─ DA_FlameDart (toggle)
│  ├─ DA_FireSpear (toggle)
│  ├─ DA_FlameBarrier
│  ├─ DA_EmberClone
│  ├─ DA_FlameWhip (toggle)
│  └─ DA_FireElemental
└─ Universal Auras (3)
   ├─ DA_EnhancementAura (Body buff)
   ├─ DA_ElementalAura (Retaliation)
   └─ DA_ManipulationAura (Mind buff)
```

### Testing Tools
- BP_AbilityDataTester
- BP_SpellDataTester

---

## Remaining Systems

### 1. Items System

**From wor_advancement Code:**
```cpp
CrystalType.Opal
CrystalType.Onyx
CrystalType.Emerald
CrystalType.Sapphire
CrystalType.Citrine
CrystalType.Amber
CrystalType.Amethyst
CrystalType.Iolite
CrystalType.Quartz
CrystalType.Garnet
```

**Design Questions:**
- Are crystals consumable or permanent?
- What do they do? (Healing, buffs, energy restore?)
- Do they have requirements?
- Element-specific or universal?
- How many can be equipped?

**Likely Implementation:**
- ItemData.h/cpp similar to AbilityData
- Consumable effects
- No requirements (anyone can use)
- Inventory system for battle

---

### 2. Ultimates System

**Design Questions:**
- One ultimate per character or multiple?
- Unlocked through progression?
- Requirements to use? (Energy cost, cooldown?)
- Cinematic/powerful effects
- How does it interact with combat flow?

**Likely Implementation:**
- UltimateData.h/cpp asset
- High energy cost or cooldown system
- Cinematic camera/animations
- Replaces ultimate button in UI
- Character-specific or element-specific?

---

### 3. Evolutions System

**Concept from Discussion:**
- Permanent form transformations
- Unlocked outside of combat
- Changes character stats/appearance
- May grant new spells or alter existing ones
- Replaces ultimate button when evolved

**Example:**
- Water Evolution: Transform into liquid form
- Grants new spell access
- Alters stat distribution
- Permanent upgrade (cannot revert?)

**Design Questions:**
- Multiple evolution paths per character?
- Stat changes quantified how?
- Do they replace or augment spells?
- Story/progression gated?
- Does it disable ultimates or merge with them?

**Likely Implementation:**
- EvolutionData.h/cpp asset
- Stat modifier system
- Spell unlock/modification system
- UI toggle between ultimate and evolution modes
- Permanent character state tracking

---

### 4. Loadout/Equipment System

**Requirements:**
- Equip X abilities (3-6?)
- Equip Y spells (3-6?)
- Equip Z items (3?)
- Configure spell modes (Elemental/Raw)
- Set ultimate OR evolution
- Save/load configurations

**Likely Implementation:**
- CharacterLoadout.h/cpp
- FEquippedAbility struct (ability + infusion preference?)
- FEquippedSpell struct (spell + mode choice)
- FEquippedItem struct (item + quick-slot)
- UI for configuration
- Save to character or separate loadout assets

---

### 5. Combat Manager

**The Big One:**
- Turn-based orchestration
- Action selection UI (Attack, Ability, Spell, Item, Ultimate/Evolution)
- Target selection
- Damage calculation integration
- Status effect tracking and application
- Turn order calculation
- Energy management
- AI decision-making

**Dependencies:**
- Needs Items, Ultimates, Evolutions defined
- Needs Loadout system for equipped actions
- Needs Status Effect system design

---

## Next Steps

### Immediate (Current Session)

**1. Create Element-Specific Water Spells**
- Identify unique Water-only mechanics
- Create 3-5 spells that differentiate Water from Fire
- Test universal spells work correctly for Water

**Example Water-Specific Spells:**
- Tidal Wave (unique AOE mechanic)
- Ice Prison (crowd control)
- Whirlpool (pull enemies together)

---

### Short-Term (Next 1-2 Sessions)

**2. Design Discussion: Items/Ultimates/Evolutions**
- Define what crystals do
- Clarify ultimate mechanics
- Flesh out evolution system
- Determine UI action economy (how many buttons?)
- Decide on combat flow

**3. Create ItemData System**
- Similar to AbilityData/SpellData
- 10 crystal types
- Effect system
- Testing tools

**4. Create Ultimate/Evolution Systems**
- Determine which comes first
- Build base architecture
- Create example assets

---

### Medium-Term (Next 3-5 Sessions)

**5. Loadout/Equipment System**
- UI for ability/spell/item selection
- Mode configuration for spells
- Ultimate vs Evolution toggle
- Save/load system

**6. Status Effect Manager**
- Define all 11 element status effects
- Buildup tracking
- Trigger system at 100 threshold
- Duration and effect application
- Visual feedback

**7. Combat Manager Foundation**
- Turn-based state machine
- Action selection flow
- Target selection
- Basic damage application

---

### Long-Term (Next 10+ Sessions)

**8. Combat Polish**
- AI implementation
- Animation integration
- VFX and audio
- UI polish
- Balance tuning

**9. Progression Systems**
- Character advancement
- Spell/ability unlocks
- Evolution unlock conditions
- Story integration

**10. Campaign/Content**
- Enemy design
- Encounter design
- Boss battles
- Story missions

---

## Design Principles Carried Forward

### 1. Soft Restrictions Over Hard Locks
- Requirements guide, penalties discourage
- Any character can attempt any action
- Specialization emerges naturally

### 2. Strategic Depth Through Trade-offs
- Ability infusion: Damage vs Status
- Spell modes: Damage vs Status (different timing)
- Resource management: Energy costs
- Risk/reward: Ritual spells, self-damage

### 3. Scalable Complexity
- Simple options for beginners (Quick Strike)
- Complex options for mastery (16 Hit Combo)
- System depth emerges from combinations

### 4. Character Identity Through Optimization
- Fire Lord naturally gravitates to Mind/Spirit
- Water Lord excels at Body/Spirit
- Distinct without artificial restrictions

### 5. Universal Systems Where Possible
- Reduce redundancy (universal spells)
- Easier to balance and maintain
- Element-specific only when meaningful

---

## Technical Architecture Summary

### Asset Types
```
UPrimaryDataAsset
├─ UCharacterData (character stats and progression)
├─ UAbilityData (universal skills)
├─ USpellData (element-locked magic)
├─ UItemData (planned - consumables)
├─ UUltimateData (planned - powerful abilities)
└─ UEvolutionData (planned - form transformations)
```

### Enums
```
ERefractionElement (11 elements + Generic + Broken Darkness)
ESpellSchool (Destruction, Enhancement, Restoration, Conjuration)
ETargetType (Self, Single Enemy, All Enemies, etc.)
EAbilityEffectType (30+ effect types including buffs, debuffs, DOT, special)
```

### Constants
```cpp
// Requirements
REQUIREMENT_PENALTY_SCALE = 0.10f
REQUIREMENT_PENALTY_MAX = 0.6f

// Infusion
INFUSION_DAMAGE_PENALTY = 0.30f
INFUSION_ENERGY_MULTIPLIER = 1.5f
BASE_STATUS_BUILDUP_PER_HIT = 5

// Status
STATUS_EFFECT_THRESHOLD = 100
```

### Calculation Patterns
```
Normal Damage = Base × (1 - ReqPenalty) × RawDamageMultiplier
Infused Damage = Base × (1 - ReqPenalty) × (1 - 0.30) × RawDamageMultiplier
Elemental Spell = Base × (1 - ReqPenalty) × EffectDamageMultiplier
Raw Spell = Base × (1 - ReqPenalty) × RawDamageMultiplier

Energy Cost = Base × (1 + ReqPenalty) [× 1.5 if infused]
Status Buildup = BasePerHit × EffectMultiplier × HitCount
```

---

## Current State Summary

**What We Have:**
- ✅ Complete ability system (10 abilities)
- ✅ Complete spell system (27 universal spells)
- ✅ Universal spell innovation (87% efficiency gain)
- ✅ Character stat system
- ✅ Requirement and penalty system
- ✅ Effect system (buffs, debuffs, DOT, special)
- ✅ Comprehensive debug tools
- ✅ Two test characters (Fire Lord, Water Lord)

**What We're Creating Next:**
- 🟡 Element-specific spells (3-5 Water spells)

**What We Need to Design:**
- ⬜ Items system (10 crystals)
- ⬜ Ultimates system
- ⬜ Evolutions system
- ⬜ Action economy and UI layout

**What We Need to Build:**
- ⬜ Loadout/Equipment system
- ⬜ Status Effect Manager
- ⬜ Combat Manager

---

## File Locations

### C++ Files
```
Source/world_of_refraction/Public/
├─ AbilityData.h
├─ AbilityDataDebug.h
├─ SpellData.h
├─ SpellDataDebug.h
├─ CharacterData.h
├─ TargetType.h
├─ AbilityEffectType.h
├─ SpellSchool.h
└─ RefractionElement.h

Source/world_of_refraction/Private/
├─ AbilityData.cpp
├─ AbilityDataDebug.cpp
├─ SpellData.cpp
├─ SpellDataDebug.cpp
└─ CharacterData.cpp

Source/world_of_refraction/
└─ CombatConstants.h
```

### Content Assets
```
Content/DataAssets/
├─ Characters/
│  ├─ DA_FireLord
│  └─ DA_WaterLord
├─ Abilities/
│  └─ (10 ability assets)
└─ Spells/
   └─ (27 spell assets)

Content/Blueprints/Testing/
├─ BP_AbilityDataTester
└─ BP_SpellDataTester
```

---

## Changelog

### Version 2.0 (November 25, 2024)
**Spell System Implementation:**
- Complete spell system with 4 schools
- Universal spell innovation (bIsUniversalSpell, bPrependElementName)
- 27 universal spells created
- Mode toggle system (Elemental vs Raw/Construct)
- Turn cost system for rituals
- Cross-school effects
- DOT effect types
- Three universal auras

**New Effect Types:**
- Pillar buffs/debuffs (Mind/Body/Spirit all sub-stats)
- DOT effects (Burn, Chill, Poison, etc.)
- Special effects (Retaliation, SelfDamage)

**Technical:**
- SpellData.h/cpp
- SpellSchool.h
- SpellDataDebug.h/cpp
- GetDisplayName() dynamic naming
- Updated AbilityEffectType with 12+ new types

### Version 1.0 (November 25, 2024)
**Ability System Implementation:**
- Complete ability system with requirements
- 10 diverse abilities
- Infusion system
- Effect system
- Status buildup
- Comprehensive debug tools

---

**End of Documentation**

Next Actions:
1. Create 3-5 Water-specific spells
2. Design discussion: Items/Ultimates/Evolutions
3. Build Loadout/Equipment system