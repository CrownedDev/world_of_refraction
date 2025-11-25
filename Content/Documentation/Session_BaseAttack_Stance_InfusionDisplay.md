# Session: BaseAttack, Stance & InfusionDisplay Systems

## Date: [Today's Date]

---

## Overview

Implemented foundational systems for combat loadout:
1. **BaseAttackData** - Attack animations and properties
2. **StanceData** - Idle pose customization
3. **InfusionDisplayData** - Visual effects for element infusion
4. **ElementColors** - Color constants for all elements

---

## 1. BaseAttackData System

### Structure

```
BaseAttackData (UPrimaryDataAsset)
├─ Identity
│   ├─ AttackName (FString)
│   └─ Description (FString)
├─ Combat
│   ├─ HitCount (int32, 1-2)
│   ├─ FirstHitPercent (float)
│   ├─ SecondHitPercent (float)
│   └─ InfusionEnergyCost (float)
├─ Animation
│   ├─ AttackMontage (UAnimMontage*)
│   └─ BaseAnimSpeed (float, 0.5-2.0)
└─ Presentation
    └─ Icon (UTexture2D*)
```

### Functions

| Function | Purpose |
|----------|---------|
| GetHitDamagePercent(int32) | Get damage % for specific hit |
| GetTotalDamagePercent() | Total damage % (validation) |
| CalculateAnimSpeed(float) | Apply Attack Speed multiplier |
| GetAttackSummary() | Formatted summary string |
| IsMultiHit() | Returns true if HitCount > 1 |

### Test Assets Created

| Asset | Type | Hits | Distribution | Cost | Speed |
|-------|------|------|--------------|------|-------|
| DA_Attack_Strike | Unarmed | 1 | 100% | 10 | 1.0x |
| DA_Attack_DoubleStrike | Unarmed | 2 | 50%+50% | 15 | 1.1x |
| DA_Attack_Slash | Weapon | 1 | 100% | 10 | 1.0x |
| DA_Attack_DoubleSlash | Weapon | 2 | 50%+50% | 15 | 1.2x |

### Files

| File | Location |
|------|----------|
| BaseAttackData.h | Public/ |
| BaseAttackData.cpp | Private/ |
| BaseAttackDataDebug.h | Public/ |
| BaseAttackDataDebug.cpp | Private/ |

---

## 2. StanceData System

### Structure

```
StanceData (UPrimaryDataAsset)
├─ Identity
│   ├─ StanceName (FString)
│   └─ Description (FString)
├─ Animation
│   └─ IdleAnimMontage (UAnimMontage*)
└─ Presentation
    └─ Icon (UTexture2D*)
```

### Usage

| State | Stance Used |
|-------|-------------|
| Unarmed | Character's UnarmedStance |
| Armed | Weapon's WeaponStance |

### Test Assets Created

| Asset | Type | Description |
|-------|------|-------------|
| DA_Stance_Relaxed | Unarmed | Calm, neutral pose |
| DA_Stance_CombatReady | Unarmed | Alert fighting stance |
| DA_Stance_SwordGuard | Weapon | Sword held ready |
| DA_Stance_SpearReady | Weapon | Spear defensive position |

### Files

| File | Location |
|------|----------|
| StanceData.h | Public/ |
| StanceDataDebug.h | Public/ |
| StanceDataDebug.cpp | Private/ |

---

## 3. InfusionDisplayData System

### Class Hierarchy

```
UInfusionDisplayData (Abstract Base)
├─ UCharacterInfusionDisplayData (Body/Aura)
└─ UWeaponInfusionDisplayData (Weapon)
```

### Base Structure (InfusionDisplayData)

```
InfusionDisplayData (Abstract)
├─ Identity
│   ├─ DisplayName (FString)
│   └─ Description (FString)
├─ Display
│   ├─ VFXSystem (UNiagaraSystem*)
│   ├─ EffectIntensity (float, 0.1-5.0)
│   ├─ bOverrideElementColor (bool)
│   └─ ColorOverride (FLinearColor)
└─ Presentation
    └─ Icon (UTexture2D*)
```

### CharacterInfusionDisplayData

```
CharacterInfusionDisplayData : InfusionDisplayData
└─ DisplayType (ECharacterInfusionDisplayType: Body/Aura)
```

### WeaponInfusionDisplayData

```
WeaponInfusionDisplayData : InfusionDisplayData
└─ (No additional fields - weapon type implied)
```

### Test Assets Created

| Asset | Class | Type |
|-------|-------|------|
| DA_Infusion_BodyGlow | CharacterInfusionDisplayData | Body |
| DA_Infusion_FloatingOrbs | CharacterInfusionDisplayData | Aura |
| DA_Infusion_WeaponAura | WeaponInfusionDisplayData | Weapon |

### Files

| File | Location |
|------|----------|
| ECharacterInfusionDisplayType.h | Public/ |
| InfusionDisplayData.h | Public/ |
| CharacterInfusionDisplayData.h | Public/ |
| WeaponInfusionDisplayData.h | Public/ |
| InfusionDisplayDataDebug.h | Public/ |
| InfusionDisplayDataDebug.cpp | Private/ |

---

## 4. ElementColors System

### Color Mapping (Rainbow Order)

| Element | Color | RGB |
|---------|-------|-----|
| Fire | Red | (1.0, 0.0, 0.0) |
| Lightning | Orange | (1.0, 0.5, 0.0) |
| Earth | Yellow | (1.0, 1.0, 0.0) |
| Wind | Green | (0.0, 1.0, 0.0) |
| Water | Blue | (0.0, 0.5, 1.0) |
| Reality | Indigo | (0.3, 0.0, 0.5) |
| Void | Violet | (0.6, 0.0, 1.0) |
| Light | White | (1.0, 1.0, 1.0) |
| Darkness | Black | (0.1, 0.1, 0.1) |
| Generic | Brown | (0.6, 0.4, 0.2) |
| BrokenDarkness | Black (base) | (0.1, 0.1, 0.1) |

### Helper Functions

| Function | Purpose |
|----------|---------|
| GetColorForElement(ERefractionElement) | Returns color for any element |
| GetBrokenDarknessColor(ERefractionElement) | Blends black + absorbed element |

### Special Element Rules

| Element | Infusion Behavior |
|---------|-------------------|
| Normal Elements | Uses energy, shows element color |
| Generic | Uses energy + HP, no visual (brown) |
| BrokenDarkness | Absorbs from defending/parrying, black + absorbed |

### BrokenDarkness Absorption

| State | Energy | Absorbed | Visual |
|-------|--------|----------|--------|
| Empty | 0 | None | Pure Black |
| Absorbed Fire | >0 | Fire | Black + Red |
| Absorbs Wind | >0 | Wind (replaces) | Black + Green |
| Spends all | 0 | None (resets) | Pure Black |

**Rules:**
- One element at a time
- New absorption replaces previous
- Resets to black at 0 energy

### Files

| File | Location |
|------|----------|
| ElementColors.h | Public/ |

---

## 5. CharacterData Updates

### New Properties Added

```cpp
// Forward declarations
class UBaseAttackData;
class UStanceData;
class UCharacterInfusionDisplayData;

// Loadout section
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Attack")
UBaseAttackData* BaseAttack = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Stance")
UStanceData* UnarmedStance = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Infusion")
UCharacterInfusionDisplayData* InfusionDisplay = nullptr;
```

---

## 6. Bug Fixes

- **SpellData.h**: Added `GetElementName()` function
- **EvolutionData.cpp**: Fixed spell/ultimate element validation
- **world_of_refraction.Build.cs**: Added "Niagara" module dependency

---

## 7. Debug Output Examples

### BaseAttack

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

### Attack Comparison

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

### Stance

```
==========================================
STANCE: Combat Ready
==========================================
Description: Alert and ready to fight
Animation: None
==========================================
```

### InfusionDisplay

```
==========================================
INFUSION DISPLAY: Body Glow
==========================================
Description: Elemental energy radiates from within
Type: Character (Body)
VFX: None
Color: Uses element default
Intensity: 1.00x
==========================================
```

---

## 8. Next Steps

1. **WeaponData** - Ties everything together:
   - WeaponAttack → BaseAttackData
   - WeaponAbilities[4] → AbilityData
   - WeaponStance → StanceData
   - InfusionDisplay → WeaponInfusionDisplayData

2. **CharacterData Final Updates**:
   - BaseAbilities[4]
   - PrimaryWeapon / SecondaryWeapon (Generic)
   - EquippedWeapon (Elemental)
   - bStartArmed

3. **Conjuration System** (Elemental only)

---

## 9. File Summary

### New Files Created

| File | Type |
|------|------|
| BaseAttackData.h/cpp | DataAsset |
| BaseAttackDataDebug.h/cpp | Debug |
| StanceData.h | DataAsset |
| StanceDataDebug.h/cpp | Debug |
| ECharacterInfusionDisplayType.h | Enum |
| InfusionDisplayData.h | Abstract DataAsset |
| CharacterInfusionDisplayData.h | DataAsset |
| WeaponInfusionDisplayData.h | DataAsset |
| InfusionDisplayDataDebug.h/cpp | Debug |
| ElementColors.h | Constants |

### Modified Files

| File | Changes |
|------|---------|
| CharacterData.h | Added BaseAttack, UnarmedStance, InfusionDisplay |
| SpellData.h | Added GetElementName() |
| world_of_refraction.Build.cs | Added Niagara dependency |

### Deleted Files

| File | Reason |
|------|--------|
| EInfusionDisplayType.h | Replaced by class hierarchy |
