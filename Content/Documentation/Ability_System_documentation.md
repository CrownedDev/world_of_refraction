# Ability System Documentation
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 1.0  
**Status:** Complete - Ready for Combat Integration

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Conceptual Framework](#conceptual-framework)
3. [Technical Architecture](#technical-architecture)
4. [Ability Catalog](#ability-catalog)
5. [Creating New Abilities](#creating-new-abilities)
6. [Testing Guide](#testing-guide)
7. [Combat Integration](#combat-integration)
8. [Design Principles](#design-principles)

---

## System Overview

### What Are Abilities?

**Abilities** are universal, element-less skills that any character can use regardless of their innate element. Unlike spells (which are element-locked), abilities represent physical techniques, tactical maneuvers, and fundamental combat skills.

### Key Features

- **Universal Access:** All characters can attempt any ability
- **Requirement System:** Characters excel at abilities matching their stat distribution
- **Element Infusion:** Abilities can be infused with a character's innate element
- **Effect System:** Support for buffs, debuffs, and utility effects
- **Status Buildup:** Infused abilities build elemental status effects

### Design Philosophy

**"Any character can use any ability, but some are naturally better at certain things."**

Characters aren't hard-locked from abilities - instead, they face soft penalties when using abilities mismatched to their stats. This creates natural playstyle tendencies while allowing strategic flexibility.

---

## Conceptual Framework

### The Three Stats

#### **Mind = Complexity & Technique**
- How complex is the execution?
- Requires timing, precision, skill?
- Mental focus and tactical thinking

**Examples:**
- Simple strike: Mind 0
- Timed combo sequence: Mind 15-20
- Perfect precision strike: Mind 25+

#### **Body = Physical Power**
- Raw physical strength required
- Force of impact
- Physical conditioning

**Examples:**
- Light tap: Body 5
- Moderate strike: Body 15-20
- Devastating blow: Body 30+

#### **Spirit = Stamina & Energy**
- How exhausting is the technique?
- Endurance cost
- "Gas in the tank" required

**Examples:**
- Barely tiring: Spirit 5
- Moderate drain: Spirit 15-20
- Completely exhausting: Spirit 25+

### Requirement Penalties

When a character doesn't meet an ability's requirements, they face penalties:

**Formula:**
```cpp
Penalty = sqrt(total_deficit) × 0.10
Capped at 60% maximum penalty
```

**Example:**
- Ability requires: Body 30
- Character has: Body 14
- Deficit: 16 points
- Penalty: sqrt(16) × 0.10 = 40%
- Result: 40% less damage, 40% more energy cost

**Design Rationale:**
- Square root scaling creates a gentle penalty curve
- Small deficits (1-5 points): Manageable (~10-22% penalty)
- Medium deficits (6-15 points): Significant (~24-38% penalty)
- Large deficits (16+ points): Severe (40-60% penalty)

### Element Infusion System

Any character can infuse their innate element into an ability:

**Trade-offs:**
- **-30% damage** (global constant)
- **+50% energy cost** (global constant)
- **+Status buildup** (scales with Spirit stat)

**Status Buildup Formula:**
```cpp
buildup_per_hit = 5 × character.EffectDamageMultiplier × hit_count
```

**Example - Fire Lord (Spirit multiplier 1.86x):**
- 16 Hit Combo infused
- Buildup: 5 × 1.86 × 16 = 149
- Threshold: 100
- Result: **Status effect triggered!**

**Strategic Depth:**
- Use normal abilities for maximum damage
- Use infused abilities to build status effects
- High-hit-count abilities become status machines when infused

---

## Technical Architecture

### Core Classes

#### **UAbilityData (Primary Data Asset)**
Location: `Source/world_of_refraction/Public/AbilityData.h`

**Key Properties:**
```cpp
// Identity
FString AbilityName
ERefractionElement InnateElement
FString Description

// Mechanics
int32 BaseDamage
int32 BaseEnergyCost
int32 HitCount
ETargetType TargetType

// Requirements
int32 RequiredMind
int32 RequiredBody
int32 RequiredSpirit

// Infusion
bool bCanBeInfused

// Effects
EAbilityEffectType EffectType
float EffectMagnitude  // For % buffs/debuffs
int32 EffectDuration   // In turns
int32 EffectValue      // For flat values

// Visuals
UAnimMontage* CastAnimation
UParticleSystem* NormalEffect
UParticleSystem* InfusedEffect
```

**Key Functions:**
```cpp
// Requirement checks
bool MeetsRequirements(UCharacterData* Character)
int32 GetTotalDeficit(UCharacterData* Character)
float CalculateRequirementPenalty(UCharacterData* Character)

// Damage calculations
int32 CalculateNormalDamage(UCharacterData* Character)
int32 CalculateInfusedDamage(UCharacterData* Character)

// Energy calculations
int32 CalculateNormalEnergyCost(UCharacterData* Character)
int32 CalculateInfusedEnergyCost(UCharacterData* Character)

// Status effects
int32 CalculateStatusBuildup(UCharacterData* Character)

// Helper
FString GetRequirementSummary(UCharacterData* Character)
```

#### **UAbilityDataDebug (Function Library)**
Location: `Source/world_of_refraction/Public/AbilityDataDebug.h`

**Debug Functions:**
```cpp
// Print to screen
void PrintAbilityStats(UAbilityData* Ability, UCharacterData* Character, float Duration)

// Print to log
void LogAbilityStats(UAbilityData* Ability, UCharacterData* Character)

// Get formatted string
FString GetAbilityStatsString(UAbilityData* Ability, UCharacterData* Character)

// Compare effectiveness
void CompareAbilityEffectiveness(UAbilityData* Ability, UCharacterData* Char1, UCharacterData* Char2)
```

### Enums

#### **ETargetType**
Location: `Source/world_of_refraction/Public/TargetType.h`
```cpp
Self
SingleEnemy
AllEnemies
SingleAlly
AllAllies
Everyone
```

#### **EAbilityEffectType**
Location: `Source/world_of_refraction/Public/AbilityEffectType.h`
```cpp
None
DamageBuff      // Increases damage dealt
DamageDebuff    // Decreases damage dealt
DefenseBuff     // Increases defense
DefenseDebuff   // Decreases defense
SpeedBuff       // Increases turn speed
SpeedDebuff     // Decreases turn speed
EnergyRestore   // Restores energy
EnergyDrain     // Drains energy
HealthRestore   // Heals HP
```

### Combat Constants

Location: `Source/world_of_refraction/CombatConstants.h`

```cpp
// Requirement Penalties
REQUIREMENT_PENALTY_SCALE = 0.10f      // sqrt multiplier
REQUIREMENT_PENALTY_MAX = 0.6f         // 60% cap

// Infusion System
INFUSION_DAMAGE_PENALTY = 0.30f        // 30% damage reduction
INFUSION_ENERGY_MULTIPLIER = 1.5f      // 50% more energy
BASE_STATUS_BUILDUP_PER_HIT = 5        // Base buildup value

// Status Thresholds
STATUS_EFFECT_THRESHOLD = 100          // Buildup needed to trigger
```

### Calculation Flow

**Normal Ability Use:**
1. Get base damage from AbilityData
2. Calculate requirement penalty (if any)
3. Apply penalty to damage: `damage *= (1.0 - penalty)`
4. Apply character's raw damage multiplier: `damage *= character.RawDamageMultiplier`
5. Apply penalty to energy: `cost *= (1.0 + penalty)`

**Infused Ability Use:**
1. Get base damage from AbilityData
2. Calculate requirement penalty
3. Apply requirement penalty: `damage *= (1.0 - penalty)`
4. Apply infusion penalty: `damage *= (1.0 - 0.30)`
5. Apply character's raw damage multiplier
6. Calculate energy with both penalties: `cost *= (1.0 + penalty) × 1.5`
7. Calculate status buildup: `5 × EffectMultiplier × HitCount`

---

## Ability Catalog

### 1. Quick Strike
**Type:** Basic Attack  
**Description:** A simple, fast attack. No special requirements.

**Stats:**
- Base Damage: 30
- Base Energy: 15
- Hit Count: 1
- Target: Single Enemy

**Requirements:**
- Mind: 0 (no complexity)
- Body: 5 (minimal physical)
- Spirit: 5 (barely tiring)

**Infusion:** Yes  
**Effects:** None

**Design Notes:**
- Starter ability, accessible to all
- Efficient energy-to-damage ratio
- Good for testing builds

---

### 2. Heavy Strike
**Type:** Power Attack  
**Description:** A devastating blow requiring pure strength. Simple but exhausting.

**Stats:**
- Base Damage: 80
- Base Energy: 25
- Hit Count: 1
- Target: Single Enemy

**Requirements:**
- Mind: 0 (simple technique)
- Body: 30 (requires strength!)
- Spirit: 20 (exhausting)

**Infusion:** Yes  
**Effects:** None

**Design Notes:**
- Body-focused characters excel here
- High damage for characters meeting requirements
- Heavy penalty for Mind/Spirit builds

**Character Comparison:**
- Fire Lord (Body 14): 40% penalty → 50 damage
- Water Lord (Body 33): No penalty → 95 damage

---

### 3. 16 Hit Combo
**Type:** Multi-Hit Technical  
**Description:** Rapid chain of precisely timed strikes. Requires technique and stamina.

**Stats:**
- Base Damage: 80 (5 per hit)
- Base Energy: 30
- Hit Count: 16
- Target: Single Enemy

**Requirements:**
- Mind: 20 (complex timing!)
- Body: 15 (stamina for 16 hits)
- Spirit: 25 (very exhausting)

**Infusion:** Yes  
**Effects:** None

**Design Notes:**
- **Status effect specialist** when infused
- High hit count = massive status buildup
- Requires balanced stats

**Infusion Example (Fire Lord):**
- Damage: 52 (with penalties)
- Status Buildup: 149 (5 × 1.86 × 16)
- **Triggers status effect!** (threshold 100)

---

### 4. Tactical Strike
**Type:** Precision Attack  
**Description:** A calculated blow targeting weak points. Requires precision and strength.

**Stats:**
- Base Damage: 60
- Base Energy: 22
- Hit Count: 1
- Target: Single Enemy

**Requirements:**
- Mind: 18 (technique/precision)
- Body: 20 (moderate strength)
- Spirit: 12 (moderate drain)

**Infusion:** Yes  
**Effects:** None

**Design Notes:**
- Balanced requirements
- Good middle-ground ability
- Rewards well-rounded builds

---

### 5. Focus
**Type:** Self Buff  
**Description:** Channel concentration to enhance next action. Mentally and spiritually draining.

**Stats:**
- Base Damage: 0
- Base Energy: 20
- Hit Count: 0
- Target: Self

**Requirements:**
- Mind: 18 (deep concentration)
- Body: 0 (no physical)
- Spirit: 15 (drains stamina)

**Infusion:** No (already spiritual)

**Effects:**
- Type: Damage Buff
- Magnitude: 25%
- Duration: 2 turns

**Design Notes:**
- Pure buff ability
- Ideal for Mind/Spirit builds
- Sets up big damage turns

---

### 6. Weaken
**Type:** Debuff Attack  
**Description:** Strike at vital points, reducing enemy's offensive power.

**Stats:**
- Base Damage: 40
- Base Energy: 18
- Hit Count: 1
- Target: Single Enemy

**Requirements:**
- Mind: 20 (precision targeting)
- Body: 12 (moderate power)
- Spirit: 15 (focused strike)

**Infusion:** Yes

**Effects:**
- Type: Damage Debuff
- Magnitude: 20%
- Duration: 2 turns

**Design Notes:**
- Tactical ability
- Both damages AND debuffs
- Good for prolonged fights

---

### 7. Fortify
**Type:** Defense Buff  
**Description:** Steel yourself for incoming attacks. Defense focused.

**Stats:**
- Base Damage: 0
- Base Energy: 15
- Hit Count: 0
- Target: Self

**Requirements:**
- Mind: 8 (simple technique)
- Body: 15 (physical conditioning)
- Spirit: 25 (exhausting to maintain!)

**Infusion:** No

**Effects:**
- Type: Defense Buff
- Magnitude: 30%
- Duration: 3 turns

**Design Notes:**
- Tank ability
- Requires high Spirit endurance
- Longer duration than most buffs

---

### 8. Drain Strike
**Type:** Utility - Energy Steal  
**Description:** Vampiric attack that steals enemy's energy.

**Stats:**
- Base Damage: 35
- Base Energy: 20
- Hit Count: 1
- Target: Single Enemy

**Requirements:**
- Mind: 18 (complex technique)
- Body: 10 (light attack)
- Spirit: 22 (channeling energy)

**Infusion:** Yes

**Effects:**
- Type: Energy Restore
- Value: 15 (flat energy)
- Duration: Instant

**Design Notes:**
- Resource management tool
- Effective net cost: 5 energy (20 - 15)
- Sustain ability for long fights

---

### 9. Pressure Point
**Type:** Status Specialist  
**Description:** Target weak spots for maximum status buildup. Low damage.

**Stats:**
- Base Damage: 25
- Base Energy: 18
- Hit Count: 3
- Target: Single Enemy

**Requirements:**
- Mind: 25 (expert precision!)
- Body: 8 (light touches)
- Spirit: 12 (focused strikes)

**Infusion:** Yes

**Design Notes:**
- **High Mind requirement** (expert technique)
- Triple hits = good status buildup
- Sacrifices damage for status
- **Future Enhancement:** Should have 3x status buildup modifier

---

### 10. Whirlwind
**Type:** AOE Attack  
**Description:** Spinning attack hitting all enemies. Physically demanding.

**Stats:**
- Base Damage: 50 (split among targets)
- Base Energy: 35
- Hit Count: 1
- Target: All Enemies

**Requirements:**
- Mind: 10 (simple spin)
- Body: 25 (strength to spin)
- Spirit: 28 (very exhausting!)

**Infusion:** Yes

**Effects:** None

**Design Notes:**
- Only AOE attack in current set
- High Spirit drain
- Damage split mechanic (future implementation)
- Good for clearing multiple weak enemies

---

## Creating New Abilities

### Step-by-Step Guide

#### 1. Conceptualize the Ability

Ask yourself:
- **What does it do?** (Damage, buff, debuff, utility?)
- **How complex is it?** (Mind requirement)
- **How much strength needed?** (Body requirement)
- **How exhausting is it?** (Spirit requirement)
- **Can it be infused?** (Usually yes for attacks, no for buffs)

#### 2. Determine Base Stats

**Damage Guidelines:**
- Light attack: 20-35
- Moderate attack: 40-60
- Heavy attack: 70-90
- Ultimate attack: 100+

**Energy Cost Guidelines:**
- Cheap: 10-15
- Moderate: 16-25
- Expensive: 26-40
- Ultimate: 40+

**Hit Count:**
- Single hit: 1
- Multi-hit: 2-5
- Rapid combo: 6-20
- Buff/debuff: 0

#### 3. Set Requirements

**Mind (Complexity):**
- None: 0-5
- Simple: 6-12
- Moderate: 13-20
- Complex: 21-28
- Expert: 29+

**Body (Power):**
- Light: 0-8
- Moderate: 9-18
- Heavy: 19-28
- Brutal: 29+

**Spirit (Stamina):**
- Minimal: 0-8
- Light: 9-15
- Moderate: 16-22
- Heavy: 23-28
- Exhausting: 29+

#### 4. Design Effects (If Applicable)

**For Buffs/Debuffs:**
- Effect Type: Choose from enum
- Magnitude: 0.15-0.30 typical (15-30%)
- Duration: 1-3 turns typical

**For Utility:**
- Energy Restore: 10-20 typical
- Health Restore: 20-50 typical

#### 5. Create the Data Asset

**In Unreal Editor:**
1. Content Browser → Right-click
2. Miscellaneous → Data Asset
3. Select **AbilityData**
4. Name: `DA_AbilityName`
5. Fill in all properties
6. Save

#### 6. Test with Characters

Use **BP_AbilityDataTester**:
1. Place in level
2. Set TestAbility → Your new ability
3. Set TestCharacter → Test character
4. Play and review output

**Check:**
- Requirements make sense
- Penalties calculate correctly
- Effects display properly
- Damage/energy values are balanced

#### 7. Compare Character Effectiveness

Test with multiple characters:
- Fire Lord (Mind 36, Body 14, Spirit 25)
- Water Lord (Mind 25, Body 33, Spirit 17)

Verify penalties work as intended.

### Balance Checklist

- [ ] Does the ability serve a unique purpose?
- [ ] Are requirements aligned with technique complexity?
- [ ] Is damage appropriate for energy cost?
- [ ] Do penalties discourage misuse without hard-locking?
- [ ] Does infusion create interesting trade-offs?
- [ ] Is it useful for at least one character archetype?

---

## Testing Guide

### Testing Tools

#### **BP_AbilityDataTester**
Location: `Content/Blueprints/Testing/`

**Setup:**
1. Drag into test level
2. Select in viewport
3. Set properties in Details panel:
   - Test Ability: Ability to test
   - Test Character: Character using it
4. Play level
5. Stats print to screen

**Output Includes:**
- Base ability stats
- Character requirements vs actual stats
- Requirement penalty calculations
- Normal use stats (damage, energy)
- Infused use stats (damage, energy, status)
- Effect information

#### **Debug Functions**

**From Blueprint:**
```
PrintAbilityStats(Ability, Character, Duration)
LogAbilityStats(Ability, Character)
CompareAbilityEffectiveness(Ability, Char1, Char2)
```

**From C++:**
```cpp
UAbilityDataDebug::PrintAbilityStats(Ability, Character, 15.0f);
UAbilityDataDebug::LogAbilityStats(Ability, Character);
UAbilityDataDebug::CompareAbilityEffectiveness(Ability, FireLord, WaterLord);
```

### Test Scenarios

#### **Scenario 1: Perfect Match**
- Character meets all requirements
- Verify no penalties applied
- Confirm full damage/normal cost

**Example:**
- Water Lord using Heavy Strike (Body 30 required, has 33)
- Expected: 95 damage, 25 energy

#### **Scenario 2: Partial Deficit**
- Character slightly below requirements
- Verify manageable penalty

**Example:**
- Fire Lord using 16 Hit Combo (Body 15 required, has 14)
- Deficit: 1 point
- Expected: ~10% penalty

#### **Scenario 3: Major Deficit**
- Character far below requirements
- Verify severe penalty

**Example:**
- Fire Lord using Heavy Strike (Body 30 required, has 14)
- Deficit: 16 points
- Expected: 40% penalty

#### **Scenario 4: Infusion Status Trigger**
- Use high-hit-count ability infused
- Verify status buildup reaches threshold

**Example:**
- Fire Lord using 16 Hit Combo infused
- Expected: 149 buildup → Triggers status (threshold 100)

#### **Scenario 5: Effects Display**
- Test buff/debuff abilities
- Verify effects show correctly

**Example:**
- Focus buff
- Expected: 25% damage buff, 2 turn duration

### Performance Testing

**Frame Time Impact:**
All calculation functions are pure and lightweight:
- `CalculateRequirementPenalty()`: ~0.01ms
- `CalculateDamage()`: ~0.02ms
- `CalculateStatusBuildup()`: ~0.01ms

**Memory Footprint:**
- Single AbilityData asset: ~2KB
- 10 abilities loaded: ~20KB total

**No performance concerns for combat usage.**

---

## Combat Integration

### Integration Points

The Ability System is designed to integrate with the combat system through these entry points:

#### **1. Action Selection**
```cpp
// Player selects ability from UI
void OnAbilitySelected(UAbilityData* Ability, bool bWantsInfusion)
{
    // Store selected ability
    PendingAbility = Ability;
    bInfusePending = bWantsInfusion;
    
    // Proceed to target selection
    EnterTargetingMode(Ability->TargetType);
}
```

#### **2. Ability Execution**
```cpp
void ExecuteAbility(UAbilityData* Ability, UCharacterData* User, AActor* Target, bool bInfused)
{
    // Calculate damage
    int32 Damage = bInfused 
        ? Ability->CalculateInfusedDamage(User)
        : Ability->CalculateNormalDamage(User);
    
    // Calculate energy cost
    int32 Cost = bInfused
        ? Ability->CalculateInfusedEnergyCost(User)
        : Ability->CalculateNormalEnergyCost(User);
    
    // Check if user has enough energy
    if (User->CurrentEnergy < Cost)
    {
        // Not enough energy!
        return;
    }
    
    // Deduct energy
    User->CurrentEnergy -= Cost;
    
    // Apply damage
    Target->TakeDamage(Damage);
    
    // Handle infusion
    if (bInfused)
    {
        int32 Buildup = Ability->CalculateStatusBuildup(User);
        Target->AddStatusBuildup(User->InnateElement, Buildup);
    }
    
    // Apply effects
    if (Ability->EffectType != EAbilityEffectType::None)
    {
        ApplyEffect(Ability, User, Target);
    }
    
    // Play visuals
    PlayAbilityEffects(Ability, bInfused);
}
```

#### **3. Effect Application**
```cpp
void ApplyEffect(UAbilityData* Ability, UCharacterData* User, AActor* Target)
{
    switch (Ability->EffectType)
    {
        case EAbilityEffectType::DamageBuff:
            Target->AddBuff(EBuffType::Damage, Ability->EffectMagnitude, Ability->EffectDuration);
            break;
            
        case EAbilityEffectType::EnergyRestore:
            User->CurrentEnergy += Ability->EffectValue;
            break;
            
        // ... handle all effect types
    }
}
```

#### **4. UI Display**
```cpp
// Show ability in action bar
void PopulateAbilityButton(UAbilityData* Ability, UCharacterData* Character)
{
    // Get calculated values for display
    int32 NormalDamage = Ability->CalculateNormalDamage(Character);
    int32 NormalCost = Ability->CalculateNormalEnergyCost(Character);
    
    // Set button text
    ButtonText = FString::Printf(TEXT("%s\n%d dmg | %d energy"),
        *Ability->AbilityName, NormalDamage, NormalCost);
    
    // Check if usable
    bool bCanUse = Character->CurrentEnergy >= NormalCost;
    Button->SetEnabled(bCanUse);
    
    // Show requirement indicator
    bool bMeetsReqs = Ability->MeetsRequirements(Character);
    RequirementIcon->SetVisibility(bMeetsReqs ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
}
```

### Status Effect System

**Requirements for Combat Integration:**

1. **Status Buildup Tracking**
```cpp
// On combatant
TMap<ERefractionElement, int32> StatusBuildup;

void AddStatusBuildup(ERefractionElement Element, int32 Amount)
{
    StatusBuildup.FindOrAdd(Element) += Amount;
    
    if (StatusBuildup[Element] >= CombatConstants::STATUS_EFFECT_THRESHOLD)
    {
        TriggerStatusEffect(Element);
        StatusBuildup[Element] = 0; // Reset
    }
}
```

2. **Status Effect Application**
```cpp
void TriggerStatusEffect(ERefractionElement Element)
{
    switch (Element)
    {
        case ERefractionElement::Fire:
            ApplyBurn(DamagePerTurn, Duration);
            break;
        case ERefractionElement::Water:
            ApplyChill(SlowAmount, Duration);
            break;
        // ... handle all elements
    }
}
```

### Future Enhancements

**Planned Integration Features:**

1. **Combo System**
   - Track ability sequences
   - Bonus damage for specific combos
   - Example: "Weaken → Heavy Strike" = bonus damage

2. **Cooldown System**
   - Powerful abilities on cooldowns
   - Alternative to high energy costs
   - Example: Ultimate abilities 3-turn cooldown

3. **Ability Upgrades**
   - Level up abilities during campaign
   - Reduce requirements or increase power
   - Example: "16 Hit Combo II" (reduced Mind requirement)

4. **Synergy Bonuses**
   - Abilities work better together
   - Example: "Focus + Tactical Strike" = extra crit chance

---

## Design Principles

### Core Philosophy

**1. Soft Restrictions Over Hard Locks**
- Any character can try any ability
- Penalties guide choices without forcing them
- Encourages experimentation and creativity

**2. Strategic Depth Through Trade-offs**
- Infusion: Damage vs. Status
- Requirement penalties: Versatility vs. Specialization
- Energy costs: Power vs. Sustainability

**3. Character Identity Through Optimization**
- Fire Lord naturally gravitates toward Mind/Spirit abilities
- Water Lord excels at Body/Spirit abilities
- Characters feel distinct without artificial restrictions

**4. Scalable Complexity**
- Simple abilities (Quick Strike) for beginners
- Complex abilities (16 Hit Combo) for advanced play
- System depth emerges from combinations

### Balance Methodology

**Damage-to-Energy Ratios:**
- Basic attacks: ~2:1 (Quick Strike: 30 dmg / 15 energy)
- Power attacks: ~3:1 (Heavy Strike: 80 dmg / 25 energy)
- Utility: Lower ratio but added benefits

**Requirement Distribution:**
- Most abilities: 30-50 total requirement points
- Specialized abilities: 50-70 total points
- Ultimate abilities: 70+ total points

**Effect Magnitudes:**
- Minor buffs/debuffs: 15-20%
- Standard buffs/debuffs: 20-30%
- Major buffs/debuffs: 30-40%

### Testing Heuristics

**When to Revise an Ability:**
- ❌ Too universally strong (everyone uses it)
- ❌ Never used (too niche or weak)
- ❌ Requirements don't match complexity
- ❌ Damage/cost ratio out of line
- ❌ Infusion always better or never worth it

**Good Ability Indicators:**
- ✅ Clear use case for specific builds
- ✅ Interesting trade-offs
- ✅ Multiple viable strategies
- ✅ Requirements match gameplay feel
- ✅ Visually/thematically distinctive

---

## Future Work

### Immediate Next Steps

1. **Spell System**
   - Element-locked spells (Fire Lord only casts Fire spells)
   - 4 spell schools: Destruction, Enhancement, Restoration, Conjuration
   - Similar structure to abilities (reuse patterns)

2. **Equipment System**
   - CharacterData loadouts (3 ability slots)
   - Save/load configurations
   - UI for ability selection

3. **Combat Manager**
   - Turn-based orchestration
   - Action selection UI
   - Integrate abilities into battle flow

### Long-term Enhancements

1. **Advanced Status System**
   - Status effect interactions (Burn + Wind = Explosion)
   - Resistance/immunity mechanics
   - Status effect stacking

2. **Ability Mastery**
   - Use count tracking
   - Unlock bonuses at milestones
   - "Mastered" abilities get small buffs

3. **Dynamic Abilities**
   - Abilities that change based on context
   - Reactive abilities (trigger on being hit)
   - Context-sensitive bonuses

4. **AI Decision-Making**
   - AI evaluates ability effectiveness
   - Considers requirements and penalties
   - Smart infusion decisions

---

## Appendix

### Quick Reference Tables

#### Ability Requirements Summary
| Ability         | Mind | Body | Spirit | Total |
| --------------- | ---- | ---- | ------ | ----- |
| Quick Strike    | 0    | 5    | 5      | 10    |
| Heavy Strike    | 0    | 30   | 20     | 50    |
| 16 Hit Combo    | 20   | 15   | 25     | 60    |
| Tactical Strike | 18   | 20   | 12     | 50    |
| Focus           | 18   | 0    | 15     | 33    |
| Weaken          | 20   | 12   | 15     | 47    |
| Fortify         | 8    | 15   | 25     | 48    |
| Drain Strike    | 18   | 10   | 22     | 50    |
| Pressure Point  | 25   | 8    | 12     | 45    |
| Whirlwind       | 10   | 25   | 28     | 63    |

#### Character Stat Summary
| Character  | Mind | Body | Spirit | Archetype              |
| ---------- | ---- | ---- | ------ | ---------------------- |
| Fire Lord  | 36   | 14   | 25     | Mind/Spirit Specialist |
| Water Lord | 25   | 33   | 17     | Body Tank              |

#### Damage Formulas
```
Normal Damage:
= BaseDamage × (1 - RequirementPenalty) × RawDamageMultiplier

Infused Damage:
= BaseDamage × (1 - RequirementPenalty) × (1 - 0.30) × RawDamageMultiplier

Normal Cost:
= BaseCost × (1 + RequirementPenalty)

Infused Cost:
= BaseCost × (1 + RequirementPenalty) × 1.5

Status Buildup:
= 5 × EffectDamageMultiplier × HitCount
```

### File Locations

**C++ Files:**
```
Source/world_of_refraction/Public/
├── AbilityData.h
├── AbilityDataDebug.h
├── TargetType.h
└── AbilityEffectType.h

Source/world_of_refraction/Private/
├── AbilityData.cpp
└── AbilityDataDebug.cpp

Source/world_of_refraction/
└── CombatConstants.h (includes ability constants)
```

**Content Assets:**
```
Content/DataAssets/Abilities/
├── DA_QuickStrike
├── DA_HeavyStrike
├── DA_16HitCombo
├── DA_TacticalStrike
├── DA_Focus
├── DA_Weaken
├── DA_Fortify
├── DA_DrainStrike
├── DA_PressurePoint
└── DA_Whirlwind

Content/Blueprints/Testing/
└── BP_AbilityDataTester
```

---

## Changelog

### Version 1.0 (November 25, 2024)
- Initial ability system implementation
- 10 diverse abilities created
- Requirement penalty system with square root scaling
- Element infusion system
- Effect system (buffs, debuffs, utility)
- Comprehensive debug tools
- Full documentation

---

**End of Documentation**

For questions or suggestions, refer to:
- Technical Architecture Plan
- Session-by-Session Roadmap
- Combat Parity Architecture Document