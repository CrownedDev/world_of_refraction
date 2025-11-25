# Broken Darkness Character System
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 1.0  
**Status:** Design Complete - Ready for Implementation

---

## Table of Contents

1. [Overview](#overview)
2. [Core Mechanics](#core-mechanics)
3. [Energy System](#energy-system)
4. [Absorption Mechanics](#absorption-mechanics)
5. [Overload State](#overload-state)
6. [Spell System](#spell-system)
7. [Item Interaction](#item-interaction)
8. [Technical Implementation](#technical-implementation)
9. [Balance Considerations](#balance-considerations)

---

## Overview

### **Broken Darkness Identity**

Broken Darkness is a unique elemental archetype that operates fundamentally differently from all other elements in World of Refraction. Where other elements have stable, inherent power, Broken Darkness characters are adaptive parasites who **absorb elemental energy from their enemies** to fuel their own abilities.

**Key Characteristics:**
- Start every battle at **0 energy** (critical disadvantage)
- Absorb energy by **defending/parrying elemental attacks**
- Gain **elemental alignment** based on what they absorb (Dark Flame, Dark Wind, etc.)
- Enter **Overload state** when energy exceeds maximum
- Build **Absorption Stacks** for exponentially stronger status effects
- Cannot infuse spells (dark hybrids are pre-filtered)

**Design Philosophy:**
> "A Broken Darkness fighter is a starving predator at the start of combat, desperately defending attacks to steal energy. As the fight progresses, they transform into an unstoppable force - overflowing with stolen power, dealing massive status damage, and wielding dark variants of their enemies' own elements against them."

---

## Core Mechanics

### **The Broken Darkness Loop**

**1. Start Weak (0 Energy)**
- Cannot cast most spells
- Only Tier 1 weak spells available
- Heavily reliant on items (Citrine crystals for energy)
- Must defend/parry to survive and build energy

**2. Absorb Energy**
- Defend/parry elemental spells → gain 30% of spell's energy cost
- Gain elemental alignment (Fire → Dark Flame)
- Build absorption stacks (same element repeatedly)

**3. Power Up (30-100 Energy)**
- Unlock stronger spell tiers
- More spell slots become available
- Can engage more aggressively

**4. Overload (101+ Energy)**
- Enter high-risk, high-reward state
- Elemental aura damages nearby enemies
- Take self-damage over time
- Maximum power available
- Energy drains back down

**5. Adapt or Reset**
- Absorb new element → change alignment (lose stacks)
- Absorb Darkness → revert to pure Broken Darkness
- Strategic decision: maintain stacks or adapt to enemy?

---

## Energy System

### **Energy Zones**

**Base Stats:**
```
Base Max Energy: 100
Overload Capacity: 30
Total Maximum: 130
```

**Energy Thresholds (Fixed):**

| Energy Range | State        | Spell Access | Notes                                   |
| ------------ | ------------ | ------------ | --------------------------------------- |
| 0-29         | Starving     | Tier 1 only  | Critical vulnerability, limited options |
| 30-99        | Powered      | Tier 1-2     | Functional combat capability            |
| 100          | Full Power   | Tier 1-3     | At maximum, no overload yet             |
| 101-130      | **OVERLOAD** | All tiers    | Aura active, self-damage begins         |

---

### **Starting Disadvantage**

**Unlike ALL other elements:**
- Fire Lord starts with 100/100 energy
- Water Lord starts with 100/100 energy
- **Broken Darkness starts with 0/100 energy**

**Implications:**
- Cannot cast most spells at fight start
- Must use items immediately (Citrine for energy)
- Must defend aggressively to build energy
- Extreme early-game vulnerability
- Late-game advantage if managed well

---

### **Energy Gain Sources**

**1. Defending/Parrying Spells (Primary)**
```
Energy Absorbed = Spell Energy Cost × 0.30

Examples:
- Defend Fireball (50 energy) → gain 15 energy
- Defend Inferno (100 energy) → gain 30 energy
- Defend Meteor Strike (80 energy) → gain 24 energy
- Defend Basic Attack (infused, 24 energy) → gain 7 energy
```

**To Reach Overload from 0:**
- Need to defend 4-7 medium-high cost spells
- OR combination of spells + items

**2. Items (Secondary)**
- Citrine crystals restore energy directly
- Critical for early-game survival
- Enables offensive play before absorption

**3. Natural Regeneration?**
- TBD - may regenerate 5 energy/turn when below 30?
- Or strictly absorption + items only?

---

## Absorption Mechanics

### **Elemental Alignment**

**How It Works:**
When Broken Darkness defends/parries an **elemental spell or infused ability**, they absorb that element's energy and gain alignment with it.

**Elemental Types (11 possible alignments):**
1. **Pure Broken Darkness** (default, no element absorbed)
2. **Dark Flame** (Fire absorbed)
3. **Dark Water** (Water absorbed)
4. **Dark Earth** (Earth absorbed)
5. **Dark Wind** (Wind absorbed)
6. **Dark Lightning** (Lightning absorbed)
7. **Dark Light** (Light absorbed)
8. **Dark Void** (Void absorbed)
9. **Dark Reality** (Reality absorbed)
10. **Dark Generic** (Generic absorbed)
11. **Dark Darkness** (Darkness absorbed - can exist!)

**Alignment Rules:**
- Absorbing an element **overwrites** current alignment
- Only **ONE** alignment active at a time (no multi-element)
- Absorbing **Darkness element** → **reverts to Pure Broken Darkness**
- Can still have Dark Darkness alignment (different from pure state)

**Example Flow:**
```
Start: Pure Broken Darkness
Defend Fire spell → Dark Flame alignment
Defend Fire spell → Still Dark Flame (stack increases)
Defend Wind spell → Dark Wind alignment (Dark Flame lost!)
Defend Darkness spell → Pure Broken Darkness (reset)
```

---

### **Absorption Stack System**

**Stack Progression:**
Repeatedly absorbing the **same element** builds stacks, exponentially increasing status effect power.

**Stack Multipliers (Max 3 Stacks):**
```
Initial Absorption: 1x status buildup (base)
Stack 1 (2nd absorption): 1x status buildup
Stack 2 (3rd absorption): 2x status buildup
Stack 3 (4th absorption): 4x status buildup
```

**Example - Dark Flame Burn Buildup:**
```
Base burn buildup per hit: 10

Stack 0 (first absorption):
- 10 buildup/hit

Stack 1 (absorbed Fire twice):
- 10 buildup/hit (no change yet)

Stack 2 (absorbed Fire 3 times):
- 20 buildup/hit (2x multiplier!)

Stack 3 (absorbed Fire 4 times):
- 40 buildup/hit (4x multiplier!)
```

**Stack Rules:**
- Max 3 stacks (requires 4 total absorptions of same element)
- Stacks **only** apply to **status effect buildup/damage**
- Does NOT increase raw spell damage
- Switching elements → **resets stacks to 0**
- Strategic choice: maintain stacks vs adapt to new element

**Design Intent:**
- Rewards specialization (keep absorbing same element)
- Creates tension (adapt or commit?)
- Exponential scaling prevents status effect weakness
- "Weakening the enemy's element by absorbing it"

---

### **What Can Be Absorbed**

**Absorbable Sources:**
- ✅ Elemental spells (Fire, Water, Earth, etc.)
- ✅ Infused abilities (any ability with element added)
- ✅ Enemy Broken Darkness dark hybrid spells
- ❌ Raw/Construct mode spells (no element to absorb)
- ❌ Non-infused abilities (no element present)
- ❌ Generic element attacks (Generic cannot cast)

**Defend vs Parry:**
- **Defend:** Successfully block attack (absorb energy)
- **Parry:** Perfect timing block (absorb energy + bonus?)
- Both trigger absorption mechanics

---

## Overload State

### **Entering Overload**

**Trigger Condition:**
```
Energy > Max Energy (100)
Energy >= 101
```

**How to Reach:**
- Absorb multiple high-cost spells
- Use energy-restoring items (Citrine)
- Combination of both

**Example:**
```
Current: 85 energy
Defend Inferno (100 cost) → gain 30 energy
New total: 115 energy (OVERLOAD TRIGGERED!)
```

---

### **Overload Effects**

**While Overloaded (Energy 101-130):**

**1. Elemental Aura (Offense)**
```
Enemies in close range take damage over time
Damage per turn = 15 × Effect Damage Multiplier
Visual: Crackling dark energy mixed with absorbed element

Example with Effect Damage 1.5:
- 22.5 damage/turn to nearby enemies
- Element type based on current alignment (Dark Flame = fire damage)
```

**2. Self-Damage (Cost)**
```
Take damage to self each turn
Damage per turn = 15 × Effect Damage Multiplier

Example with Effect Damage 1.5:
- 22.5 self-damage/turn
- High risk for high reward
```

**3. Energy Drain (Duration)**
```
Energy drains each turn, scaling inversely with Efficiency
Base Drain = 15 energy/turn
Actual Drain = Base × (1.0 - Efficiency × 0.01)

Examples:
- Efficiency 0: 15 energy/turn (overload lasts ~2 turns)
- Efficiency 30: 10.5 energy/turn (overload lasts ~3 turns)  
- Efficiency 60: 6 energy/turn (overload lasts ~5 turns)
- Efficiency 90: 1.5 energy/turn (overload lasts ~20 turns!)
```

**4. Maximum Power**
```
All highest-tier spells accessible
All spell slots unlocked
Peak combat effectiveness
```

---

### **Exiting Overload**

**Automatic Exit:**
```
When energy drops to 100 or below:
- Aura disappears
- Self-damage stops
- Returns to normal state
- Maintains elemental alignment and stacks
```

**Cannot be manually cancelled** - must wait for energy drain

**Strategic Considerations:**
- Time your overload for critical moments
- High Efficiency stat = longer overload duration
- High Effect Damage = more aura damage BUT more self-damage
- Balance offense vs survivability

---

## Spell System

### **Spell Categories**

**1. Pure Broken Darkness Spells**
- Available in Pure Broken Darkness state (no element absorbed)
- Always accessible regardless of alignment
- Dark/shadow themed abilities
- Examples: Shadow Bolt, Dark Shield, Void Strike

**2. Dark Hybrid Spells**
- Element-specific dark variants
- Only accessible with matching elemental alignment
- Player selects which spells to create dark variants of
- Examples: Dark Fireball (Fire), Dark Tidal Wave (Water)

**3. Cannot Be Infused**
- Dark hybrid spells are **pre-filtered** with element
- No infusion option available (unlike normal spells)
- Locked to their hybrid form
- Avoids double-element stacking

---

### **Spell Selection System**

**Player Customization:**
Players choose which elemental spells to create dark variants of during character creation/progression.

**Selection Process:**
```
1. Pure Broken Darkness Spells: 
   - Set permanently (6 per school available)
   
2. Dark Variant Selection:
   - Choose from elemental spell library
   - Create dark version for each chosen spell
   - Some spells have automatic variants (Fireball, etc.)
   - Complex spells may not have variants

Example Selection:
"I want Dark Fireball, Dark Inferno, Dark Meteor Strike"
→ Creates dark variants of those three Fire spells
→ Accessible when I have Dark Flame alignment
```

**Spell Availability:**
- Some spells universally available for dark variants (simple)
- Some spells element-locked (Fire-only mechanics)
- Some spells unavailable (too complex to corrupt)

---

### **Energy Threshold Spell Access**

**Player-Assigned Loadout:**

For each spell slot, player assigns spells at different energy thresholds.

**Example Destruction Slot 1:**
```
0-29 Energy (Tier 1):
└─ Dark Ember (30 damage, 15 energy cost)

30-99 Energy (Tier 2):  
└─ Dark Fireball (65 damage, 25 energy cost)

100+ Energy (Tier 3):
└─ Dark Inferno (140 damage, 50 energy cost)
```

**Rules:**
- Player controls what appears at each threshold
- Spells auto-switch based on current energy
- Can leave tiers empty to save energy
- Full control over power curve

**Strategic Examples:**

**Aggressive Build:**
```
Slot 1: Weak spell → Strong spell → Nuke
Slot 2: Empty → Medium spell → Strong spell
Slot 3: Empty → Empty → Ultimate spell

Strategy: Save energy early, unleash at high energy
```

**Sustained Build:**
```
Slot 1: Weak → Medium → Strong (always has something)
Slot 2: Weak → Medium → Strong
Slot 3: Utility → Utility → Utility

Strategy: Consistent access to full toolkit
```

**Overload Focused:**
```
Slot 1: Cheap → Cheap → Expensive (save energy)
Slot 2: Empty → Empty → Expensive
Slot 3: Empty → Empty → Expensive

Strategy: Intentionally stay under 100, then burst to overload
```

---

### **Spell Balance**

**Dark Hybrid Spells vs Normal Spells:**
- Dark variants are **slightly weaker** than normal element versions
- Prevents Broken Darkness from being overpowered
- Approximate power: 85-90% of normal spell

**Example Comparison:**
```
Normal Fireball:
- Base Damage: 70
- Energy Cost: 25
- Status Buildup: 12

Dark Fireball:
- Base Damage: 60 (85% of normal)
- Energy Cost: 25 (same)
- Status Buildup: 12 (same, BUT stacks multiply this!)
```

**Balance Rationale:**
- Lower base damage compensates for:
  - Absorption stack multipliers (up to 4x status!)
  - Overload aura damage
  - Extreme adaptability (access to all elements)
  - No element restrictions

---

## Item Interaction

### **Items Are Critical for Broken Darkness**

**Why Items Matter More:**
- Start at 0 energy (need Citrine crystals)
- Cannot reliably defend early (might not face spells)
- Items provide guaranteed energy source
- Enable offensive play before absorption

**Key Item Types:**

**Citrine (Energy Restore) - ESSENTIAL:**
```
F-tier: Restore 25 energy
A-tier: Restore 50 energy, take 10 damage
S-tier: Restore 80 energy, take 25 damage

Critical for:
- Early game survival
- Forcing overload state
- Recovery after energy drain
```

**Sapphire (Healing) - SURVIVAL:**
```
F-tier: Restore 40 HP
S-tier: Restore 150 HP

Counters:
- Overload self-damage
- Enemy pressure while building energy
```

**Other Items:**
- Work exactly same as other elements
- Bonus resistance from item element (Generic trait)
- Energy absorption from items (Broken Darkness trait)

**Strategic Item Priority:**
1. Citrine (energy) - Always bring 3 stacks
2. Sapphire (healing) - Counter overload damage
3. Iolite (debuff removal) - Recovery utility

---

## Technical Implementation

### **New Systems Required**

**1. Energy Threshold Manager**
```cpp
class UEnergyThresholdManager
{
    // Track current energy and determine available spells
    int32 CurrentEnergy;
    int32 MaxEnergy = 100;
    int32 OverloadCapacity = 30;
    
    EEnergyTier GetCurrentTier() const;
    TArray<USpellData*> GetAvailableSpells() const;
    bool IsOverloaded() const { return CurrentEnergy > MaxEnergy; }
};
```

**2. Elemental Alignment System**
```cpp
enum class EDarkAlignment : uint8
{
    PureBrokenDarkness,
    DarkFlame,
    DarkWater,
    DarkEarth,
    DarkWind,
    DarkLightning,
    DarkLight,
    DarkVoid,
    DarkReality,
    DarkGeneric,
    DarkDarkness
};

class UAlignmentManager
{
    EDarkAlignment CurrentAlignment;
    int32 AbsorptionStack; // 0-3
    
    void AbsorbElement(ERefractionElement Element);
    float GetStatusMultiplier() const;
    void ResetAlignment();
};
```

**3. Overload State Manager**
```cpp
class UOverloadManager
{
    bool bIsOverloaded;
    float OverloadDamagePerTurn;
    float EnergyDrainPerTurn;
    
    void UpdateOverloadState(float DeltaTime);
    void ApplyAuraDamage(TArray<AEnemy*> NearbyEnemies);
    void ApplySelfDamage();
    void DrainEnergy();
};
```

**4. Absorption Calculator**
```cpp
class UAbsorptionCalculator
{
    static float CalculateAbsorbedEnergy(float SpellEnergyCost)
    {
        return SpellEnergyCost * 0.30f;
    }
    
    static float CalculateStackMultiplier(int32 StackLevel)
    {
        // 1x, 1x, 2x, 4x
        switch(StackLevel)
        {
            case 0: return 1.0f;
            case 1: return 1.0f;
            case 2: return 2.0f;
            case 3: return 4.0f;
            default: return 1.0f;
        }
    }
};
```

**5. Spell Loadout System**
```cpp
struct FThresholdSpellSlot
{
    USpellData* Tier1Spell; // 0-29 energy
    USpellData* Tier2Spell; // 30-99 energy
    USpellData* Tier3Spell; // 100+ energy
};

class UBrokenDarknessLoadout
{
    TArray<FThresholdSpellSlot> DestructionSlots; // Max 6
    TArray<FThresholdSpellSlot> EnhancementSlots; // Max 6
    TArray<FThresholdSpellSlot> RestorationSlots; // Max 6
    TArray<FThresholdSpellSlot> ConjurationSlots; // Max 6
    
    TArray<USpellData*> PureBrokenDarknessSpells; // Always available
};
```

---

### **CharacterData Modifications**

**Add Broken Darkness-Specific Fields:**
```cpp
// In CharacterData.h
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
bool bIsBrokenDarkness = false;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness", meta = (EditCondition = "bIsBrokenDarkness"))
int32 OverloadCapacity = 30;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness", meta = (EditCondition = "bIsBrokenDarkness"))
EDarkAlignment CurrentAlignment = EDarkAlignment::PureBrokenDarkness;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness", meta = (EditCondition = "bIsBrokenDarkness"))
int32 AbsorptionStack = 0;
```

---

### **Combat System Integration**

**Defend/Parry Event:**
```cpp
void OnSuccessfulDefend(USpellData* IncomingSpell, bool bWasParry)
{
    if (Character->bIsBrokenDarkness && IncomingSpell->Element != ERefractionElement::None)
    {
        // Absorb energy
        float EnergyGained = IncomingSpell->BaseEnergyCost * 0.30f;
        Character->CurrentEnergy += EnergyGained;
        
        // Update alignment and stacks
        AlignmentManager->AbsorbElement(IncomingSpell->Element);
        
        // Check for overload
        if (Character->CurrentEnergy > Character->MaxEnergy)
        {
            OverloadManager->EnterOverload();
        }
    }
}
```

**Overload Tick:**
```cpp
void TickOverload(float DeltaTime)
{
    if (!bIsOverloaded) return;
    
    // Apply aura damage to enemies
    TArray<AEnemy*> NearbyEnemies = GetEnemiesInRange(AuraRadius);
    for (AEnemy* Enemy : NearbyEnemies)
    {
        float Damage = 15.0f * Character->EffectDamageMultiplier;
        Enemy->TakeDamage(Damage, EDamageType::Elemental);
    }
    
    // Apply self-damage
    float SelfDamage = 15.0f * Character->EffectDamageMultiplier;
    Character->TakeDamage(SelfDamage, EDamageType::Overload);
    
    // Drain energy
    float Drain = 15.0f * (1.0f - Character->Efficiency * 0.01f);
    Character->CurrentEnergy -= Drain;
    
    // Check exit condition
    if (Character->CurrentEnergy <= Character->MaxEnergy)
    {
        ExitOverload();
    }
}
```

---

### **Constants**

```cpp
// In CombatConstants.h
namespace BrokenDarknessConstants
{
    // Energy
    constexpr float ABSORPTION_PERCENTAGE = 0.30f; // 30% of spell cost
    constexpr int32 DEFAULT_OVERLOAD_CAPACITY = 30;
    
    // Overload
    constexpr float BASE_OVERLOAD_DAMAGE = 15.0f; // Base damage/turn
    constexpr float BASE_ENERGY_DRAIN = 15.0f; // Base drain/turn
    
    // Thresholds
    constexpr int32 TIER_1_MAX = 29;
    constexpr int32 TIER_2_MIN = 30;
    constexpr int32 TIER_2_MAX = 99;
    constexpr int32 TIER_3_MIN = 100;
    
    // Stack multipliers
    constexpr float STACK_0_MULTIPLIER = 1.0f;
    constexpr float STACK_1_MULTIPLIER = 1.0f;
    constexpr float STACK_2_MULTIPLIER = 2.0f;
    constexpr float STACK_3_MULTIPLIER = 4.0f;
    
    // Balance
    constexpr float DARK_SPELL_POWER_MODIFIER = 0.85f; // 85% of normal spells
}
```

---

## Balance Considerations

### **Strengths**

**1. Extreme Adaptability**
- Access to all 11 elements through absorption
- Can counter any enemy element
- Versatile spell selection

**2. Exponential Scaling**
- Absorption stacks multiply status effects (up to 4x!)
- Overload state adds massive damage
- Becomes unstoppable late-fight

**3. No Element Restrictions**
- Can use Fire, Water, Earth, etc. all in one loadout
- Strategic flexibility unmatched

**4. High Skill Ceiling**
- Rewards perfect defense/parry timing
- Energy management critical
- Alignment decision-making

---

### **Weaknesses**

**1. Catastrophic Early Game**
- 0 energy start = near-helpless
- Limited spell access initially
- Must survive long enough to absorb

**2. Item Dependent**
- REQUIRES Citrine crystals to function
- Without items, energy gain is uncertain
- Vulnerable if items depleted

**3. Self-Damage Risk**
- Overload damages self significantly
- Can die to own power
- Must manage health carefully

**4. Weaker Spell Power**
- Base spell damage 85% of normal
- Relies on stacks/overload to compensate
- Lower damage ceiling without setup

**5. Alignment Volatility**
- Switching elements resets stacks
- Must commit or lose progress
- Enemy can force element switches

---

### **Counterplay**

**Against Broken Darkness:**

**1. Pressure Early**
- Attack aggressively while they're at 0 energy
- Don't give them time to build up
- Force item usage

**2. Use Raw/Construct Mode Spells**
- No element = cannot be absorbed
- Denies energy gain
- Prevents alignment building

**3. Non-Elemental Damage**
- Physical attacks (no infusion)
- Raw damage spells
- Prevents absorption entirely

**4. Force Alignment Switches**
- Mix different element attacks
- Reset their stacks repeatedly
- Prevent exponential scaling

**5. Burst During Overload**
- They're taking self-damage
- Capitalize on weakened state
- Finish them before energy drains

---

### **Design Goals Achieved**

✅ **High Risk, High Reward:** Overload mechanic delivers  
✅ **Unique Playstyle:** Completely different from all other elements  
✅ **Strategic Depth:** Energy management, alignment choices, stack decisions  
✅ **Skill Expression:** Rewards perfect defense, energy optimization  
✅ **Balanced Power:** Strong late, weak early, item dependent  
✅ **Thematic Consistency:** Absorbing enemy power, parasitic gameplay  

---

## Future Considerations

### **Spell Point System**

**Eventually implement spell selection limits:**
```
Each spell has point cost:
- Basic spell = 1 point
- Advanced spell = 2 points  
- Master spell = 3 points

Budget per school = 6 points maximum

Example valid builds:
- 6 Basic spells (versatile)
- 2 Basic + 2 Advanced (balanced)
- 1 Basic + 1 Advanced + 1 Master (powerful but limited)

Prevents cherry-picking all master spells
Encourages build diversity
```

**Not implemented yet - for now, unrestricted selection.**

---

### **Additional Mechanics to Consider**

**1. Absorption Radius**
- Currently: defend/parry required
- Alternative: passive absorption aura (might be too strong)

**2. Energy Regeneration**
- Currently: absorption + items only
- Consider: 5 energy/turn when below 30? (emergency safety net)

**3. Overload Variants**
- Different overload effects per element?
- Dark Flame overload = more damage, less drain?
- Dark Water overload = more healing, less damage?

**4. Stack Decay**
- Currently: stacks persist until element change
- Alternative: stacks decay over time (encourages aggressive absorption)

**5. Alignment Bonuses**
- Dark Flame: +10% damage to Water enemies?
- Element-specific passive bonuses per alignment?

---

## Spell Examples

### **Pure Broken Darkness Spells**

**Available regardless of alignment:**

**1. Shadow Bolt (Destruction - Basic)**
```
Damage: 50
Energy: 18
Description: "Pure darkness projectile"
Always accessible
```

**2. Void Shield (Enhancement - Basic)**
```
Damage: 0
Energy: 20
Effect: Defense Buff 0.25, 3 turns
Description: "Shield of nothingness"
Always accessible
```

**3. Dark Drain (Restoration - Advanced)**
```
Damage: 40
Energy: 25
Effect: Health Restore 30 (to self)
Description: "Drain life essence"
Always accessible
```

---

### **Dark Hybrid Spell Examples**

**Dark Flame (Fire Absorbed):**

**1. Dark Fireball (Destruction - Basic)**
```
Damage: 60 (vs 70 normal Fireball)
Energy: 25
Status Buildup: 12 × Stack Multiplier
Description: "Fireball corrupted by darkness"
```

**2. Dark Inferno (Destruction - Master)**
```
Damage: 140 (vs 165 normal Inferno)
Energy: 50
Status Buildup: 35 × Stack Multiplier
Description: "Overwhelming dark flames"
```

**Stack Impact:**
```
Stack 0: 12 buildup/hit
Stack 1: 12 buildup/hit  
Stack 2: 24 buildup/hit (2x!)
Stack 3: 48 buildup/hit (4x!)

With 3 stacks, Dark Fireball applies status FOUR TIMES faster!
```

---

**Dark Water (Water Absorbed):**

**1. Dark Tidal Wave (Destruction - Advanced)**
```
Damage: 105 (vs 120 normal Tidal Wave)
Energy: 50
Status Buildup: 25 × Stack Multiplier
Effect: Speed Debuff -0.30, 3 turns
Description: "Corrupted wave of shadow water"
```

---

## Summary

**Broken Darkness is the most mechanically complex element in World of Refraction:**

**Unique Systems:**
- 0 energy start
- Absorption-based energy gain (30% of spell cost)
- Dynamic elemental alignment (11 possible)
- Exponential stack scaling (1x → 4x status)
- Overload risk/reward state
- Energy threshold spell access
- Player-controlled power curve

**Playstyle:**
- Defensive early (absorb energy)
- Aggressive mid (powered up)
- Explosive late (overload)
- Highly skill-dependent
- Extreme adaptability
- Item reliant

**Balance:**
- Weakest early game
- Strongest late game (if managed well)
- High skill ceiling
- High risk, high reward
- Unique counters (raw damage, element mixing)

**Implementation Priority:**
1. Energy threshold system
2. Absorption mechanics
3. Alignment tracking
4. Overload state
5. Spell loadout UI
6. Stack multiplier system

---

**End of Documentation**

Next Steps:
1. Create Broken Darkness character asset (DA_BrokenDarkness)
2. Implement energy threshold manager
3. Design Pure Broken Darkness spell set
4. Create dark hybrid spell variants
5. Build overload state system
6. Test absorption and stack mechanics