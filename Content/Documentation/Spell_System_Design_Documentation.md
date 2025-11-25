# Spell System Documentation
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 1.0 (Design Phase)  
**Status:** Design Complete - Ready for Implementation

---

## Table of Contents

1. [System Overview](#system-overview)
2. [The Three Auras](#the-three-auras)
3. [Spell Schools](#spell-schools)
4. [Mode Toggle System](#mode-toggle-system)
5. [Element System](#element-system)
6. [Requirements Framework](#requirements-framework)
7. [Cross-School Effects](#cross-school-effects)
8. [Spells vs Abilities](#spells-vs-abilities)
9. [Technical Design](#technical-design)
10. [Example Spell Catalog](#example-spell-catalog)
11. [Implementation Plan](#implementation-plan)

---

## System Overview

### What Are Spells?

**Spells (Refractions)** are elemental magic abilities locked to a character's innate element. Unlike abilities (which are universal and element-neutral), spells represent pure manipulation of elemental energy and require deep understanding of one's element.

### Key Features

- **Element-Locked:** Characters can ONLY cast spells of their innate element
- **Four Schools:** Destruction, Enhancement, Restoration, Conjuration
- **Mode Toggle:** Many spells can be configured as Elemental or Raw/Construct
- **Aura Concept:** Reflects how elemental energy is channeled
- **Stat Requirements:** Based on school type and complexity
- **No Generic Spells:** Generic element characters cannot cast spells (abilities only)

### Core Philosophy

> **"Elemental energy is neutral - how you manifest it defines the spell's nature."**

A Fire Lord can shape fire into:
- Pure flame (elemental damage + burns)
- Explosive force (raw kinetic damage)
- Solid constructs (conjured weapons)
- Enhancement energy (physical buffs)

The SAME elemental power, different applications.

---

## The Three Auras

**Auras** represent the fundamental ways elemental energy can be channeled. Understanding these concepts is key to the spell system's identity.

### Enhancement Aura

**Concept:** Elemental energy enhances physical capabilities

**Effect:**
- Buffs ALL Body sub-stats
  - Defense Points
  - Attack Speed Points  
  - Raw Damage Points
- Physical power amplification
- Your element empowers your body

**Visual:**
- Energy flows through muscles
- Heightened physical presence
- Body glows with elemental power

**Example Spells:**
- **Blazing Strength** (Fire) - Explosive power boost
- **Flowing Grace** (Water) - Fluid motion enhancement
- **Stone Skin** (Earth) - Hardened defense

**Lore:**
> "The warrior channels fire not as flame, but as raw power coursing through their veins."

---

### Elemental Aura

**Concept:** Pure elemental energy surrounds and protects

**Effect:**
- Element coats character's body
- Damages attackers on ANY physical contact
- Triggers on: attacks, parries, blocks, dodges
- Defensive barrier with offensive punishment

**Visual:**
- Flames dance around body (Fire)
- Water swirls protectively (Water)
- Crackling electricity (Lightning)

**Example Spells:**
- **Flame Shroud** (Fire) - Burning aura
- **Frost Armor** (Water) - Chilling shield
- **Storm Cloak** (Lightning) - Shocking barrier

**Lore:**
> "To strike them is to strike the flame itself."

---

### Manipulation Aura

**Concept:** Precise control and shaping of element

**Effect:**
- Buffs ALL Mind sub-stats
  - Cost Reduction Points
  - Turn Speed Points
  - Crit Chance Points
- Mental clarity and tactical superiority
- Creative elemental control

**Visual:**
- Refined, controlled energy
- Geometric patterns
- Precision movements

**Example Spells:**
- **Tactical Insight** (Fire) - Enhanced focus
- **Fluid Thinking** (Water) - Adaptive mind
- **Calculated Strike** (Lightning) - Perfect timing

**Lore:**
> "The master does not fight harder - they fight smarter."

---

## Spell Schools

### Destruction

**Focus:** Offensive magic, damage output

**Primary Stats:**
- **Body:** Power of the attack
- **Spirit:** Elemental intensity

**Spell Types:**
- Direct damage (Fireball, Lightning Bolt)
- Area damage (Flame Wave, Blizzard)
- Damage-over-time (Burn, Poison Cloud)

**Mode Toggle:** ✅ Available
- **Elemental Mode:** Lower damage, applies status effects
- **Raw Mode:** Higher damage, no status effects

**Character Fit:**
- Water Lord (Body 33, Spirit 17) - Good for raw mode
- Fire Lord (Spirit 25) - Better for elemental mode

---

### Enhancement

**Focus:** Buffs, combat augmentation

**Primary Stats:**
- **Mind:** Understanding of magic
- **Spirit:** Sustaining the enhancement

**Spell Types:**
- Self buffs (Flame Aura, Stone Skin)
- Ally buffs (Group Enhancement)
- Combat modifications (Blazing Speed)

**Mode Toggle:** ❌ Not available (pure buffs)

**Cross-School Note:**
> Enhancement spells can debuff enemies! Example: "Searing Weakness" (Fire Enhancement) - lowers enemy's defense

**Character Fit:**
- Fire Lord (Mind 36, Spirit 25) - Excellent
- Water Lord (Mind 25, Spirit 17) - Moderate

---

### Restoration

**Focus:** Healing, support, cleansing

**Primary Stats:**
- **Mind:** Control and precision
- **Spirit:** Energy to heal/restore

**Spell Types:**
- Health restoration (Warmth, Healing Spring)
- Energy restoration (Mana Surge)
- Status cleansing (Purifying Flame)
- Regeneration (Phoenix Rebirth)

**Mode Toggle:** ❌ Not available (pure support)

**Cross-School Note:**
> Restoration spells can damage! Example: "Dehydration" (Fire Restoration) - drains enemy energy while restoring yours

**Character Fit:**
- Fire Lord (Mind 36, Spirit 25) - Excellent healer
- Water Lord (Mind 25, Spirit 17) - Moderate healer

---

### Conjuration

**Focus:** Summoning, creation, constructs

**Primary Stats:**
- **Mind:** Complexity of creation
- **Spirit:** Maintaining existence

**Spell Types:**
- Summons (Fire Elemental, Ice Golem)
- Constructs (Fire Spear, Water Wall)
- Barriers (Flame Barrier, Stone Wall)
- Decoys (Ember Clone, Water Mirror)

**Mode Toggle:** ✅ Available
- **Elemental Mode:** Living/burning construct, applies status
- **Construct Mode:** Solid physical form, higher damage

**Character Fit:**
- Fire Lord (Mind 36, Spirit 25) - Master conjurer
- Water Lord (Mind 25, Spirit 17) - Decent conjurer

---

## Mode Toggle System

### Overview

**The Innovation:** Players configure spell modes during loadout creation, NOT in combat.

**Philosophy:**
> "I've trained this spell to manifest as pure elemental force."  
> vs  
> "I've practiced this spell as raw kinetic power."

### How It Works

#### **During Loadout Configuration:**
```
[Equip Fireball]
  
  Mode Selection:
  ○ Elemental Mode
     - Damage: 70
     - Energy: 25
     - Effect: High burn buildup
  
  ● Raw Mode (Selected)
     - Damage: 90
     - Energy: 20
     - Effect: None
  
  [Confirm]
```

#### **In Combat:**
- Cast the spell as configured
- No mode switching mid-battle
- Reflects your training/preparation

### Which Spells Have Toggles?

**✅ Destruction Spells:**
- **Elemental Mode:** Damage + status effects
- **Raw Mode:** Pure damage, no effects

**✅ Conjuration Spells:**
- **Elemental Mode:** Living/burning construct + status
- **Construct Mode:** Solid physical form, higher damage

**❌ Enhancement Spells:**
- Pure buffs/debuffs
- No mode variations

**❌ Restoration Spells:**
- Pure healing/support
- No mode variations

### Design Rationale

**Why Loadout-Time, Not Combat-Time?**

1. **Lore Consistency**
   - Reflects how you've trained the spell
   - Magic mastery takes practice
   - Can't change mid-fight (different from ability infusion)

2. **Strategic Depth**
   - Pre-mission planning matters
   - Build loadout for specific encounters
   - Preparation vs improvisation

3. **Differentiation from Abilities**
   - Abilities: In-combat infusion choice
   - Spells: Pre-configured manifestation
   - Two different tactical layers

4. **Simplicity in Combat**
   - One button = one action
   - No toggle UI during intense fights
   - Cleaner combat flow

### Mode Comparison

| Aspect             | Elemental Mode         | Raw/Construct Mode   |
| ------------------ | ---------------------- | -------------------- |
| **Damage**         | Lower (70-80% of raw)  | Higher (100%)        |
| **Energy Cost**    | Higher (+20-30%)       | Lower (base cost)    |
| **Status Buildup** | Yes (applies effects)  | No                   |
| **Use Case**       | Status effect focus    | Pure damage focus    |
| **Enemy Type**     | Neutral/weak to status | Resistant to element |
| **Build Synergy**  | High Spirit builds     | High Body builds     |

---

## Element System

### Element Access Rules

**Strict Element Lock:**
- Fire Lord → Fire spells ONLY
- Water Lord → Water spells ONLY
- Earth Lord → Earth spells ONLY
- Generic → NO SPELLS (abilities only)

**Rationale:**
- Clear character identity
- Elemental mastery takes dedication
- Generic characters trade spell access for universal ability proficiency

### The 11 Elements

**Primary Elements:**
1. **Fire** - Damage, aggression, burns
2. **Water** - Control, adaptation, chills
3. **Earth** - Defense, stability, slow
4. **Wind** - Speed, mobility, knockback

**Advanced Elements:**
5. **Light** - Healing, revelation, blind
6. **Darkness** - Debuffs, drain, fear
7. **Lightning** - Speed, precision, shock
8. **Void** - Negation, silence, void
9. **Reality** - Manipulation, chaos, ???

**Special Elements:**
10. **Generic** - No spells, all abilities
11. **Broken Darkness** - Unique mechanics

### Element-School Combinations

**Every element can access all four schools, but with different flavor:**

**Fire Examples:**
- Destruction: Fireball, Inferno
- Enhancement: Blazing Strength
- Restoration: Warmth, Phoenix Rebirth
- Conjuration: Fire Elemental, Flame Spear

**Water Examples:**
- Destruction: Ice Shard, Blizzard
- Enhancement: Flowing Grace
- Restoration: Healing Spring
- Conjuration: Water Wall, Ice Golem

**Not all combinations are equal:**
- Fire Restoration feels less natural than Water Restoration
- But ALL are possible (creative design space!)

---

## Requirements Framework

### School-Based Requirements

Similar to abilities, spells have stat requirements:

**Destruction Spells:**
- **Body:** 10-30 (attack power)
- **Spirit:** 15-35 (elemental intensity)
- **Mind:** 0-15 (usually low)

**Enhancement Spells:**
- **Mind:** 15-30 (understanding)
- **Spirit:** 15-30 (sustaining buff)
- **Body:** 0-10 (usually low)

**Restoration Spells:**
- **Mind:** 15-30 (precision)
- **Spirit:** 20-35 (energy to heal)
- **Body:** 0-10 (usually low)

**Conjuration Spells:**
- **Mind:** 20-35 (complexity)
- **Spirit:** 15-30 (maintaining form)
- **Body:** 0-15 (usually low)

### Power Tiers

**Basic Spells:**
- Requirements: 10-20 per stat
- Accessible to most characters
- Foundation spells

**Advanced Spells:**
- Requirements: 20-30 per stat
- Specialized builds excel
- Core combat spells

**Master Spells:**
- Requirements: 30-40 per stat
- Build-defining
- Ultimate abilities

### Penalty System

**Same as Ability System:**
```cpp
Penalty = sqrt(total_deficit) × 0.10
Capped at 60%
```

**Fire Lord using Water Lord's spells?**
- Can't! Element-locked.

**Fire Lord using high-Body Destruction spell?**
- Can cast, but with penalties if Body too low
- Same penalty mechanics as abilities

---

## Cross-School Effects

### The Key Innovation

**Spell schools are NOT limited to their primary function.**

**Enhancement Can Debuff:**
- "Searing Weakness" (Fire Enhancement)
- Lowers enemy defense
- Enhancement school, debuff effect

**Restoration Can Damage:**
- "Dehydration" (Fire Restoration)
- Drains enemy energy
- Restoration school, harmful effect
- Restores YOUR energy

**Destruction Can Buff:**
- "Explosive Fury" (Fire Destruction)
- Deal damage + self damage buff
- Destruction school, buff side-effect

**Conjuration Can Heal:**
- "Healing Construct" (Water Conjuration)
- Summons healing entity
- Conjuration school, restoration effect

### Design Philosophy

> **"Schools define HOW you use elemental energy, not WHAT the effect must be."**

**Destruction** = Offensive application (usually damage, but not always)  
**Enhancement** = Modification application (usually buffs, but not always)  
**Restoration** = Restorative application (usually healing, but not always)  
**Conjuration** = Creation application (usually summons, but not always)

### Implementation

**In SpellData:**
```cpp
ESpellSchool School;  // Defines stat requirements

// But effects are flexible:
EAbilityEffectType PrimaryEffect;   // Main effect
EAbilityEffectType SecondaryEffect; // Optional cross-school effect
```

**Example - Dehydration:**
```cpp
School: Restoration (Mind 20, Spirit 25)
PrimaryEffect: EnergyRestore (restore 20 to self)
SecondaryEffect: EnergyDrain (drain 15 from enemy)
```

---

## Spells vs Abilities

### Direct Comparison

| Aspect              | Abilities                  | Spells                 |
| ------------------- | -------------------------- | ---------------------- |
| **Access**          | Universal (all characters) | Element-locked         |
| **Nature**          | Physical/tactical          | Elemental/magical      |
| **Infusion**        | Can infuse with element    | Already elemental      |
| **Configuration**   | In-combat choice           | Loadout-time choice    |
| **Requirements**    | Mind/Body/Spirit           | School-based stats     |
| **Status Effects**  | Only when infused          | Elemental mode applies |
| **Generic Element** | Can use all                | Cannot use any         |

### Tactical Layers

**Abilities:**
- "Do I infuse this Quick Strike right now?"
- In-combat decision
- Tactical flexibility

**Spells:**
- "How did I train this Fireball?"
- Pre-battle decision
- Strategic preparation

**Both:**
- Stat requirement penalties
- Effect system
- Character optimization

### When to Use Each?

**Use Abilities When:**
- Need elemental flexibility (infuse or not?)
- Fighting multiple enemy types
- Want tactical options
- Generic character (only option!)

**Use Spells When:**
- Fighting specific enemy type (configured for it)
- Playing to elemental strengths
- Need specialized elemental effects
- Have pre-battle information

---

## Technical Design

### SpellData Structure (Proposed)

```cpp
UCLASS(BlueprintType)
class USpellData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString SpellName;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement Element;  // Which element is this?
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ESpellSchool School;  // Destruction/Enhancement/Restoration/Conjuration
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Description;
    
    // ==================== MODE TOGGLE ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode")
    bool bHasModeToggle;  // Can this spell be configured?
    
    // ELEMENTAL MODE (if toggle available)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeDamage;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeEnergyCost;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Elemental", meta = (EditCondition = "bHasModeToggle"))
    int32 ElementalModeStatusBuildup;
    
    // RAW/CONSTRUCT MODE (if toggle available)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Raw", meta = (EditCondition = "bHasModeToggle"))
    int32 RawModeDamage;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode|Raw", meta = (EditCondition = "bHasModeToggle"))
    int32 RawModeEnergyCost;
    
    // NO MODE TOGGLE (single configuration)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (EditCondition = "!bHasModeToggle"))
    int32 BaseDamage;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (EditCondition = "!bHasModeToggle"))
    int32 BaseEnergyCost;
    
    // ==================== COMMON STATS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 HitCount;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    ETargetType TargetType;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float CastTime;  // Future: multi-turn casting
    
    // ==================== REQUIREMENTS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    int32 RequiredMind;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    int32 RequiredBody;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    int32 RequiredSpirit;
    
    // ==================== EFFECTS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    EAbilityEffectType PrimaryEffect;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    float PrimaryEffectMagnitude;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    int32 PrimaryEffectDuration;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    int32 PrimaryEffectValue;
    
    // Secondary effect (cross-school)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    EAbilityEffectType SecondaryEffect;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    float SecondaryEffectMagnitude;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    int32 SecondaryEffectDuration;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects|Secondary")
    int32 SecondaryEffectValue;
    
    // ==================== VISUALS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UAnimMontage* CastAnimation;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem* ElementalModeEffect;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    UParticleSystem* RawModeEffect;
    
    // ==================== CALCULATIONS ====================
    
    UFUNCTION(BlueprintPure, Category = "Spell|Calculations")
    int32 CalculateDamage(UCharacterData* Character, bool bUsingElementalMode) const;
    
    UFUNCTION(BlueprintPure, Category = "Spell|Calculations")
    int32 CalculateEnergyCost(UCharacterData* Character, bool bUsingElementalMode) const;
    
    UFUNCTION(BlueprintPure, Category = "Spell|Calculations")
    int32 CalculateStatusBuildup(UCharacterData* Character) const;
    
    UFUNCTION(BlueprintPure, Category = "Spell|Requirements")
    bool MeetsRequirements(UCharacterData* Character) const;
    
    UFUNCTION(BlueprintPure, Category = "Spell|Requirements")
    float CalculateRequirementPenalty(UCharacterData* Character) const;
};
```

### New Enums Needed

```cpp
UENUM(BlueprintType)
enum class ESpellSchool : uint8
{
    Destruction    UMETA(DisplayName = "Destruction"),
    Enhancement    UMETA(DisplayName = "Enhancement"),
    Restoration    UMETA(DisplayName = "Restoration"),
    Conjuration    UMETA(DisplayName = "Conjuration")
};
```

### Loadout System

```cpp
// In CharacterData or separate loadout class
struct FEquippedSpell
{
    UPROPERTY()
    USpellData* Spell;
    
    UPROPERTY()
    bool bUsingElementalMode;  // Player's loadout choice
};

UPROPERTY()
TArray<FEquippedSpell> EquippedSpells;
```

---

## Example Spell Catalog

### Fire - Destruction

#### **Fireball**

**School:** Destruction  
**Description:** Classic fire spell. Hurl a ball of flame at your enemy.

**Mode Toggle:** ✅ Yes

**Elemental Mode:**
- Damage: 70
- Energy: 25
- Status Buildup: 15
- Effect: High burn chance

**Raw Mode:**
- Damage: 90
- Energy: 20
- No status buildup

**Requirements:**
- Mind: 10
- Body: 15
- Spirit: 20

**Target:** Single Enemy

---

#### **Inferno**

**School:** Destruction  
**Description:** Unleash devastating flames. Master-tier destruction.

**Mode Toggle:** ✅ Yes

**Elemental Mode:**
- Damage: 110
- Energy: 45
- Status Buildup: 25
- Effect: Guaranteed burn

**Raw Mode:**
- Damage: 140
- Energy: 35
- No status buildup

**Requirements:**
- Mind: 15
- Body: 25
- Spirit: 35

**Target:** Single Enemy

---

### Fire - Enhancement

#### **Blazing Strength**

**School:** Enhancement  
**Description:** Channel fire energy through your body, enhancing physical power.

**Mode Toggle:** ❌ No

**Stats:**
- Damage: 0
- Energy: 20
- Hit Count: 0

**Requirements:**
- Mind: 18
- Body: 10
- Spirit: 20

**Effect:**
- Type: Enhancement Aura
- Buffs: All Body sub-stats +20%
- Duration: 3 turns

**Target:** Self

---

#### **Searing Weakness**

**School:** Enhancement (Cross-School!)  
**Description:** Project fire energy to weaken enemy's defenses.

**Mode Toggle:** ❌ No

**Stats:**
- Damage: 30 (minor damage)
- Energy: 18

**Requirements:**
- Mind: 20
- Body: 8
- Spirit: 18

**Effects:**
- Primary: Defense Debuff (-25%)
- Secondary: Minor burn buildup (5)
- Duration: 2 turns

**Target:** Single Enemy

---

### Fire - Restoration

#### **Warmth**

**School:** Restoration  
**Description:** Comforting flames restore vitality over time.

**Mode Toggle:** ❌ No

**Stats:**
- Damage: 0
- Energy: 25
- Hit Count: 0

**Requirements:**
- Mind: 20
- Body: 0
- Spirit: 25

**Effect:**
- Type: Health Restore
- Value: 15 HP per turn
- Duration: 3 turns

**Target:** Self or Single Ally

---

#### **Dehydration** (Cross-School!)

**School:** Restoration  
**Description:** Searing flames drain enemy's stamina while restoring yours.

**Mode Toggle:** ❌ No

**Stats:**
- Damage: 40
- Energy: 22

**Requirements:**
- Mind: 22
- Body: 10
- Spirit: 25

**Effects:**
- Primary: Energy Restore (restore 20 to self)
- Secondary: Energy Drain (drain 15 from enemy)

**Target:** Single Enemy

---

### Fire - Conjuration

#### **Fire Spear**

**School:** Conjuration  
**Description:** Conjure a spear of flame. Can be solid construct or living fire.

**Mode Toggle:** ✅ Yes

**Elemental Mode:**
- Damage: 65
- Energy: 22
- Status Buildup: 12
- Effect: Flaming weapon, burns on hit

**Construct Mode:**
- Damage: 80
- Energy: 18
- No status buildup
- Effect: Solid piercing weapon

**Requirements:**
- Mind: 22
- Body: 15
- Spirit: 18

**Target:** Single Enemy

---

#### **Fire Elemental**

**School:** Conjuration  
**Description:** Summon a being of pure flame to fight alongside you.

**Mode Toggle:** ❌ No (always elemental)

**Stats:**
- Damage: 0 (summon acts independently)
- Energy: 40
- Hit Count: 0

**Requirements:**
- Mind: 30
- Body: 10
- Spirit: 28

**Effect:**
- Summons Fire Elemental ally
- Duration: 4 turns or until defeated
- Elemental has independent stats

**Target:** Self (summon appears near caster)

---

## Implementation Plan

### Phase 1: Core Structure
**Time Estimate:** 2-3 hours

1. **Create ESpellSchool enum**
   - Destruction, Enhancement, Restoration, Conjuration

2. **Create SpellData.h/cpp**
   - Mirror AbilityData structure
   - Add mode toggle fields
   - Add school-based requirements

3. **Create SpellDataDebug.h/cpp**
   - Copy from AbilityDataDebug
   - Adapt for spell-specific display

4. **Update CombatConstants**
   - Add spell-specific constants if needed

### Phase 2: First Test Spells
**Time Estimate:** 1-2 hours

Create 5 test spells (one per type):
1. Fireball (Destruction, with toggle)
2. Blazing Strength (Enhancement, no toggle)
3. Warmth (Restoration, no toggle)
4. Fire Spear (Conjuration, with toggle)
5. Dehydration (Restoration cross-school)

### Phase 3: Testing & Validation
**Time Estimate:** 1 hour

1. Test with BP_SpellDataTester (adapt from ability tester)
2. Verify mode toggles work
3. Test cross-school effects
4. Validate with Fire Lord stats

### Phase 4: Full Fire Spell Set
**Time Estimate:** 2-3 hours

Create complete Fire element spell catalog:
- 3-4 Destruction spells
- 3-4 Enhancement spells
- 3-4 Restoration spells
- 3-4 Conjuration spells

### Phase 5: Additional Elements
**Time Estimate:** Variable

Replicate pattern for:
- Water spells
- Earth spells
- Wind spells
- (Advanced elements later)

### Phase 6: Loadout System
**Time Estimate:** 3-4 hours

1. Add FEquippedSpell struct
2. Create spell loadout UI
3. Mode selection interface
4. Save/load configurations

---

## Testing Strategy

### Test Scenarios

#### **Scenario 1: Mode Toggle**
- Create Fireball with both modes
- Configure as Elemental in loadout
- Verify applies burn in combat
- Reconfigure as Raw
- Verify no burn, higher damage

#### **Scenario 2: School Requirements**
- Test Destruction spell (Body/Spirit) with Fire Lord
- Test Enhancement spell (Mind/Spirit) with Fire Lord
- Verify penalties apply correctly

#### **Scenario 3: Cross-School Effects**
- Test Dehydration (Restoration that damages)
- Verify both drain and restore work
- Test Searing Weakness (Enhancement that debuffs)

#### **Scenario 4: Element Lock**
- Verify Fire Lord can't equip Water spells
- Verify Generic characters can't equip any spells
- Test error handling

---

## Balance Guidelines

### Spell Power Levels

**Compared to Abilities:**
- Spells: ~20% more damage (element specialization)
- Spells: ~10% higher energy cost (powerful magic)
- Spells: Built-in status effects (elemental mode)

**Example:**
- Heavy Strike (Ability): 80 damage, 25 energy
- Fireball Raw (Spell): 90 damage, 20 energy
- Fireball Elemental (Spell): 70 damage, 25 energy + burn

### Mode Balance

**Elemental Mode:**
- 70-80% of raw damage
- 20-30% higher energy cost
- Significant status buildup

**Raw Mode:**
- 100% damage
- Base energy cost
- No status effects

### School Difficulty

**Easiest to Meet:**
- Destruction (Body/Spirit common on fighters)
- Enhancement (Mind/Spirit common on casters)

**Hardest to Meet:**
- Conjuration (high Mind + Spirit)
- Restoration (high Mind + Spirit)

**Design Implication:**
- Destruction/Enhancement = common combat spells
- Restoration/Conjuration = specialist/support spells

---

## Future Enhancements

### Combo System
- Spell sequences unlock bonuses
- Example: "Fireball → Fire Spear" = bonus damage
- Encourages spell diversity

### Spell Upgrades
- Level up spells through use
- Unlock improved versions
- Example: "Fireball II" (reduced requirements)

### Ritual Spells
- Multi-turn casting
- Extremely powerful effects
- High risk/reward

### Elemental Fusion
- Combine elements in party play
- Fire + Wind = Explosion
- Water + Lightning = Electrocution

### Dynamic Modes
- Unlock ability to switch modes in combat (advanced mastery)
- High Mind requirement
- Limited uses per battle

---

## Design Principles

### Core Pillars

**1. Element Identity**
- Each element feels unique
- Same schools, different flavor
- Character locked to one element

**2. Strategic Preparation**
- Loadout matters
- Mode choice is meaningful
- Pre-battle planning rewarded

**3. Cross-School Creativity**
- Schools define HOW, not WHAT
- Unexpected combinations encouraged
- Design space for innovation

**4. Balance with Abilities**
- Spells: Specialized power
- Abilities: Universal flexibility
- Both have clear roles

### What Makes a Good Spell?

✅ **Clear element identity** - Feels like Fire/Water/Earth  
✅ **School appropriate** - Matches Destruction/Enhancement/etc.  
✅ **Interesting mode choice** - Both modes valuable  
✅ **Requirement aligned** - Stats match complexity  
✅ **Visual distinctiveness** - Looks unique  

---

## Appendix

### Quick Reference

#### School Requirements Pattern
| School      | Mind | Body | Spirit | Focus    |
| ----------- | ---- | ---- | ------ | -------- |
| Destruction | Low  | High | High   | Damage   |
| Enhancement | High | Low  | High   | Buffs    |
| Restoration | High | Low  | High   | Support  |
| Conjuration | High | Low  | High   | Creation |

#### Mode Comparison
| Aspect   | Elemental    | Raw/Construct |
| -------- | ------------ | ------------- |
| Damage   | 70-80%       | 100%          |
| Energy   | +20-30%      | Base          |
| Status   | Yes          | No            |
| Use Case | Status focus | Damage focus  |

---

**End of Documentation**

Ready for implementation: Spell System Phase 1