// FCombatLoadout.cpp
// Full combat loadout implementation

#include "FCombatLoadout.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "CrystalType.h"
#include "FSpellCollection.h"
#include "FAbilityCollection.h"

// Forward declare - will be implemented in Phase 3
// #include "InventoryComponent.h"

bool FCombatLoadout::Validate(ECharacterClass CharClass, UInventoryComponent* Inventory) const
{
    // TODO: Implement full validation when InventoryComponent exists
    switch (CharClass)
    {
        case ECharacterClass::Generic:
            return ValidateGeneric(Inventory);
        case ECharacterClass::Caster:
            return ValidateCaster(Inventory);
        case ECharacterClass::Resonator:
            return ValidateResonator(Inventory);
        default:
            return false;
    }
}

bool FCombatLoadout::ValidateGeneric(UInventoryComponent* Inventory) const
{
    // Generic: Primary weapon required, secondary is weapon OR ring
    if (!PrimaryWeapon.IsValid())
    {
        return false;
    }
    
    // Secondary must be one or the other, not both
    bool bHasSecondaryWeapon = SecondaryWeapon.IsValid();
    bool bHasSecondaryRing = SecondaryRing.IsValid();
    
    if (bSecondaryIsRing && bHasSecondaryWeapon)
    {
        return false;
    }
    if (!bSecondaryIsRing && bHasSecondaryRing)
    {
        return false;
    }
    
    // TODO: Validate against inventory when component exists
    return true;
}

bool FCombatLoadout::ValidateCaster(UInventoryComponent* Inventory) const
{
    // Caster: Primary is weapon OR ring
    bool bHasPrimaryWeapon = PrimaryWeapon.IsValid();
    bool bHasPrimaryRing = PrimaryRing.IsValid();
    
    if (bPrimaryIsRing && !bHasPrimaryRing)
    {
        return false;
    }
    if (!bPrimaryIsRing && !bHasPrimaryWeapon)
    {
        return false;
    }
    
    // Innate spells: max 6 per school
    for (int32 i = 0; i < static_cast<int32>(ESpellSchool::Conjuration) + 1; ++i)
    {
        if (GetInnateSpellCountForSchool(static_cast<ESpellSchool>(i)) > InventoryConstants::MAX_INNATE_SPELLS_PER_SCHOOL)
        {
            return false;
        }
    }
    
    // Total innate spells
    if (InnateSpells.Num() > InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
    {
        return false;
    }
    
    // TODO: Validate against inventory when component exists
    return true;
}

bool FCombatLoadout::ValidateResonator(UInventoryComponent* Inventory) const
{
    // Resonator: Primary weapon required, ring loadout
    if (!PrimaryWeapon.IsValid())
    {
        return false;
    }
    
    // Ring loadout: max 5 rings, max 2 evolved
    if (RingLoadout.Num() > InventoryConstants::MAX_RESONATOR_RINGS)
    {
        return false;
    }
    
    int32 EvolvedCount = 0;
    for (const FRingLoadoutEntry& Entry : RingLoadout)
    {
        if (Entry.IsEvolved())
        {
            EvolvedCount++;
        }
    }
    
    if (EvolvedCount > InventoryConstants::MAX_EVOLVED_RESONATOR_RINGS)
    {
        return false;
    }
    
    // TODO: Validate against inventory when component exists
    return true;
}

TArray<USpellData*> FCombatLoadout::GetInnateSpellsBySchool(ESpellSchool School) const
{
    TArray<USpellData*> Result;
    
    for (USpellData* Spell : InnateSpells)
    {
        if (Spell && Spell->School == School)
        {
            Result.Add(Spell);
        }
    }
    
    return Result;
}

int32 FCombatLoadout::GetInnateSpellCountForSchool(ESpellSchool School) const
{
    int32 Count = 0;
    
    for (USpellData* Spell : InnateSpells)
    {
        if (Spell && Spell->School == School)
        {
            Count++;
        }
    }
    
    return Count;
}

bool FCombatLoadout::ValidateInnateSpells(ESpellElement InnateElement, const FSpellCollection& OwnedSpells) const
{
    // Check school limits
    for (int32 i = 0; i < static_cast<int32>(ESpellSchool::Conjuration) + 1; ++i)
    {
        if (GetInnateSpellCountForSchool(static_cast<ESpellSchool>(i)) > InventoryConstants::MAX_INNATE_SPELLS_PER_SCHOOL)
        {
            return false;
        }
    }
    
    // Check each spell
    for (USpellData* Spell : InnateSpells)
    {
        if (!Spell)
        {
            continue;
        }
        
        // Must be owned
        if (!OwnedSpells.HasSpell(Spell))
        {
            return false;
        }
        
        // Must match innate element (unless universal)
        if (!Spell->bIsUniversalSpell && Spell->Element != InnateElement)
        {
            return false;
        }
    }
    
    return true;
}

bool FCombatLoadout::HasDuplicateItemTypes() const
{
    TSet<ECrystalType> SeenTypes;
    
    for (const FItemLoadoutSlot& Slot : ItemSlots)
    {
        if (!Slot.HasCrystal())
        {
            continue;
        }
        
        ECrystalType Type = Slot.Crystal->CrystalType;
        if (SeenTypes.Contains(Type))
        {
            return true; // Duplicate found
        }
        SeenTypes.Add(Type);
    }
    
    return false;
}

int32 FCombatLoadout::GetTotalItemUses() const
{
    int32 Total = 0;
    
    for (const FItemLoadoutSlot& Slot : ItemSlots)
    {
        Total += Slot.GetRemainingUses();
    }
    
    return Total;
}

TArray<UAbilityData*> FCombatLoadout::GetAllAbilities() const
{
    TArray<UAbilityData*> Result;
    
    // Primary weapon abilities
    if (PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllAbilities());
    }
    
    // Secondary weapon abilities (Generic only)
    if (!bSecondaryIsRing && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllAbilities());
    }
    
    return Result;
}

TArray<USpellData*> FCombatLoadout::GetAllSpells() const
{
    TArray<USpellData*> Result;
    
    // Innate spells (Caster)
    Result.Append(InnateSpells);
    
    // Primary weapon spells
    if (!bPrimaryIsRing && PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllSpells());
    }
    
    // Primary ring spells
    if (bPrimaryIsRing && PrimaryRing.IsValid())
    {
        Result.Append(PrimaryRing.GetAllSpells());
    }
    
    // Secondary weapon spells (Generic)
    if (!bSecondaryIsRing && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllSpells());
    }
    
    // Secondary ring spells (Generic)
    if (bSecondaryIsRing && SecondaryRing.IsValid())
    {
        Result.Append(SecondaryRing.GetAllSpells());
    }
    
    // Ring loadout spells (Resonator)
    for (const FRingLoadoutEntry& Entry : RingLoadout)
    {
        if (Entry.IsValid())
        {
            Result.Append(Entry.GetAllSpells());
        }
    }
    
    return Result;
}

TArray<FItemLoadoutSlot> FCombatLoadout::GetUsableItemSlots() const
{
    TArray<FItemLoadoutSlot> Result;
    
    for (const FItemLoadoutSlot& Slot : ItemSlots)
    {
        if (Slot.CanUse())
        {
            Result.Add(Slot);
        }
    }
    
    return Result;
}

void FCombatLoadout::Clear()
{
    LoadoutName = TEXT("Default Loadout");
    
    PrimaryWeapon.Clear();
    PrimaryRing.Clear();
    bPrimaryIsRing = false;
    
    SecondaryWeapon.Clear();
    SecondaryRing.Clear();
    bSecondaryIsRing = false;
    
    RingLoadout.Empty();
    InnateSpells.Empty();
    ItemSlots.Empty();
}

void FCombatLoadout::InitializeForClass(ECharacterClass CharClass)
{
    Clear();
    
    switch (CharClass)
    {
        case ECharacterClass::Generic:
            // Generic: Primary weapon, optional secondary
            bPrimaryIsRing = false;
            bSecondaryIsRing = false;
            break;
            
        case ECharacterClass::Caster:
            // Caster: Primary weapon by default, innate spells
            bPrimaryIsRing = false;
            InnateSpells.SetNum(0); // Empty, to be filled
            break;
            
        case ECharacterClass::Resonator:
            // Resonator: Primary weapon, ring loadout
            bPrimaryIsRing = false;
            RingLoadout.SetNum(0); // Empty, to be filled
            break;
    }
    
    // Initialize item slots (empty)
    ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
    for (FItemLoadoutSlot& Slot : ItemSlots)
    {
        Slot.Clear();
    }
}
