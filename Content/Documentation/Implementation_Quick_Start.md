# Character Data System - Implementation Guide
## Quick Start for UE5 Development

**Companion to:** Character_Data_System_Design.md  
**Purpose:** Step-by-step implementation instructions

---

## Step 1: Create Element Type Enum

**File:** `Source/RefractionPVP/Public/ElementType.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ElementType.generated.h"

UENUM(BlueprintType)
enum class EElementType : uint8
{
    Fire           UMETA(DisplayName = "Fire"),
    Water          UMETA(DisplayName = "Water"),
    Earth          UMETA(DisplayName = "Earth"),
    Wind           UMETA(DisplayName = "Wind"),
    Light          UMETA(DisplayName = "Light"),
    Darkness       UMETA(DisplayName = "Darkness"),
    Lightning      UMETA(DisplayName = "Lightning"),
    Void           UMETA(DisplayName = "Void"),
    Reality        UMETA(DisplayName = "Reality"),
    Generic        UMETA(DisplayName = "Generic (Non-Elemental)"),
    BrokenDarkness UMETA(DisplayName = "Broken Darkness")
};
```

---

## Step 2: Create Character Data Asset

**File:** `Source/RefractionPVP/Public/CharacterData.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ElementType.h"
#include "CharacterData.generated.h"

// Forward declaration
class USpellData;

UCLASS(BlueprintType)
class REFRACTIONPVP_API UCharacterData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString CharacterName = "Unnamed Character";
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EElementType InnateElement = EElementType::Fire;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = "Character description...";
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    UTexture2D* Portrait = nullptr;

    // ==================== STAT BUDGET ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "20", ClampMax = "40"))
    int32 DistributablePoints = 30;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
    int32 DistributedMind = 10;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
    int32 DistributedBody = 10;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Distribution", meta = (ClampMin = "0"))
    int32 DistributedSpirit = 10;

    // ==================== WORLD STAT BONUSES ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
    int32 WorldMindLevel = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
    int32 WorldBodyLevel = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses", meta = (ClampMin = "0", ClampMax = "7"))
    int32 WorldSpiritLevel = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|World Bonuses")
    int32 PointsPerWorldStatLevel = 3;

    // ==================== SUB-STAT DISTRIBUTION (MIND) ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
    int32 CostReductionPoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
    int32 TurnSpeedPoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
    int32 CritChancePoints = 0;

    // ==================== SUB-STAT DISTRIBUTION (BODY) ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
    int32 DefensePoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
    int32 AttackSpeedPoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
    int32 RawDamagePoints = 0;

    // ==================== SUB-STAT DISTRIBUTION (SPIRIT) ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
    int32 EffectDamagePoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
    int32 ResistancePoints = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
    int32 AbilitySizePoints = 0;

    // ==================== SPELL POOLS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
    TArray<USpellData*> GenericSpellPool;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
    TArray<USpellData*> UniqueSpells;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells", meta = (ClampMin = "3", ClampMax = "6"))
    int32 MaxGenericSpellSlots = 4;

    // ==================== VISUAL DATA ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    USkeletalMesh* CharacterMesh = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    TSubclassOf<UAnimInstance> AnimationBlueprint = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    FLinearColor PrimaryColor = FLinearColor::White;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
    FLinearColor SecondaryColor = FLinearColor::Black;

    // ==================== BALANCE FLAGS ====================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance")
    bool bHasBrokenAbilities = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (EditCondition = "bHasBrokenAbilities", MultiLine = true))
    FString BalanceNotes = "";

    // ==================== EFFECTIVE STATS ====================
    
    UFUNCTION(BlueprintPure, Category = "Stats|Effective")
    float GetEffectiveMind() const 
    { 
        return DistributedMind * (1.0f + WorldMindLevel * 0.05f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Effective")
    float GetEffectiveBody() const 
    { 
        return DistributedBody * (1.0f + WorldBodyLevel * 0.05f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Effective")
    float GetEffectiveSpirit() const 
    { 
        return DistributedSpirit * (1.0f + WorldSpiritLevel * 0.05f);
    }

    // ==================== VALIDATION ====================
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetTotalDistributedPoints() const 
    { 
        return DistributedMind + DistributedBody + DistributedSpirit; 
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    bool IsValidDistribution() const 
    { 
        return GetTotalDistributedPoints() == DistributablePoints; 
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetAvailableMindPoints() const
    {
        return WorldMindLevel * PointsPerWorldStatLevel;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetUsedMindPoints() const
    {
        return CostReductionPoints + TurnSpeedPoints + CritChancePoints;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    bool IsValidMindDistribution() const
    {
        return GetUsedMindPoints() == GetAvailableMindPoints();
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetAvailableBodyPoints() const
    {
        return WorldBodyLevel * PointsPerWorldStatLevel;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetUsedBodyPoints() const
    {
        return DefensePoints + AttackSpeedPoints + RawDamagePoints;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    bool IsValidBodyDistribution() const
    {
        return GetUsedBodyPoints() == GetAvailableBodyPoints();
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetAvailableSpiritPoints() const
    {
        return WorldSpiritLevel * PointsPerWorldStatLevel;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    int32 GetUsedSpiritPoints() const
    {
        return EffectDamagePoints + ResistancePoints + AbilitySizePoints;
    }
    
    UFUNCTION(BlueprintPure, Category = "Stats|Validation")
    bool IsValidSpiritDistribution() const
    {
        return GetUsedSpiritPoints() == GetAvailableSpiritPoints();
    }

    // ==================== MIND CALCULATIONS ====================
    
    UFUNCTION(BlueprintPure, Category = "Combat|Mind")
    float CalculateSpellCostReduction() const
    {
        float EffectiveMind = GetEffectiveMind();
        return FMath::Clamp(EffectiveMind * CostReductionPoints * 0.006f, 0.0f, 0.7f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Mind")
    float CalculateTurnSpeed() const
    {
        float EffectiveMind = GetEffectiveMind();
        return 10.0f + (EffectiveMind * TurnSpeedPoints * 0.5f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Mind")
    float CalculateCriticalChance() const
    {
        float EffectiveMind = GetEffectiveMind();
        return FMath::Clamp(0.05f + (EffectiveMind * CritChancePoints * 0.003f), 0.05f, 0.6f);
    }

    // ==================== BODY CALCULATIONS ====================
    
    UFUNCTION(BlueprintPure, Category = "Combat|Body")
    int32 CalculateFlatDefense() const
    {
        float EffectiveBody = GetEffectiveBody();
        return FMath::RoundToInt(EffectiveBody * DefensePoints * 0.4f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Body")
    float CalculateAttackSpeed() const
    {
        float EffectiveBody = GetEffectiveBody();
        return 1.0f + (EffectiveBody * AttackSpeedPoints * 0.05f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Body")
    float CalculateRawDamageMultiplier() const
    {
        float EffectiveBody = GetEffectiveBody();
        return 1.0f + (EffectiveBody * RawDamagePoints * 0.006f);
    }

    // ==================== SPIRIT CALCULATIONS ====================
    
    UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
    float CalculateEffectDamageMultiplier() const
    {
        float EffectiveSpirit = GetEffectiveSpirit();
        return 1.0f + (EffectiveSpirit * EffectDamagePoints * 0.006f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
    float CalculateElementalResistance() const
    {
        float EffectiveSpirit = GetEffectiveSpirit();
        return FMath::Clamp(EffectiveSpirit * ResistancePoints * 0.005f, 0.0f, 0.5f);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
    float CalculateAbilitySizeMultiplier() const
    {
        float EffectiveSpirit = GetEffectiveSpirit();
        return 1.0f + (EffectiveSpirit * AbilitySizePoints * 0.007f);
    }

    // ==================== HELPER FUNCTIONS ====================
    
    UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
    int32 CalculateTurnRatio(float EnemySpeed) const
    {
        float MySpeed = CalculateTurnSpeed();
        float Difference = MySpeed - EnemySpeed;
        
        if (Difference >= 15.0f)
            return 2; // 2:1 ratio (double turn)
        else
            return 1; // 1:1 ratio (normal)
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
    int32 CalculateSpellCost(int32 BaseCost) const
    {
        float Reduction = CalculateSpellCostReduction();
        return FMath::RoundToInt(BaseCost * (1.0f - Reduction));
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
    int32 CalculateSpellDamage(int32 BaseDamage) const
    {
        float Multiplier = CalculateEffectDamageMultiplier();
        return FMath::RoundToInt(BaseDamage * Multiplier);
    }
    
    UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
    int32 CalculateReducedDamage(int32 IncomingDamage, bool bIsElemental) const
    {
        // Apply defense
        int32 AfterDefense = FMath::Max(0, IncomingDamage - CalculateFlatDefense());
        
        // Apply resistance if elemental
        if (bIsElemental)
        {
            float Resistance = CalculateElementalResistance();
            return FMath::RoundToInt(AfterDefense * (1.0f - Resistance));
        }
        
        return AfterDefense;
    }

    // ==================== EDITOR VALIDATION ====================
    
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override
    {
        EDataValidationResult Result = Super::IsDataValid(ValidationErrors);
        
        // Check base stat distribution
        if (!IsValidDistribution())
        {
            ValidationErrors.Add(FText::FromString(
                FString::Printf(TEXT("Base stat distribution (%d) doesn't match budget (%d)"), 
                GetTotalDistributedPoints(), DistributablePoints)
            ));
            Result = EDataValidationResult::Invalid;
        }
        
        // Check Mind sub-stat distribution
        if (WorldMindLevel > 0 && !IsValidMindDistribution())
        {
            ValidationErrors.Add(FText::FromString(
                FString::Printf(TEXT("Mind sub-stats (%d) don't match available (%d)"), 
                GetUsedMindPoints(), GetAvailableMindPoints())
            ));
            Result = EDataValidationResult::Invalid;
        }
        
        // Check Body sub-stat distribution
        if (WorldBodyLevel > 0 && !IsValidBodyDistribution())
        {
            ValidationErrors.Add(FText::FromString(
                FString::Printf(TEXT("Body sub-stats (%d) don't match available (%d)"), 
                GetUsedBodyPoints(), GetAvailableBodyPoints())
            ));
            Result = EDataValidationResult::Invalid;
        }
        
        // Check Spirit sub-stat distribution
        if (WorldSpiritLevel > 0 && !IsValidSpiritDistribution())
        {
            ValidationErrors.Add(FText::FromString(
                FString::Printf(TEXT("Spirit sub-stats (%d) don't match available (%d)"), 
                GetUsedSpiritPoints(), GetAvailableSpiritPoints())
            ));
            Result = EDataValidationResult::Invalid;
        }
        
        // Check unique spells exist
        if (UniqueSpells.Num() == 0)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Character has no unique spells!")));
            Result = EDataValidationResult::Invalid;
        }
        
        return Result;
    }
#endif
};
```

**File:** `Source/RefractionPVP/Private/CharacterData.cpp`

```cpp
#include "CharacterData.h"

// Constructor if needed
// Implementation is all in header (inline functions)
```

---

## Step 3: Create First Character DataAsset

**In Unreal Editor:**

1. **Content Browser** → Right-click → **Miscellaneous** → **Data Asset**
2. Select **CharacterData** class
3. Name: `DA_FireMage_Inferno`
4. Open and configure:

```
Identity:
├─ Character Name: "Inferno"
├─ Innate Element: Fire
├─ Description: "Master of flames, excels at sustained damage"

Stats:
├─ Distributable Points: 30

Stats | Distribution:
├─ Distributed Mind: 15
├─ Distributed Body: 5
├─ Distributed Spirit: 10

Stats | World Bonuses:
├─ World Mind Level: 5
├─ World Body Level: 3
├─ World Spirit Level: 7

Sub-Stats | Mind (15 points available):
├─ Cost Reduction Points: 7
├─ Turn Speed Points: 5
├─ Crit Chance Points: 3

Sub-Stats | Body (9 points available):
├─ Defense Points: 5
├─ Attack Speed Points: 2
├─ Raw Damage Points: 2

Sub-Stats | Spirit (21 points available):
├─ Effect Damage Points: 10
├─ Resistance Points: 6
├─ Ability Size Points: 5

Balance:
├─ Has Broken Abilities: false
```

---

## Step 4: Testing in Blueprints

**Create test Blueprint:** `BP_CharacterDataTester`

```
Event BeginPlay:
├─ Get Data Asset: DA_FireMage_Inferno
├─ Get Effective Mind → Print String
├─ Get Effective Body → Print String
├─ Get Effective Spirit → Print String
├─ Calculate Turn Speed → Print String
├─ Calculate Effect Damage Multiplier → Print String
├─ Calculate Flat Defense → Print String

Expected Output:
├─ Effective Mind: 18.75
├─ Effective Body: 5.75
├─ Effective Spirit: 13.5
├─ Turn Speed: 56.875
├─ Effect Damage: 1.81
├─ Flat Defense: 11.5
```

---

## Step 5: Next Steps

**After CharacterData is working:**

1. **Create SpellData structure** (similar DataAsset approach)
2. **Build character selection UI** (UMG widgets)
3. **Implement turn-based combat system** (uses CharacterData calculations)
4. **Create more character presets**

---

## Common Issues & Solutions

### Issue: "Cannot find CharacterData class"
**Solution:** Make sure to compile C++ code and restart editor

### Issue: "Invalid distribution" validation error
**Solution:** Check that distributed points equal budget, and sub-stat points equal world stat allocation

### Issue: "Null pointer when accessing SpellData"
**Solution:** SpellData hasn't been created yet - leave GenericSpellPool and UniqueSpells empty for now

---

## File Structure

```
Source/RefractionPVP/
├── Public/
│   ├── ElementType.h
│   ├── CharacterData.h
│   └── SpellData.h (future)
│
└── Private/
    ├── CharacterData.cpp
    └── SpellData.cpp (future)

Content/
├── DataAssets/
│   ├── Characters/
│   │   ├── DA_FireMage_Inferno
│   │   ├── DA_Tank_Ironwall
│   │   └── DA_Trickster_Paradox
│   │
│   └── Spells/
│       └── (future)
│
└── Blueprints/
    └── Testing/
        └── BP_CharacterDataTester
```

---

**Ready to start coding! Begin with Step 1 and test each step before proceeding.**