// LoadoutComponent.cpp
// Active combat loadout and runtime battle state implementation

#include "LoadoutComponent.h"
#include "InventoryComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "WeaponData.h"
#include "RingData.h"
#include "CharacterData.h"
#include "EvolutionData.h"

ULoadoutComponent::ULoadoutComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void ULoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureDefaultLoadout();
}

void ULoadoutComponent::EnsureDefaultLoadout()
{
    if (SavedLoadouts.Num() == 0)
    {
        FCombatLoadout DefaultLoadout;
        DefaultLoadout.LoadoutName = TEXT("Default");
        DefaultLoadout.InitializeForClass(CharacterClass);
        SavedLoadouts.Add(DefaultLoadout);
        ActiveLoadoutIndex = 0;
    }
}

// ==================== ACTIVE LOADOUT ====================

FCombatLoadout ULoadoutComponent::GetActiveLoadout() const
{
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return SavedLoadouts[ActiveLoadoutIndex];
    }
    return FCombatLoadout();
}

bool ULoadoutComponent::GetActiveLoadoutRef(FCombatLoadout &OutLoadout) const
{
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        OutLoadout = SavedLoadouts[ActiveLoadoutIndex];
        return true;
    }
    return false;
}

bool ULoadoutComponent::SetActiveLoadoutIndex(int32 Index)
{
    if (!SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    ActiveLoadoutIndex = Index;
    bIsReadyForBattle = false; // Need to re-validate
    OnLoadoutChanged.Broadcast(Index);
    return true;
}

FString ULoadoutComponent::GetActiveLoadoutName() const
{
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return SavedLoadouts[ActiveLoadoutIndex].LoadoutName;
    }
    return TEXT("Invalid");
}

// ==================== LOADOUT MANAGEMENT ====================

int32 ULoadoutComponent::CreateNewLoadout(const FString &LoadoutName)
{
    if (SavedLoadouts.Num() >= MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout;
    NewLoadout.LoadoutName = LoadoutName;
    NewLoadout.InitializeForClass(CharacterClass);

    int32 NewIndex = SavedLoadouts.Add(NewLoadout);
    return NewIndex;
}

bool ULoadoutComponent::DeleteLoadout(int32 Index)
{
    if (!SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    // Don't delete last loadout
    if (SavedLoadouts.Num() <= 1)
    {
        return false;
    }

    SavedLoadouts.RemoveAt(Index);

    // Adjust active index if needed
    if (ActiveLoadoutIndex >= SavedLoadouts.Num())
    {
        ActiveLoadoutIndex = SavedLoadouts.Num() - 1;
    }
    else if (ActiveLoadoutIndex > Index)
    {
        ActiveLoadoutIndex--;
    }

    return true;
}

int32 ULoadoutComponent::DuplicateLoadout(int32 SourceIndex, const FString &NewName)
{
    if (!SavedLoadouts.IsValidIndex(SourceIndex))
    {
        return -1;
    }

    if (SavedLoadouts.Num() >= MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout = SavedLoadouts[SourceIndex];
    NewLoadout.LoadoutName = NewName;

    int32 NewIndex = SavedLoadouts.Add(NewLoadout);
    return NewIndex;
}

bool ULoadoutComponent::RenameLoadout(int32 Index, const FString &NewName)
{
    if (!SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    SavedLoadouts[Index].LoadoutName = NewName;
    return true;
}

TArray<FString> ULoadoutComponent::GetLoadoutNames() const
{
    TArray<FString> Names;
    for (const FCombatLoadout &Loadout : SavedLoadouts)
    {
        Names.Add(Loadout.LoadoutName);
    }
    return Names;
}

// ==================== VALIDATION ====================

bool ULoadoutComponent::ValidateActiveLoadout(UInventoryComponent *Inventory)
{
    return ValidateLoadout(ActiveLoadoutIndex, Inventory);
}

bool ULoadoutComponent::ValidateLoadout(int32 Index, UInventoryComponent *Inventory)
{
    if (!SavedLoadouts.IsValidIndex(Index) || !Inventory)
    {
        return false;
    }

    TArray<FString> Errors = GetValidationErrors(Index, Inventory);

    if (Errors.Num() > 0)
    {
        for (const FString &Error : Errors)
        {
            OnValidationFailed.Broadcast(Error);
        }
        return false;
    }

    return true;
}

TArray<FString> ULoadoutComponent::GetValidationErrors(int32 Index, UInventoryComponent *Inventory) const
{
    TArray<FString> Errors;

    if (!SavedLoadouts.IsValidIndex(Index))
    {
        Errors.Add(TEXT("Invalid loadout index"));
        return Errors;
    }

    if (!Inventory)
    {
        Errors.Add(TEXT("No inventory component"));
        return Errors;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[Index];

    // Validate primary weapon
    if (!Loadout.bPrimaryIsRing && Loadout.PrimaryWeapon.IsValid())
    {
        // Check weapon is owned
        bool bFound = false;
        for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
        {
            if (Entry.Weapon == Loadout.PrimaryWeapon.WeaponEntry.Weapon)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            Errors.Add(TEXT("Primary weapon not in inventory"));
        }

        // Validate abilities
        for (UAbilityData *Ability : Loadout.PrimaryWeapon.AssignedAbilities)
        {
            if (Ability && !Inventory->HasAbility(Ability))
            {
                Errors.Add(FString::Printf(TEXT("Ability '%s' not learned"), *Ability->AbilityName));
            }
        }

        // Validate spells
        for (USpellData *Spell : Loadout.PrimaryWeapon.AssignedSpells)
        {
            if (Spell && !Inventory->HasSpell(Spell))
            {
                Errors.Add(FString::Printf(TEXT("Spell '%s' not learned"), *Spell->SpellName));
            }
        }
    }

    // Validate primary ring (Caster)
    if (Loadout.bPrimaryIsRing && Loadout.PrimaryRing.IsValid())
    {
        bool bFound = false;
        for (const FRingInventoryEntry &Entry : Inventory->Rings)
        {
            if (Entry.Ring == Loadout.PrimaryRing.RingEntry.Ring)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            Errors.Add(TEXT("Primary ring not in inventory"));
        }
    }

    // Validate secondary (Generic)
    if (CharacterClass == ECharacterClass::Generic)
    {
        if (!Loadout.bSecondaryIsRing && Loadout.SecondaryWeapon.IsValid())
        {
            bool bFound = false;
            for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
            {
                if (Entry.Weapon == Loadout.SecondaryWeapon.WeaponEntry.Weapon)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Errors.Add(TEXT("Secondary weapon not in inventory"));
            }
        }

        if (Loadout.bSecondaryIsRing && Loadout.SecondaryRing.IsValid())
        {
            bool bFound = false;
            for (const FRingInventoryEntry &Entry : Inventory->Rings)
            {
                if (Entry.Ring == Loadout.SecondaryRing.RingEntry.Ring)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Errors.Add(TEXT("Secondary ring not in inventory"));
            }
        }
    }

    // Validate ring loadout (Resonator)
    if (CharacterClass == ECharacterClass::Resonator)
    {
        int32 TotalSlotCost = 0;
        for (const FRingLoadoutEntry &RingEntry : Loadout.RingLoadout)
        {
            if (RingEntry.IsValid())
            {
                bool bFound = false;
                for (const FRingInventoryEntry &Entry : Inventory->Rings)
                {
                    if (Entry.Ring == RingEntry.RingEntry.Ring)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    Errors.Add(TEXT("Ring in loadout not in inventory"));
                }

                // Add slot cost (1 for normal, 2 for evolved)
                TotalSlotCost += InventoryConstants::GetRingSlotCost(RingEntry.IsEvolved());
            }
        }

        if (TotalSlotCost > InventoryConstants::RESONATOR_RING_LOADOUT_SLOT_CAPACITY)
        {
            Errors.Add(FString::Printf(TEXT("Ring loadout exceeds slot capacity (%d/%d slots)"),
                                       TotalSlotCost, InventoryConstants::RESONATOR_RING_LOADOUT_SLOT_CAPACITY));
        }
    }

    // Validate innate spells (Caster)
    if (CharacterClass == ECharacterClass::Caster)
    {
        for (USpellData *Spell : Loadout.InnateSpells)
        {
            if (Spell && !Inventory->HasSpell(Spell))
            {
                Errors.Add(FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->SpellName));
            }
        }
    }

    // Validate items
    for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
    {
        if (Slot.HasCrystal() && !Inventory->HasItem(Slot.Crystal))
        {
            Errors.Add(TEXT("Item in loadout not in inventory"));
        }
    }

    // Check for duplicate item types
    if (Loadout.HasDuplicateItemTypes())
    {
        Errors.Add(TEXT("Duplicate item types in loadout"));
    }

    return Errors;
}

// ==================== BATTLE PREPARATION ====================

bool ULoadoutComponent::PrepareForBattle(UInventoryComponent *Inventory)
{
    if (!ValidateActiveLoadout(Inventory))
    {
        bIsReadyForBattle = false;
        return false;
    }

    // Reset item uses
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        for (FItemLoadoutSlot &Slot : SavedLoadouts[ActiveLoadoutIndex].ItemSlots)
        {
            Slot.ResetForBattle();
        }
    }

    bIsReadyForBattle = true;
    return true;
}

// ==================== RUNTIME COMBAT STATE ====================

bool ULoadoutComponent::UseItem(int32 SlotIndex)
{
    if (!bIsReadyForBattle)
    {
        return false;
    }

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (!Loadout.ItemSlots.IsValidIndex(SlotIndex))
    {
        return false;
    }

    FItemLoadoutSlot &Slot = Loadout.ItemSlots[SlotIndex];

    if (!Slot.UseOne())
    {
        return false;
    }

    OnItemUsed.Broadcast(SlotIndex, Slot.Crystal);
    return true;
}

int32 ULoadoutComponent::GetItemRemainingUses(int32 SlotIndex) const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return 0;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (!Loadout.ItemSlots.IsValidIndex(SlotIndex))
    {
        return 0;
    }

    return Loadout.ItemSlots[SlotIndex].GetRemainingUses();
}

TArray<UAbilityData *> ULoadoutComponent::GetAvailableAbilities() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return TArray<UAbilityData *>();
    }

    return SavedLoadouts[ActiveLoadoutIndex].GetAllAbilities();
}

TArray<USpellData *> ULoadoutComponent::GetAvailableSpells() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GetAvailableSpells] Invalid ActiveLoadoutIndex: %d"), ActiveLoadoutIndex);
        return TArray<USpellData *>();
    }

    TArray<USpellData *> Result = SavedLoadouts[ActiveLoadoutIndex].GetAllSpells();
    UE_LOG(LogTemp, Warning, TEXT("[GetAvailableSpells] Returning %d spells"), Result.Num());
    return Result;
}

TArray<FItemLoadoutSlot> ULoadoutComponent::GetUsableItems() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return TArray<FItemLoadoutSlot>();
    }

    return SavedLoadouts[ActiveLoadoutIndex].GetUsableItemSlots();
}

int32 ULoadoutComponent::GetUsableItemCount() const
{
    return GetUsableItems().Num();
}

// ==================== POST-BATTLE ====================

void ULoadoutComponent::ConsumeUsedItems(UInventoryComponent *Inventory)
{
    if (!Inventory || !SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    for (FItemLoadoutSlot &Slot : Loadout.ItemSlots)
    {
        int32 ToConsume = Slot.GetItemsToConsume();
        for (int32 i = 0; i < ToConsume; ++i)
        {
            Inventory->RemoveItem(Slot.Crystal);
        }
    }
}

TArray<UItemData *> ULoadoutComponent::GetItemsToConsume() const
{
    TArray<UItemData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
    {
        int32 ToConsume = Slot.GetItemsToConsume();
        for (int32 i = 0; i < ToConsume; ++i)
        {
            Result.Add(Slot.Crystal);
        }
    }

    return Result;
}

void ULoadoutComponent::ResetBattleState()
{
    bIsReadyForBattle = false;

    // Reset item slots
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        for (FItemLoadoutSlot &Slot : SavedLoadouts[ActiveLoadoutIndex].ItemSlots)
        {
            Slot.ResetForBattle();
        }
    }
}

// ==================== QUICK SETUP ====================

bool ULoadoutComponent::AutoPopulateLoadout(int32 LoadoutIndex, UInventoryComponent *Inventory)
{
    if (!SavedLoadouts.IsValidIndex(LoadoutIndex) || !Inventory)
    {
        return false;
    }

    FCombatLoadout &Loadout = SavedLoadouts[LoadoutIndex];
    Loadout.Clear();
    Loadout.InitializeForClass(CharacterClass);

    // Auto-assign first available weapon
    if (Inventory->Weapons.Num() > 0)
    {
        Loadout.PrimaryWeapon.WeaponEntry = Inventory->Weapons[0];
        Loadout.PrimaryWeapon.InitializeFromWeapon();
    }

    // Auto-assign items (one of each type, highest tier first)
    // TODO: Implement smarter auto-population

    return true;
}

void ULoadoutComponent::ClearLoadout(int32 LoadoutIndex)
{
    if (!SavedLoadouts.IsValidIndex(LoadoutIndex))
    {
        return;
    }

    SavedLoadouts[LoadoutIndex].Clear();
    SavedLoadouts[LoadoutIndex].InitializeForClass(CharacterClass);
}

void ULoadoutComponent::InitializeFromCharacterData(UCharacterData *CharacterData, UInventoryComponent *Inventory)
{
    if (!CharacterData || !Inventory)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeFromCharacterData: Null CharacterData or Inventory"));
        return;
    }

    // Set class from CharacterData
    CharacterClass = CharacterData->CharacterClass;

    // Clear and create one default loadout
    SavedLoadouts.Empty();
    FCombatLoadout DefaultLoadout;
    DefaultLoadout.LoadoutName = TEXT("Default");
    DefaultLoadout.InitializeForClass(CharacterClass);

    // Find weapons in inventory
    FWeaponInventoryEntry *PrimaryEntry = nullptr;
    FWeaponInventoryEntry *SecondaryEntry = nullptr;

    for (FWeaponInventoryEntry &Entry : Inventory->Weapons)
    {
        if (Entry.Weapon == CharacterData->PrimaryWeapon && !PrimaryEntry)
        {
            PrimaryEntry = &Entry;
        }
        else if (Entry.Weapon == CharacterData->SecondaryWeapon && !SecondaryEntry)
        {
            SecondaryEntry = &Entry;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[LoadoutInit] PrimaryEntry found: %s, HasEvolution: %s"),
           PrimaryEntry ? *PrimaryEntry->Weapon->WeaponName : TEXT("NULL"),
           (PrimaryEntry && PrimaryEntry->Evolution) ? TEXT("YES") : TEXT("NO"));

    // Configure primary weapon loadout
    if (PrimaryEntry)
    {
        DefaultLoadout.PrimaryWeapon.WeaponEntry = *PrimaryEntry;

        // Copy preset abilities
        if (PrimaryEntry->Weapon)
        {
            for (UAbilityData *Ability : PrimaryEntry->Weapon->PresetAbilities)
            {
                if (Ability && DefaultLoadout.PrimaryWeapon.AssignedAbilities.Num() < LoadoutConstants::MAX_WEAPON_ABILITIES)
                {
                    DefaultLoadout.PrimaryWeapon.AssignedAbilities.Add(Ability);
                }
            }
        }

        // Copy evolution spells if weapon has evolution
        if (PrimaryEntry->Evolution)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutInit] Copying %d evolution spells from primary weapon"),
                   PrimaryEntry->Evolution->EquippedSpells.Num());
            for (USpellData *Spell : PrimaryEntry->Evolution->EquippedSpells)
            {
                if (Spell && DefaultLoadout.PrimaryWeapon.AssignedSpells.Num() < LoadoutConstants::MAX_SPELL_SLOTS)
                {
                    DefaultLoadout.PrimaryWeapon.AssignedSpells.Add(Spell);
                }
            }
        }
    }

    // Configure secondary based on class
    if (CharacterClass == ECharacterClass::Generic)
    {
        if (SecondaryEntry)
        {
            DefaultLoadout.SecondaryWeapon.WeaponEntry = *SecondaryEntry;

            if (SecondaryEntry->Weapon)
            {
                for (UAbilityData *Ability : SecondaryEntry->Weapon->PresetAbilities)
                {
                    if (Ability && DefaultLoadout.SecondaryWeapon.AssignedAbilities.Num() < LoadoutConstants::MAX_WEAPON_ABILITIES)
                    {
                        DefaultLoadout.SecondaryWeapon.AssignedAbilities.Add(Ability);
                    }
                }
            }
            // Copy evolution spells if secondary weapon has evolution
            if (SecondaryEntry->Evolution)
            {
                for (USpellData *Spell : SecondaryEntry->Evolution->EquippedSpells)
                {
                    if (Spell && DefaultLoadout.SecondaryWeapon.AssignedSpells.Num() < LoadoutConstants::MAX_SPELL_SLOTS)
                    {
                        DefaultLoadout.SecondaryWeapon.AssignedSpells.Add(Spell);
                    }
                }
            }
        }
        else if (CharacterData->SecondaryRing)
        {
            // Find ring in inventory
            for (FRingInventoryEntry &RingEntry : Inventory->Rings)
            {
                if (RingEntry.Ring == CharacterData->SecondaryRing)
                {
                    DefaultLoadout.SecondaryRing.RingEntry = RingEntry;
                    break;
                }
            }
        }
    }
    else if (CharacterClass == ECharacterClass::Caster)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutInit] Caster - InnateSpells: %d, IsEvolved: %s, HasActiveEvolution: %s"),
               CharacterData->InnateSpells.Num(),
               CharacterData->IsEvolved() ? TEXT("YES") : TEXT("NO"),
               CharacterData->ActiveEvolution ? TEXT("YES") : TEXT("NO"));
        // Caster innate spells go into InnateSpells
        for (USpellData *Spell : CharacterData->InnateSpells)
        {
            if (Spell && DefaultLoadout.InnateSpells.Num() < InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
            {
                DefaultLoadout.InnateSpells.Add(Spell);
            }
        }

        // Copy evolution spells if caster is evolved
        if (CharacterData->IsEvolved() && CharacterData->ActiveEvolution)
        {
            for (USpellData *Spell : CharacterData->ActiveEvolution->EquippedSpells)
            {
                if (Spell && DefaultLoadout.InnateSpells.Num() < InventoryConstants::MAX_INNATE_SPELLS_TOTAL)
                {
                    DefaultLoadout.InnateSpells.Add(Spell);
                }
            }
        }
    }
    else if (CharacterClass == ECharacterClass::Resonator)
    {
        // Ensure RingLoadout is properly sized
        if (DefaultLoadout.RingLoadout.Num() < InventoryConstants::MAX_RESONATOR_RINGS)
        {
            DefaultLoadout.RingLoadout.SetNum(InventoryConstants::MAX_RESONATOR_RINGS);
        }

        // Copy equipped rings to ring loadout
        int32 RingIndex = 0;
        for (URingData *RingData : CharacterData->EquippedRings)
        {
            if (RingData && RingIndex < InventoryConstants::MAX_RESONATOR_RINGS)
            {
                // Find in inventory
                for (FRingInventoryEntry &RingEntry : Inventory->Rings)
                {
                    if (RingEntry.Ring == RingData)
                    {
                        DefaultLoadout.RingLoadout[RingIndex].RingEntry = RingEntry;
                        RingIndex++;
                        break;
                    }
                }
            }
        }
    }

    DefaultLoadout.ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
    for (FItemLoadoutSlot &Slot : DefaultLoadout.ItemSlots)
    {
        Slot.Clear();
    }

    SavedLoadouts.Add(DefaultLoadout);

    ActiveLoadoutIndex = 0;

    UE_LOG(LogTemp, Display, TEXT("Initialized loadout for %s (%s class)"),
           *CharacterData->CharacterName,
           *UEnum::GetValueAsString(CharacterClass));
}

#if WITH_EDITOR
void ULoadoutComponent::DebugLogLoadout()
{
    UE_LOG(LogTemp, Display, TEXT("=== LOADOUT DEBUG ==="));
    UE_LOG(LogTemp, Display, TEXT("Character Class: %d"), static_cast<int32>(CharacterClass));
    UE_LOG(LogTemp, Display, TEXT("Saved Loadouts: %d/%d"), SavedLoadouts.Num(), MaxSavedLoadouts);
    UE_LOG(LogTemp, Display, TEXT("Active Index: %d"), ActiveLoadoutIndex);
    UE_LOG(LogTemp, Display, TEXT("Ready for Battle: %s"), bIsReadyForBattle ? TEXT("Yes") : TEXT("No"));

    for (int32 i = 0; i < SavedLoadouts.Num(); ++i)
    {
        const FCombatLoadout &L = SavedLoadouts[i];
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("[%d] %s %s"), i, *L.LoadoutName,
               i == ActiveLoadoutIndex ? TEXT("(ACTIVE)") : TEXT(""));

        if (!L.bPrimaryIsRing && L.PrimaryWeapon.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("  Primary: %s (%d abilities, %d spells)"),
                   *L.PrimaryWeapon.WeaponEntry.Weapon->WeaponName,
                   L.PrimaryWeapon.AssignedAbilities.Num(),
                   L.PrimaryWeapon.AssignedSpells.Num());
        }

        UE_LOG(LogTemp, Display, TEXT("  Items: %d slots"), L.ItemSlots.Num());
        UE_LOG(LogTemp, Display, TEXT("  Total Abilities: %d"), L.GetAllAbilities().Num());
        UE_LOG(LogTemp, Display, TEXT("  Total Spells: %d"), L.GetAllSpells().Num());
    }
}
#endif