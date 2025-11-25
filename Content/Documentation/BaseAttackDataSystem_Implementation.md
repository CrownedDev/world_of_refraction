# Session: BaseAttackData System Implementation

## Date: [Today's Date]

## Changes Made

### New Files Created

| File                    | Location | Purpose                         |
| ----------------------- | -------- | ------------------------------- |
| BaseAttackData.h        | Public/  | Attack data asset definition    |
| BaseAttackData.cpp      | Private/ | Attack functions implementation |
| BaseAttackDataDebug.h   | Public/  | Debug utilities header          |
| BaseAttackDataDebug.cpp | Private/ | Debug utilities implementation  |

### BaseAttackData Structure
```cpp
BaseAttackData
├─ Identity
│   ├─ AttackName (FString)
│   └─ Description (FString)
├─ Combat
│   ├─ HitCount (int32, 1-2)
│   ├─ FirstHitPercent (float, for 2-hit)
│   ├─ SecondHitPercent (float, for 2-hit)
│   └─ InfusionEnergyCost (float)
├─ Animation
│   ├─ AttackMontage (UAnimMontage*)
│   └─ BaseAnimSpeed (float, 0.5-2.0)
└─ Presentation
    └─ Icon (UTexture2D*)
```

### Functions

| Function                   | Purpose                             |
| -------------------------- | ----------------------------------- |
| GetHitDamagePercent(int32) | Get damage % for specific hit index |
| GetTotalDamagePercent()    | Get total damage % (for validation) |
| CalculateAnimSpeed(float)  | Apply Attack Speed multiplier       |
| GetAttackSummary()         | Formatted one-line summary          |
| IsMultiHit()               | Returns true if HitCount > 1        |

### Debug Utilities

| Function               | Purpose                 |
| ---------------------- | ----------------------- |
| PrintAttackStats()     | On-screen display       |
| LogAttackStats()       | Console output          |
| GetAttackStatsString() | Formatted string        |
| CompareAttacks()       | Side-by-side comparison |

### Test Assets Created

| Asset                  | Type    | Hits | Distribution | Infusion Cost | Speed |
| ---------------------- | ------- | ---- | ------------ | ------------- | ----- |
| DA_Attack_Strike       | Unarmed | 1    | 100%         | 10            | 1.0x  |
| DA_Attack_DoubleStrike | Unarmed | 2    | 50% + 50%    | 15            | 1.1x  |
| DA_Attack_Slash        | Weapon  | 1    | 100%         | 10            | 1.0x  |
| DA_Attack_DoubleSlash  | Weapon  | 2    | 50% + 50%    | 15            | 1.2x  |

### Bug Fixes

- **SpellData.h**: Added `GetElementName()` function for element validation
- **EvolutionData.cpp**: Spell/Ultimate element validation now works

### Debug Output Examples

**Single Attack:**
```
==========================================
ATTACK: Strike
==========================================
Description: Basic Strike to the opponent
COMBAT:
  Hits: 1 (100%)
  Infusion Cost: 10 Energy
ANIMATION:
  Base Speed: 1.00x
  Montage: None
==========================================
```

**Comparison:**
```
========== ATTACK COMPARISON ==========
Property             | Double Strike        | Double Slash        
--------------------------------------------------------------
Hit Count            | 2                    | 2                   
Total Damage         | 100                 % | 100                 %
Infusion Cost        | 15                   | 15                  
Anim Speed           | 1.10                x | 1.20                x
==========================================
```

### Validation Rules

- Attack must have unique name
- Multi-hit attacks: Both hits must have positive damage
- Warning if total damage % outside 80-120% range
- Warning if no animation montage assigned

## Design Notes

- Damage is NOT stored on attack - calculated from character stats at runtime
- AnimSpeed is base value, multiplied by character's Attack Speed stat
- InfusionEnergyCost only applies when infusion is toggled ON
- Unarmed uses Strike/DoubleStrike, Weapons use Slash/DoubleSlash (or custom)

## Next Steps

1. StanceData - Idle pose customization
2. InfusionDisplayData - VFX for element infusion
3. WeaponData - Combines attack, abilities, stance
4. CharacterData updates - Integrate all new systems