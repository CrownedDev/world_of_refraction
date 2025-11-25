# Design Document: Base Attack, Weapon, Stance & Infusion Display Systems

## Date: Session Follow-up

## Overview

This document outlines the design for four interconnected systems:
1. **Base Attack** - Character's unarmed attack
2. **Weapon** - Equipment that changes attack and abilities
3. **Stance** - Idle pose customization
4. **Infusion Display** - Visual effects when infusing elements

---

## 1. Base Attack System

### BaseAttackData Structure

```
BaseAttackData (UPrimaryDataAsset)
├─ Identity
│   ├─ AttackName (FString)
│   └─ Description (FString)
│
├─ Combat
│   ├─ HitCount (int32, 1-2)
│   ├─ DamageDistribution (float array, e.g., [100] or [50, 50])
│   └─ InfusionEnergyCost (float)
│
├─ Animation
│   ├─ AnimMontage (UAnimMontage*)
│   └─ AnimSpeed (float, default 1.0)
│
└─ Presentation
    └─ Icon (UTexture2D*)
```

### Design Notes

- **Damage** is NOT stored on attack - calculated from character stats
- **HitCount** max 2 - more hits = weaker per hit
- **AnimSpeed** affects attack tempo (influenced by Attack Speed stat)
- **InfusionEnergyCost** - energy per infused attack (0 when not infused)
- Set before battle, cannot change mid-fight
- All characters can use any base attack (no requirements)

---

## 2. Weapon System

### WeaponData Structure

```
WeaponData (UPrimaryDataAsset)
├─ Identity
│   ├─ WeaponName (FString)
│   ├─ WeaponType (EWeaponType: Sword, Spear, Staff, etc.)
│   └─ Description (FString)
│
├─ Combat
│   ├─ WeaponAttack (UBaseAttackData*) - replaces base attack
│   └─ WeaponAbilities[4] (TArray<UAbilityData*>) - replaces base abilities
│
├─ Animation
│   └─ StanceAnimation (UAnimMontage*) - idle pose when armed
│
├─ Presentation
│   ├─ WeaponMesh (UStaticMesh* or USkeletalMesh*)
│   ├─ DefaultInfusionDisplay (UInfusionDisplayData*) - player can override
│   └─ Icon (UTexture2D*)
│
└─ CanCharacterUse() - always true (no requirements)
```

### Weapon Slot Rules

| Character Type | Weapon Slots            | Notes                      |
| -------------- | ----------------------- | -------------------------- |
| Generic        | 2 (Primary + Secondary) | Can switch between both    |
| Elemental      | 1                       | Can use Conjuration spells |

### Weapon Switching

- **Cost:** Free action
- **Timing:** Beginning of turn only
- **Options:** Unarmed ↔ Primary ↔ Secondary (Generic) or Unarmed ↔ Weapon (Elemental)

---

## 3. Stance System

### StanceData Structure

```
StanceData (UPrimaryDataAsset)
├─ Identity
│   ├─ StanceName (FString)
│   └─ Description (FString)
│
├─ Animation
│   └─ IdleAnimMontage (UAnimMontage*)
│
└─ Presentation
    └─ Icon (UTexture2D*)
```

### Stance Rules

| State   | Stance Used               |
| ------- | ------------------------- |
| Unarmed | Character's UnarmedStance |
| Armed   | Weapon's StanceAnimation  |

- Weapons have locked stances (cannot override)
- Player customizes their unarmed stance only

---

## 4. Infusion Display System

### InfusionDisplayData Structure

```
InfusionDisplayData (UPrimaryDataAsset)
├─ Identity
│   ├─ DisplayName (FString)
│   └─ Description (FString)
│
├─ Display
│   ├─ DisplayType (EInfusionDisplayType: Body, Weapon, Aura)
│   └─ VFXSystem (UNiagaraSystem* or UParticleSystem*)
│
└─ Presentation
    └─ PreviewIcon (UTexture2D*)
```

### Display Types

| Type   | Description                | Examples                                                  |
| ------ | -------------------------- | --------------------------------------------------------- |
| Body   | Effects on character model | Glow, glowing eyes, element markings, skin shimmer        |
| Weapon | Effects on weapon          | Flaming blade, frost edge, lightning arcs, void tendrils  |
| Aura   | Effects around character   | Floating orbs, swirling leaves, shadow wisps, light halos |

### Infusion Display Assignment

```
Character
├─ BodyInfusionDisplay → InfusionDisplayData
├─ WeaponInfusionDisplay → InfusionDisplayData (overrides weapon default)
└─ AuraInfusionDisplay → InfusionDisplayData

Weapon
└─ DefaultInfusionDisplay → InfusionDisplayData (used if player doesn't override)
```

---

## 5. Character Loadout (Complete)

### Generic Character

```
CharacterData (Generic)
├─ ... existing stats ...
│
├─ Combat Loadout
│   ├─ BaseAttack → BaseAttackData (unarmed)
│   ├─ BaseAbilities[4] → AbilityData (unarmed)
│   ├─ PrimaryWeapon → WeaponData
│   ├─ SecondaryWeapon → WeaponData
│   ├─ Spells → SpellData[]
│   ├─ EquippedUltimate → UltimateData
│   └─ bStartArmed (bool)
│
└─ Cosmetics
    ├─ UnarmedStance → StanceData
    ├─ BodyInfusionDisplay → InfusionDisplayData
    ├─ WeaponInfusionDisplay → InfusionDisplayData (optional)
    └─ AuraInfusionDisplay → InfusionDisplayData
```

### Elemental Character

```
CharacterData (Elemental)
├─ ... existing stats ...
│
├─ Combat Loadout
│   ├─ BaseAttack → BaseAttackData (unarmed)
│   ├─ BaseAbilities[4] → AbilityData (unarmed)
│   ├─ EquippedWeapon → WeaponData (single slot)
│   ├─ Spells → SpellData[]
│   ├─ EquippedUltimate → UltimateData
│   └─ bStartArmed (bool)
│
└─ Cosmetics
    ├─ UnarmedStance → StanceData
    ├─ BodyInfusionDisplay → InfusionDisplayData
    ├─ WeaponInfusionDisplay → InfusionDisplayData (optional)
    └─ AuraInfusionDisplay → InfusionDisplayData
```

---

## 6. Battle States

### Generic Character States

```
Unarmed:
├─ Attack: BaseAttack
├─ Abilities: BaseAbilities[4]
├─ Spells: All available
└─ Actions: Switch to Primary OR Secondary

Primary Weapon:
├─ Attack: PrimaryWeapon.WeaponAttack
├─ Abilities: PrimaryWeapon.WeaponAbilities[4]
├─ Spells: All available
└─ Actions: Switch to Unarmed OR Secondary

Secondary Weapon:
├─ Attack: SecondaryWeapon.WeaponAttack
├─ Abilities: SecondaryWeapon.WeaponAbilities[4]
├─ Spells: All available
└─ Actions: Switch to Unarmed OR Primary
```

### Elemental Character States

```
Unarmed:
├─ Attack: BaseAttack
├─ Abilities: BaseAbilities[4]
├─ Spells: All available
└─ Actions: Switch to Weapon, Cast Conjuration

Armed:
├─ Attack: EquippedWeapon.WeaponAttack
├─ Abilities: EquippedWeapon.WeaponAbilities[4]
├─ Spells: All available
└─ Actions: Switch to Unarmed

Conjured (from Conjuration spell):
├─ Attack: ConjuredWeapon.WeaponAttack
├─ Abilities: ConjuredWeapon.WeaponAbilities[3] + Dispel
├─ Spells: SEALED
└─ Actions: Dispel only (returns to previous state)
```

---

## 7. Conjuration System (Elemental Only)

### Overview

Conjuration spells create temporary weapons for elemental characters, providing weapon access at the cost of spell flexibility.

### Rules

- Only elemental characters can use Conjuration
- Casting Conjuration:
  - Creates a temporary weapon
  - SEALS all other spells
  - Replaces one ability slot with "Dispel"
- Dispelling:
  - Free action, beginning of turn
  - Returns to previous state (armed/unarmed)
  - Unseals all spells

### ConjurationSpellData (extends SpellData?)

```
ConjurationSpellData
├─ ... base spell fields ...
└─ ConjuredWeapon → WeaponData
```

---

## 8. Infusion Mechanics

### Toggle Behavior

- Infusion is toggled ON/OFF
- When ON: Attacks cost energy, deal elemental damage
- When OFF: Attacks are free, deal physical damage
- Visual feedback based on InfusionDisplay settings

### Visual Priority

```
When Infused:
1. Body effect always shows (if set)
2. Aura effect always shows (if set)
3. Weapon effect shows:
   - Player's WeaponInfusionDisplay if set
   - Otherwise Weapon's DefaultInfusionDisplay
   - If unarmed, body/aura only
```

---

## 9. New Enums Needed

```cpp
// EWeaponType.h
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    Sword       UMETA(DisplayName = "Sword"),
    Greatsword  UMETA(DisplayName = "Greatsword"),
    Spear       UMETA(DisplayName = "Spear"),
    Staff       UMETA(DisplayName = "Staff"),
    Dagger      UMETA(DisplayName = "Dagger"),
    Axe         UMETA(DisplayName = "Axe"),
    Hammer      UMETA(DisplayName = "Hammer"),
    Bow         UMETA(DisplayName = "Bow"),
    Fists       UMETA(DisplayName = "Fists/Gauntlets"),
    Scythe      UMETA(DisplayName = "Scythe")
};

// EInfusionDisplayType.h
UENUM(BlueprintType)
enum class EInfusionDisplayType : uint8
{
    Body    UMETA(DisplayName = "Body Effect"),
    Weapon  UMETA(DisplayName = "Weapon Effect"),
    Aura    UMETA(DisplayName = "Aura Effect")
};
```

---

## 10. Files to Create

| File                      | Type      | Purpose                   |
| ------------------------- | --------- | ------------------------- |
| EWeaponType.h             | Enum      | Weapon categories         |
| EInfusionDisplayType.h    | Enum      | Infusion visual types     |
| BaseAttackData.h/cpp      | DataAsset | Attack definitions        |
| WeaponData.h/cpp          | DataAsset | Weapon definitions        |
| StanceData.h/cpp          | DataAsset | Stance definitions        |
| InfusionDisplayData.h/cpp | DataAsset | Visual effect definitions |

### Debug Files (Optional)

| File                      | Purpose         |
| ------------------------- | --------------- |
| BaseAttackDataDebug.h/cpp | Debug utilities |
| WeaponDataDebug.h/cpp     | Debug utilities |

---

## 11. CharacterData Updates Needed

Add to CharacterData.h:

```cpp
// Combat Loadout
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Attack")
UBaseAttackData* BaseAttack = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Abilities")
TArray<UAbilityData*> BaseAbilities; // Max 4

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapon", 
          meta = (EditCondition = "InnateElement == ERefractionElement::Generic"))
UWeaponData* PrimaryWeapon = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapon",
          meta = (EditCondition = "InnateElement == ERefractionElement::Generic"))
UWeaponData* SecondaryWeapon = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Weapon",
          meta = (EditCondition = "InnateElement != ERefractionElement::Generic"))
UWeaponData* EquippedWeapon = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Battle")
bool bStartArmed = false;

// Cosmetics
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Stance")
UStanceData* UnarmedStance = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Infusion")
UInfusionDisplayData* BodyInfusionDisplay = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Infusion")
UInfusionDisplayData* WeaponInfusionDisplay = nullptr;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Infusion")
UInfusionDisplayData* AuraInfusionDisplay = nullptr;
```

---

## 12. Implementation Order

1. **Enums** - EWeaponType, EInfusionDisplayType
2. **BaseAttackData** - Simple, no dependencies
3. **StanceData** - Simple, no dependencies
4. **InfusionDisplayData** - Simple, no dependencies
5. **WeaponData** - Depends on BaseAttackData, AbilityData
6. **CharacterData updates** - Integrate all new systems
7. **Debug utilities** - Testing tools

---

## 13. Future Considerations

- Unlock conditions for stances
- Per-element infusion display variants
- Weapon upgrade/enhancement system
- Conjuration spell integration
- Animation blending for stance transitions