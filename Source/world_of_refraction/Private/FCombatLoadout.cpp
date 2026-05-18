// FCombatLoadout.cpp
// Full combat loadout implementation

#include "FCombatLoadout.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "CrystalType.h"
#include "FSpellCollection.h"
#include "FAbilityCollection.h"
#include "LoadoutData.h"

// ==================== VALIDATION ====================

bool FCombatLoadout::Validate(ECharacterClass CharClass, UInventoryComponent *Inventory) const
{
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

bool FCombatLoadout::ValidateGeneric(UInventoryComponent *Inventory) const
{
    // Generic: Must have valid primary (Weapon, Ring, or Evolution)
    switch (PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (!PrimaryWeapon.IsValid())
            return false;
        break;
    case EPrimarySlotType::Ring:
        if (!PrimaryRing.IsValid())
            return false;
        break;
    case EPrimarySlotType::Evolution:
        if (!PrimaryEvolution)
            return false;
        break;
    }

    // Secondary is weapon only (if set)
    if (SecondarySlotType == ESecondarySlotType::Weapon && !SecondaryWeapon.IsValid())
    {
        return false;
    }

    // TODO: Validate against inventory when component exists
    return true;
}

bool FCombatLoadout::ValidateCaster(UInventoryComponent *Inventory) const
{
    // Caster: Must have valid primary (Weapon, Ring, or Evolution)
    switch (PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (!PrimaryWeapon.IsValid())
            return false;
        break;
    case EPrimarySlotType::Ring:
        if (!PrimaryRing.IsValid())
            return false;
        break;
    case EPrimarySlotType::Evolution:
        if (!PrimaryEvolution)
            return false;
        break;
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

bool FCombatLoadout::ValidateResonator(UInventoryComponent *Inventory) const
{
    // Resonator: Must have Weapon or Evolution primary (NOT Ring)
    if (PrimarySlotType == EPrimarySlotType::Ring)
    {
        return false;
    }

    switch (PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (!PrimaryWeapon.IsValid())
            return false;
        break;
    case EPrimarySlotType::Evolution:
        if (!PrimaryEvolution)
            return false;
        break;
    default:
        return false;
    }

    // Ring loadout limits depend on evolution state
    const bool bIsEvolved = (PrimarySlotType == EPrimarySlotType::Evolution);
    const int32 MaxRings = bIsEvolved ? LoadoutConstants::RESONATOR_RING_SLOTS_EVOLVED
                                      : LoadoutConstants::RESONATOR_RING_SLOTS_NORMAL;
    const int32 MaxEvolvedRings = bIsEvolved ? LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_EVOLVED
                                             : LoadoutConstants::RESONATOR_MAX_EVOLVED_RINGS_NORMAL;

    if (RingLoadout.Num() > MaxRings)
    {
        return false;
    }

    int32 EvolvedCount = 0;
    for (const FRingLoadoutEntry &Entry : RingLoadout)
    {
        if (Entry.IsValid() && Entry.RingEntry.IsEvolved())
        {
            EvolvedCount++;
        }
    }

    if (EvolvedCount > MaxEvolvedRings)
    {
        return false;
    }

    // TODO: Validate against inventory when component exists
    return true;
}

// ==================== ACCESSORS ====================

int32 FCombatLoadout::GetInnateSpellCountForSchool(ESpellSchool School) const
{
    int32 Count = 0;
    for (const USpellData *Spell : InnateSpells)
    {
        if (Spell && Spell->School == School)
        {
            Count++;
        }
    }
    return Count;
}

bool FCombatLoadout::HasDuplicateItemTypes() const
{
    TSet<ECrystalType> SeenTypes;

    for (const FItemLoadoutSlot &Slot : ItemSlots)
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

    for (const FItemLoadoutSlot &Slot : ItemSlots)
    {
        Total += Slot.GetRemainingUses();
    }

    return Total;
}

TArray<UAbilityData *> FCombatLoadout::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    // Primary weapon abilities (only if primary is weapon)
    if (PrimarySlotType == EPrimarySlotType::Weapon && PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllAbilities());
    }

    // Secondary weapon abilities (Generic only)
    if (SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllAbilities());
    }

    // Note: Evolution abilities would come from PrimaryEvolution
    // but abilities from evolution are handled separately

    return Result;
}

TArray<USpellData *> FCombatLoadout::GetAllSpells() const
{
    TArray<USpellData *> Result;

    // Evolution spells
    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(EvolutionSpells);
    }

    // Primary weapon spells
    if (PrimarySlotType == EPrimarySlotType::Weapon && PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllSpells());
    }

    // Primary ring spells
    if (PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing.IsValid())
    {
        Result.Append(PrimaryRing.GetAllSpells());
    }

    // Secondary weapon spells (Generic only)
    if (SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllSpells());
    }

    // Innate spells (Caster)
    Result.Append(InnateSpells);

    // Ring loadout spells (Resonator)
    for (const FRingLoadoutEntry &Entry : RingLoadout)
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

    for (const FItemLoadoutSlot &Slot : ItemSlots)
    {
        if (Slot.CanUse())
        {
            Result.Add(Slot);
        }
    }

    return Result;
}

// ==================== HELPERS ====================

void FCombatLoadout::Clear()
{
    LoadoutName = TEXT("Default Loadout");

    PrimarySlotType = EPrimarySlotType::Weapon;
    PrimaryWeapon.Clear();
    PrimaryRing.Clear();
    PrimaryEvolution = nullptr;
    EvolutionSpells.Empty();

    SecondarySlotType = ESecondarySlotType::None;
    SecondaryWeapon.Clear();

    RingLoadout.Empty();
    InnateSpells.Empty();
    BDSpellPools.Empty();
    ItemSlots.Empty();

    bShowPrimary = true;
    ActiveRingIndex = 0;
}

void FCombatLoadout::InitializeForClass(ECharacterClass CharClass)
{
    Clear();

    switch (CharClass)
    {
    case ECharacterClass::Generic:
        // Generic: Primary weapon/ring/evolution, optional secondary weapon
        PrimarySlotType = EPrimarySlotType::Weapon;
        SecondarySlotType = ESecondarySlotType::None;
        break;

    case ECharacterClass::Caster:
        // Caster: Primary weapon/ring/evolution, innate spells
        PrimarySlotType = EPrimarySlotType::Weapon;
        InnateSpells.SetNum(0); // Empty, to be filled
        break;

    case ECharacterClass::Resonator:
        // Resonator: Primary weapon/evolution, ring loadout
        PrimarySlotType = EPrimarySlotType::Weapon;
        RingLoadout.SetNum(0); // Empty, to be filled
        break;
    }

    // Initialize item slots (empty)
    ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
    for (FItemLoadoutSlot &Slot : ItemSlots)
    {
        Slot.Clear();
    }
}

void FCombatLoadout::ResetForBattle()
{
    for (FItemLoadoutSlot &Slot : ItemSlots)
    {
        Slot.ResetForBattle();
    }
}

// ==================== FACTORY ====================

FCombatLoadout FCombatLoadout::CreateFromAsset(const ULoadoutData *Asset)
{
    FCombatLoadout Result;

    if (!Asset)
    {
        UE_LOG(LogTemp, Error, TEXT("[FCombatLoadout] CreateFromAsset: Null asset"));
        return Result;
    }

    Result.LoadoutName = Asset->LoadoutName;
    Result.PrimarySlotType = Asset->PrimarySlotType;

    // Resonator cannot have Ring primary - guard against bad asset data
    if (Asset->RequiredClass == ECharacterClass::Resonator &&
        Result.PrimarySlotType == EPrimarySlotType::Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] Resonator asset '%s' has invalid Ring primary - forcing Weapon"),
               *Asset->LoadoutName);
        Result.PrimarySlotType = EPrimarySlotType::Weapon;
    }

    // ==================== PRIMARY EQUIPMENT ====================

    switch (Asset->PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (Asset->PrimaryWeapon)
        {
            Result.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(
                Asset->PrimaryWeapon, true);
            Result.PrimaryWeapon.InitializeFromWeapon();
            Result.PrimaryWeapon.AssignedAbilities = Asset->PrimaryWeaponAbilities;

            UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] Weapon '%s' HasCrystal=%d Crystal=%s"),
                   *Asset->PrimaryWeapon->Name,
                   Result.PrimaryWeapon.WeaponEntry.HasCrystal(),
                   Result.PrimaryWeapon.WeaponEntry.HasCrystal() ? *Result.PrimaryWeapon.WeaponEntry.AttachedCrystal.Crystal->ItemName : TEXT("none"));
        }
        break;

    case EPrimarySlotType::Ring:
        if (Asset->PrimaryRing)
        {
            Result.PrimaryRing.RingEntry = FRingInventoryEntry::CreateFromRing(
                Asset->PrimaryRing, true); // true = copy SlottedCrystal from RingData
            Result.PrimaryRing.InitializeFromRing();
        }
        break;

    case EPrimarySlotType::Evolution:
        Result.PrimaryEvolution = Asset->PrimaryEvolution;
        Result.EvolutionSpells = Asset->EvolutionSpells;
        break;
    }

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    if (Asset->RequiredClass == ECharacterClass::Generic)
    {
        Result.SecondarySlotType = Asset->SecondarySlotType;

        if (Asset->SecondarySlotType == ESecondarySlotType::Weapon && Asset->SecondaryWeapon)
        {
            Result.SecondaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(
                Asset->SecondaryWeapon, true);
            Result.SecondaryWeapon.InitializeFromWeapon();
            Result.SecondaryWeapon.AssignedAbilities = Asset->SecondaryWeaponAbilities;
        }
    }

    // ==================== RESONATOR RINGS ====================

    if (Asset->RequiredClass == ECharacterClass::Resonator)
    {
        for (URingData *Ring : Asset->EquippedRings)
        {
            if (Ring)
            {
                FRingLoadoutEntry RingEntry;
                RingEntry.RingEntry = FRingInventoryEntry::CreateFromRing(Ring, true);
                RingEntry.InitializeFromRing();
                Result.RingLoadout.Add(RingEntry);
            }
        }
    }

    // ==================== CASTER INNATE SPELLS ====================

    if (Asset->RequiredClass == ECharacterClass::Caster)
    {
        Result.InnateSpells = Asset->InnateSpells;
    }

    // ==================== ITEMS ====================

    Result.ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
    for (int32 i = 0; i < Asset->EquippedItems.Num() && i < Result.ItemSlots.Num(); i++)
    {
        if (Asset->EquippedItems[i])
        {
            Result.ItemSlots[i].Crystal = Asset->EquippedItems[i];
            Result.ItemSlots[i].ResetForBattle();
        }
    }

    // ==================== COSMETICS & DEFENSE ====================

    Result.bShowPrimary = Asset->bShowPrimary;

    // Defense animations
    Result.bUseWeaponParryAnimation = Asset->bUseWeaponParryAnimation;

    UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] Created from asset '%s' (PrimarySlotType: %d)"),
           *Asset->LoadoutName, static_cast<int32>(Asset->PrimarySlotType));
    return Result;
}