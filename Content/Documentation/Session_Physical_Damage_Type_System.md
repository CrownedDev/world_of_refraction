# Session: Physical Damage Type System & Weapon Attack Separation

## Date: November 25, 2025

## Summary
Implemented a physical damage type system for Generic character weapon infusion, separating weapon attacks from base attacks. Generic warriors can now infuse abilities with their weapon's damage type (Slash/Pierce/Blunt) to build status bars, keeping them competitive with spell casters who use elemental damage.

---

## Core Design Philosophy

### The Problem
- Spell casters: Cast elemental spells → Build status bars (Burn, Freeze, etc.)
- Generic warriors: Use abilities → No status effects → Not competitive

### The Solution
**Weapon Infusion System:**
- Generic warriors **infuse abilities** with weapon's physical damage type
- Abilities inherit weapon's damage type and build status bars
- Creates parity: Spell casters get elemental effects, warriors get physical effects

---

## Physical Damage Type System

### Status Effects by Type

| Damage Type | Status Bar | Full Effect | Combat Role |
|-------------|-----------|-------------|-------------|
| **Slash** | Slash Bar | **Bleed** (DoT) | Sustained damage over time |
| **Pierce** | Pierce Bar | **Armor Break** (Defense down) | Anti-tank, penetration |
| **Blunt** | Blunt Bar | **Stun** (Can't act) | Crowd control |

### Combat Flow Example

```
Generic Warrior with Iron Sword (Slash):
1. Uses "Power Strike" ability (base 50 damage)
2. Ability infused with Sword → 50 Slash damage
3. Builds enemy's Slash bar by 10 points per hit
4. Bar fills (100 points) → Enemy starts Bleeding
5. Bleed deals DoT until cleansed

vs

Fire Spell Caster:
1. Casts "Fireball" (base 50 damage)
2. Deals 50 Fire damage
3. Builds enemy's Burn bar by 12 points
4. Bar fills → Enemy starts Burning
5. Burn deals DoT until cleansed
```

**Result: Both playstyles have status effects, balanced and competitive**

---

## Architecture Changes

### File Structure

**New Files Created:**
```
Source/world_of_refraction/Public/
├─ EPhysicalDamageType.h         (Slash/Pierce/Blunt enum)
└─ WeaponAttackData.h            (Inherits BaseAttackData)

Source/world_of_refraction/Private/
└─ WeaponAttackData.cpp
```

**Modified Files:**
```
WeaponData.h/cpp                 (Changed to UWeaponAttackData*, added infusion)
CharacterData.h                  (bUsePrimary, hidden BaseAttack for Generic)
CharacterDataDebug.h/cpp         (Weapon loadout display)
```

---

## Implementation Details

### EPhysicalDamageType.h

```cpp
UENUM(BlueprintType)
enum class EPhysicalDamageType : uint8
{
    Slash    UMETA(DisplayName = "Slash (Bleed)"),
    Pierce   UMETA(DisplayName = "Pierce (Armor Break)"),
    Blunt    UMETA(DisplayName = "Blunt (Stun)")
};
```

---

### WeaponAttackData.h

**Inheritance Structure:**
```
UPrimaryDataAsset
    └─ UBaseAttackData (unarmed attacks)
           └─ UWeaponAttackData (armed attacks + physical type)
```

**New Fields:**
```cpp
class UWeaponAttackData : public UBaseAttackData
{
    // Physical damage type for Generic character infusion
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type")
    EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::Slash;

    // Status buildup per hit (fills status bar for effects)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type")
    int32 StatusBuildup = 10;

    UFUNCTION(BlueprintPure, Category = "WeaponAttack")
    FString GetDamageTypeName() const;
};
```

**Why Inheritance?**
- Shares common fields: HitCount, DamageDistribution, InfusionCosts
- Adds weapon-specific: PhysicalDamageType, StatusBuildup
- Maintains type safety (WeaponData requires UWeaponAttackData*)

---

### WeaponData Changes

**Attack Type Changed:**
```cpp
// OLD
UBaseAttackData* WeaponAttack = nullptr;

// NEW
UWeaponAttackData* WeaponAttack = nullptr;
```

**Infusion System Added:**
```cpp
// Can Generic characters infuse this weapon with abilities?
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
bool bCanBeInfused = true;

// Status buildup multiplier when abilities are infused
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion", 
    meta = (EditCondition = "bCanBeInfused", ClampMin = "0.0", ClampMax = "2.0"))
float InfusionStatusMultiplier = 1.0f;
```

**Design Intent:**
- Some weapons channel abilities better (gauntlets vs daggers)
- Multiplier affects how fast status bars fill
- 1.0x = standard, 1.5x = 50% faster buildup, 0.5x = 50% slower

---

### CharacterData Changes

**BaseAttack Visibility:**
```cpp
// Base attack for unarmed state (Elemental only)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Attack",
    meta = (EditCondition = "InnateElement != ERefractionElement::Generic", EditConditionHides))
UBaseAttackData* BaseAttack = nullptr;
```

**Behavior:**
- **Elemental**: See BaseAttack (use when unarmed)
- **Generic**: BaseAttack hidden (always use weapon attacks)

---

**bUsePrimary Toggle:**
```cpp
// Use primary weapon/state at combat start?
// Elemental: true = Armed (Primary), false = Unarmed
// Generic: true = Primary weapon, false = Secondary weapon
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
bool bUsePrimary = true;
```

**Behavior Table:**

| Character Type | bUsePrimary = true | bUsePrimary = false |
|----------------|-------------------|---------------------|
| **Elemental** | Armed with Primary | Unarmed (spells only) |
| **Generic** | Armed with Primary | Armed with Secondary |

---

### CharacterDataDebug Output

**New Section Added:**
```
WEAPON LOADOUT:
  Starts With: Primary/Secondary (Generic)
  Starts: Armed/Unarmed (Elemental)
  Primary: [name] ([type])
    Attack: [name] [damage type] (Buildup: X)
    Abilities: X/6 [LOCKED]
    Infusion: X.Xx multiplier
  Secondary: [name] ([type])  (Generic only)
    Attack: [name] [damage type] (Buildup: X)
    Abilities: X/6
```

**Example Output:**
```
WEAPON LOADOUT:
  Starts With: Primary
  Primary: Sword (Sword)
    Attack: Slash [Slash] (Buildup: 10)
    Abilities: 0/6 
    Infusion: 1.0x multiplier
  Secondary: Gauntlets (Fists)
    Attack: Double Strike [Blunt] (Buildup: 15)
    Abilities: 0/6
```

---

## Test Assets Created

### Weapon Attacks

**DA_WeaponAttack_Slash** (Content/Data/Attacks/)
| Field | Value |
|-------|-------|
| Attack Name | Slash |
| Description | A slashing sword attack |
| Hit Count | 1 |
| Damage Distribution | [100] |
| Infusion Costs | [0] |
| **Physical Damage Type** | Slash (Bleed) |
| **Status Buildup** | 10 |

**DA_WeaponAttack_Strike** (Content/Data/Attacks/)
| Field | Value |
|-------|-------|
| Attack Name | Strike |
| Description | A powerful striking blow |
| Hit Count | 2 |
| Damage Distribution | [50, 50] |
| Infusion Costs | [0, 0] |
| **Physical Damage Type** | Blunt (Stun) |
| **Status Buildup** | 12 |

---

### Weapons Updated

**DA_Weapon_IronSword:**
- WeaponAttack: DA_WeaponAttack_Slash
- bCanBeInfused: ✓
- InfusionStatusMultiplier: 1.0

**DA_Weapon_BrawlerFists:**
- WeaponAttack: DA_WeaponAttack_Strike
- bCanBeInfused: ✓
- InfusionStatusMultiplier: 1.2 (better channeling)

---

## Design Rationale

### Why Separate Base vs Weapon Attacks?

**Conceptual Clarity:**
- BaseAttack = Unarmed (punches, kicks) - Elemental only
- WeaponAttack = Armed (sword, hammer) - All characters

**Technical Benefits:**
- Type safety (WeaponData requires weapon attack)
- Clear editor experience (no mixing)
- Separate balance tuning

---

### Why Infusion Instead of Element Conversion?

**Option Rejected:** Convert weapon to elemental damage
```
Fire Warrior: Sword deals Fire damage (becomes spell caster)
```

**Option Chosen:** Keep physical damage, add infusion
```
Generic Warrior: Abilities deal Slash damage (stays physical)
```

**Reasoning:**
- Preserves class identity (physical vs magical)
- Generic remains distinct from Elemental
- Creates unique playstyle (physical status effects)
- Balances against spell casters without copying them

---

### Why Per-Weapon Infusion Multipliers?

Different weapon types should have different feel:

| Weapon Type | Multiplier | Design Intent |
|-------------|-----------|---------------|
| Heavy weapons (Hammer) | 1.5x | High buildup, slow attacks |
| Light weapons (Dagger) | 0.8x | Low buildup, fast attacks |
| Balanced (Sword) | 1.0x | Standard baseline |
| Magical (Staff) | 0.5x | Poor physical channeling |

Creates weapon diversity without complex stat systems.

---

## Status Bar System Design

### Buildup Formula (Runtime)
```
Status Gain Per Hit = 
    WeaponAttack.StatusBuildup 
    × WeaponData.InfusionStatusMultiplier
    × Ability.PowerMultiplier (optional)
```

### Example Calculation
```
Iron Sword (Slash):
- StatusBuildup: 10
- InfusionStatusMultiplier: 1.0

Power Strike ability:
- Uses 1 hit
- Status gain: 10 × 1.0 = 10 points

Heavy Strike ability:
- Uses 2 hits
- Status gain: (10 × 1.0) × 2 = 20 points

After 5 Heavy Strikes:
- Total: 100 points → Slash bar full → Bleed activates
```

---

## Current System State

### Complete Data Pipeline

```
CharacterData
    └─ PrimaryWeapon (WeaponData)
           ├─ WeaponAttack (WeaponAttackData)
           │      ├─ PhysicalDamageType (Slash/Pierce/Blunt)
           │      └─ StatusBuildup (10)
           ├─ PresetAbilities[6]
           ├─ bCanBeInfused (true)
           └─ InfusionStatusMultiplier (1.0x)
```

**Runtime Flow:**
1. Character equips weapon
2. Abilities inherit weapon's PhysicalDamageType
3. Each ability hit builds status bar
4. Bar fills → Status effect activates
5. Effect persists until cleansed/expires

---

## Next Steps

### Immediate Priorities

1. **Spell Slots on CharacterData**
   - Add TArray<USpellData*> EquippedSpells (max 6)
   - Elemental only (Generic can't cast)
   - Update debug output

2. **Helper Functions**
   - GetActiveWeapon() - which weapon is equipped
   - CanSwitchWeapons() - Generic only
   - IsArmed() - combat state check

3. **Full Loadout Validation**
   - Ensure abilities come from weapons
   - Validate spell element matching
   - Check complete combat readiness

4. **Combat State System**
   - Track armed/unarmed state at runtime
   - Handle weapon switching (Generic)
   - Manage construct activation/dismissal

---

### Future Enhancements

**Status Effect Implementation:**
- Bleed DoT calculations
- Armor Break defense reduction
- Stun duration and CC rules
- Cleanse/dispel mechanics

**Advanced Infusion:**
- Combo bonuses (multiple status types)
- Status bar decay over time
- Critical hits boost buildup
- Weapon mastery system

**Balance Tuning:**
- Status bar thresholds per enemy type
- Effect duration scaling
- Buildup resistance stats
- Counter-play mechanics

---

## Commit
```
Add physical damage type system and weapon attack separation

- Created EPhysicalDamageType enum (Slash/Pierce/Blunt for status effects)
- Created WeaponAttackData inheriting from BaseAttackData
- Added PhysicalDamageType and StatusBuildup to weapon attacks
- Added bCanBeInfused and InfusionStatusMultiplier to WeaponData
- Renamed bStartsArmed to bUsePrimary for clearer behavior
- Elemental: bUsePrimary = Armed/Unarmed
- Generic: bUsePrimary = Primary/Secondary weapon
- Hidden BaseAttack field for Generic characters (weapon attacks only)
- Updated CharacterDataDebug to show weapon loadout with damage types
- Test assets: DA_WeaponAttack_Slash, DA_WeaponAttack_Strike
```
