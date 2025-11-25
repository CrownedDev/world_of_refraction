# Items System Documentation
## World of Refraction - Unreal Engine 5.7

**Last Updated:** November 25, 2024  
**Version:** 1.0  
**Status:** Design Complete - Ready for Implementation

---

## Table of Contents

1. [Overview](#overview)
2. [Crystal Types](#crystal-types)
3. [Tier System](#tier-system)
4. [Usage Mechanics](#usage-mechanics)
5. [Inventory System](#inventory-system)
6. [Quartz Transformation](#quartz-transformation)
7. [Technical Implementation](#technical-implementation)
8. [Balance Considerations](#balance-considerations)

---

## Overview

### **Items in World of Refraction**

Items are consumable crystals that provide immediate tactical benefits during combat. Unlike abilities and spells which are character-dependent, items are **universal resources** that any character can use - though different character types gain unique bonuses from item usage.

**Core Design Principles:**
- **Loot-Based Progression:** Items are found/looted at different tiers (F → S)
- **No Crafting/Upgrading:** You find what you find - S-tier is rare and powerful
- **Strategic Resource:** Limited uses (18 total per battle) forces tactical decisions
- **Universal Access:** Any character can use any item
- **Element-Specific Effects:** Each crystal type has an associated element

**Three Usage Systems:**

**1. Base Effect (Everyone)**
- Primary crystal effect applies to all characters
- Examples: healing, damage, buffs

**2. Generic Element Bonus**
- Generic element characters gain resistance to item's element
- Stackable across multiple item uses
- Compensates for Generic's inability to cast spells

**3. Broken Darkness Bonus**
- Broken Darkness absorbs energy from items (tier-based)
- Critical for their 0-energy start
- Scales with item tier

---

## Crystal Types

### **Element Assignments**

| Crystal  | Element   | Primary Effect               | Secondary Notes              |
| -------- | --------- | ---------------------------- | ---------------------------- |
| Opal     | Light     | Crit chance + Info reveal    | Vision/perception theme      |
| Onyx     | Darkness  | Energy silence               | Oppression/suppression theme |
| Emerald  | Wind      | Attack speed buff            | Swiftness/agility theme      |
| Sapphire | Water     | HP restoration               | Healing/life theme           |
| Citrine  | Lightning | Energy restore + self-damage | Volatile power theme         |
| Amber    | Earth     | Defense buff                 | Sturdiness/protection theme  |
| Amethyst | Void      | Random buff/debuff gambling  | Chaos/unpredictability theme |
| Iolite   | Reality   | Debuff removal               | Purification/clarity theme   |
| Quartz   | None      | Absorb damage & transform    | Adaptive/reactive theme      |
| Garnet   | Fire      | Direct damage                | Offensive/aggressive theme   |

---

## Tier System

### **Tier Rarity & Power**

**Tier Progression:** F → E → D → C → B → A → S

**Loot Distribution (Suggested):**
- **F-tier:** Common (40% drop rate)
- **E-tier:** Common (30% drop rate)
- **D-tier:** Uncommon (15% drop rate)
- **C-tier:** Uncommon (8% drop rate)
- **B-tier:** Rare (4% drop rate)
- **A-tier:** Rare (2% drop rate)
- **S-tier:** Legendary (1% drop rate)

**No Upgrading:**
- Items cannot be crafted or upgraded
- Finding S-tier Sapphire is a major reward
- Forces decisions on when to use best items
- Rarity creates meaningful choices

---

### **Crystal Tier Scaling Tables**

---

### **1. Opal (Light) - Critical Strike & Information**

**Effect:** Increase critical strike chance and reveal enemy stats.

| Tier | Crit Bonus | Info Revealed     | Duration | BD Energy |
| ---- | ---------- | ----------------- | -------- | --------- |
| F    | +10%       | World stats only  | 1 turn   | +15       |
| E    | +15%       | World stats only  | 2 turns  | +20       |
| D    | +20%       | World stats only  | 3 turns  | +25       |
| C    | +25%       | World + sub-stats | 2 turns  | +30       |
| B    | +30%       | World + sub-stats | 3 turns  | +35       |
| A    | +35%       | World + sub-stats | 4 turns  | +40       |
| S    | +40%       | World + sub-stats | 5 turns  | +50       |

**Strategic Use:**
- Essential for boss fights (learn enemy stats)
- Crit bonus stacks with character's natural crit chance
- Higher tiers reveal more detailed information longer
- S-tier = perfect scouting tool

**Generic Bonus:** Gain Light resistance (20% for duration)

---

### **2. Onyx (Darkness) - Energy Silence**

**Effect:** Silence portion of opponent's energy, preventing spell casting.

| Tier | Energy Silenced | Duration | BD Energy |
| ---- | --------------- | -------- | --------- |
| F    | 20%             | 1 turn   | +15       |
| E    | 30%             | 1 turn   | +20       |
| D    | 40%             | 1 turn   | +25       |
| C    | 50%             | 2 turns  | +30       |
| B    | 70%             | 2 turns  | +35       |
| A    | 80%             | 2 turns  | +40       |
| S    | 100% (complete) | 1 turn   | +50       |

**Mechanics:**
- Silences energy = cannot use spells requiring that energy
- Example: Enemy has 100 energy, use B-tier → only 30 usable energy for 2 turns
- S-tier = complete shutdown for 1 turn (ultimate disruption)

**Strategic Use:**
- Interrupt enemy ultimate/expensive spells
- S-tier right before enemy overload to shut them down
- Counter spell-heavy opponents

**Generic Bonus:** Gain Darkness resistance (20% for duration)

---

### **3. Emerald (Wind) - Attack Speed**

**Effect:** Increase animation speed and attack frequency.

| Tier | Speed Bonus | Duration | BD Energy |
| ---- | ----------- | -------- | --------- |
| F    | +15%        | 2 turns  | +15       |
| E    | +20%        | 2 turns  | +20       |
| D    | +25%        | 3 turns  | +25       |
| C    | +30%        | 3 turns  | +30       |
| B    | +40%        | 4 turns  | +35       |
| A    | +50%        | 4 turns  | +40       |
| S    | +60%        | 5 turns  | +50       |

**Mechanics:**
- Increases animation speed (attacks/abilities execute faster)
- More actions per turn cycle
- Stacks with character's natural speed stat

**Strategic Use:**
- Combo enabler (more hits = more status buildup)
- Multi-hit ability synergy (16 Hit Combo becomes devastating)
- Pressure tool (overwhelm slower enemies)

**Generic Bonus:** Gain Wind resistance (20% for duration)

---

### **4. Sapphire (Water) - Healing**

**Effect:** Instant HP restoration.

| Tier | HP Restored | BD Energy |
| ---- | ----------- | --------- |
| F    | 40          | +15       |
| E    | 55          | +20       |
| D    | 70          | +25       |
| C    | 90          | +30       |
| B    | 110         | +35       |
| A    | 135         | +40       |
| S    | 160         | +50       |

**Mechanics:**
- Instant heal (no over-time)
- Can exceed max HP briefly? (design decision)
- Most reliable sustain option

**Strategic Use:**
- Emergency recovery
- Counter overload self-damage (Broken Darkness)
- Sustain through long fights
- S-tier = nearly full heal for most characters

**Generic Bonus:** Gain Water resistance (20% for duration)

---

### **5. Citrine (Lightning) - Energy Restore**

**Effect:** Restore energy with increasing self-damage at higher tiers.

| Tier | Energy Restored | Self-Damage | BD Energy |
| ---- | --------------- | ----------- | --------- |
| F    | 25              | 0           | +15       |
| E    | 35              | 0           | +20       |
| D    | 45              | 0           | +25       |
| C    | 55              | 5           | +30       |
| B    | 70              | 15          | +35       |
| A    | 85              | 20          | +40       |
| S    | 100             | 30          | +50       |

**Mechanics:**
- Instant energy restoration
- Higher tiers = overcharge damage (risk/reward)
- Essential for spell-heavy builds
- **CRITICAL for Broken Darkness** (0 energy start!)

**Strategic Use:**
- Enable expensive spells mid-fight
- Force overload state (Broken Darkness)
- Emergency energy recovery
- S-tier = full energy bar but significant HP cost

**Risk Management:**
- Low tiers = safe, modest energy
- High tiers = massive energy but dangerous with low HP
- Don't use S-tier when already low health!

**Generic Bonus:** Gain Lightning resistance (20% for duration)

---

### **6. Amber (Earth) - Defense**

**Effect:** Increase defense stat, reducing incoming damage.

| Tier | Defense Bonus | Duration | BD Energy |
| ---- | ------------- | -------- | --------- |
| F    | +20%          | 2 turns  | +15       |
| E    | +25%          | 2 turns  | +20       |
| D    | +30%          | 3 turns  | +25       |
| C    | +35%          | 3 turns  | +30       |
| B    | +45%          | 4 turns  | +35       |
| A    | +50%          | 5 turns  | +40       |
| S    | +60%          | 5 turns  | +50       |

**Mechanics:**
- Reduces all incoming damage
- Stacks with character's defense stat
- Multiplicative or additive? (design decision)

**Strategic Use:**
- Tank enemy burst damage
- Survive dangerous phases (boss ultimates)
- Enable aggressive trades
- S-tier = near invulnerability for 5 turns

**Generic Bonus:** Gain Earth resistance (20% for duration)

---

### **7. Amethyst (Void) - Gambling**

**Effect:** Random buff or debuff to random stat with random magnitude.

| Tier | Buff Chance | Debuff Chance | Magnitude Range | Duration | BD Energy |
| ---- | ----------- | ------------- | --------------- | -------- | --------- |
| F    | 30%         | 70%           | 10-30%          | 3 turns  | +15       |
| E    | 40%         | 60%           | 15-35%          | 3 turns  | +20       |
| D    | 50%         | 50%           | 20-40%          | 3 turns  | +25       |
| C    | 60%         | 40%           | 25-45%          | 4 turns  | +30       |
| B    | 75%         | 25%           | 30-50%          | 4 turns  | +35       |
| A    | 85%         | 15%           | 35-60%          | 4 turns  | +40       |
| S    | 95%         | 5%            | 40-80%          | 5 turns  | +50       |

**Possible Effects (Random):**
- All stat buffs (Mind, Body, Spirit, sub-stats)
- All stat debuffs
- Confusion (random target selection)
- Berserk (increased damage, cannot defend)
- Slow/Haste
- ANY status effect possible

**Mechanics:**
- Completely random on use
- Could affect self OR enemy (ultra chaos at low tiers?)
- Could be game-winning or game-losing

**Strategic Use:**
- Desperation play when behind
- High-risk, high-reward gambit
- S-tier = 95% chance of massive buff (still risky!)
- Never use when winning comfortably

**Design Notes:**
- Most chaotic item in the game
- Could include visual effects (purple void energy)
- Log what effect triggered for player awareness

**Generic Bonus:** Gain Void resistance (20% for duration)

---

### **8. Iolite (Reality) - Debuff Removal**

**Effect:** Remove debuffs and grant temporary immunity.

| Tier | Debuffs Removed | Immunity Duration | BD Energy |
| ---- | --------------- | ----------------- | --------- |
| F    | 1 random        | 0 turns           | +15       |
| E    | 1 random        | 0 turns           | +20       |
| D    | 2 random        | 0 turns           | +25       |
| C    | 2 random        | 0 turns           | +30       |
| B    | All debuffs     | 0 turns           | +35       |
| A    | All debuffs     | 1 turn            | +40       |
| S    | All debuffs     | 2 turns           | +50       |

**Mechanics:**
- Instantly removes debuffs
- Low tiers = random selection (might not get the one you want)
- B+ tiers = full cleanse
- A/S tiers = immunity window (cannot be debuffed)

**Strategic Use:**
- Counter heavy debuff strategies
- Cleanse critical debuffs (defense down, damage down)
- S-tier = perfect reset button
- Combo with aggressive play (immunity window)

**Generic Bonus:** Gain Reality resistance (20% for duration)

---

### **9. Quartz (None) - Absorb & Transform**

**Effect:** Passively absorbs elemental damage and transforms into that element's crystal.

| Tier | Damage Absorbed | Result                                   |
| ---- | --------------- | ---------------------------------------- |
| F    | 80              | Transforms to element's crystal (F-tier) |
| E    | 100             | Transforms to element's crystal (E-tier) |
| D    | 120             | Transforms to element's crystal (D-tier) |
| C    | 150             | Transforms to element's crystal (C-tier) |
| B    | 180             | Transforms to element's crystal (B-tier) |
| A    | 220             | Transforms to element's crystal (A-tier) |
| S    | 250             | Transforms to element's crystal (S-tier) |

**Transformation Chart:**
```
Fire damage absorbed → Garnet (Fire)
Water damage absorbed → Sapphire (Water)
Earth damage absorbed → Amber (Earth)
Wind damage absorbed → Emerald (Wind)
Lightning damage absorbed → Citrine (Lightning)
Light damage absorbed → Opal (Light)
Darkness damage absorbed → Onyx (Darkness)
Void damage absorbed → Amethyst (Void)
Reality damage absorbed → Iolite (Reality)
Generic damage absorbed → Stays Quartz (no transform)
Broken Darkness absorbed → Onyx (Darkness base)
```

**Mechanics:**
- **Passive:** Continuously absorbs elemental damage while equipped
- Counter tracks cumulative damage absorbed
- Once threshold reached → transforms
- S-tier Quartz transforms into S-tier of that element!

**Strategic Use:**
- Adaptive defense (converts enemy's element against them)
- Guarantee high-tier items (S-tier Quartz → S-tier anything)
- Counter element-heavy enemies
- Inventory flexibility (one slot becomes many options)

**Unique Properties:**
- Only item with passive effect
- Only item that changes type
- No immediate use effect (pure passive)
- Can be "used" to check absorption progress?

**Generic Bonus:** No element (no resistance gained)  
**Broken Darkness:** No energy gain (element-neutral)

---

### **10. Garnet (Fire) - Damage**

**Effect:** Direct offensive damage to enemy.

| Tier | Damage | Burn DOT         | BD Energy |
| ---- | ------ | ---------------- | --------- |
| F    | 60     | None             | +15       |
| E    | 75     | None             | +20       |
| D    | 95     | None             | +25       |
| C    | 120    | None             | +30       |
| B    | 150    | None             | +35       |
| A    | 180    | None             | +40       |
| S    | 220    | 15/turn, 3 turns | +50       |

**Mechanics:**
- Instant damage to single target
- Only offensive item
- S-tier adds burn DOT (45 total additional damage)

**Strategic Use:**
- Finish low-HP enemies
- Consistent damage output
- Doesn't require stat investment (unlike spells)
- S-tier = 220 + 45 = 265 total damage!

**Generic Bonus:** Gain Fire resistance (20% for duration)

---

## Usage Mechanics

### **Base Usage (All Characters)**

**Using an item:**
1. Consumes 1 stack from selected crystal slot
2. Applies primary effect immediately
3. Triggers any secondary effects (Generic/BD bonuses)
4. Cannot be undone or cancelled

**Turn Cost:**
- Using an item = 1 action (same as ability/spell)
- Can only use 1 item per turn
- Must choose: Item OR Ability OR Spell

---

### **Generic Element Bonus**

**Special Mechanic for Generic Characters:**

When Generic element uses an item, they gain **temporary resistance** to that item's element.

**Resistance Effect:**
```
Duration: Entire battle (permanent for combat)
Magnitude: 20% damage reduction from that element
Stackable: Yes (use Fire + Water items = resist both)

Example:
Generic uses Garnet (Fire) → 20% Fire resistance
Generic uses Sapphire (Water) → 20% Water + Fire resistance
Generic uses Amber (Earth) → 20% Earth + Water + Fire resistance
```

**Strategic Implications:**
- Compensates Generic's inability to cast spells
- Encourages diverse item usage
- Can build multi-element resistance
- Becomes very tanky with all items used
- Defensive playstyle enabled

**Design Intent:**
> "Generic lacks offensive magic, but becomes an adaptable tank through items, gaining resistance to whatever elements they encounter."

---

### **Broken Darkness Bonus**

**Special Mechanic for Broken Darkness:**

When Broken Darkness uses an item, they **absorb energy** from it.

**Energy Absorption Table:**

| Tier | Energy Gained |
| ---- | ------------- |
| F    | +15           |
| E    | +20           |
| D    | +25           |
| C    | +30           |
| B    | +35           |
| A    | +40           |
| S    | +50           |

**Mechanics:**
- Energy gain in ADDITION to item's primary effect
- Critical for 0-energy start
- Enables early spell casting
- Can trigger overload state

**Strategic Implications:**
- Citrine (energy item) gives DOUBLE energy to BD
  - S-tier Citrine: 100 base + 50 BD bonus = 150 energy!
- Items enable aggressive early game
- Can force overload faster
- High-tier items = massive energy gains

**Example Opening:**
```
Broken Darkness Turn 1:
- 0 energy → Use S-tier Citrine
- Gain: 100 energy (item) + 50 (BD bonus) - 30 (self-damage) = 150 energy
- State: Immediately enters Overload (150 > 100 max)
- Aggressive play enabled from turn 1!
```

**Design Intent:**
> "Broken Darkness can convert their item resources into immediate combat power, trading consumables for early aggression."

---

## Inventory System

### **Loadout Configuration**

**Rules:**
- **6 item slots** (one per crystal type)
- **3 stacks per slot** (3 uses of each crystal)
- **Must be different crystal types** (no duplicate slots)
- **Mixed tiers allowed** (S/A/F in same slot, NO duplicate tiers)
- **Configured outside battle** (locked once combat starts)

**Example Valid Loadout:**
```
Slot 1: Garnet (Fire)
├─ Stack 1: S-tier (220 damage + burn)
├─ Stack 2: B-tier (150 damage)
└─ Stack 3: F-tier (60 damage)

Slot 2: Sapphire (Water)
├─ Stack 1: A-tier (135 HP)
├─ Stack 2: D-tier (70 HP)
└─ Stack 3: E-tier (55 HP)

Slot 3: Citrine (Lightning)
├─ Stack 1: S-tier (100 energy, 30 self-damage)
├─ Stack 2: C-tier (55 energy, 5 self-damage)
└─ Stack 3: F-tier (25 energy, 0 self-damage)

Slot 4: Opal (Light)
├─ Stack 1: S-tier (40% crit, full info, 5 turns)
├─ Stack 2: A-tier (35% crit, full info, 4 turns)
└─ Stack 3: C-tier (25% crit, partial info, 2 turns)

Slot 5: Iolite (Reality)
├─ Stack 1: S-tier (remove all, 2 turn immunity)
├─ Stack 2: B-tier (remove all, no immunity)
└─ Stack 3: D-tier (remove 2 random)

Slot 6: Amber (Earth)
├─ Stack 1: A-tier (50% defense, 5 turns)
├─ Stack 2: C-tier (35% defense, 3 turns)
└─ Stack 3: F-tier (20% defense, 2 turns)

Total: 18 item uses (6 slots × 3 stacks)
```

**Invalid Examples:**
```
❌ Two Garnet slots (must be different types)
❌ Garnet slot with two S-tier stacks (no duplicate tiers)
❌ Only 4 slots filled (allowed but not optimal)
✅ Empty stacks in slot (can have 1-2 items in slot)
```

---

### **Usage Order**

**Stack Consumption:**
- Items consumed from Stack 1 → Stack 2 → Stack 3
- Cannot choose which stack to use (FIFO - First In First Out)
- Strategic: Put best items first or last? (design decision)

**Alternative: Player Choice**
- Allow player to select which stack to consume
- More control, more complexity
- Recommended for tactical depth

---

### **Inventory Management**

**Outside Battle:**
- Access full item inventory
- Assign 6 crystal types to slots
- Arrange tier order within each slot
- Preview total loadout

**During Battle:**
- Cannot swap items
- Cannot rearrange stacks
- Can only consume what's equipped
- See remaining stacks in UI

---

## Quartz Transformation

### **Unique Mechanics**

**Quartz is the only transforming item:**

**Passive Absorption:**
- Equipped Quartz continuously absorbs elemental damage
- Tracks cumulative damage from combat start
- Visual indicator shows absorption progress?

**Transformation Trigger:**
```
If total elemental damage absorbed >= tier threshold:
→ Quartz transforms into that element's crystal
→ Maintains same tier
→ Can now be used for that crystal's effect

Example:
S-tier Quartz equipped (250 damage threshold)
Takes 180 Fire damage total
Takes 85 Water damage total (265 total, Fire dominant)
→ Transforms to S-tier Garnet!
```

**Dominant Element:**
- If multiple elements absorbed, highest damage wins
- Example: 100 Fire + 80 Water + 50 Earth → becomes Garnet (Fire)

---

### **Strategic Uses**

**1. Guaranteed High-Tier Items**
```
Find S-tier Quartz (rare)
Equip and absorb damage
Transform to S-tier of needed element
Guaranteed S-tier crystal!
```

**2. Adaptive Defense**
```
Enemy is Fire-heavy
Quartz absorbs Fire → becomes Garnet
Use Garnet (Fire damage) against Fire enemy
Counter with their own element
```

**3. Element Scouting**
```
Equip Quartz at fight start
See which element dominates enemy attacks
Plan strategy around transformation
```

**4. Inventory Flexibility**
```
Don't know what you'll need?
Equip Quartz instead of guessing
Transforms into what you encounter
Ultimate flexibility
```

---

### **Transformation Permanence**

**Once transformed:**
- Remains that crystal type for entire battle
- Cannot transform again
- Cannot revert to Quartz
- Counts as that element for Generic resistance bonus

**Outside battle:**
- Transformed Quartz becomes new crystal permanently
- Add to inventory as that crystal type
- Original Quartz consumed/converted

**Strategic Implication:**
> "Quartz is a one-time adaptive resource that becomes whatever you need most."

---

## Technical Implementation

### **Item Data Asset**

```cpp
// ItemData.h
UCLASS(BlueprintType)
class UItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString ItemName;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ERefractionElement Element; // Fire, Water, None (Quartz), etc.
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ECrystalType CrystalType; // Opal, Onyx, Emerald, etc.
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier; // F, E, D, C, B, A, S
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description;
    
    // ==================== EFFECTS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    EItemEffectType PrimaryEffectType;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    float PrimaryEffectValue; // Damage, healing, buff magnitude, etc.
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    int32 EffectDuration; // Turns (0 = instant)
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    EItemEffectType SecondaryEffectType; // For complex items
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    float SecondaryEffectValue;
    
    // ==================== SPECIAL PROPERTIES ====================
    
    // Citrine self-damage
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    float SelfDamage = 0.0f;
    
    // Quartz absorption threshold
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    float AbsorptionThreshold = 0.0f;
    
    // Broken Darkness energy gain
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Special")
    int32 BrokenDarknessEnergyGain = 0;
    
    // ==================== VISUAL/AUDIO ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UTexture2D* Icon;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    UParticleSystem* UseEffect;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    USoundBase* UseSound;
};
```

---

### **Enums**

```cpp
// CrystalType.h
UENUM(BlueprintType)
enum class ECrystalType : uint8
{
    Opal        UMETA(DisplayName = "Opal (Light)"),
    Onyx        UMETA(DisplayName = "Onyx (Darkness)"),
    Emerald     UMETA(DisplayName = "Emerald (Wind)"),
    Sapphire    UMETA(DisplayName = "Sapphire (Water)"),
    Citrine     UMETA(DisplayName = "Citrine (Lightning)"),
    Amber       UMETA(DisplayName = "Amber (Earth)"),
    Amethyst    UMETA(DisplayName = "Amethyst (Void)"),
    Iolite      UMETA(DisplayName = "Iolite (Reality)"),
    Quartz      UMETA(DisplayName = "Quartz (None)"),
    Garnet      UMETA(DisplayName = "Garnet (Fire)")
};

// ItemTier.h
UENUM(BlueprintType)
enum class EItemTier : uint8
{
    F_Tier      UMETA(DisplayName = "F (Common)"),
    E_Tier      UMETA(DisplayName = "E (Common)"),
    D_Tier      UMETA(DisplayName = "D (Uncommon)"),
    C_Tier      UMETA(DisplayName = "C (Uncommon)"),
    B_Tier      UMETA(DisplayName = "B (Rare)"),
    A_Tier      UMETA(DisplayName = "A (Rare)"),
    S_Tier      UMETA(DisplayName = "S (Legendary)")
};

// ItemEffectType.h
UENUM(BlueprintType)
enum class EItemEffectType : uint8
{
    None,
    Damage,
    Healing,
    EnergyRestore,
    CritBuff,
    InfoReveal,
    EnergySilence,
    AttackSpeedBuff,
    DefenseBuff,
    DebuffRemoval,
    DebuffImmunity,
    RandomEffect,      // Amethyst gambling
    PassiveAbsorb      // Quartz
};
```

---

### **Inventory System**

```cpp
// ItemLoadout.h
struct FItemSlot
{
    UPROPERTY()
    ECrystalType CrystalType;
    
    UPROPERTY()
    TArray<UItemData*> Stacks; // Max 3, must be different tiers
    
    UPROPERTY()
    int32 CurrentStackIndex = 0; // Which stack to consume next
};

class UItemLoadout
{
    UPROPERTY()
    TArray<FItemSlot> ItemSlots; // Max 6 slots
    
    bool AddItemToSlot(int32 SlotIndex, UItemData* Item);
    UItemData* UseItem(int32 SlotIndex);
    bool CanAddItem(int32 SlotIndex, UItemData* Item) const;
    int32 GetRemainingUses() const;
};
```

---

### **Quartz Transformation System**

```cpp
// QuartzManager.h
class UQuartzManager
{
    UPROPERTY()
    float TotalDamageAbsorbed = 0.0f;
    
    UPROPERTY()
    TMap<ERefractionElement, float> DamageByElement;
    
    UPROPERTY()
    UItemData* EquippedQuartz = nullptr;
    
    void AbsorbDamage(float Damage, ERefractionElement Element);
    void CheckTransformation();
    UItemData* TransformQuartz();
    ERefractionElement GetDominantElement() const;
};

// Implementation
void UQuartzManager::AbsorbDamage(float Damage, ERefractionElement Element)
{
    if (!EquippedQuartz || Element == ERefractionElement::None) return;
    
    TotalDamageAbsorbed += Damage;
    DamageByElement.FindOrAdd(Element) += Damage;
    
    CheckTransformation();
}

void UQuartzManager::CheckTransformation()
{
    if (TotalDamageAbsorbed >= EquippedQuartz->AbsorptionThreshold)
    {
        UItemData* NewCrystal = TransformQuartz();
        // Replace Quartz with new crystal in inventory
    }
}
```

---

### **Generic Resistance System**

```cpp
// GenericResistanceManager.h
class UGenericResistanceManager
{
    UPROPERTY()
    TMap<ERefractionElement, float> ElementResistances;
    
    void AddResistance(ERefractionElement Element, float Amount);
    float GetResistance(ERefractionElement Element) const;
    float CalculateDamageReduction(float IncomingDamage, ERefractionElement Element) const;
};

// Usage
void OnItemUsed(UItemData* Item)
{
    if (Character->Element == ERefractionElement::Generic)
    {
        ResistanceManager->AddResistance(Item->Element, 0.20f);
    }
}
```

---

### **Constants**

```cpp
// In CombatConstants.h
namespace ItemConstants
{
    // Inventory
    constexpr int32 MAX_ITEM_SLOTS = 6;
    constexpr int32 MAX_STACKS_PER_SLOT = 3;
    constexpr int32 MAX_TOTAL_ITEMS = MAX_ITEM_SLOTS * MAX_STACKS_PER_SLOT; // 18
    
    // Generic bonus
    constexpr float GENERIC_RESISTANCE_BONUS = 0.20f; // 20% reduction
    
    // Broken Darkness energy gain (tier-based)
    constexpr int32 BD_ENERGY_F = 15;
    constexpr int32 BD_ENERGY_E = 20;
    constexpr int32 BD_ENERGY_D = 25;
    constexpr int32 BD_ENERGY_C = 30;
    constexpr int32 BD_ENERGY_B = 35;
    constexpr int32 BD_ENERGY_A = 40;
    constexpr int32 BD_ENERGY_S = 50;
}
```

---

## Balance Considerations

### **Strengths**

**1. Universal Accessibility**
- Any character can use any item
- No stat requirements
- Reliable effects

**2. Strategic Depth**
- Limited uses (18 total) forces decisions
- Tier mixing within slots creates granularity
- When to use S-tier vs save for later?

**3. Character Synergies**
- Generic: Multi-element resistance
- Broken Darkness: Energy bootstrapping
- Others: Tactical flexibility

**4. Loot-Based Progression**
- Finding S-tier items feels rewarding
- RNG excitement
- No grinding/crafting

---

### **Weaknesses**

**1. Limited Quantity**
- Only 18 uses per battle
- Run out in long fights
- Must conserve resources

**2. RNG Dependent**
- Might never find S-tier
- Luck-based power spikes
- Can't guarantee specific items

**3. Inventory Constraints**
- Only 6 crystal types equipped
- Must choose which to bring
- Can't have everything

**4. No Tier Stacking**
- Can't bring 3 S-tier Sapphires
- Forced variety
- Best items used quickly

---

### **Design Goals Achieved**

✅ **Resource Management:** Limited uses create tactical decisions  
✅ **Universal Access:** Items don't discriminate by character  
✅ **Character Identity:** Bonuses for Generic/BD maintain uniqueness  
✅ **Strategic Variety:** 10 crystal types cover all needs  
✅ **Loot Excitement:** Tier system creates progression  
✅ **No Grinding:** Can't craft/upgrade, only find  

---

### **Counterplay**

**Against Item-Heavy Strategies:**

**1. Pressure Resources**
- Force early item usage
- Deny late-game stockpile
- Aggressive play

**2. Dispel Buffs**
- Counter buff items (Amber, Emerald)
- Debuff removal items finite
- Outlast their buffs

**3. Burst Damage**
- Sapphire healing is limited
- Only 3 stacks per fight
- Can't outheal burst

**4. Energy Drain**
- Counter Citrine spam
- Energy silence (Onyx)
- Deny resources

---

## Item Synergies

### **Character-Specific Builds**

**Generic Tank:**
```
Use all 6 different element items early
→ 6 × 20% = multi-element resistance
→ Becomes very tanky
→ Outlast opponent with defense
```

**Broken Darkness Aggressive:**
```
Equip 3 S-tier Citrine stacks
→ 150 energy × 3 = 450 total energy gain
→ Constant overload state
→ Overwhelming pressure
```

**Spell-Heavy Build:**
```
Equip multiple Citrine tiers
→ Energy on demand
→ Cast expensive spells freely
→ Sustain mage playstyle
```

**Crit Assassin:**
```
Use S-tier Opal (40% crit)
→ Combine with crit-focused character
→ Pressure Point ability (status on crit)
→ Devastate with crits
```

---

### **Item Combos**

**Emerald + Garnet:**
- Attack speed buff + damage item
- More item uses per turn cycle?
- Or faster animations = more ability uses

**Amber + Aggressive Spells:**
- Defense buff enables risky plays
- Tank damage while dealing own
- Survive retaliation

**Iolite + Citrine:**
- Remove debuffs
- Restore energy
- Full reset combo

**Opal + Status Abilities:**
- Reveal enemy resistances
- Target weak stats
- Perfect information play

---

## Future Considerations

### **Potential Expansions**

**1. Crafting System?**
- Combine lower tiers into higher?
- 3 F-tiers = 1 E-tier?
- Alternative progression path

**2. Item Abilities?**
- Active use vs passive equip?
- Some items grant passive bonuses?
- Trade-off: fewer slots for actives

**3. Set Bonuses?**
- Equip all Earth crystals (Amber, Emerald)?
- Grant Earth mastery bonus?
- Encourage themed loadouts

**4. Cursed Items?**
- Powerful effects + drawbacks?
- S+ tier with permanent debuff?
- High risk, high reward variants

**5. Unique Items?**
- One-of-a-kind crystals?
- Story/boss rewards?
- Special mechanics

---

## Summary

**Items are the universal tactical resource in World of Refraction:**

**10 Crystal Types:**
- Opal (Light) - Crit/Info
- Onyx (Darkness) - Silence
- Emerald (Wind) - Speed
- Sapphire (Water) - Healing
- Citrine (Lightning) - Energy
- Amber (Earth) - Defense
- Amethyst (Void) - Gambling
- Iolite (Reality) - Cleanse
- Quartz (None) - Transform
- Garnet (Fire) - Damage

**Core Systems:**
- 7-tier rarity (F → S)
- Loot-based acquisition
- 6 slots, 3 stacks (18 uses total)
- Generic resistance bonus
- Broken Darkness energy bonus
- Quartz transformation

**Strategic Depth:**
- Limited resources
- Tier mixing
- Character synergies
- Tactical timing
- Loadout flexibility

---

**End of Documentation**

Next Steps:
1. Create ItemData.h/cpp
2. Create all enum files
3. Build 70 item assets (10 types × 7 tiers)
4. Implement inventory system
5. Create Quartz transformation logic
6. Test Generic/BD **bonuses**