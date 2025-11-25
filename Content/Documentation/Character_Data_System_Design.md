# Character Data & Stat System Design Document
## RefractionPVP - Unreal Engine 5.7.0

**Date Created:** November 23, 2025  
**Status:** Ready for Implementation  
**Version:** 1.0

---

## Table of Contents
1. [System Overview](#system-overview)
2. [Core Design Philosophy](#core-design-philosophy)
3. [Character Data Structure](#character-data-structure)
4. [Stat Distribution System](#stat-distribution-system)
5. [World Stats & Sub-Stats](#world-stats--sub-stats)
6. [Complete Formula Reference](#complete-formula-reference)
7. [Balance Considerations](#balance-considerations)
8. [Example Character Builds](#example-character-builds)
9. [Implementation Checklist](#implementation-checklist)

---

## System Overview

### Three-Pillar Stat System
Characters are defined by three core attributes:
- **Mind** (Spell Mastery) - Efficiency, speed, critical hits
- **Body** (Physical Prowess) - Defense, attack speed, raw damage
- **Spirit** (Energy & Scale) - Spell power, resistance, AoE size

### Dual-Layer Progression
1. **Base Stats**: Distributable points (25-35 depending on character)
2. **World Stats**: Collectibles (0-7 per stat) that unlock sub-stat customization

### Resource Systems
- **Energy**: Elemental characters, no regen per turn, restores between battles
- **Stamina**: Generic element characters, similar rules
- **Absorption**: BrokenDarkness element, gained by defending, overflow system

---

## Core Design Philosophy

### Balance Through Variety
Characters are balanced through three levers:
1. **Stat Point Budget** (25-35 points)
   - Low budget (25 pts) = Broken abilities
   - Standard budget (30 pts) = Balanced kit
   - High budget (35 pts) = Simple/weak abilities

2. **Unique Spell Pools**
   - Generic spells (element-based, shared)
   - Unique spells (character-specific)

3. **Innate Element**
   - Determines generic spell pool
   - 11 element types total

### Story vs PVP
- **Story Mode**: Collect world stats for progressive power growth
- **PVP Mode**: Pre-select world stat distribution (same 0-7 limits per stat)

### Combat Parity Philosophy
- Any combatant can perform any action available to others
- No hardcoded player/enemy distinctions
- Character data drives ALL combat behavior

---

## Character Data Structure

### UCharacterData (Primary Data Asset)

```cpp
UCLASS(BlueprintType)
class UCharacterData : public UPrimaryDataAsset
{
    // ==================== IDENTITY ====================
    FString CharacterName
    EElementType InnateElement
    FString Description (multiline)
    UTexture2D* Portrait
    
    // ==================== STAT BUDGET ====================
    int32 DistributablePoints (20-40, default 30)
    int32 DistributedMind (0+)
    int32 DistributedBody (0+)
    int32 DistributedSpirit (0+)
    
    // ==================== WORLD STATS ====================
    int32 WorldMindLevel (0-7)
    int32 WorldBodyLevel (0-7)
    int32 WorldSpiritLevel (0-7)
    int32 PointsPerWorldStatLevel (default 3)
    
    // ==================== SUB-STAT DISTRIBUTION ====================
    // Mind Sub-Stats (21 points max from 7 world levels × 3)
    int32 CostReductionPoints
    int32 TurnSpeedPoints
    int32 CritChancePoints
    
    // Body Sub-Stats
    int32 DefensePoints
    int32 AttackSpeedPoints
    int32 RawDamagePoints
    
    // Spirit Sub-Stats
    int32 EffectDamagePoints
    int32 ResistancePoints
    int32 AbilitySizePoints
    
    // ==================== SPELL POOLS ====================
    TArray<USpellData*> GenericSpellPool
    TArray<USpellData*> UniqueSpells
    int32 MaxGenericSpellSlots (3-6, default 4)
    
    // ==================== VISUAL DATA ====================
    USkeletalMesh* CharacterMesh
    TSubclassOf<UAnimInstance> AnimationBlueprint
    FLinearColor PrimaryColor
    FLinearColor SecondaryColor
    
    // ==================== BALANCE FLAGS ====================
    bool bHasBrokenAbilities
    FString BalanceNotes
}
```

---

## Stat Distribution System

### Base Stat Points

**Character receives a point budget to distribute freely:**

```
Character Creation:
├─ Total Points: 30 (example)
├─ Mind: 15 points
├─ Body: 5 points
└─ Spirit: 10 points
Total: 30/30 ✓

Validation: DistributedMind + DistributedBody + DistributedSpirit == DistributablePoints
```

### Effective Stats (with World Bonuses)

**World stats provide percentage scaling:**

```cpp
EffectiveMind = DistributedMind × (1 + WorldMindLevel × 0.05)
EffectiveBody = DistributedBody × (1 + WorldBodyLevel × 0.05)
EffectiveSpirit = DistributedSpirit × (1 + WorldSpiritLevel × 0.05)
```

**Example:**
```
Base Mind: 15
World Mind Level: 5
Effective Mind: 15 × (1 + 5 × 0.05) = 15 × 1.25 = 18.75
```

---

## World Stats & Sub-Stats

### World Stat Levels (0-7 each)

**Each world stat level generates 3 sub-stat points:**

```
Collect Mind Crystal → WorldMindLevel increases 1 → 7
Each level: +3 points to distribute among Mind sub-stats

Maximum per stat: 7 levels × 3 points = 21 points
Maximum total: 7+7+7 levels = 63 sub-stat points
```

### Sub-Stat Categories

#### Mind Sub-Stats (Spell Mastery)
1. **Cost Reduction** - Reduces spell Energy costs
2. **Turn Speed** - Affects turn order and action frequency
3. **Crit Chance** - Increases critical hit probability

#### Body Sub-Stats (Physical Prowess)
1. **Defense** - Flat damage reduction per hit
2. **Attack Speed** - Animation speed (affects real-time defense difficulty)
3. **Raw Damage** - Multiplier for non-elemental/physical attacks

#### Spirit Sub-Stats (Energy & Scale)
1. **Effect Damage** - Multiplier for ALL spell damage (elemental)
2. **Resistance** - Percentage reduction to incoming elemental damage
3. **Ability Size** - AoE radius multiplier

### Sub-Stat Point Validation

```cpp
// Mind points must equal world stat allocation
GetUsedMindPoints() == GetAvailableMindPoints()
where:
    UsedMindPoints = CostReductionPoints + TurnSpeedPoints + CritChancePoints
    AvailableMindPoints = WorldMindLevel × 3

// Same for Body and Spirit
```

---

## Complete Formula Reference

### Mind Formulas

#### 1. Cost Reduction
```cpp
float CalculateSpellCostReduction() const
{
    float EffectiveMind = GetEffectiveMind();
    // 0.6% reduction per point, capped at 70%
    return FMath::Clamp(EffectiveMind * CostReductionPoints * 0.006f, 0.0f, 0.7f);
}

// Usage:
// Spell base cost: 40 Energy
// Mind 20, Cost Reduction 15 points
// Reduction: 20 × 15 × 0.006 = 0.18 (18%)
// Final cost: 40 × (1 - 0.18) = 32.8 Energy
```

#### 2. Turn Speed
```cpp
float CalculateTurnSpeed() const
{
    float EffectiveMind = GetEffectiveMind();
    // Base 10 + (EffectiveMind × Points × 0.5)
    return 10.0f + (EffectiveMind * TurnSpeedPoints * 0.5f);
}

int32 CalculateTurnRatio(float MySpeed, float EnemySpeed) const
{
    float Difference = MySpeed - EnemySpeed;
    
    if (Difference >= 15.0f)
        return 2; // 2:1 ratio (double turn)
    else
        return 1; // 1:1 ratio (normal)
}

// Turn Order System:
// Difference < 15: Normal (1:1 - everyone acts once)
// Difference ≥ 15: Double turn (2:1 - fast character acts twice per slow turn)
// Maximum: Capped at 2 turns (cannot get 3:1)
```

#### 3. Critical Chance
```cpp
float CalculateCriticalChance() const
{
    float EffectiveMind = GetEffectiveMind();
    // Base 5% + (EffectiveMind × Points × 0.3%), capped at 60%
    return FMath::Clamp(0.05f + (EffectiveMind * CritChancePoints * 0.003f), 0.05f, 0.6f);
}

// Critical Damage Multiplier: 1.5x (fixed)
```

---

### Body Formulas

#### 4. Defense (Flat Reduction)
```cpp
int32 CalculateFlatDefense() const
{
    float EffectiveBody = GetEffectiveBody();
    // EffectiveBody × Points × 0.4 (flat damage blocked)
    return FMath::RoundToInt(EffectiveBody * DefensePoints * 0.4f);
}

// Usage:
// Incoming damage: 150
// Body 20, Defense 21 points
// Blocks: 20 × 21 × 0.4 = 168 damage
// Final: 150 - 168 = 0 damage (fully blocked)
```

#### 5. Attack Speed (Animation Speed)
```cpp
float CalculateAttackSpeed() const
{
    float EffectiveBody = GetEffectiveBody();
    // Base 1.0 + (EffectiveBody × Points × 0.05)
    return 1.0f + (EffectiveBody * AttackSpeedPoints * 0.05f);
}

// Affects real-time defense window:
// Attack Speed 1.0 = Normal (1.5s to defend)
// Attack Speed 2.0 = 2x faster (0.75s to defend)
// Attack Speed 3.0 = 3x faster (0.5s to defend - very hard!)
```

#### 6. Raw Damage (Non-Elemental Multiplier)
```cpp
float CalculateRawDamageMultiplier() const
{
    float EffectiveBody = GetEffectiveBody();
    // Base 1.0 + (EffectiveBody × Points × 0.006)
    return 1.0f + (EffectiveBody * RawDamagePoints * 0.006f);
}

// Usage:
// Generic attack base: 80 damage
// Body 20, Raw Damage 21 points
// Multiplier: 1.0 + (20 × 21 × 0.006) = 1.0 + 2.52 = 3.52x
// Final: 80 × 3.52 = 281.6 damage
```

---

### Spirit Formulas

#### 7. Effect Damage (Spell Power)
```cpp
float CalculateEffectDamageMultiplier() const
{
    float EffectiveSpirit = GetEffectiveSpirit();
    // Base 1.0 + (EffectiveSpirit × Points × 0.006)
    return 1.0f + (EffectiveSpirit * EffectDamagePoints * 0.006f);
}

// Affects ALL elemental spell damage
// Usage:
// Fireball base: 50 damage
// Spirit 20, Effect Damage 21 points
// Multiplier: 1.0 + (20 × 21 × 0.006) = 1.0 + 2.52 = 3.52x
// Final: 50 × 3.52 = 176 damage
```

#### 8. Resistance (Elemental Damage Reduction)
```cpp
float CalculateElementalResistance() const
{
    float EffectiveSpirit = GetEffectiveSpirit();
    // EffectiveSpirit × Points × 0.5%, capped at 50%
    return FMath::Clamp(EffectiveSpirit * ResistancePoints * 0.005f, 0.0f, 0.5f);
}

// Usage:
// Incoming spell damage: 176
// Spirit 15, Resistance 15 points
// Resistance: 15 × 15 × 0.005 = 0.1125 (11.25%)
// Final: 176 × (1 - 0.1125) = 156.2 damage
```

#### 9. Ability Size (AoE Multiplier)
```cpp
float CalculateAbilitySizeMultiplier() const
{
    float EffectiveSpirit = GetEffectiveSpirit();
    // Base 1.0 + (EffectiveSpirit × Points × 0.07)
    return 1.0f + (EffectiveSpirit * AbilitySizePoints * 0.07f);
}

// Affects spell radius/AoE
// Usage:
// Fireball base radius: 2 meters
// Spirit 12, Size 10 points
// Multiplier: 1.0 + (12 × 10 × 0.07) = 1.0 + 8.4 = 9.4x
// Wait, that seems high... Let me recalculate formula
// Actually: 1.0 + (12 × 10 × 0.007) = 1.0 + 0.84 = 1.84x
// Final radius: 2 × 1.84 = 3.68 meters
```

**CORRECTION - Ability Size should use 0.007 not 0.07:**
```cpp
return 1.0f + (EffectiveSpirit * AbilitySizePoints * 0.007f);
```

---

## Balance Considerations

### Maximum Investment (21 points in one sub-stat)

**Assumes:**
- Base stat: 20
- World level: 7
- Effective stat: 20 × 1.35 = 27
- Sub-stat points: 21 (max)

**Maximum Effects:**
```
Mind Max:
├─ Cost Reduction: 27 × 21 × 0.006 = 0.34 (34%)
├─ Turn Speed: 10 + (27 × 21 × 0.5) = 293.5
└─ Crit Chance: 5% + (27 × 21 × 0.003) = 22% (capped at 60%)

Body Max:
├─ Defense: 27 × 21 × 0.4 = 226 blocked
├─ Attack Speed: 1 + (27 × 21 × 0.05) = 29.35x
└─ Raw Damage: 1 + (27 × 21 × 0.006) = 4.4x

Spirit Max:
├─ Effect Damage: 1 + (27 × 21 × 0.006) = 4.4x
├─ Resistance: 27 × 21 × 0.005 = 0.28 (28%, capped at 50%)
└─ Ability Size: 1 + (27 × 21 × 0.007) = 4.96x
```

### Power Scaling Range

**With 3-4x multipliers at max investment:**

```
Minimum (0 world stats, 0 points):
- Effect Damage: 1.0x (base)
- Defense: 0 (none)
- Turn Speed: 10 (base)

Medium (Moderate investment):
- Effect Damage: 2.0-2.5x
- Defense: 80-120 blocked
- Turn Speed: 50-100

Maximum (Max investment):
- Effect Damage: 3.5-4.4x
- Defense: 150-226 blocked
- Turn Speed: 150-300
```

### Counterplay Design

**Glass Cannon (Max Offense):**
```
Strengths:
├─ 4x spell damage
├─ Acts twice (2:1 turn ratio)
└─ High crit chance

Weaknesses:
├─ Low defense (blocks ~20 damage)
├─ No resistance (~10%)
└─ One-shot vulnerable

Counter: High defense tank survives burst, one-shots back
```

**Tank (Max Defense):**
```
Strengths:
├─ 200+ damage blocked per hit
├─ High HP (Body investment)
└─ Devastating counter-hits (4x raw damage)

Weaknesses:
├─ Acts slowly (Turn Speed 10-20)
├─ Low spell damage
└─ Vulnerable to sustained DPS

Counter: Fast attackers with 2:1 turns chip damage
```

**Balanced (Distributed):**
```
Strengths:
├─ No major weaknesses
├─ Adaptable to situations
└─ Consistent performance

Weaknesses:
├─ Not best at anything
├─ Loses to specialists in their domain
└─ Requires tactical skill

Counter: Out-specialist them in key moments
```

---

## Example Character Builds

### Example 1: Fire Mage "Inferno" (Balanced - 30 Points)

**Base Distribution:**
```
Mind: 15
Body: 5
Spirit: 10
Total: 30/30 ✓
```

**World Stats Collected:**
```
World Mind: 5 (+25% scaling)
World Body: 3 (+15% scaling)
World Spirit: 7 (+35% scaling)

Effective Stats:
├─ Mind: 15 × 1.25 = 18.75
├─ Body: 5 × 1.15 = 5.75
└─ Spirit: 10 × 1.35 = 13.5
```

**Sub-Stat Distribution:**
```
Mind Points (15 available):
├─ Cost Reduction: 7
├─ Turn Speed: 5
└─ Crit Chance: 3

Body Points (9 available):
├─ Defense: 5
├─ Attack Speed: 2
└─ Raw Damage: 2

Spirit Points (21 available):
├─ Effect Damage: 10
├─ Resistance: 6
└─ Ability Size: 5
```

**Final Stats:**
```
Mind Effects:
├─ Cost Reduction: 18.75 × 7 × 0.006 = 0.79 = 7.9%
├─ Turn Speed: 10 + (18.75 × 5 × 0.5) = 56.9
└─ Crit Chance: 5% + (18.75 × 3 × 0.003) = 6.7%

Body Effects:
├─ Defense: 5.75 × 5 × 0.4 = 11.5 blocked
├─ Attack Speed: 1 + (5.75 × 2 × 0.05) = 1.58x
└─ Raw Damage: 1 + (5.75 × 2 × 0.006) = 1.07x

Spirit Effects:
├─ Effect Damage: 1 + (13.5 × 10 × 0.006) = 1.81x
├─ Resistance: 13.5 × 6 × 0.005 = 0.405 = 40.5%
└─ Ability Size: 1 + (13.5 × 5 × 0.007) = 1.47x
```

**Combat Performance:**
```
Fireball (50 base):
├─ Cost: 40 × (1 - 0.079) = 36.8 Energy
├─ Damage: 50 × 1.81 = 90.5
└─ Crit (6.7%): 90.5 × 1.5 = 135.8

Turn Order: Speed 56.9
Defense: Blocks 11.5 damage per hit
Resistance: Takes 59.5% of elemental damage
```

**Playstyle:** Versatile mage with good damage, moderate survivability, decent turn speed.

---

### Example 2: Tank "Ironwall" (High Budget - 35 Points)

**Base Distribution:**
```
Mind: 5
Body: 20
Spirit: 10
Total: 35/35 ✓
```

**World Stats:**
```
World Mind: 0
World Body: 7 (+35%)
World Spirit: 3 (+15%)

Effective Stats:
├─ Mind: 5 × 1.0 = 5
├─ Body: 20 × 1.35 = 27
└─ Spirit: 10 × 1.15 = 11.5
```

**Sub-Stat Distribution:**
```
Mind Points (0 available):
├─ All zero (no world stats)

Body Points (21 available):
├─ Defense: 15
├─ Attack Speed: 0
└─ Raw Damage: 6

Spirit Points (9 available):
├─ Effect Damage: 0
├─ Resistance: 6
└─ Ability Size: 3
```

**Final Stats:**
```
Body Effects:
├─ Defense: 27 × 15 × 0.4 = 162 blocked
├─ Attack Speed: 1.0x (base)
└─ Raw Damage: 1 + (27 × 6 × 0.006) = 1.97x

Spirit Effects:
├─ Resistance: 11.5 × 6 × 0.005 = 0.345 = 34.5%
└─ Ability Size: 1 + (11.5 × 3 × 0.007) = 1.24x

Mind Effects:
└─ Turn Speed: 10 (base only)
```

**Combat Performance:**
```
Heavy Strike (80 base):
├─ Damage: 80 × 1.97 = 157.6
└─ Speed: 1.0x (defender has 1.5s window)

Defense: Blocks 162 damage per hit
Resistance: Takes 65.5% of elemental damage
Turn Order: Speed 10 (acts last)
```

**Playstyle:** Immovable wall, tanks hits, devastating counter-attacks, slow but deadly.

---

### Example 3: Trickster "Paradox" (Low Budget - 25 Points)

**Base Distribution:**
```
Mind: 12
Body: 5
Spirit: 8
Total: 25/25 ✓
```

**World Stats:**
```
World Mind: 5 (+25%)
World Body: 0
World Spirit: 5 (+25%)

Effective Stats:
├─ Mind: 12 × 1.25 = 15
├─ Body: 5 × 1.0 = 5
└─ Spirit: 8 × 1.25 = 10
```

**Sub-Stat Distribution:**
```
Mind Points (15 available):
├─ Cost Reduction: 6
├─ Turn Speed: 6
└─ Crit Chance: 3

Body Points (0):
├─ All zero

Spirit Points (15 available):
├─ Effect Damage: 8
├─ Resistance: 4
└─ Ability Size: 3
```

**Final Stats:**
```
Mind Effects:
├─ Cost Reduction: 15 × 6 × 0.006 = 0.54 = 5.4%
├─ Turn Speed: 10 + (15 × 6 × 0.5) = 55
└─ Crit Chance: 5% + (15 × 3 × 0.003) = 6.35%

Spirit Effects:
├─ Effect Damage: 1 + (10 × 8 × 0.006) = 1.48x
├─ Resistance: 10 × 4 × 0.005 = 0.2 = 20%
└─ Ability Size: 1 + (10 × 3 × 0.007) = 1.21x

Body Effects:
└─ Defense: 0 (none!)
```

**Unique Spells (BROKEN):**
```
1. Time Stop: Opponent skips next turn (50% chance)
2. Paradox Shift: Swap stats with opponent for 3 turns
3. Void Walk: Next attack ignores ALL defenses
```

**Combat Performance:**
```
Void Blast (40 base):
├─ Damage: 40 × 1.48 = 59.2
└─ With Void Walk: 59.2 damage (IGNORES ALL DEFENSE!)

Defense: 0 blocked (dies easily)
Turn Speed: 55 (acts before most)
```

**Playstyle:** High-risk trickster, weak stats compensated by gamebreaking abilities, requires skill.

---

## Implementation Checklist

### Phase 1: Core Data Structures (Week 1)

**C++ Classes:**
- [ ] Create `EElementType` enum (11 elements)
- [ ] Create `UCharacterData` DataAsset class
- [ ] Implement all stat calculation functions
- [ ] Add editor validation (#if WITH_EDITOR)

**Blueprint Setup:**
- [ ] Create BP_CharacterDataBase blueprint parent
- [ ] Setup default values and categories

### Phase 2: Example Characters (Week 1-2)

**Create DataAssets for:**
- [ ] DA_FireMage_Inferno (Balanced - 30 points)
- [ ] DA_Tank_Ironwall (High budget - 35 points)
- [ ] DA_Trickster_Paradox (Low budget - 25 points)
- [ ] DA_IceMage_Frost (Balanced - 30 points)
- [ ] DA_BrokenDarkness_Abyss (Special - 30 points)

### Phase 3: UI/UX (Week 2-3)

**Character Selection:**
- [ ] Character portrait display
- [ ] Stat distribution widget
- [ ] Sub-stat allocation UI
- [ ] Real-time preview of calculated stats
- [ ] Validation feedback

**In-Game HUD:**
- [ ] Display effective stats
- [ ] Show turn speed indicators
- [ ] Defense/resistance visualizations

### Phase 4: Combat Integration (Week 3-4)

**Turn System:**
- [ ] Implement turn speed calculation
- [ ] Create turn order manager
- [ ] Handle 2:1 turn ratios

**Damage Calculation:**
- [ ] Apply Effect Damage multipliers
- [ ] Apply Raw Damage multipliers
- [ ] Implement Defense reduction
- [ ] Implement Resistance reduction
- [ ] Critical hit system

**Resource Management:**
- [ ] Energy pool from CharacterData
- [ ] Spell cost with Mind reduction
- [ ] Energy restoration (between battles only)

### Phase 5: Testing & Balance (Week 4-5)

**Unit Tests:**
- [ ] Stat calculation accuracy
- [ ] Sub-stat point validation
- [ ] Edge case handling (0 stats, max stats)

**Balance Tests:**
- [ ] Glass cannon vs Tank matchups
- [ ] Turn speed differential impacts
- [ ] Damage/defense equilibrium

**Iteration:**
- [ ] Adjust multiplier values if needed
- [ ] Fine-tune threshold values
- [ ] Character-specific balancing

### Phase 6: Polish (Week 5-6)

**Visual Feedback:**
- [ ] Stat increase animations
- [ ] Turn order indicators
- [ ] Damage number displays (with modifiers shown)

**Documentation:**
- [ ] In-game stat tooltips
- [ ] Build guide examples
- [ ] Tutorial for new players

---

## Technical Notes

### Performance Considerations
- All stat calculations are const functions (no side effects)
- Calculations happen on-demand (not cached)
- If performance becomes an issue, consider caching effective stats

### Extensibility
- Easy to add new sub-stats (just add to UCharacterData)
- Easy to adjust formulas (centralized in DataAsset functions)
- Character balance via DataAsset values (no code changes)

### Data Validation
- Editor-time validation prevents invalid stat distributions
- Runtime checks for sub-stat point allocation
- Clear error messages for designers

---

## Future Enhancements (Post-Launch)

### Potential Additions:
1. **Fourth Stat Category** (if needed for depth)
2. **Equipment System** (modifies base/world stats)
3. **Prestige System** (unlock higher world stat caps)
4. **Character Variants** (same character, different stat budgets)
5. **Dynamic Difficulty** (AI adjusts stat distribution)

### Balance Levers:
- Adjust multiplier constants (0.006 → 0.005, etc.)
- Change world stat caps (7 → 5 for PVP)
- Add diminishing returns formulas
- Implement hard caps on specific effects

---

## Appendix: Element Types

```cpp
UENUM(BlueprintType)
enum class EElementType : uint8
{
    Fire           // Offensive, DoT
    Water          // Control, Slow
    Earth          // Defense, Tanky
    Wind           // Mobility, Knockback
    Light          // Healing, Buffs
    Darkness       // Debuffs, Drains
    Lightning      // Speed, Stuns
    Void           // Stat reduction, Anti-buff
    Reality        // Random effects, Chaos
    Generic        // Non-elemental, Physical
    BrokenDarkness // Special, Absorption mechanics
};
```

---

## Version History

**v1.0 - November 23, 2025**
- Initial design document
- 3-stat system with 9 sub-stats
- Balanced multipliers (3-4x at max)
- Turn speed system (2:1 max)
- Ready for implementation

---

**END OF DOCUMENT**