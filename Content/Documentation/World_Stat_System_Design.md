# World Stat System Design Documentation

## Overview
The World Stat System provides progression through collectible stat boosts that scale character power from E-Rank to S-Rank.

---

## Core Concept

**World Stats are collectibles that grant substat point budgets:**
- Mind World Stat (0-7)
- Body World Stat (0-7)
- Spirit World Stat (0-7)

**Each world stat level = substat points budget**
- Players distribute these points among 3 substats per pillar
- Points are spent permanently in each pillar

---

## Scaling Progression

### Current Implementation (Phase 1)
```
PointsPerWorldStatLevel = 3

World Stats 0/0/0 → 0 substat points
World Stats 1/1/1 → 9 substat points (3 per pillar)
World Stats 7/7/7 → 63 substat points (21 per pillar)
```

### Planned Scaling (Phase 2)
```
PointsPerWorldStatLevel = 10

World Stats 0/0/0 → 0 substat points
World Stats 1/1/1 → 30 substat points (10 per pillar)
World Stats 7/7/7 → 210 substat points (70 per pillar)
```

### Why Phased Approach?
1. **Phase 1 (Current - 3 points):** Easier to balance and test combat
2. **Phase 2 (Future - 10 points):** More granular character customization
3. **Flexible:** Can adjust to 5, 10, 15, 20+ based on testing

---

## Rank System

| Rank | World Stat Levels | Total Substats (×3) | Total Substats (×10) |
|------|-------------------|---------------------|----------------------|
| **Unranked** | 0/0/0 | 0 points | 0 points |
| **E-Rank** | 1/1/1 | 9 points | 30 points |
| **D-Rank** | 2/2/2 | 18 points | 60 points |
| **C-Rank** | 3/3/3 | 27 points | 90 points |
| **C+ Rank** | 4/4/4 | 36 points | 120 points |
| **B-Rank** | 5/5/5 | 45 points | 150 points |
| **A-Rank** | 6/6/6 | 54 points | 180 points |
| **S-Rank** | 7/7/7 | 63 points | 210 points |

**Note:** Players can have unbalanced stats (e.g., 7/3/5), creating hybrid ranks.

---

## Substat Distribution

### Mind Substats (World Mind × Points)
1. **Cost Reduction Points** - Reduces Energy costs for spells/abilities
2. **Turn Speed Points** - Affects turn order frequency
3. **Crit Chance Points** - Increases critical hit probability

### Body Substats (World Body × Points)
1. **Defense Points** - Flat damage reduction per hit
2. **Attack Speed Points** - Animation speed (affects dodge difficulty)
3. **Raw Damage Points** - Physical/Generic damage multiplier

### Spirit Substats (World Spirit × Points)
1. **Effect Damage Points** - Elemental spell damage multiplier
2. **Resistance Points** - Percentage reduction to elemental damage
3. **Ability Size Points** - Area of effect scaling

---

## Requirements System

### World Stat Requirements (1-7 Range)
Weapons, spells, abilities, and evolutions can require **World Stat Levels**, not substat points.

**Examples:**
```
Legendary Weapon:
- Required World Mind: 5
- Required World Body: 6
- Required World Spirit: 7
└─ Player must have collected enough world stats to reach these levels

Master Spell:
- Required World Mind: 6
└─ Player must be at least A-Rank in Mind (6/7)

Evolution Path:
- Required World Body: 4
- Required World Spirit: 4
└─ Player must be at least C+ Rank in Body and Spirit
```

**Why World Stat Levels, Not Substat Points?**
- Cleaner requirements (1-7 is easier to understand than 21-63)
- Represents character progression milestones
- Prevents substat distribution from affecting equipment access
- Collectibles directly unlock content

---

## Validation Rules

### CharacterData Validation
```cpp
// World stat budget calculation
int32 ExpectedPoints = (WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * PointsPerWorldStatLevel;

// Verify substat spending matches budget
int32 UsedPoints = WorldCostReductionPoints + WorldTurnSpeedPoints + WorldCritChancePoints +
                   WorldDefensePoints + WorldAttackSpeedPoints + WorldRawDamagePoints +
                   WorldEffectDamagePoints + WorldResistancePoints + WorldAbilitySizePoints;

Valid = (UsedPoints == ExpectedPoints);
```

### Current Validation (PointsPerWorldStatLevel = 3)
```
World Stats 5/3/7 = 15 levels × 3 = 45 points budget
Must spend exactly 45 points across all 9 world substats
```

### Future Validation (PointsPerWorldStatLevel = 10)
```
World Stats 5/3/7 = 15 levels × 10 = 150 points budget
Must spend exactly 150 points across all 9 world substats
```

---

## Implementation Notes

### Current Code Location
**File:** `Source/world_of_refraction/Public/CharacterData.h`

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|World Bonuses")
int32 PointsPerWorldStatLevel = 3;  // CURRENT: Phase 1 scaling
```

### When to Change to 10 Points
**Indicators that scaling needs adjustment:**
1. Combat feels shallow - not enough character differentiation
2. Substat choices feel meaningless - want more granularity
3. Rank progression too fast - want slower power curve
4. Need more build diversity - 3 points limits creativity

**Change Process:**
1. Update `PointsPerWorldStatLevel = 10` in CharacterData.h
2. Rebalance all test characters (multiply world substats by 3.33x)
3. Update weapon/spell requirements if needed
4. Test combat balance with new scaling
5. Update all documentation

---

## Design Philosophy

### Why Not 30 Points Per Level?
Originally considered but rejected because:
- 210 points per pillar (7 × 30) = 630 total substats at S-Rank
- Too granular for initial balancing
- Harder to feel progression increments
- Overwhelming for players to distribute

### Why 3 → 10 Progression?
- **3 points:** Simple, easy to balance, good for prototyping
- **10 points:** Sweet spot - granular but not overwhelming
- **30+ points:** Reserved for potential late-game expansions

### Flexibility Built In
The `PointsPerWorldStatLevel` variable means we can adjust scaling:
- **Conservative:** 5 points per level
- **Balanced:** 10 points per level (planned)
- **Aggressive:** 15-20 points per level
- **Endgame:** 30+ points per level

---

## Example Character Progressions

### Fire Mage at E-Rank (1/1/1 with 3 points each)
```
World Stats: 1/1/1 (E-Rank)
Budget: 9 points total

Mind (3 points):
├─ Cost Reduction: 2
├─ Turn Speed: 1
└─ Crit Chance: 0

Body (3 points):
├─ Defense: 1
├─ Attack Speed: 0
└─ Raw Damage: 2

Spirit (3 points):
├─ Effect Damage: 3  (Focus on magic damage)
├─ Resistance: 0
└─ Ability Size: 0
```

### Fire Mage at S-Rank (7/7/7 with 3 points each)
```
World Stats: 7/7/7 (S-Rank)
Budget: 63 points total

Mind (21 points):
├─ Cost Reduction: 10
├─ Turn Speed: 6
└─ Crit Chance: 5

Body (21 points):
├─ Defense: 8
├─ Attack Speed: 5
└─ Raw Damage: 8

Spirit (21 points):
├─ Effect Damage: 15  (Still specializing)
├─ Resistance: 4
└─ Ability Size: 2
```

### Same Fire Mage at S-Rank (7/7/7 with 10 points each)
```
World Stats: 7/7/7 (S-Rank)
Budget: 210 points total

Mind (70 points):
├─ Cost Reduction: 30
├─ Turn Speed: 20
└─ Crit Chance: 20

Body (70 points):
├─ Defense: 25
├─ Attack Speed: 20
└─ Raw Damage: 25

Spirit (70 points):
├─ Effect Damage: 50  (Heavy magic focus)
├─ Resistance: 12
└─ Ability Size: 8
```

**Notice:** 10-point scaling allows for more nuanced specialization while maintaining clear build identity.

---

## Weapon Stat Bonuses Integration

### How Weapon Bonuses Work
Weapons provide **temporary stat bonuses while equipped**, separate from world stats:

```
Iron Sword:
├─ Requirements: Body 10 (Base stat requirement)
├─ Bonuses: +5 Attack, +2 Speed (Calculated stat bonuses)
└─ World Requirement: None (accessible early)

Legendary Blade:
├─ Requirements: Body 20, Spirit 15
├─ Bonuses: +15 Attack, +10 Magic Power, +5 Speed
└─ World Requirement: Body 5, Spirit 4 (B-Rank minimum)
```

### Requirement Types Compared

| Requirement Type | What It Checks | Range | Purpose |
|------------------|---------------|-------|---------|
| **Base Stats** | DistributedMind/Body/Spirit | 0-30 | Initial character creation |
| **World Stats** | WorldMindLevel/BodyLevel/SpiritLevel | 0-7 | Progression milestones |
| **Substats** | CostReduction, Defense, etc. | Variable | Fine-tuned bonuses |

**Example Equipment Requirements:**
```
Basic Sword:
├─ Base Body: 10 (needs strength)
└─ World Stats: None

Enchanted Staff:
├─ Base Mind: 15, Base Spirit: 10
└─ World Mind: 3, World Spirit: 2 (C-Rank minimum)

Legendary Weapon:
├─ Base: 20/20/20 (perfectly balanced base)
└─ World: 6/6/6 (A-Rank in all stats)
```

---

## Future Considerations

### Potential Scaling Adjustments
1. **Dynamic Scaling:** Different points per rank tier
   - Levels 1-3: 5 points each
   - Levels 4-5: 10 points each
   - Levels 6-7: 15 points each
   
2. **Pillar-Specific Scaling:** Different rates per pillar
   - Mind: 8 points per level (63 max)
   - Body: 10 points per level (70 max)
   - Spirit: 12 points per level (84 max)

3. **Prestige Levels:** Beyond level 7
   - Levels 8-10: New game+ content
   - 20+ points per level for endgame scaling

### Testing Checklist When Changing Scaling
- [ ] Update PointsPerWorldStatLevel value
- [ ] Rebalance all test characters
- [ ] Verify combat damage calculations
- [ ] Check equipment requirements still valid
- [ ] Test rank progression feel
- [ ] Update UI displays
- [ ] Regenerate all character debug outputs
- [ ] Update this documentation

---

## Summary

**Current State (Phase 1):**
- 3 points per world stat level
- S-Rank = 63 total substat points
- Good for initial balancing and prototyping

**Planned State (Phase 2):**
- 10 points per world stat level
- S-Rank = 210 total substat points
- More granular character builds

**Key Design Principle:**
World stat **levels** (1-7) unlock content and represent milestones.
Substat **points** provide the detailed customization within those milestones.

**Flexibility:**
The system is designed to easily adjust scaling by changing a single variable, allowing iteration based on gameplay testing and balance feedback.
