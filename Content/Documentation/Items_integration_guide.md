# Items System - Integration Guide
## Adding C++ Files to Your UE5 Project

**Created:** November 25, 2024  
**Files Created:** 5 C++ files (3 enums + ItemData header/cpp)

---

## Step 1: Add Files to Project

### **File Locations**

Place files in your project's Source folder structure:

```
YourProject/Source/YourProject/
├─ CrystalType.h
├─ ItemTier.h
├─ ItemEffectType.h
├─ ItemData.h
└─ ItemData.cpp
```

**Recommended sub-folder (optional):**
```
YourProject/Source/YourProject/Items/
├─ CrystalType.h
├─ ItemTier.h
├─ ItemEffectType.h
├─ ItemData.h
└─ ItemData.cpp
```

---

## Step 2: Update Build Files

### **If you get compile errors:**

You may need to add dependencies to your `YourProject.Build.cs` file:

```csharp
PublicDependencyModuleNames.AddRange(new string[] 
{ 
    "Core", 
    "CoreUObject", 
    "Engine",
    "InputCore"
});
```

---

## Step 3: Compile in Unreal

### **Two Methods:**

**Method 1: Hot Reload (Fastest)**
1. Save all files in your IDE
2. In Unreal Editor: Click "Live Coding" button (bottom-right)
3. Or press **Ctrl+Alt+F11**
4. Wait for compilation

**Method 2: Full Rebuild**
1. Close Unreal Editor
2. Right-click `.uproject` file → "Generate Visual Studio project files"
3. Open solution in Visual Studio/Rider
4. Build → Build Solution
5. Launch editor

---

## Step 4: Verify Compilation

### **Check for errors:**

**If compile succeeds:**
- ✅ All files compiled
- ✅ Ready to create Data Assets

**If compile fails:**

Check for missing dependencies:
```
Error: Cannot find RefractionElement.h
```

**Solution:** ItemData.h references `RefractionElement.h` (your existing element enum). Make sure:
- Path is correct in `#include "RefractionElement.h"`
- Or update to correct path: `#include "YourFolder/RefractionElement.h"`

---

## Step 5: Create Data Asset Blueprint Class

### **In Unreal Editor:**

1. **Content Browser** → Right-click → **Miscellaneous** → **Data Asset**
2. Choose parent class: **ItemData**
3. Name it: `DA_ItemTemplate`
4. Open the asset

**You should see all the properties:**
- Identity (Crystal Type, Tier, Name, Description)
- Effects (Primary Effect Type, values)
- Character Bonuses (Generic/BD bonuses)
- Presentation (Icon, Color, VFX, Sound)

---

## Step 6: Create Your First Item

### **Example: F-Tier Garnet (Fire Damage)**

**Create asset:**
1. Duplicate `DA_ItemTemplate`
2. Name: `DA_Garnet_F`

**Configure in editor:**
```
Identity:
├─ Crystal Type: Garnet
├─ Tier: F_Tier
├─ Item Name: (auto-generates "F-Tier Garnet")
├─ Description: "Crude fire crystal. Deals minor damage."
└─ Associated Element: Fire (auto-set)

Effects:
├─ Primary Effect Type: Damage
├─ Hit Point Value: 60.0
├─ Has Secondary Effect: false

Character Bonuses:
├─ Generic Resistance Bonus: 20.0
├─ Generic Resistance Duration: 8
└─ Broken Darkness Energy Bonus: 15

Presentation:
├─ Icon: (assign texture)
├─ Tier Color: (Orange/Red for F-tier)
├─ Use Effect: (particle system)
└─ Use Sound: (sound effect)
```

---

## Step 7: Create All 70 Items

### **Efficient Method:**

**1. Create Template Per Crystal**
Create 10 base templates (one per crystal type):
- `DA_Garnet_Template`
- `DA_Sapphire_Template`
- `DA_Citrine_Template`
- etc.

**2. Duplicate for Each Tier**
For each template, duplicate 7 times:
- `DA_Garnet_F`
- `DA_Garnet_E`
- `DA_Garnet_D`
- ... up to `DA_Garnet_S`

**3. Update Tier and Values**
For each item:
- Change Tier dropdown (F → E → D → etc.)
- Update effect values per tier (see documentation)
- Item name auto-updates!

**Total: 10 crystals × 7 tiers = 70 items**

---

## Step 8: Testing Setup

### **Create Test Blueprint**

**Create:** `BP_ItemDataTester` (Blueprint Actor)

**Add components:**
```
Components:
└─ Scene Root

Variables:
├─ TestItem (ItemData object reference)
├─ TestCharacterType (Enum: Normal, Generic, BrokenDarkness)
└─ OutputText (String)
```

**Functions:**

**TestItemUse:**
```blueprint
Input: ItemData
Output: String (result description)

1. Get item properties
2. Calculate effects based on character type
3. Format output string
4. Print to screen/log
```

**Example outputs:**
```
"F-Tier Garnet used:"
├─ "Base Damage: 60"
├─ "Generic Bonus: +20% Fire Resistance (8 turns)"
└─ "Broken Darkness: +15 energy"
```

---

## Step 9: Integration with Combat

### **When character uses item in combat:**

```cpp
void UseItem(UItemData* Item, UCharacterData* User, UCharacterData* Target)
{
    // Apply base effect
    switch (Item->PrimaryEffectType)
    {
        case EItemEffectType::Damage:
            Target->TakeDamage(Item->HitPointValue);
            break;
            
        case EItemEffectType::Healing:
            User->Heal(Item->HitPointValue);
            break;
            
        case EItemEffectType::EnergyRestore:
            User->RestoreEnergy(Item->EnergyValue);
            User->TakeDamage(Item->SelfDamage);
            break;
            
        // ... other effects
    }
    
    // Apply character-specific bonuses
    if (User->ElementType == ERefractionElement::None) // Generic
    {
        ApplyGenericResistance(User, Item->AssociatedElement, 
            Item->GenericResistanceBonus, Item->GenericResistanceDuration);
    }
    else if (User->ElementType == ERefractionElement::BrokenDarkness)
    {
        User->GainEnergy(Item->BrokenDarknessEnergyBonus);
    }
    
    // Play VFX/SFX
    PlayItemEffect(Item->UseEffect, Item->UseSound);
}
```

---

## Validation & Testing

### **Automatic Validation**

The ItemData class includes editor validation that checks:
- ✅ Damage/Healing values > 0
- ✅ Energy values > 0
- ✅ Buff percentages > 0
- ✅ Durations > 0
- ✅ Transform thresholds > 0
- ✅ Secondary effects configured properly

**If validation fails:**
- Red warning icon appears on asset
- Error message explains what's wrong
- Must fix before use in combat

### **Testing Checklist**

For each item, verify:
- [ ] Base effect works
- [ ] Generic character gains resistance
- [ ] Broken Darkness gains energy bonus
- [ ] VFX/SFX play correctly
- [ ] Tier scaling is correct
- [ ] No compile errors
- [ ] No runtime crashes

---

## Common Issues & Solutions

### **Issue: "Cannot find RefractionElement.h"**

**Solution:**
Update include path in ItemData.h:
```cpp
#include "Path/To/RefractionElement.h"
```

### **Issue: "ItemData not showing in Data Asset menu"**

**Solution:**
1. Ensure files compiled successfully
2. Try full rebuild (close editor, rebuild solution)
3. Check that UCLASS() macro is present

### **Issue: "Properties not visible in editor"**

**Solution:**
- Check UPROPERTY macros have `EditAnywhere` or `BlueprintReadOnly`
- Ensure categories are spelled correctly
- Try closing and reopening the asset

### **Issue: "Validation errors spam console"**

**Solution:**
- Fix the reported validation errors
- Or disable validation in editor preferences (not recommended)

---

## Next Steps

**After successful integration:**

1. ✅ Create all 70 item assets (10 crystals × 7 tiers)
2. ✅ Test each item type
3. ✅ Integrate with inventory system
4. ✅ Connect to combat manager
5. ✅ Add UI for item usage
6. ✅ Implement Quartz transformation logic
7. ✅ Test Generic/BD bonuses in combat

---

## Files Summary

**Created Files:**
- `CrystalType.h` - 10 crystal types enum
- `ItemTier.h` - F-S tier enum
- `ItemEffectType.h` - Effect categories enum
- `ItemData.h` - Main data asset (header)
- `ItemData.cpp` - Implementation

**Total Lines:** ~300 lines of production-grade C++ code

**Ready for:** Asset creation, testing, combat integration

---

**Questions? Issues? Check the Item_System_Documentation.md for full design specs!**