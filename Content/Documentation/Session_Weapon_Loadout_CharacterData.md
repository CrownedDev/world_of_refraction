# Session: Weapon Loadout on CharacterData

## Date: November 25, 2025

## Summary
Added weapon loadout fields to CharacterData, completing the character-to-weapon relationship. Generic characters can equip 2 weapons, Elemental characters get 1.

---

## Design Decisions

### Weapon Slot Allocation

| Character Type | Primary Weapon | Secondary Weapon | Total Abilities |
|----------------|----------------|------------------|-----------------|
| Generic | ✓ | ✓ | 12 (6+6, switch to access) |
| Elemental | ✓ | ✗ (hidden) | 6 |

### Armed State
- `bStartsArmed` determines if character begins combat with weapon drawn
- Unarmed = BaseAttack + Spells only
- Armed = WeaponAttack + PresetAbilities + Spells

---

## Files Modified

### CharacterData.h

**Added forward declaration:**
```cpp
class UWeaponData;
```

**Added weapon loadout section:**
```cpp
// ==================== WEAPON LOADOUT ====================

// Does this character start combat with weapon drawn?
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
bool bStartsArmed = false;

// Primary weapon (all characters)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons")
UWeaponData* PrimaryWeapon = nullptr;

// Secondary weapon (Generic characters only)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapons", 
    meta = (EditCondition = "InnateElement == ERefractionElement::Generic", EditConditionHides))
UWeaponData* SecondaryWeapon = nullptr;
```

**Added validation (in IsDataValid):**
```cpp
// Validate weapon loadout
if (InnateElement == ERefractionElement::Generic)
{
    if (PrimaryWeapon == nullptr && SecondaryWeapon == nullptr)
    {
        Context.AddWarning(FText::FromString(TEXT("Generic character has no weapons assigned")));
    }
}
else
{
    if (SecondaryWeapon != nullptr)
    {
        Context.AddError(FText::FromString(TEXT("Elemental characters cannot have a secondary weapon")));
        Result = EDataValidationResult::Invalid;
    }
}
```

### CharacterData.cpp

**Added include:**
```cpp
#include "WeaponData.h"
```

---

## Editor Behavior

### Elemental Characters (Fire, Water, etc.)
```
▼ Loadout | Weapons
   ☐ Starts Armed
   Primary Weapon: [None]
   (Secondary Weapon hidden)
```

### Generic Characters
```
▼ Loadout | Weapons
   ☐ Starts Armed
   Primary Weapon: [None]
   Secondary Weapon: [None]
```

---

## Current System Architecture

```
CharacterData
├─ Identity (Name, Element, Description, Portrait)
├─ Loadout
│   ├─ Attack (BaseAttack)
│   ├─ Stance (UnarmedStance)
│   ├─ Infusion (InfusionDisplay)
│   └─ Weapons ← NEW
│       ├─ bStartsArmed
│       ├─ PrimaryWeapon
│       └─ SecondaryWeapon (Generic only)
├─ Stats (Budget, World Levels)
├─ Initial Sub-Stats (Mind/Body/Spirit distribution)
└─ World Sub-Stats (Bonus distribution)

WeaponData
├─ Identity (Name, Type, Description)
├─ Combat
│   ├─ WeaponAttack
│   ├─ PresetAbilities[6]
│   └─ bAbilitiesLocked
├─ Animation (WeaponStance)
├─ Display (InfusionDisplay)
└─ Mesh (WeaponMesh, Icon)
```

---

## Combat State Flow

```
CHARACTER CREATION:
┌─────────────────────────────────────────┐
│ CharacterData                           │
│ ├─ InnateElement: Fire                  │
│ ├─ BaseAttack: DA_Attack_Punch          │
│ ├─ PrimaryWeapon: DA_Weapon_IronSword   │
│ └─ bStartsArmed: false                  │
└─────────────────────────────────────────┘

COMBAT START (bStartsArmed = false):
┌─────────────────────────────────────────┐
│ State: UNARMED                          │
│ ├─ Attack: BaseAttack (Punch)           │
│ ├─ Abilities: NONE                      │
│ └─ Spells: 6 equipped                   │
└─────────────────────────────────────────┘

AFTER DRAWING WEAPON:
┌─────────────────────────────────────────┐
│ State: ARMED (Primary)                  │
│ ├─ Attack: WeaponAttack (Slash)         │
│ ├─ Abilities: 6 from weapon             │
│ └─ Spells: 6 equipped                   │
└─────────────────────────────────────────┘

GENERIC SWITCHES TO SECONDARY:
┌─────────────────────────────────────────┐
│ State: ARMED (Secondary)                │
│ ├─ Attack: SecondaryWeapon attack       │
│ ├─ Abilities: 6 from secondary          │
│ └─ Spells: 6 equipped                   │
└─────────────────────────────────────────┘
```

---

## Validation Rules

| Rule | Severity | Message |
|------|----------|---------|
| Generic with no weapons | Warning | "Generic character has no weapons assigned" |
| Elemental with secondary | Error | "Elemental characters cannot have a secondary weapon" |

---

## Next Steps

1. **CharacterDataDebug** - Update to show weapon loadout
2. **Helper functions** - `GetActiveWeapon()`, `CanSwitchWeapons()`, `IsArmed()`
3. **Spell slots** - Add TArray<USpellData*> EquippedSpells to CharacterData
4. **Full loadout validation** - Ensure complete combat readiness

---

## Commit
```
Add weapon loadout to CharacterData

- Added bStartsArmed, PrimaryWeapon, SecondaryWeapon fields
- SecondaryWeapon only visible for Generic characters (EditConditionHides)
- Validation: Elemental cannot have secondary weapon
- Warning for Generic with no weapons assigned
```
