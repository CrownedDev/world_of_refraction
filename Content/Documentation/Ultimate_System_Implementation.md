# Session Summary: Ultimate System Implementation

**Date:** November 24, 2025  
**Project:** World of Refraction (UE5 5.7)  
**Branch:** `refactor/character-stats-base`

---

## Overview

Implemented a complete Ultimate ability system with element restrictions, world stat requirements, and flexible stat scaling. The system follows established patterns from AbilityData, SpellData, and ItemData.

---

## Files Created

### Enums (Public/)

| File                      | Purpose                                                                            |
| ------------------------- | ---------------------------------------------------------------------------------- |
| `EUltimateType.h`         | Ultimate categories (Damage, DamageAOE, CrowdControl, Buff, Debuff, Heal, Utility) |
| `EUltimateCooldownType.h` | Cooldown behavior (OncePerBattle, TurnBased)                                       |
| `EStatScalingType.h`      | Stat scaling options (None, Body, Spirit, Mind)                                    |

### Data Asset (Public/ & Private/)

| File               | Purpose                          |
| ------------------ | -------------------------------- |
| `UltimateData.h`   | Main data asset class definition |
| `UltimateData.cpp` | Function implementations         |

### Debug Tools (Public/ & Private/)

| File                    | Purpose                            |
| ----------------------- | ---------------------------------- |
| `UltimateDataDebug.h`   | Debug function library declaration |
| `UltimateDataDebug.cpp` | Debug output implementations       |

---

## UltimateData Structure

### Identity
- `UltimateName` - Display name
- `Element` - ERefractionElement (element-locked)
- `UltimateType` - Category for validation
- `Description` - Functional description
- `Theme` - Flavor text for cinematics

### Requirements
- `RequiredWorldMind` (0-7)
- `RequiredWorldBody` (0-7)
- `RequiredWorldSpirit` (0-7)

### Cooldown
- `CooldownType` - OncePerBattle or TurnBased
- `TurnCooldown` - Turns between uses (if TurnBased)

### Cost
- `EnergyCost` (0-150)

### Targeting
- `TargetType` - Reuses existing ETargetType enum

### Primary Effect
- `PrimaryEffectType` - For buff/debuff ultimates
- `BaseDamage` - Pre-multiplier damage
- `BaseHealing` - Pre-multiplier healing
- `EffectPercentage` - Buff/debuff strength
- `EffectDuration` - Effect duration in turns

### Secondary Effect
- `bHasSecondaryEffect` - Toggle
- `SecondaryEffectType` - Effect type
- `SecondaryValue` - Effect strength
- `SecondaryDuration` - Duration

### Status Effect
- `bAppliesStatus` - Toggle
- `StatusBuildup` - Buildup amount (triggers at 100)

### Scaling
- `ScalingType` - EStatScalingType (None/Body/Spirit/Mind)

### Special Flags
- `bIgnoresDefense`
- `bCanCrit`
- `bGrantsInvulnerability`

### Presentation
- `Icon`, `UltimateColor`, `ActivationSound`, `UltimateVFX`, `UltimateAnimation`

---

## Element Access Rules

| Ultimate Element | Who Can Use               |
| ---------------- | ------------------------- |
| Fire             | Fire characters only      |
| Water            | Water characters only     |
| Generic          | **Anyone**                |
| Darkness         | Darkness + BrokenDarkness |
| BrokenDarkness   | BrokenDarkness only       |
| (other elements) | Matching element only     |

**Special Case:** BrokenDarkness characters can use:
- BrokenDarkness ultimates
- Darkness ultimates
- Generic ultimates

---

## Stat Scaling System

| ScalingType | Effect                                       |
| ----------- | -------------------------------------------- |
| **None**    | Fixed damage, no multipliers                 |
| **Body**    | Damage × RawDamageMultiplier                 |
| **Spirit**  | Damage × EffectDamageMultiplier              |
| **Mind**    | Bonus crit chance + enhanced crit multiplier |

### Mind Scaling Details

Mind-scaling ultimates don't increase base damage. Instead:

1. **Bonus Crit Chance:** +50% of character's base crit chance
2. **Enhanced Crit Multiplier:** 1.5x + (EffectiveMind × 0.02)

Example (Fire Lord with 18 Effective Mind):
- Base Crit: 15%
- Bonus Crit: +7.5%
- Total Crit: 22.5%
- Crit Multiplier: 1.5 + (18 × 0.02) = 1.86x

---

## Key Functions

### Requirement Checks
```cpp
bool MeetsElementRequirement(const UCharacterData* Character) const;
bool MeetsWorldStatRequirements(const UCharacterData* Character) const;
bool CanCharacterUse(const UCharacterData* Character) const;
```

### Calculations
```cpp
float CalculateDamage(const UCharacterData* Character) const;
float CalculateHealing(const UCharacterData* Character) const;
float CalculateBonusCritChance(const UCharacterData* Character) const;
float CalculateCritMultiplier(const UCharacterData* Character) const;
```

### Utility
```cpp
FString GetElementName() const;
FString GetUltimateTypeName() const;
FString GetCooldownDescription() const;
FString GetScalingTypeName() const;
FString GetRequirementsString() const;
```

---

## Test Assets Created

| Asset                          | Element | Scaling | Notes                                   |
| ------------------------------ | ------- | ------- | --------------------------------------- |
| `DA_Ultimate_InfernoCataclysm` | Fire    | Spirit  | BurnDOT secondary, once per battle      |
| `DA_Ultimate_TidalDevastation` | Water   | Spirit  | ChillDOT secondary, turn-based cooldown |
| `DA_Ultimate_DesperateStrike`  | Generic | Mind    | Anyone can use, crit-focused            |

---

## Validation (Editor)

The `IsDataValid()` function checks:
- Name is set (not "Unnamed Ultimate")
- Energy cost > 0
- Damage ultimates have BaseDamage > 0
- Heal ultimates have BaseHealing > 0
- Buff/Debuff ultimates have percentage and duration
- CC ultimates have duration
- Turn cooldown > 0 if TurnBased
- Secondary effect has type if enabled
- World stat requirements ≤ 7

---

## Debug Output Example

```
==========================================
ULTIMATE: Desperate Strike
==========================================

IDENTITY:
  Element: Generic
  Type: Single Target Damage
  Cooldown: 3 turn cooldown
  Theme: "When all else fails."

REQUIREMENTS:
  Element: Generic
  World Mind: 1
  Water Lord: CAN USE

COST:
  Energy: 60

PRIMARY EFFECT:
  Damage: 300 (base) -> 300 (with Water Lord)

SCALING:
  Type: Mind (Crit Chance + Crit Damage)
  Base Crit: 0.3%
  Bonus Crit: +0.1%
  Total Crit: 0.4%
  Crit Multiplier: 2.08x

SPECIAL:
  Ignores Defense: No
  Can Crit: Yes
  Grants Invuln: No
==========================================
```

---

## Integration Points

### CharacterData (Future)
```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ultimate")
UUltimateData* EquippedUltimate = nullptr;

UFUNCTION(BlueprintPure, Category = "Ultimate")
UUltimateData* GetActiveUltimate() const;
```

### Combat Manager (Future)
- Check `CanCharacterUse()` before allowing activation
- Apply `CalculateDamage()` or `CalculateHealing()`
- Use `CalculateBonusCritChance()` and `CalculateCritMultiplier()` for Mind-scaling
- Track cooldowns (once per battle flag or turn counter)
- Check energy cost

---

## Next Steps

1. **EvolutionData** - Can override ultimate slot, grant passive effects
2. **CharacterData integration** - Add ultimate slot
3. **Combat Manager** - Ultimate activation logic

---

## Commit Suggested

```
feat(ultimate): Complete ultimate ability system

- Add EUltimateType, EUltimateCooldownType, EStatScalingType enums
- Implement UltimateData asset with full validation
- Add element-locked access rules (Generic available to all)
- Add world stat requirements system
- Implement 4-way stat scaling (None/Body/Spirit/Mind)
- Mind scaling: bonus crit chance + enhanced crit multiplier
- Add UltimateDataDebug function library
- Create test assets (Fire, Water, Generic ultimates)
- BrokenDarkness special access to Darkness ultimates
```