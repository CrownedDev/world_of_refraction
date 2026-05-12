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
#include "CharacterDataComponent.h"
#include "ElementHelpers.h"
#include "LoadoutData.h"
#include "RingData.h"
#include "ItemData.h"
#include "StanceData.h"

#include "WeaponAttackData.h"

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

int32 ULoadoutComponent::CreateAndConfigureLoadout(
    const FString &LoadoutName,
    UInventoryComponent *Inventory,
    const TArray<USpellData *> &SpellsToAdd,
    const TArray<UAbilityData *> &AbilitiesToAdd,
    const TArray<UItemData *> &ItemsToAdd)
{
    // Validate inventory
    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] CreateAndConfigureLoadout: Null Inventory"));
        return -1;
    }

    // Create new empty loadout
    int32 NewIndex = CreateNewLoadout(LoadoutName);
    if (NewIndex < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] Failed to create loadout - max loadouts reached (%d/%d)"),
               SavedLoadouts.Num(), MaxSavedLoadouts);
        return -1;
    }

    FCombatLoadout &Loadout = SavedLoadouts[NewIndex];

    UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] Created loadout '%s' at index %d"), *LoadoutName, NewIndex);

    // Add spells (validate against inventory)
    int32 SpellsAdded = 0;
    for (USpellData *Spell : SpellsToAdd)
    {
        if (!Spell)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Null spell in SpellsToAdd array"));
            continue;
        }

        if (!Inventory->HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Spell '%s' not in inventory - skipping"),
                   *Spell->SpellName);
            continue;
        }

        // Add to primary weapon or ring depending on class
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to weapon"), *Spell->SpellName);
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
        {
            Loadout.PrimaryRing.RingEntry.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to ring"), *Spell->SpellName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] No valid weapon/ring to add spell to"));
        }
    }

    // Add abilities (validate against inventory)
    int32 AbilitiesAdded = 0;
    for (UAbilityData *Ability : AbilitiesToAdd)
    {
        if (!Ability)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Null ability in AbilitiesToAdd array"));
            continue;
        }

        if (!Inventory->HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Ability '%s' not in inventory - skipping"),
                   *Ability->AbilityName);
            continue;
        }

        // Add to primary weapon
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedAbilities.Add(Ability);
            AbilitiesAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added ability '%s'"), *Ability->AbilityName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] No valid weapon to add ability to"));
        }
    }

    // Add items (validate against inventory)
    int32 ItemsAdded = 0;
    int32 SlotIndex = 0;
    for (UItemData *Item : ItemsToAdd)
    {
        if (!Item)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Null item in ItemsToAdd array"));
            continue;
        }

        if (SlotIndex >= Loadout.ItemSlots.Num())
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Item slots full (%d) - skipping item '%s'"),
                   Loadout.ItemSlots.Num(), *Item->ItemName);
            break;
        }

        if (!Inventory->HasItem(Item))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Item '%s' not in inventory - skipping"),
                   *Item->ItemName);
            SlotIndex++;
            continue;
        }

        Loadout.ItemSlots[SlotIndex].Crystal = Item;
        Loadout.ItemSlots[SlotIndex].ResetForBattle();
        ItemsAdded++;
        UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added item '%s' to slot %d"),
               *Item->ItemName, SlotIndex);
        SlotIndex++;
    }

    UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] Loadout '%s' configured: %d spells, %d abilities, %d items"),
           *LoadoutName, SpellsAdded, AbilitiesAdded, ItemsAdded);

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
    if (!SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    // Asset-based loadouts don't require inventory validation
    if (bInitializedFromAsset)
    {
        return true;
    }

    if (!Inventory)
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
    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
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

    // Validate primary ring (Generic/Caster)
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
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

    // Validate primary evolution
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution)
    {
        // Evolution validation - check if character owns this evolution crystal
        if (!Inventory->HasItem(Loadout.PrimaryEvolution))
        {
            Errors.Add(TEXT("Primary evolution crystal not in inventory"));
        }

        // Validate evolution spells count
        if (Loadout.EvolutionSpells.Num() > LoadoutConstants::MAX_EVOLUTION_SPELLS)
        {
            Errors.Add(FString::Printf(TEXT("Too many evolution spells (%d/%d)"),
                                       Loadout.EvolutionSpells.Num(), LoadoutConstants::MAX_EVOLUTION_SPELLS));
        }
    }

    // Validate secondary (Generic)
    if (CharacterClass == ECharacterClass::Generic)
    {
        if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
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

        // Ring limits depend on evolution state
        const bool bIsEvolved = (Loadout.PrimarySlotType == EPrimarySlotType::Evolution);
        const int32 MaxSlots = bIsEvolved ? LoadoutConstants::RESONATOR_RING_SLOTS_EVOLVED
                                          : LoadoutConstants::RESONATOR_RING_SLOTS_NORMAL;

        if (TotalSlotCost > MaxSlots)
        {
            Errors.Add(FString::Printf(TEXT("Ring loadout exceeds slot capacity (%d/%d slots)"),
                                       TotalSlotCost, MaxSlots));
        }
        {
            Errors.Add(FString::Printf(TEXT("Ring loadout exceeds slot capacity (%d/%d slots)"),
                                       TotalSlotCost, InventoryConstants::RESONATOR_RING_LOADOUT_SLOT_CAPACITY));
        }
    }

    // Validate innate spells (Caster)
    if (CharacterClass == ECharacterClass::Caster)
    {
        // Need CharacterData for the innate-element check. Pull it from the
        // owning actor's CharacterDataComponent. If unavailable, skip the
        // element check (ownership check still runs).
        UCharacterData *CharData = nullptr;
        if (AActor *OwnerActor = GetOwner())
        {
            if (UCharacterDataComponent *CDC = OwnerActor->FindComponentByClass<UCharacterDataComponent>())
            {
                CharData = CDC->CharacterData;
            }
        }

        const bool bAnyElement = CharData && ElementHelpers::IsAnySpellSource(CharData->InnateElement);

        for (USpellData *Spell : Loadout.InnateSpells)
        {
            if (!Spell)
                continue;

            if (!Inventory->HasSpell(Spell))
            {
                Errors.Add(FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->SpellName));
                continue;
            }

            // Element-match check (only if we resolved CharData; otherwise skip)
            if (CharData && !bAnyElement && Spell->Element != CharData->InnateElement)
            {
                Errors.Add(FString::Printf(
                    TEXT("Innate spell '%s' element does not match Caster's innate element"),
                    *Spell->SpellName));
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
    // Not initialized yet - can't prepare
    if (SavedLoadouts.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] PrepareForBattle called before initialization"));
        return false;
    }

    // Asset-based loadouts (AI) skip inventory validation - designer configured
    if (bInitializedFromAsset)
    {
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

    // Player loadouts - validate against inventory
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

TArray<UWeaponAttackData *> ULoadoutComponent::GetAllWeaponAttacks() const
{
    TArray<UWeaponAttackData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Primary weapon attack
    if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        if (UWeaponData *Weapon = Loadout.PrimaryWeapon.WeaponEntry.Weapon)
        {
            if (Weapon->WeaponAttack)
            {
                Result.Add(Weapon->WeaponAttack);
            }
        }
    }

    // Secondary weapon attack (Generic only)
    if (CharacterClass == ECharacterClass::Generic &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
        Loadout.SecondaryWeapon.IsValid())
    {
        if (UWeaponData *Weapon = Loadout.SecondaryWeapon.WeaponEntry.Weapon)
        {
            if (Weapon->WeaponAttack)
            {
                Result.Add(Weapon->WeaponAttack);
            }
        }
    }

    return Result;
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
    UE_LOG(LogTemp, Verbose, TEXT("[GetAvailableSpells] Returning %d spells"), Result.Num());
    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetPrimarySlotSpells() const
{
    TArray<USpellData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Ring primary
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        Result = Loadout.PrimaryRing.GetAllSpells();
    }
    // Evolution primary
    else if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result = Loadout.EvolutionSpells;
    }
    // Weapon primary with crystal
    else if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        Result = Loadout.PrimaryWeapon.GetAllSpells();
    }

    // Caster also has innate spells
    if (CharacterClass == ECharacterClass::Caster)
    {
        Result.Append(Loadout.InnateSpells);
    }

    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetSecondarySlotSpells() const
{
    TArray<USpellData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Only Generic has secondary
    if (CharacterClass != ECharacterClass::Generic)
    {
        return Result;
    }

    // Secondary weapon with crystal spells
    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
    {
        Result = Loadout.SecondaryWeapon.GetAllSpells();
    }

    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetActiveSlotSpells() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return TArray<USpellData *>();
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Resonator uses active ring
    if (CharacterClass == ECharacterClass::Resonator)
    {
        return GetRingResonateSpells();
    }

    // Generic dual-weapon: bShowPrimary IS a real gameplay toggle —
    // only the shown weapon's spells are available.
    if (CharacterClass == ECharacterClass::Generic &&
        Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
    {
        return Loadout.bShowPrimary ? GetPrimarySlotSpells() : GetSecondarySlotSpells();
    }

    // All other configurations: bShowPrimary is display-only, so all spells are available.
    return GetAvailableSpells();
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
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] InitializeFromCharacterData: Null CharacterData"));
        return;
    }

    // Set class from CharacterData
    CharacterClass = CharacterData->CharacterClass;

    // If DefaultLoadout exists, use it (AI path or player template)
    if (CharacterData->DefaultLoadout)
    {
        InitializeFromAsset(CharacterData->DefaultLoadout);

        // For players with inventory, we may want to validate against inventory later
        // For now, asset-based initialization is sufficient
        if (Inventory)
        {
            UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] %s: Initialized from DefaultLoadout (inventory available for future validation)"),
                   *CharacterData->CharacterName);
        }
        return;
    }

    // No DefaultLoadout - create empty loadout for manual setup
    SavedLoadouts.Empty();
    FCombatLoadout EmptyLoadout;
    EmptyLoadout.LoadoutName = TEXT("Default");
    EmptyLoadout.InitializeForClass(CharacterClass);
    SavedLoadouts.Add(EmptyLoadout);
    ActiveLoadoutIndex = 0;
    bInitializedFromAsset = false;
    bIsReadyForBattle = false;

    UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] %s has no DefaultLoadout - created empty loadout. Assign equipment via UI or set DefaultLoadout in CharacterData."),
           *CharacterData->CharacterName);
}

void ULoadoutComponent::InitializeFromAsset(ULoadoutData *LoadoutAsset)
{
    if (!LoadoutAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] InitializeFromAsset: Null LoadoutAsset"));
        return;
    }

    // Log validation warnings (but don't block - designer may be testing)
    if (LoadoutAsset->HasValidationErrors())
    {
        TArray<FString> Errors = LoadoutAsset->GetValidationErrors();
        for (const FString &Error : Errors)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] LoadoutData '%s' validation: %s"),
                   *LoadoutAsset->LoadoutName, *Error);
        }
    }

    // Set class from asset
    CharacterClass = LoadoutAsset->RequiredClass;

    // Clear existing loadouts
    SavedLoadouts.Empty();

    // Create loadout from asset using factory function
    FCombatLoadout NewLoadout = FCombatLoadout::CreateFromAsset(LoadoutAsset);
    SavedLoadouts.Add(NewLoadout);
    ActiveLoadoutIndex = 0;

    // Mark as asset-based (skips inventory validation)
    bInitializedFromAsset = true;
    bIsReadyForBattle = true; // Asset-based loadouts are always ready

    UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] Initialized from asset '%s' (Class: %s)"),
           *LoadoutAsset->LoadoutName,
           *UEnum::GetValueAsString(CharacterClass));
}

// ==================== COMBAT ACCESSORS ====================

UWeaponData *ULoadoutComponent::GetActiveWeapon() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Evolution primary = no weapon access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic uses secondary weapon
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        // Caster/Resonator have no weapon when evolved
        return nullptr;
    }

    // Ring primary - bShowPrimary controls stance display only, not combat capability.
    // Generic with weapon-secondary always has weapon access; Caster/Resonator have none.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }

        return nullptr;
    }

    // Weapon primary - class-specific behavior
    if (CharacterClass == ECharacterClass::Generic)
    {
        // Generic dual-weapon: bShowPrimary IS a real gameplay toggle.
        if (Loadout.bShowPrimary)
        {
            return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        else if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        return nullptr;
    }

    // Caster/Resonator: weapon equipped iff primary slot has weapon.
    // bShowPrimary controls stance display only, not combat capability.
    return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
}

UWeaponData *ULoadoutComponent::GetPrimaryWeapon() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
    {
        return nullptr;
    }

    return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
}

UWeaponData *ULoadoutComponent::GetSecondaryWeapon() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (CharacterClass != ECharacterClass::Generic)
    {
        return nullptr;
    }

    if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
    {
        return nullptr;
    }

    return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
}

URingData *ULoadoutComponent::GetPrimaryRing() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
    {
        return nullptr;
    }

    return Loadout.PrimaryRing.IsValid() ? Loadout.PrimaryRing.RingEntry.Ring : nullptr;
}

URingData *ULoadoutComponent::GetActiveRing() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Only Resonators have ring loadout
    if (CharacterClass != ECharacterClass::Resonator)
    {
        return nullptr;
    }

    if (!Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
    {
        return nullptr;
    }

    const FRingLoadoutEntry &Entry = Loadout.RingLoadout[Loadout.ActiveRingIndex];
    return Entry.IsValid() ? Entry.RingEntry.Ring : nullptr;
}

UItemData *ULoadoutComponent::GetPrimaryEvolution() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return nullptr;
    }

    return Loadout.PrimaryEvolution;
}

TArray<FEquippedCrystalSlot> ULoadoutComponent::GetEquippedCrystals() const
{
    TArray<FEquippedCrystalSlot> Result;

    auto AddIfCrystal = [&Result](UItemData *Crystal, UObject *Holder)
    {
        if (Crystal && Holder)
        {
            FEquippedCrystalSlot Slot;
            Slot.Crystal = Crystal;
            Slot.Holder = Holder;
            Result.Add(Slot);
        }
    };

    // Weapons — primary and secondary
    if (UWeaponData *Primary = GetPrimaryWeapon())
    {
        AddIfCrystal(Primary->SlottedCrystal, Primary);
    }
    if (UWeaponData *Secondary = GetSecondaryWeapon())
    {
        AddIfCrystal(Secondary->SlottedCrystal, Secondary);
    }

    // Primary ring (Generic / Caster)
    if (URingData *PrimaryRing = GetPrimaryRing())
    {
        AddIfCrystal(PrimaryRing->SlottedCrystal, PrimaryRing);
    }

    // Resonator ring loadout — every ring in the loadout array
    const FCombatLoadout ActiveLoadout = GetActiveLoadout();
    for (const FRingLoadoutEntry &Entry : ActiveLoadout.RingLoadout)
    {
        if (URingData *Ring = Entry.RingEntry.Ring)
        {
            AddIfCrystal(Ring->SlottedCrystal, Ring);
        }
    }

    // Evolution crystal slot — holder is the crystal itself (no separate holder asset).
    if (UItemData *Evolution = GetPrimaryEvolution())
    {
        AddIfCrystal(Evolution, Evolution);
    }

    return Result;
}

EPrimarySlotType ULoadoutComponent::GetPrimarySlotType() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return EPrimarySlotType::Weapon;
    }
    return SavedLoadouts[ActiveLoadoutIndex].PrimarySlotType;
}

ESecondarySlotType ULoadoutComponent::GetSecondarySlotType() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return ESecondarySlotType::None;
    }
    return SavedLoadouts[ActiveLoadoutIndex].SecondarySlotType;
}

bool ULoadoutComponent::IsShowingPrimary() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return true;
    }
    return SavedLoadouts[ActiveLoadoutIndex].bShowPrimary;
}

void ULoadoutComponent::ToggleEquipment()
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];
    Loadout.bShowPrimary = !Loadout.bShowPrimary;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Toggled equipment: bShowPrimary = %s"),
           Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));
}

bool ULoadoutComponent::HasWeaponAccess() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Evolution primary = no weapon access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic can still use secondary weapon
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid())
        {
            return true;
        }
        // Caster/Resonator lose all weapon access when evolved
        return false;
    }

    // Ring primary - only Generic with secondary weapon has access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        return CharacterClass == ECharacterClass::Generic &&
               Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
               Loadout.SecondaryWeapon.IsValid();
    }

    // Weapon primary - check if any weapon exists
    return Loadout.PrimaryWeapon.IsValid() ||
           (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid());
}

bool ULoadoutComponent::IsArmed() const
{
    // Armed iff combat layer has a weapon. bShowPrimary controls stance display only.
    return GetActiveWeapon() != nullptr;
}
bool ULoadoutComponent::IsEvolved() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }

    return SavedLoadouts[ActiveLoadoutIndex].PrimarySlotType == EPrimarySlotType::Evolution;
}
// ==================== EQUIPMENT STATE HELPERS ====================

bool ULoadoutComponent::HasSecondaryEquipment() const
{
    if (CharacterClass != ECharacterClass::Generic)
    {
        return false;
    }

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Evolved characters lose secondary
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        return false;
    }

    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
    {
        return Loadout.SecondaryWeapon.IsValid();
    }

    return false;
}

bool ULoadoutComponent::HasRingInSecondary() const
{
    if (CharacterClass != ECharacterClass::Generic)
    {
        return false;
    }

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Evolved characters lose secondary
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        return false;
    }
    return false;
}

// ==================== STANCE & ANIMATION ====================

UStanceData *ULoadoutComponent::GetCurrentStance() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Stance is a visual concern, decoupled from combat capability:
    // bShowPrimary picks which slot is currently DISPLAYED, regardless of
    // whether the other slot's weapon is also available for combat actions.
    const FWeaponLoadoutEntry *ShownWeapon = nullptr;

    if (Loadout.bShowPrimary)
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            ShownWeapon = Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
        }
        // Ring/Evolution primary while showing primary => no weapon stance, fall through to unarmed.
    }
    else
    {
        // Showing secondary. Only Generic has secondary slots that can hold a weapon.
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            ShownWeapon = Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
        }
    }

    if (ShownWeapon && ShownWeapon->WeaponEntry.Weapon &&
        ShownWeapon->WeaponEntry.Weapon->WeaponStance)
    {
        return ShownWeapon->WeaponEntry.Weapon->WeaponStance;
    }

    return GetUnarmedStance();
}

UAnimMontage *ULoadoutComponent::GetCurrentIdleMontage() const
{
    UStanceData *Stance = GetCurrentStance();
    return Stance ? Stance->IdleAnimMontage : nullptr;
}

UWeaponAttackData *ULoadoutComponent::GetCurrentAttack() const
{
    if (!IsArmed())
    {
        return nullptr;
    }

    const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout();

    if (WeaponEntry && WeaponEntry->WeaponEntry.Weapon)
    {
        return WeaponEntry->WeaponEntry.Weapon->WeaponAttack;
    }

    return nullptr;
}

UAnimMontage *ULoadoutComponent::GetCurrentAttackMontage() const
{
    UWeaponAttackData *Attack = GetCurrentAttack();
    return Attack ? Attack->AttackMontage : nullptr;
}

// ==================== SPELL ACCESS ====================

TArray<USpellData *> ULoadoutComponent::GetCombatSpells() const
{
    TArray<USpellData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Caster innate spells
    if (CharacterClass == ECharacterClass::Caster)
    {
        Result.Append(Loadout.InnateSpells);
    }

    // Evolution spells
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(Loadout.EvolutionSpells);
    }

    // Weapon crystal spells
    const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout();
    if (WeaponEntry)
    {
        Result.Append(WeaponEntry->GetAllSpells());
    }

    // Ring spells (Resonator)
    if (CharacterClass == ECharacterClass::Resonator)
    {
        const FRingLoadoutEntry *RingEntry = GetActiveRingLoadout();
        if (RingEntry)
        {
            Result.Append(RingEntry->GetAllSpells());
        }
    }

    // Primary ring spells (Caster/Generic with ring primary)
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        const FRingLoadoutEntry *RingEntry = GetPrimaryRingLoadout();
        if (RingEntry)
        {
            Result.Append(RingEntry->GetAllSpells());
        }
    }

    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetWeaponResonateSpells() const
{
    TArray<USpellData *> Result;

    const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout();
    if (!WeaponEntry)
    {
        return Result;
    }

    // Only evolved weapons have resonate spells
    if (!WeaponEntry->WeaponEntry.IsEvolved())
    {
        return Result;
    }

    if (WeaponEntry->WeaponEntry.AttachedCrystal.IsValid())
    {
        // Get spells from weapon's assigned spells (no longer from crystal)
        Result = WeaponEntry->WeaponEntry.AssignedSpells;
    }
    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetRingResonateSpells() const
{
    TArray<USpellData *> Result;

    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Caster with ring primary
    if (CharacterClass == ECharacterClass::Caster && Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        const FRingLoadoutEntry *RingEntry = GetPrimaryRingLoadout();
        if (RingEntry)
        {
            return RingEntry->GetAllSpells();
        }
    }

    // Resonator uses active ring
    if (CharacterClass == ECharacterClass::Resonator)
    {
        const FRingLoadoutEntry *RingEntry = GetActiveRingLoadout();
        if (RingEntry)
        {
            return RingEntry->GetAllSpells();
        }
    }

    return Result;
}

// ==================== LOADOUT ENTRY ACCESSORS ====================

const FWeaponLoadoutEntry *ULoadoutComponent::GetActiveWeaponLoadout() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    // Evolution primary = no weapon
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic uses secondary weapon
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid())
        {
            return &Loadout.SecondaryWeapon;
        }
        return nullptr;
    }

    // Ring primary - bShowPrimary controls stance display only, not combat capability.
    // Generic with weapon-secondary always has weapon access; Caster/Resonator have none.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        if (CharacterClass == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
            return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;

        return nullptr;
    }

    // Weapon primary - class-specific behavior
    if (CharacterClass == ECharacterClass::Generic)
    {
        // Generic dual-weapon: bShowPrimary IS a real gameplay toggle.
        if (Loadout.bShowPrimary)
            return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
        else if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
            return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
        return nullptr;
    }

    // Caster/Resonator: weapon equipped iff primary slot has weapon.
    // bShowPrimary controls stance display only, not combat capability.
    return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
}

const FWeaponLoadoutEntry *ULoadoutComponent::GetPrimaryWeaponLoadout() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
        return nullptr;

    return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
}

const FWeaponLoadoutEntry *ULoadoutComponent::GetSecondaryWeaponLoadout() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (CharacterClass != ECharacterClass::Generic)
        return nullptr;

    if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
        return nullptr;

    return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetPrimaryRingLoadout() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
        return nullptr;

    return Loadout.PrimaryRing.RingEntry.Ring != nullptr ? &Loadout.PrimaryRing : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetActiveRingLoadout() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (CharacterClass != ECharacterClass::Resonator)
        return nullptr;

    if (!Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
        return nullptr;

    const FRingLoadoutEntry &Entry = Loadout.RingLoadout[Loadout.ActiveRingIndex];
    return Entry.IsValid() ? &Entry : nullptr;
}

void ULoadoutComponent::SetActiveRingIndex(int32 NewIndex)
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];

    if (!Loadout.RingLoadout.IsValidIndex(NewIndex))
    {
        return;
    }

    Loadout.ActiveRingIndex = NewIndex;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Set ActiveRingIndex to %d"), NewIndex);
}

void ULoadoutComponent::DebugLogLoadout()
{
    UE_LOG(LogTemp, Log, TEXT("=== LoadoutComponent Debug ==="));
    UE_LOG(LogTemp, Log, TEXT("CharacterClass: %s"), *UEnum::GetValueAsString(CharacterClass));
    UE_LOG(LogTemp, Log, TEXT("ActiveLoadoutIndex: %d"), ActiveLoadoutIndex);
    UE_LOG(LogTemp, Log, TEXT("SavedLoadouts: %d"), SavedLoadouts.Num());
    UE_LOG(LogTemp, Log, TEXT("bIsReadyForBattle: %s"), bIsReadyForBattle ? TEXT("true") : TEXT("false"));

    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];
        UE_LOG(LogTemp, Log, TEXT("Active Loadout: %s"), *Loadout.LoadoutName);
        UE_LOG(LogTemp, Log, TEXT("  PrimarySlotType: %s"), *UEnum::GetValueAsString(Loadout.PrimarySlotType));
        UE_LOG(LogTemp, Log, TEXT("  bShowPrimary: %s"), Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));
    }

    UE_LOG(LogTemp, Log, TEXT("=============================="));
}

bool ULoadoutComponent::ShouldUseWeaponParry() const
{
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return false;
    }
    return SavedLoadouts[ActiveLoadoutIndex].bUseWeaponParryAnimation;
}

UAnimMontage *ULoadoutComponent::GetDodgeLeftMontage() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->DodgeLeftMontage;
    }
    return nullptr;
}

UAnimMontage *ULoadoutComponent::GetDodgeRightMontage() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->DodgeRightMontage;
    }
    return nullptr;
}

UAnimMontage *ULoadoutComponent::GetBlockMontage() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->BlockMontage;
    }
    return nullptr;
}

UAnimMontage *ULoadoutComponent::GetParryMontage() const
{
    // Check if loadout prefers weapon parry
    if (SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];
        if (Loadout.bUseWeaponParryAnimation)
        {
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                if (Weapon->ParryMontageOverride)
                {
                    return Weapon->ParryMontageOverride;
                }
            }
        }
    }

    // Fall back to character's parry
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->ParryMontage;
    }
    return nullptr;
}

UStanceData *ULoadoutComponent::GetUnarmedStance() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->UnarmedStance;
    }
    return nullptr;
}

UInfusionDisplayData *ULoadoutComponent::GetInfusionDisplay() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->InfusionDisplay;
    }
    return nullptr;
}

UAnimMontage *ULoadoutComponent::GetRingSwitchMontage() const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return CharData->RingSwitchMontage;
    }
    return nullptr;
}

// GetItemUseAnimation() - change from reading FCombatLoadout to CharacterData
UAnimMontage *ULoadoutComponent::GetItemUseAnimation(bool bIsSelfTarget) const
{
    if (UCharacterData *CharData = GetOwnerCharacterData())
    {
        return bIsSelfTarget ? CharData->ItemUseSelfMontage : CharData->ItemUseTargetMontage;
    }
    return nullptr;
}

UCharacterData *ULoadoutComponent::GetOwnerCharacterData() const
{
    if (!GetOwner())
    {
        return nullptr;
    }

    if (UCharacterDataComponent *CharComp = GetOwner()->FindComponentByClass<UCharacterDataComponent>())
    {
        return CharComp->CharacterData;
    }
    return nullptr;
}
