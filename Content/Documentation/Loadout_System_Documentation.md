# Loadout System Documentation
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 1.0  
**Status:** Design Complete - Ready for Implementation

---

## Table of Contents

1. [Overview](#overview)
2. [Character Creation](#character-creation)
3. [Loadout Components](#loadout-components)
4. [Spell Point Budget System](#spell-point-budget-system)
5. [Loadout Configuration](#loadout-configuration)
6. [Validation & Requirements](#validation--requirements)
7. [Preview System](#preview-system)
8. [UI/UX Design](#uiux-design)
9. [Technical Implementation](#technical-implementation)
10. [Balance Considerations](#balance-considerations)

---

## Overview

### **What is the Loadout System?**

The Loadout System is where players customize their character's combat capabilities - choosing which abilities, spells, items, and ultimate to bring into battle. It's the strategic planning phase that determines your combat options.

**Core Philosophy:**
> "Build your character, define your playstyle, master your choices."

**Key Characteristics:**
- **Permanent Choices:** Element selection (cannot change)
- **Strategic Choices:** 30-point sub-stat distribution (character creation)
- **Tactical Choices:** Loadout configuration (pre-battle)
- **Locked in Battle:** No changes once combat starts
- **Multiple Presets:** Save different loadouts for different situations

---

### **Loadout vs Combat**

**Loadout (Pre-Battle):**
- Choose abilities (max 6)
- Choose spells (24-point budget)
- Set spell modes (Elemental/Raw)
- Choose items (6 slots, 3 stacks)
- Choose ultimate/evolution
- Configure everything strategically

**Combat (In-Battle):**
- Use equipped actions only
- Ability infusion (in-combat choice)
- No loadout changes allowed
- Execute strategy

**Design Intent:**
> "Prepare in loadout, adapt in combat."

---

## Character Creation

### **Creation Flow**

**Step 1: Choose Element (Permanent)**

Select your element - this choice is **permanent** and defines your character forever.

**Available Elements:**
```
1. Fire - Aggressive offense, burn damage
2. Water - Adaptive defense, healing
3. Earth - Sturdy tank, control
4. Wind - Speed and mobility
5. Lightning - High burst damage
6. Light - Support and vision
7. Darkness - DOT and debuffs
8. Void - Chaos and randomness
9. Reality - Manipulation and utility
10. Generic - No spells, item-focused, resistance bonuses
11. Broken Darkness - Absorption mechanics, 0 energy start
```

**Implications:**
- Fire Lord can only cast Fire spells (+ universal spells)
- Generic cannot cast any spells
- Broken Darkness has unique absorption mechanics
- Cannot change element later (permanent identity)

---

**Step 2: Distribute 30 Sub-Stat Points**

Customize your starting build by distributing **30 points** across 9 sub-stats.

**Sub-Stat Categories:**

**Mind Sub-Stats:**
- **Spell Cost:** Reduces spell energy costs
- **Effect Damage:** Increases status effect buildup/damage
- **Crit Chance:** Increases critical hit chance

**Body Sub-Stats:**
- **Defense:** Reduces incoming damage
- **Attack Speed:** Increases animation speed
- **Raw Damage:** Increases non-elemental damage

**Spirit Sub-Stats:**
- **Max Energy:** Increases maximum energy pool
- **Resistance:** Reduces status effect buildup on you
- **Ability Size:** Increases AOE ability range/size

**Distribution Rules:**
```
Total Points: 30 (must use all)
Min per stat: 0 (can ignore stats completely)
Max per stat: No cap (can put all 30 in one stat)
Permanent: Cannot respec starting distribution
```

**Example Builds:**

**Glass Cannon Mage:**
```
Effect Damage: 20 (massive spell damage)
Crit Chance: 10 (high crit rate)
Others: 0
Total: 30

Strategy: Maximum offensive power, no defense
Risk: Dies easily
Reward: Incredible damage output
```

**Balanced Fighter:**
```
Raw Damage: 8
Defense: 8
Attack Speed: 6
Max Energy: 8
Total: 30

Strategy: Well-rounded combat
Risk: Not exceptional at anything
Reward: Versatile, adaptable
```

**Tank Specialist:**
```
Defense: 15
Max Energy: 10
Resistance: 5
Total: 30

Strategy: Survive everything
Risk: Low damage output
Reward: Outlast opponents
```

**Speed Demon:**
```
Attack Speed: 20
Crit Chance: 10
Total: 30

Strategy: Fast combos, high crit
Risk: Low defense, low energy
Reward: Overwhelming pressure
```

---

**Step 3: Starting Loadout (Optional)**

Choose starting equipment or use defaults:

**Default Starting Loadout:**
```
Abilities:
├─ Quick Strike
├─ Heavy Strike
└─ Tactical Strike
(3 basic abilities, unlock more later)

Spells:
├─ 2 basic Destruction spells
├─ 1 basic Enhancement spell
└─ 1 basic Restoration spell
(4 basic spells, 4 points used of 24)

Items:
├─ 3 F-tier Sapphire (healing)
├─ 3 F-tier Citrine (energy)
└─ Empty slots
(Basic survival items)

Ultimate:
└─ Element's basic ultimate
```

**Customization:**
- Can configure immediately
- Or use defaults and edit later
- Loadout screen accessible anytime outside battle

---

**World Stats (Hidden at Creation):**

During character creation, players do NOT see or interact with World Stats. These are discovered through gameplay.

**What are World Stats?**
```
Mind World Stat: 1-7
Body World Stat: 1-7
Spirit World Stat: 1-7

Starting: All at level 1
Each level = 3 additional sub-stat points per pillar

Example:
Mind World Stat 1 → 2:
└─ Gain 3 points to distribute among Mind sub-stats
```

**Discovery in Game:**
- Find collectibles in world
- Increase world stats from 1 → 7
- Each level grants 3 points to distribute
- Total possible: 54 additional points (18 per pillar)

**Total Sub-Stat Points Possible:**
```
Character creation: 30 points
Starting world stats: 9 points (3 per pillar at level 1)
World stat progression: 54 points (18 per pillar, levels 2-7)
TOTAL MAXIMUM: 93 sub-stat points
```

**Note:** World stats remain hidden during creation - players discover this system naturally through gameplay.

---

## Loadout Components

### **What Can Be Equipped?**

**1. Abilities (Max 6)**
```
Universal skills any character can use
Choose from all unlocked abilities
No element restrictions
Requirements: Mind/Body/Spirit thresholds
```

**2. Spells (24-Point Budget)**
```
Element-locked magical abilities
24-point budget across all 4 schools
Tiers: Basic (1), Intermediate (2), Advanced (3), Master (4)
Must balance power vs quantity
```

**3. Items (6 Slots, 3 Stacks Each)**
```
6 different crystal types
3 stacks per crystal (different tiers allowed, no duplicates)
18 total item uses per battle
```

**4. Ultimate OR Evolution**
```
One powerful ultimate ability
OR
Evolution (permanent transformation with ultimate)
Swappable between battles
```

---

### **Component Details**

**Abilities:**
- Pool: All unlocked abilities (10+ available)
- Selection: Choose any 6
- Order: Arranged for quick access (slot 1-6)
- Requirements: Can equip anything, penalties apply if stats too low
- Infusion: Decided in-combat, not loadout

**Spells:**
- Pool: Element spells + universal spells + evolution spells (if applicable)
- Selection: 24-point budget (see Spell Point Budget System)
- Schools: 4 schools (Destruction, Enhancement, Restoration, Conjuration)
- Mode: Set Elemental/Raw per spell in loadout
- Requirements: Can equip anything, penalties apply

**Items:**
- Pool: All collected items (organized by crystal type and tier)
- Selection: 6 crystal types, 3 stacks each
- Tier Mixing: Can have S/A/F in same slot, no duplicate tiers
- Stack Order: Consumed in order (slot 1 → 2 → 3)

**Ultimate/Evolution:**
- Pool: All unlocked ultimates OR active evolution ultimate
- Selection: One ultimate
- Evolution: If evolved, can still swap ultimates (Option A)
- Changeable: Different ultimate per loadout preset

---

## Spell Point Budget System

### **The 24-Point Economy**

**Total Budget: 24 points across all 4 spell schools**

**Tier Costs:**
```
Basic spell = 1 point
Intermediate spell = 2 points
Advanced spell = 3 points
Master spell = 4 points
```

**Rules:**
- Must spend exactly 24 points or less
- Can spend all 24 in one school (6 Master spells)
- Can spread evenly (6 points per school)
- Can ignore schools entirely (0 points in Conjuration)
- No carry-over or banking

---

### **Strategic Archetypes**

**1. Nuke Mage (Destruction Specialist)**
```
Destruction: 18 points
├─ 3 Master spells (12 points)
├─ 2 Advanced spells (6 points)
└─ Total: 5 destruction spells

Enhancement: 3 points
└─ 3 Basic buff spells

Restoration: 3 points
└─ 3 Basic heal spells

Conjuration: 0 points
└─ None

Total: 24 points, 11 spells
Strategy: Maximum destruction power, minimal support
```

**2. Balanced Generalist**
```
Destruction: 6 points
├─ 2 Intermediate (4)
├─ 2 Basic (2)
└─ Total: 4 spells

Enhancement: 6 points
├─ 2 Intermediate (4)
├─ 2 Basic (2)
└─ Total: 4 spells

Restoration: 6 points
├─ 2 Intermediate (4)
├─ 2 Basic (2)
└─ Total: 4 spells

Conjuration: 6 points
├─ 2 Intermediate (4)
├─ 2 Basic (2)
└─ Total: 4 spells

Total: 24 points, 16 spells
Strategy: All schools covered, versatile toolkit
```

**3. Support Specialist (Healer/Buffer)**
```
Destruction: 2 points
├─ 2 Basic (for self-defense)

Enhancement: 11 points
├─ 2 Master (8)
├─ 1 Advanced (3)
└─ Total: 3 powerful buff spells

Restoration: 11 points
├─ 2 Master (8)
├─ 1 Advanced (3)
└─ Total: 3 powerful heal spells

Conjuration: 0 points

Total: 24 points, 8 spells
Strategy: Team support, minimal offense
```

**4. All-In Specialist**
```
Destruction: 24 points
├─ 6 Master spells (24 points)
└─ Maximum possible destruction power

Everything else: 0 points

Total: 24 points, 6 spells
Strategy: Pure glass cannon, one school only
Risk: No healing, no buffs, no utility
Reward: Devastating destruction arsenal
```

---

### **Point Budget Constraints**

**Cannot Have:**
- ❌ 24 Master spells (would cost 96 points)
- ❌ 6 Master per school (would cost 96 points)
- ❌ All Advanced+ spells (too expensive)

**Forces Choices:**
- Trade quantity for quality (few Master vs many Basic)
- Trade breadth for depth (all schools vs one school)
- Trade versatility for specialization

**Example Tough Choices:**
```
Want both Master spells in Destruction:
├─ Meteor Strike (4 points)
├─ Inferno (4 points)
└─ 8 points spent = 16 points left for everything else

Want full Restoration coverage:
├─ 6 spells in Restoration = at least 6 points
└─ Limits other schools to 18 points total
```

---

### **Spell Mode Configuration**

**For Spells with Mode Toggle:**

When equipping a spell with toggle capability, set its mode in loadout:

```
DA_Fireball (has toggle):
├─ Mode: Elemental ✅ (60 damage, 12 status buildup)
└─ OR Raw (75 damage, no status)

Set in loadout, cannot change in battle
```

**Why Set in Loadout:**
- Strategic preparation (know your damage output)
- Build synergy (status buildup builds vs raw damage builds)
- Clear tactical identity

**Not All Spells Have Toggles:**
```
Spells with toggle: 8 spells (like Fireball, Flame Dart)
Spells without toggle: 16 spells (like Inferno - pure elemental)

Only configure mode for spells that have it
```

---

## Loadout Configuration

### **Loadout Screen Workflow**

**Main Loadout Hub:**

**1. Ability Selection**
```
Available: Show all unlocked abilities
Equipped: 6 slots (drag-drop or select)
Order: Slot 1-6 for quick access
Preview: Show penalties based on current stats
```

**2. Spell Selection (Per School)**
```
School: Destruction
├─ Available: All Destruction spells (element + universal + evolution)
├─ Budget: 6/24 points used
├─ Equipped: 2 spells
│   ├─ Fireball (1 point, Elemental mode)
│   └─ Meteor Strike (4 points, no toggle)
└─ Remaining: 18 points

Repeat for Enhancement, Restoration, Conjuration
Total: Must be ≤ 24 points
```

**3. Item Selection**
```
Crystal Type: Garnet (Fire damage)
├─ Stack 1: S-tier (220 damage)
├─ Stack 2: A-tier (180 damage)
├─ Stack 3: F-tier (60 damage)
└─ Order: Consumed 1 → 2 → 3

Repeat for 5 more crystal types (6 total)
```

**4. Ultimate Selection**
```
Available: All unlocked ultimates
Equipped: "Inferno Cataclysm"
Can swap to any other unlocked ultimate
```

**5. Preview Panel** (Always Visible)
```
Current Stats:
├─ Mind: 46 (base 36 + evolution 10)
├─ Body: 9 (base 14 - evolution 5)
└─ Spirit: 50 (base 25 + evolution 25)

Equipped Action Penalties:
├─ Heavy Strike: 31.6% penalty (10 Body deficit)
├─ Meteor Strike: 15% penalty (5 Mind deficit)
└─ Quick Strike: No penalty

Point Budget:
├─ Spell Points: 18/24 used
├─ Ability Slots: 6/6 used
└─ Item Slots: 6/6 used
```

---

### **Loadout Presets**

**Multiple Named Loadouts:**

Players can save multiple loadout configurations:

```
Loadout 1: "PVE Boss Killer"
├─ Abilities: Heavy Strike, Drain Strike, Fortify
├─ Spells: All Master Destruction
├─ Items: Healing-focused
└─ Ultimate: Max damage

Loadout 2: "PVP Arena"
├─ Abilities: Quick Strike, Weaken, Pressure Point
├─ Spells: Balanced schools
├─ Items: Energy + debuff removal
└─ Ultimate: Control-focused

Loadout 3: "Exploration"
├─ Abilities: Versatile mix
├─ Spells: Many Basic (quantity)
├─ Items: Mixed utility
└─ Ultimate: General purpose
```

**Preset Features:**
- Unlimited presets (or cap at 10?)
- Custom names (player chooses)
- Quick swap before battle
- Copy/duplicate presets
- Share presets? (export/import codes?)

---

### **Configuration Rules**

**What You CAN Do:**
- ✅ Equip abilities you don't meet requirements for (penalties apply)
- ✅ Equip spells you don't meet requirements for (penalties apply)
- ✅ Leave slots empty (don't have to use all 6 abilities)
- ✅ Spend fewer than 24 spell points (doesn't force full use)
- ✅ Equip only 1-2 item types (don't need all 6 slots filled)
- ✅ Swap loadouts freely outside battle
- ✅ Change ultimate between loadouts

**What You CANNOT Do:**
- ❌ Change element (permanent from creation)
- ❌ Exceed 24 spell points
- ❌ Equip 2 items of same tier in same slot
- ❌ Change loadout during battle
- ❌ Equip spells from different element (unless universal/evolution)
- ❌ Have more than 6 abilities
- ❌ Have more than 6 item slots

---

## Validation & Requirements

### **Requirement Checking**

**Abilities & Spells Have Requirements:**
```
Heavy Strike:
├─ Requirements: Mind 5, Body 30, Spirit 15
├─ Your Stats: Mind 46, Body 9, Spirit 50
├─ Deficit: Body -21 (need 30, have 9)
└─ Penalty: 45.8% (sqrt(21) × 0.10 = 0.458)
```

**Validation Options:**

**Option 1: Soft Warning (Recommended)**
```
Can equip Heavy Strike
Warning shown: "⚠️ 45.8% penalty - Body too low"
Still usable, just weaker
Encourages optimization, doesn't block
```

**Option 2: Hard Block**
```
Cannot equip Heavy Strike
Error shown: "❌ Requires Body 30 (you have 9)"
Forces meeting requirements
Less flexible, more restrictive
```

**Recommendation:** Soft warning (matches combat system design)

---

### **Penalty Calculation Preview**

**Show Before Equipping:**

When hovering over ability/spell in selection screen:

```
Ability: Heavy Strike
Base Damage: 90
Base Energy: 25

YOUR STATS:
Requirements: Mind 5, Body 30, Spirit 15
Your Stats: Mind 46 ✓, Body 9 ✗, Spirit 50 ✓
Deficit: Body -21

WITH PENALTY:
Damage: 49 (90 × 0.542 = 48.8)
Energy: 37 (25 × 1.458 = 36.4)
Penalty: 45.8%

Warning: High penalty! Consider abilities with lower Body requirements.
```

**Visual Indicators:**
- 🟢 Green: No penalty (meet all requirements)
- 🟡 Yellow: Minor penalty (<20%)
- 🟠 Orange: Moderate penalty (20-40%)
- 🔴 Red: Heavy penalty (40-60%)
- ⛔ Capped: Maximum penalty (60%)

---

### **Point Budget Validation**

**Spell Point Tracker:**

Real-time calculation as you add/remove spells:

```
SPELL POINT BUDGET

Destruction: 12/24 points
├─ Meteor Strike (Master, 4 points)
├─ Inferno (Master, 4 points)
└─ Flame Wave (Advanced, 3 points)

Enhancement: 6/24 points
├─ Blazing Speed (Intermediate, 2 points)
├─ Flame Shield (Basic, 1 point)
└─ Burning Fury (Advanced, 3 points)

Restoration: 0/24 points
└─ (empty)

Conjuration: 0/24 points
└─ (empty)

TOTAL: 18/24 points used ✅
Remaining: 6 points

Can add:
├─ 1 Master spell OR
├─ 2 Advanced spells OR
├─ 3 Intermediate spells OR
└─ 6 Basic spells
```

**Over-Budget Prevention:**
```
Trying to add Flame Whip (Master, 4 points)
Current: 18 points used
New total: 22 points
Status: ✅ Allowed (under 24)

Trying to add Fire Elemental (Master, 4 points)
Current: 22 points used
New total: 26 points
Status: ❌ BLOCKED - Exceeds 24-point budget
Error: "Remove 2 points worth of spells to equip this"
```

---

## Preview System

### **Stat Preview Panel**

**Always Visible in Loadout Screen:**

```
═══════════════════════════════════════
CHARACTER STATS PREVIEW
═══════════════════════════════════════

BASE STATS:
├─ Mind: 36
├─ Body: 14
└─ Spirit: 25

EVOLUTION MODIFIERS:
├─ Mind: +10 (Eternal Flame)
├─ Body: -5 (Eternal Flame)
└─ Spirit: +25 (Eternal Flame)

SUB-STAT DISTRIBUTION:
Mind Sub-Stats (Total: 51 points):
├─ Spell Cost: 10
├─ Effect Damage: 20
└─ Crit Chance: 21

Body Sub-Stats (Total: 18 points):
├─ Defense: 0
├─ Attack Speed: 18
└─ Raw Damage: 0

Spirit Sub-Stats (Total: 40 points):
├─ Max Energy: 30
├─ Resistance: 0
└─ Ability Size: 10

FINAL STATS:
├─ Mind: 46
├─ Body: 9
├─ Spirit: 50
└─ Max Energy: 130 (base 100 + 30 from sub-stat)
```

---

### **Action Performance Preview**

**Per Equipped Action:**

```
═══════════════════════════════════════
EQUIPPED ABILITIES
═══════════════════════════════════════

[1] QUICK STRIKE
├─ Base: 50 damage, 15 energy
├─ Requirements: Mind 10, Body 15, Spirit 10
├─ Status: ✅ All requirements met
└─ Performance: 50 damage, 15 energy (NO PENALTY)

[2] HEAVY STRIKE
├─ Base: 90 damage, 25 energy
├─ Requirements: Mind 5, Body 30, Spirit 15
├─ Status: ❌ Body deficit: -21
├─ Penalty: 45.8%
└─ Performance: 49 damage, 37 energy ⚠️ HIGH PENALTY

[3] DRAIN STRIKE
├─ Base: 70 damage, 30 energy
├─ Requirements: Mind 18, Body 12, Spirit 22
├─ Status: ⚠️ Body deficit: -3
├─ Penalty: 17.3%
└─ Performance: 58 damage, 35 energy (minor penalty)

═══════════════════════════════════════
SPELL PERFORMANCE
═══════════════════════════════════════

DESTRUCTION SCHOOL:

[1] FIREBALL (Elemental Mode)
├─ Base: 60 damage, 25 energy, 12 status buildup
├─ Requirements: Mind 18, Body 10, Spirit 15
├─ Status: ✅ All requirements met
└─ Performance: 60 damage, 25 energy, 12 buildup

[2] METEOR STRIKE
├─ Base: 200 damage, 80 energy
├─ Requirements: Mind 22, Body 25, Spirit 28
├─ Status: ⚠️ Body deficit: -16
├─ Penalty: 40.0%
└─ Performance: 120 damage, 112 energy ⚠️ HIGH PENALTY

ENHANCEMENT SCHOOL: (collapsed)
RESTORATION SCHOOL: (collapsed)
CONJURATION SCHOOL: (collapsed)
```

---

### **Comparison Tools**

**Compare Loadouts:**

```
LOADOUT COMPARISON

                    PVE Boss    |    PVP Arena
─────────────────────────────────────────────────
Abilities:
├─ Heavy Strike      ✅          |    ❌
├─ Quick Strike      ❌          |    ✅
├─ Pressure Point    ❌          |    ✅

Spell Points:
├─ Destruction       18          |    6
├─ Enhancement       3           |    6
├─ Restoration       3           |    6
└─ Conjuration       0           |    6

Total Spells:        11          |    16
Avg Spell Power:     High        |    Medium

Items:
├─ Sapphire          ✅          |    ✅
├─ Citrine           ✅          |    ✅
├─ Garnet            ✅          |    ❌
├─ Iolite            ❌          |    ✅

Ultimate:
└─ Selected          Inferno     |    Solar Eclipse

Strengths:
├─ Damage Output     🔴🔴🔴🔴🔴  |    🔴🔴🔴
├─ Versatility       🔴🔴        |    🔴🔴🔴🔴🔴
├─ Survivability     🔴🔴        |    🔴🔴🔴🔴
└─ Energy Efficiency 🔴🔴🔴      |    🔴🔴🔴🔴
```

---

## UI/UX Design

### **Loadout Screen Layout**

**Main Screen Structure:**

```
┌──────────────────────────────────────────────────────────────┐
│  LOADOUT CONFIGURATION - Fire Lord                          │
│  Preset: "PVE Boss Killer" [▼]                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────┐  ┌────────────────────────────┐   │
│  │  CHARACTER PREVIEW  │  │   STAT PREVIEW PANEL       │   │
│  │                     │  │                            │   │
│  │  [Character Model]  │  │  Mind: 46                  │   │
│  │                     │  │  Body: 9 ⚠️                 │   │
│  │  Fire Lord          │  │  Spirit: 50                │   │
│  │  Evolution: Eternal │  │                            │   │
│  │  Flame Active       │  │  Point Budget:             │   │
│  │                     │  │  Spells: 18/24             │   │
│  └─────────────────────┘  │  Abilities: 6/6            │   │
│                           │  Items: 6/6                │   │
│                           └────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  TABS: [Abilities] [Spells] [Items] [Ultimate]      │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │                                                       │  │
│  │  CURRENT TAB: ABILITIES                               │  │
│  │                                                       │  │
│  │  EQUIPPED (6/6):                AVAILABLE:           │  │
│  │  ┌──────────────┐               ┌──────────────┐     │  │
│  │  │ 1. Quick     │               │ Fortify      │     │  │
│  │  │    Strike ✅ │               │ (🟡 15% pen) │     │  │
│  │  ├──────────────┤               ├──────────────┤     │  │
│  │  │ 2. Heavy     │               │ Weaken       │     │  │
│  │  │    Strike ⚠️ │               │ (🟢 No pen)  │     │  │
│  │  ├──────────────┤               ├──────────────┤     │  │
│  │  │ 3. Drain     │               │ 16 Hit Combo │     │  │
│  │  │    Strike 🟡 │               │ (🟢 No pen)  │     │  │
│  │  ├──────────────┤               └──────────────┘     │  │
│  │  │ 4. Tactical  │                                    │  │
│  │  │    Strike ✅ │               [Add Ability]        │  │
│  │  ├──────────────┤                                    │  │
│  │  │ 5. Focus ✅  │                                    │  │
│  │  ├──────────────┤                                    │  │
│  │  │ 6. Whirlwind │                                    │  │
│  │  │    🔴        │                                    │  │
│  │  └──────────────┘                                    │  │
│  │                                                       │  │
│  │  [▼] Show Performance Preview                        │  │
│  │                                                       │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
│  [Save Loadout] [Load Preset] [Reset] [Start Battle]       │
└──────────────────────────────────────────────────────────────┘
```

---

### **Spell Selection Screen**

**School-Based Organization:**

```
┌──────────────────────────────────────────────────────────────┐
│  SPELL SELECTION - Point Budget: 18/24                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SCHOOL: [Destruction] [Enhancement] [Restoration] [Conjur] │
│                                                              │
│  DESTRUCTION SPELLS (12/24 points used)                     │
│                                                              │
│  EQUIPPED:                          AVAILABLE:              │
│  ┌─────────────────────────────┐   ┌────────────────────┐  │
│  │ Meteor Strike (Master) 🔴   │   │ Ember Shot (Basic) │  │
│  │ ├─ Cost: 4 points           │   │ ├─ Cost: 1 point   │  │
│  │ ├─ Mode: N/A (no toggle)    │   │ ├─ Mode: [Elem▼]   │  │
│  │ ├─ Penalty: 40%             │   │ └─ Penalty: None   │  │
│  │ └─ Damage: 120 (200 base)   │   └────────────────────┘  │
│  ├─────────────────────────────┤                            │
│  │ Inferno (Master) 🟡         │   ┌────────────────────┐  │
│  │ ├─ Cost: 4 points           │   │ Fireball (Basic) ✅│  │
│  │ ├─ Mode: N/A                │   │ ├─ Cost: 1 point   │  │
│  │ ├─ Penalty: 15%             │   │ ├─ Mode: [Elem▼]   │  │
│  │ └─ Damage: 140 (165 base)   │   │ └─ Already Equipd │  │
│  ├─────────────────────────────┤   └────────────────────┘  │
│  │ Flame Wave (Advanced) ✅    │                            │
│  │ ├─ Cost: 3 points           │   [Filter: All▼]          │
│  │ ├─ Mode: [Elemental ✅]     │   [Sort: Power▼]          │
│  │ ├─ Penalty: None            │                            │
│  │ └─ Damage: 80               │   🔴 Master (4 pts)       │
│  └─────────────────────────────┘   🟠 Advanced (3 pts)     │
│                                     🟡 Intermediate (2 pts) │
│  Points: 12/24 (12 remaining)      🟢 Basic (1 pt)         │
│  Spells: 3 equipped                                        │
│                                                              │
│  [Add Spell] [Remove Selected] [Configure Modes]            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

### **Item Selection Screen**

```
┌──────────────────────────────────────────────────────────────┐
│  ITEM LOADOUT - 6 Slots, 3 Stacks Each                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SLOT 1: GARNET (Fire Damage)                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Stack 1: [S-tier ▼] 220 damage + burn                │   │
│  │ Stack 2: [A-tier ▼] 180 damage                       │   │
│  │ Stack 3: [F-tier ▼] 60 damage                        │   │
│  │                                                       │   │
│  │ ⚠️ Cannot have duplicate tiers                        │   │
│  │ Order consumed: 1 → 2 → 3                            │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  SLOT 2: SAPPHIRE (Healing)                                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Stack 1: [S-tier ▼] 160 HP                           │   │
│  │ Stack 2: [C-tier ▼] 90 HP                            │   │
│  │ Stack 3: [F-tier ▼] 40 HP                            │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  SLOT 3: CITRINE (Energy Restore)                           │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Stack 1: [S-tier ▼] 100 energy, 30 self-damage       │   │
│  │ Stack 2: [A-tier ▼] 85 energy, 20 self-damage        │   │
│  │ Stack 3: [Empty    ]                                  │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  SLOT 4-6: [Collapsed - Click to expand]                    │
│                                                              │
│  INVENTORY (Available Items):                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Garnet: S×1, A×2, F×5                                 │   │
│  │ Sapphire: S×1, C×3, F×4                               │   │
│  │ Citrine: S×1, A×1                                     │   │
│  │ Opal: B×2, F×1                                        │   │
│  │ ... (more crystals)                                   │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

### **Performance Preview Hover**

**When hovering over ability/spell:**

```
┌─────────────────────────────────────┐
│ HEAVY STRIKE (Ability)              │
├─────────────────────────────────────┤
│ BASE STATS:                         │
│ ├─ Damage: 90                       │
│ ├─ Energy Cost: 25                  │
│ └─ Hit Count: 1                     │
│                                     │
│ REQUIREMENTS:                       │
│ ├─ Mind: 5 (You: 46) ✅             │
│ ├─ Body: 30 (You: 9) ❌ -21 deficit │
│ └─ Spirit: 15 (You: 50) ✅          │
│                                     │
│ PERFORMANCE WITH YOUR STATS:        │
│ ├─ Damage: 49 (-45.8%)              │
│ ├─ Energy: 37 (+45.8%)              │
│ └─ Penalty: 45.8% 🔴                │
│                                     │
│ WARNING: High penalty due to low    │
│ Body stat. Consider alternatives.   │
│                                     │
│ [Equip Anyway] [Find Alternative]   │
└─────────────────────────────────────┘
```

---

### **Mobile/Controller Support**

**Considerations:**
- Touch-friendly buttons (large hit areas)
- Controller navigation (D-pad, triggers)
- Quick selection (shoulder buttons cycle)
- Contextual menus (hold for options)

---

## Technical Implementation

### **Loadout Data Structure**

```cpp
// LoadoutData.h
USTRUCT(BlueprintType)
struct FLoadoutData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LoadoutName = "Untitled Loadout";
    
    // Abilities (max 6)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UAbilityData*> EquippedAbilities;
    
    // Spells per school
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEquippedSpell> DestructionSpells;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEquippedSpell> EnhancementSpells;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEquippedSpell> RestorationSpells;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FEquippedSpell> ConjurationSpells;
    
    // Items
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FItemSlot> ItemSlots; // Max 6
    
    // Ultimate
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UUltimateData* EquippedUltimate = nullptr;
    
    // Validation
    bool IsValid() const;
    int32 GetTotalSpellPoints() const;
};

USTRUCT(BlueprintType)
struct FEquippedSpell
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USpellData* Spell = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bElementalMode = true; // For toggle spells
    
    int32 GetPointCost() const;
};

USTRUCT(BlueprintType)
struct FItemSlot
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECrystalType CrystalType;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UItemData*> Stacks; // Max 3, different tiers
};
```

---

### **Character Data Integration**

```cpp
// CharacterData.h additions
UCLASS()
class UCharacterData : public UPrimaryDataAsset
{
    // ... existing fields
    
    // Element (permanent)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement InnateElement;
    
    // Sub-stat distribution (30 starting points)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 SpellCost = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 EffectDamage = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 CritChance = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 Defense = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 AttackSpeed = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 RawDamage = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 MaxEnergy = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 Resistance = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats")
    int32 AbilitySize = 0;
    
    // World stats (discovered in-game)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Stats")
    int32 MindWorldStat = 1; // 1-7
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Stats")
    int32 BodyWorldStat = 1; // 1-7
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Stats")
    int32 SpiritWorldStat = 1; // 1-7
    
    // Active loadout
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    FLoadoutData CurrentLoadout;
    
    // Saved loadouts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
    TArray<FLoadoutData> SavedLoadouts;
    
    // Functions
    UFUNCTION(BlueprintCallable)
    void ApplyLoadout(const FLoadoutData& Loadout);
    
    UFUNCTION(BlueprintCallable)
    bool ValidateLoadout(const FLoadoutData& Loadout, FString& OutErrorMessage);
    
    UFUNCTION(BlueprintPure)
    int32 GetTotalSubStatPoints() const;
};
```

---

### **Loadout Manager**

```cpp
// LoadoutManager.h
UCLASS()
class ULoadoutManager : public UObject
{
    GENERATED_BODY()

public:
    // Validation
    UFUNCTION(BlueprintCallable)
    bool ValidateAbilitySelection(const TArray<UAbilityData*>& Abilities, FString& OutError);
    
    UFUNCTION(BlueprintCallable)
    bool ValidateSpellSelection(const TArray<FEquippedSpell>& Spells, FString& OutError);
    
    UFUNCTION(BlueprintCallable)
    bool ValidateItemSelection(const TArray<FItemSlot>& Items, FString& OutError);
    
    // Point calculation
    UFUNCTION(BlueprintPure)
    int32 CalculateSpellPoints(const TArray<FEquippedSpell>& Spells);
    
    // Preview calculations
    UFUNCTION(BlueprintPure)
    FAbilityPerformancePreview CalculateAbilityPerformance(
        UAbilityData* Ability, 
        UCharacterData* Character
    );
    
    UFUNCTION(BlueprintPure)
    FSpellPerformancePreview CalculateSpellPerformance(
        USpellData* Spell,
        UCharacterData* Character,
        bool bElementalMode
    );
    
    // Loadout operations
    UFUNCTION(BlueprintCallable)
    void SaveLoadout(UCharacterData* Character, const FLoadoutData& Loadout);
    
    UFUNCTION(BlueprintCallable)
    void LoadLoadout(UCharacterData* Character, int32 LoadoutIndex);
    
    UFUNCTION(BlueprintCallable)
    void DeleteLoadout(UCharacterData* Character, int32 LoadoutIndex);
};
```

---

### **Spell Point Tier System**

```cpp
// In SpellData.h
UENUM(BlueprintType)
enum class ESpellTier : uint8
{
    Basic           UMETA(DisplayName = "Basic (1 point)"),
    Intermediate    UMETA(DisplayName = "Intermediate (2 points)"),
    Advanced        UMETA(DisplayName = "Advanced (3 points)"),
    Master          UMETA(DisplayName = "Master (4 points)")
};

// In SpellData class
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
ESpellTier Tier = ESpellTier::Basic;

UFUNCTION(BlueprintPure, Category = "Spell|Points")
int32 GetPointCost() const
{
    switch(Tier)
    {
        case ESpellTier::Basic: return 1;
        case ESpellTier::Intermediate: return 2;
        case ESpellTier::Advanced: return 3;
        case ESpellTier::Master: return 4;
        default: return 1;
    }
}
```

---

### **Constants**

```cpp
// In CombatConstants.h
namespace LoadoutConstants
{
    // Ability limits
    constexpr int32 MAX_EQUIPPED_ABILITIES = 6;
    
    // Spell point budget
    constexpr int32 TOTAL_SPELL_POINT_BUDGET = 24;
    constexpr int32 BASIC_SPELL_COST = 1;
    constexpr int32 INTERMEDIATE_SPELL_COST = 2;
    constexpr int32 ADVANCED_SPELL_COST = 3;
    constexpr int32 MASTER_SPELL_COST = 4;
    
    // Item limits
    constexpr int32 MAX_ITEM_SLOTS = 6;
    constexpr int32 MAX_STACKS_PER_SLOT = 3;
    
    // Character creation
    constexpr int32 STARTING_SUB_STAT_POINTS = 30;
    constexpr int32 STARTING_WORLD_STAT_LEVEL = 1;
    constexpr int32 MAX_WORLD_STAT_LEVEL = 7;
    constexpr int32 POINTS_PER_WORLD_STAT_LEVEL = 3;
    
    // Loadout presets
    constexpr int32 MAX_SAVED_LOADOUTS = 10; // Or unlimited?
}
```

---

## Balance Considerations

### **30-Point Starting Distribution**

**Is 30 balanced?**

**Total possible sub-stat points:**
```
Starting distribution: 30 points
World stat level 1 (3×3): 9 points
World stat progression (6 levels × 3 × 3): 54 points
TOTAL MAX: 93 points
```

**30 points = 32% of total maximum**

**Feels right:**
- Meaningful starting customization
- Leaves room for growth (68% to discover)
- Can specialize (30 in one stat) or balance (3-4 per stat)
- Encourages different builds from creation

**Comparison:**
```
All 30 in Effect Damage:
├─ Massive status/spell damage from start
├─ Glass cannon extreme
└─ Fun but risky

10/10/10 spread:
├─ Solid foundation across pillars
├─ Room to specialize with world stats
└─ Safe, flexible approach
```

---

### **24-Point Spell Budget**

**Is 24 balanced?**

**Spell quantity vs quality:**
```
All Basic (24 spells):
├─ Maximum versatility
├─ Many options
└─ All weak individually

All Master (6 spells):
├─ Maximum power
├─ Few options
└─ Limited flexibility

Balanced (12-16 spells):
├─ Mix of power and variety
├─ Strategic depth
└─ Most common build style
```

**Forces meaningful choices:**
- Can't have everything
- Must prioritize schools
- Must balance power vs quantity
- Different builds emerge naturally

**Seems balanced!**

---

### **Ability Limit (6)**

**Why 6 abilities?**

**Considerations:**
- 10 total abilities available
- Must choose 6 (60% of pool)
- Can't bring everything
- Forces strategic picks

**Alternative limits:**
```
4 abilities: Too restrictive (40% of pool)
8 abilities: Too permissive (80% of pool)
6 abilities: Just right (60% of pool)
```

**UI consideration:**
- 6 fits well on screen (2 rows of 3)
- Easy to remember keybinds (1-6)
- Not overwhelming in combat

**Balanced!**

---

### **Item Tiers (No Duplicates)**

**Why no duplicate tiers?**

**Prevents:**
```
3 × S-tier Sapphire = 480 HP (broken!)
3 × S-tier Garnet = 660 damage (broken!)
```

**Encourages:**
```
S-tier for emergencies
A-tier for general use
F-tier for minor needs
Strategic tier usage
```

**Balanced!**

---

### **Evolution Remembrance (70% Power)**

**Weakened vs Original:**

```
Original Ultimate: 350 damage
Remembered Ultimate: 245 damage (70%)

Original Spell: Permanent burn effect
Remembered Spell: 4-turn burn (limited)
```

**Why weaken?**
- Prevents collecting all evolutions for their spells
- Makes active evolution meaningful
- Remembrance = trophy, not power creep
- Balanced nostalgia

**Seems fair!**

---

## Summary

**The Loadout System is the strategic heart of World of Refraction:**

**Character Creation:**
- Element selection (permanent)
- 30-point sub-stat distribution (unique builds)
- Starting loadout (default or custom)

**Loadout Components:**
- 6 abilities (from 10+ pool)
- 24-point spell budget (quantity vs quality)
- 6 item slots, 3 stacks each
- 1 ultimate (swappable)

**Configuration:**
- Multiple named presets
- Spell mode settings (Elemental/Raw)
- Requirement validation (soft warnings)
- Performance preview (see penalties)

**Restrictions:**
- 24-point spell budget (forces choices)
- No duplicate item tiers (prevents abuse)
- Locked in battle (no mid-fight changes)
- Element locked (permanent from creation)

**Technical:**
- FLoadoutData structure
- LoadoutManager validation
- Preview calculation system
- Save/load presets

**Design Achievement:**
> "Every character is unique from creation, every loadout tells a story, every choice matters."

---

**End of Documentation**

Next Steps:
1. Create LoadoutManager.h/cpp
2. Build character creation UI
3. Build loadout configuration UI
4. Implement validation systems
5. Create preview calculation systems
6. Test with all character types (especially Broken Darkness)
7. Balance tuning based on playtesting