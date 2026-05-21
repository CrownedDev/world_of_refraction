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
#include "CosmeticsData.h"
#include "BrokenDarknessManager.h"
#include "ElementHelpers.h"
#include "RingData.h"
#include "ItemData.h"
#include "StanceData.h"
#include "TurnManager.h"
#include "Engine/GameInstance.h"

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::EnsureDefaultLoadout] No InventoryComponent found on owner"));
        return;
    }
    if (Inv->SavedLoadouts.Num() == 0)
    {
        FCombatLoadout DefaultLoadout;
        DefaultLoadout.LoadoutName = TEXT("Default");
        DefaultLoadout.InitializeForClass(CharacterClass);
        Inv->SavedLoadouts.Add(DefaultLoadout);
        Inv->ActiveLoadoutIndex = 0;
    }
}

// ==================== ACTIVE LOADOUT ====================

FCombatLoadout ULoadoutComponent::GetActiveLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadout] No InventoryComponent found on owner"));
        return FCombatLoadout();
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    }
    return FCombatLoadout();
}

bool ULoadoutComponent::GetActiveLoadoutRef(FCombatLoadout &OutLoadout) const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadoutRef] No InventoryComponent found on owner"));
        return false;
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        OutLoadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        return true;
    }
    return false;
}

bool ULoadoutComponent::SetActiveLoadoutIndex(int32 Index)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::SetActiveLoadoutIndex] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    Inv->ActiveLoadoutIndex = Index;
    bIsReadyForBattle = false; // local field, stays
    OnLoadoutChanged.Broadcast(Index); // local delegate, broadcast preserved
    return true;
}

int32 ULoadoutComponent::GetActiveLoadoutIndex() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    return Inv ? Inv->ActiveLoadoutIndex : 0;
}

FString ULoadoutComponent::GetActiveLoadoutName() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadoutName] No InventoryComponent found on owner"));
        return TEXT("Invalid");
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].LoadoutName;
    }
    return TEXT("Invalid");
}

// ==================== LOADOUT MANAGEMENT ====================

int32 ULoadoutComponent::CreateNewLoadout(const FString &LoadoutName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::CreateNewLoadout] No InventoryComponent found on owner"));
        return -1;
    }
    if (Inv->SavedLoadouts.Num() >= Inv->MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout;
    NewLoadout.LoadoutName = LoadoutName;
    NewLoadout.InitializeForClass(CharacterClass);

    int32 NewIndex = Inv->SavedLoadouts.Add(NewLoadout);
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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::CreateAndConfigureLoadout] No InventoryComponent found on owner"));
        return -1;
    }

    // Create new empty loadout
    int32 NewIndex = CreateNewLoadout(LoadoutName);
    if (NewIndex < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] Failed to create loadout - max loadouts reached (%d/%d)"),
               Inv->SavedLoadouts.Num(), Inv->MaxSavedLoadouts);
        return -1;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[NewIndex];

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
                   *Spell->Name);
            continue;
        }

        // Add to primary weapon or ring depending on class
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to weapon"), *Spell->Name);
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
        {
            Loadout.PrimaryRing.RingEntry.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to ring"), *Spell->Name);
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
                   *Ability->Name);
            continue;
        }

        // Add to primary weapon
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedAbilities.Add(Ability);
            AbilitiesAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added ability '%s'"), *Ability->Name);
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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DeleteLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    // Don't delete last loadout
    if (Inv->SavedLoadouts.Num() <= 1)
    {
        return false;
    }

    Inv->SavedLoadouts.RemoveAt(Index);

    // Adjust active index if needed
    if (Inv->ActiveLoadoutIndex >= Inv->SavedLoadouts.Num())
    {
        Inv->ActiveLoadoutIndex = Inv->SavedLoadouts.Num() - 1;
    }
    else if (Inv->ActiveLoadoutIndex > Index)
    {
        Inv->ActiveLoadoutIndex--;
    }

    return true;
}

int32 ULoadoutComponent::DuplicateLoadout(int32 SourceIndex, const FString &NewName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DuplicateLoadout] No InventoryComponent found on owner"));
        return -1;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(SourceIndex))
    {
        return -1;
    }

    if (Inv->SavedLoadouts.Num() >= Inv->MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout = Inv->SavedLoadouts[SourceIndex];
    NewLoadout.LoadoutName = NewName;

    int32 NewIndex = Inv->SavedLoadouts.Add(NewLoadout);
    return NewIndex;
}

bool ULoadoutComponent::RenameLoadout(int32 Index, const FString &NewName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::RenameLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    Inv->SavedLoadouts[Index].LoadoutName = NewName;
    return true;
}

int32 ULoadoutComponent::GetLoadoutCount() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    return Inv ? Inv->SavedLoadouts.Num() : 0;
}

TArray<FString> ULoadoutComponent::GetLoadoutNames() const
{
    TArray<FString> Names;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetLoadoutNames] No InventoryComponent found on owner"));
        return Names;
    }
    for (const FCombatLoadout &Loadout : Inv->SavedLoadouts)
    {
        Names.Add(Loadout.LoadoutName);
    }
    return Names;
}

// ==================== VALIDATION ====================

bool ULoadoutComponent::ValidateActiveLoadout(UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ValidateActiveLoadout] No InventoryComponent found on owner"));
        return false;
    }
    return ValidateLoadout(Inv->ActiveLoadoutIndex, Inventory);
}

bool ULoadoutComponent::ValidateLoadout(int32 Index, UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ValidateLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetValidationErrors] No InventoryComponent found on owner"));
        Errors.Add(TEXT("No inventory component on owner"));
        return Errors;
    }

    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        Errors.Add(TEXT("Invalid loadout index"));
        return Errors;
    }

    if (!Inventory)
    {
        Errors.Add(TEXT("No inventory component"));
        return Errors;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Index];

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
                Errors.Add(FString::Printf(TEXT("Ability '%s' not learned"), *Ability->Name));
            }
        }

        // Validate spells
        for (USpellData *Spell : Loadout.PrimaryWeapon.AssignedSpells)
        {
            if (Spell && !Inventory->HasSpell(Spell))
            {
                Errors.Add(FString::Printf(TEXT("Spell '%s' not learned"), *Spell->Name));
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
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Crystal)
    {
        // Evolution validation - check if character owns this evolution crystal
        if (!Inventory->HasItem(Loadout.PrimaryEvolution.Crystal))
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
    }

    // Validate innate spells (Caster)
    if (CharacterClass == ECharacterClass::Caster)
    {
        // Resolve the owning actor's CharacterDataComponent + BrokenDarknessManager.
        AActor *OwnerActor = GetOwner();
        UCharacterDataComponent *CharComp = nullptr;
        UBrokenDarknessManager *BDManager = nullptr;
        if (OwnerActor)
        {
            CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
            BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
        }

        if (CharComp && CharComp->IsBrokenDarkness())
        {
            // Broken Darkness: InnateSpells is the Darkness pool, BDSpellPools
            // the per-element pools. Structural rules only (caps + element match).
            Errors.Append(FCombatLoadout::ValidateBDSpellLoadout(
                Loadout.InnateSpells, Loadout.BDSpellPools));
        }
        else
        {
            // Normal Caster: ownership + element-capability per innate spell.
            for (USpellData *Spell : Loadout.InnateSpells)
            {
                if (!Spell)
                    continue;

                if (!Inventory->HasSpell(Spell))
                {
                    Errors.Add(FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->Name));
                    continue;
                }

                // Element-capability check — shared with ActionExecutor::ValidateAction.
                if (!UBrokenDarknessManager::IsElementCastable(OwnerActor, CharComp, BDManager, Spell->Element))
                {
                    Errors.Add(FString::Printf(
                        TEXT("Innate spell '%s' element not castable by this character"),
                        *Spell->Name));
                }
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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::PrepareForBattle] No InventoryComponent found on owner"));
        return false;
    }

    // Not initialized yet - can't prepare
    if (Inv->SavedLoadouts.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] PrepareForBattle called before initialization"));
        return false;
    }

    // Soft-fail: ValidateActiveLoadout broadcasts OnValidationFailed for each
    // error but its return value is intentionally ignored. Combat proceeds
    // with whatever's valid; validation surfaces warnings, not failures.
    ValidateActiveLoadout(Inventory);

    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        for (FItemLoadoutSlot &Slot : Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].ItemSlots)
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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::UseItem] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetItemRemainingUses] No InventoryComponent found on owner"));
        return 0;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return 0;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (!Loadout.ItemSlots.IsValidIndex(SlotIndex))
    {
        return 0;
    }

    return Loadout.ItemSlots[SlotIndex].GetRemainingUses();
}

TArray<UWeaponAttackData *> ULoadoutComponent::GetAllWeaponAttacks() const
{
    TArray<UWeaponAttackData *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAllWeaponAttacks] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Primary weapon attack — OverrideAttack takes precedence over the asset attack.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        UWeaponData *Weapon = Loadout.PrimaryWeapon.WeaponEntry.Weapon;
        UWeaponAttackData *Attack = Loadout.PrimaryWeapon.OverrideAttack
                                        ? Loadout.PrimaryWeapon.OverrideAttack
                                        : (Weapon ? Weapon->WeaponAttack : nullptr);
        if (Attack)
        {
            Result.Add(Attack);
        }
    }

    // Secondary weapon attack (Generic only) — OverrideAttack takes precedence.
    if (CharacterClass == ECharacterClass::Generic &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
        Loadout.SecondaryWeapon.IsValid())
    {
        UWeaponData *Weapon = Loadout.SecondaryWeapon.WeaponEntry.Weapon;
        UWeaponAttackData *Attack = Loadout.SecondaryWeapon.OverrideAttack
                                        ? Loadout.SecondaryWeapon.OverrideAttack
                                        : (Weapon ? Weapon->WeaponAttack : nullptr);
        if (Attack)
        {
            Result.Add(Attack);
        }
    }

    return Result;
}

TArray<UAbilityData *> ULoadoutComponent::GetAvailableAbilities() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAvailableAbilities] No InventoryComponent found on owner"));
        return TArray<UAbilityData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<UAbilityData *>();
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].GetAllAbilities();
}

TArray<USpellData *> ULoadoutComponent::GetAvailableSpells() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAvailableSpells] No InventoryComponent found on owner"));
        return TArray<USpellData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GetAvailableSpells] Invalid ActiveLoadoutIndex: %d"), Inv->ActiveLoadoutIndex);
        return TArray<USpellData *>();
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    TArray<USpellData *> Result = Loadout.GetAllSpells();

    // Broken Darkness: GetAllSpells yields only the Darkness pool (InnateSpells).
    // Append each BD element pool whose element is currently absorbed — the same
    // castable set FCombatCapabilities surfaces for the player UI.
    if (AActor *OwnerActor = GetOwner())
    {
        UCharacterDataComponent *CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
        UBrokenDarknessManager *BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
        if (CharComp && CharComp->IsBrokenDarkness() && BDManager)
        {
            for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
            {
                if (BDManager->HasAbsorbedElement(Pool.Element))
                {
                    Result.Append(Pool.Spells);
                }
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[GetAvailableSpells] Returning %d spells"), Result.Num());
    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetPrimarySlotSpells() const
{
    TArray<USpellData *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimarySlotSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondarySlotSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveSlotSpells] No InventoryComponent found on owner"));
        return TArray<USpellData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<USpellData *>();
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetUsableItems] No InventoryComponent found on owner"));
        return TArray<FItemLoadoutSlot>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<FItemLoadoutSlot>();
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].GetUsableItemSlots();
}

int32 ULoadoutComponent::GetUsableItemCount() const
{
    return GetUsableItems().Num();
}

// ==================== POST-BATTLE ====================

void ULoadoutComponent::ConsumeUsedItems(UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ConsumeUsedItems] No InventoryComponent found on owner"));
        return;
    }
    if (!Inventory || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetItemsToConsume] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ResetBattleState] No InventoryComponent found on owner"));
        return;
    }

    // Reset item slots
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        for (FItemLoadoutSlot &Slot : Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].ItemSlots)
        {
            Slot.ResetForBattle();
        }
    }
}

// ==================== QUICK SETUP ====================

bool ULoadoutComponent::AutoPopulateLoadout(int32 LoadoutIndex, UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::AutoPopulateLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(LoadoutIndex) || !Inventory)
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[LoadoutIndex];
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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ClearLoadout] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(LoadoutIndex))
    {
        return;
    }

    Inv->SavedLoadouts[LoadoutIndex].Clear();
    Inv->SavedLoadouts[LoadoutIndex].InitializeForClass(CharacterClass);
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

    // New path: InventoryComponent has already populated SavedLoadouts +
    // ActiveLoadoutIndex from UInventoryData. LC just sets class, applies
    // BD pool runtime flag, and returns.
    if (CharacterData->Inventory)
    {
        ApplyBDPoolsIfBroken();
        UE_LOG(LogTemp, Display,
               TEXT("[LoadoutComponent] %s: Initialized from UInventoryData (SavedLoadouts populated by InventoryComponent)"),
               *CharacterData->Name);
        return;
    }

    // No Inventory - create empty loadout for manual setup
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::InitializeFromCharacterData] No InventoryComponent found on owner — cannot seed empty loadout"));
        return;
    }
    Inv->SavedLoadouts.Empty();
    FCombatLoadout EmptyLoadout;
    EmptyLoadout.LoadoutName = TEXT("Default");
    EmptyLoadout.InitializeForClass(CharacterClass);
    Inv->SavedLoadouts.Add(EmptyLoadout);
    Inv->ActiveLoadoutIndex = 0;
    bIsReadyForBattle = false;

    UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] %s has no Inventory - created empty loadout. Assign UInventoryData on CharacterData."),
           *CharacterData->Name);

    // Broken Darkness characters get their element-spell pools initialised.
    ApplyBDPoolsIfBroken();
}

void ULoadoutComponent::InitializeBDPools(FCombatLoadout &Loadout)
{
    // One pool per absorbable element other than Darkness. Darkness is the
    // always-available default pool and lives in InnateSpells, not here.
    // Idempotent — existing pools are kept, only missing ones are added.
    for (uint8 i = 0; i <= static_cast<uint8>(ESpellElement::BrokenDarkness); ++i)
    {
        const ESpellElement Elem = static_cast<ESpellElement>(i);
        if (Elem == ESpellElement::Darkness || !UBrokenDarknessManager::CanAbsorbElement(Elem))
        {
            continue;
        }

        const bool bExists = Loadout.BDSpellPools.ContainsByPredicate(
            [Elem](const FBDElementSpellPool &Pool) { return Pool.Element == Elem; });
        if (!bExists)
        {
            FBDElementSpellPool NewPool;
            NewPool.Element = Elem;
            Loadout.BDSpellPools.Add(NewPool);
        }
    }
}

void ULoadoutComponent::ApplyBDPoolsIfBroken()
{
    AActor *OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    UCharacterDataComponent *CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->IsBrokenDarkness())
    {
        return;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ApplyBDPoolsIfBroken] No InventoryComponent found on owner"));
        return;
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        // Pass by reference into the live inventory array so InitializeBDPools'
        // mutations land on the active loadout, not a copy.
        InitializeBDPools(Inv->SavedLoadouts[Inv->ActiveLoadoutIndex]);
    }
}

// ==================== COMBAT ACCESSORS ====================

UWeaponData *ULoadoutComponent::GetActiveWeapon() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
    {
        return nullptr;
    }

    return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
}

UWeaponData *ULoadoutComponent::GetSecondaryWeapon() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondaryWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryRing] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
    {
        return nullptr;
    }

    return Loadout.PrimaryRing.IsValid() ? Loadout.PrimaryRing.RingEntry.Ring : nullptr;
}

URingData *ULoadoutComponent::GetActiveRing() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveRing] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryEvolution] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return nullptr;
    }

    return Loadout.PrimaryEvolution.Crystal;
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

    // Weapons — read crystal from the runtime inventory entry, not the asset.
    // FCombatCapabilities uses the same pattern (WeaponEntry.AttachedCrystal).
    // The asset-side UWeaponData::SlottedCrystal field gets nulled by
    // CombatOrchestrator's combat-end broken-crystal cleanup; the runtime
    // entry preserves the crystal across combats and is the actual source
    // of truth for "what crystal is on this weapon right now."
    if (const FWeaponLoadoutEntry *PrimaryLoadout = GetPrimaryWeaponLoadout())
    {
        AddIfCrystal(PrimaryLoadout->WeaponEntry.AttachedCrystal.Crystal,
                     PrimaryLoadout->WeaponEntry.Weapon);
    }
    if (const FWeaponLoadoutEntry *SecondaryLoadout = GetSecondaryWeaponLoadout())
    {
        AddIfCrystal(SecondaryLoadout->WeaponEntry.AttachedCrystal.Crystal,
                     SecondaryLoadout->WeaponEntry.Weapon);
    }

    // Primary ring (Generic / Caster) — runtime entry
    if (const FRingLoadoutEntry *PrimaryRingLoadout = GetPrimaryRingLoadout())
    {
        AddIfCrystal(PrimaryRingLoadout->RingEntry.AttachedCrystal.Crystal,
                     PrimaryRingLoadout->RingEntry.Ring);
    }

    // Resonator ring loadout — every ring in the array, runtime entry
    const FCombatLoadout ActiveLoadout = GetActiveLoadout();
    for (const FRingLoadoutEntry &Entry : ActiveLoadout.RingLoadout)
    {
        AddIfCrystal(Entry.RingEntry.AttachedCrystal.Crystal, Entry.RingEntry.Ring);
    }

    // Evolution crystal slot — no inventory entry, evolution IS the crystal.
    if (UItemData *Evolution = GetPrimaryEvolution())
    {
        AddIfCrystal(Evolution, Evolution);
    }

    return Result;
}

bool ULoadoutComponent::HasEquippedSourceForElement(AActor *Actor, ESpellElement Element)
{
    if (!Actor)
    {
        return false;
    }

    const ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
    if (!Loadout)
    {
        return false;
    }

    // GetEquippedCrystals already covers weapon/ring crystals and the primary
    // evolution slot. A crystal channels its associated element.
    for (const FEquippedCrystalSlot &Slot : Loadout->GetEquippedCrystals())
    {
        if (Slot.Crystal && Slot.Crystal->GetAssociatedElement() == Element)
        {
            return true;
        }
    }

    return false;
}

EPrimarySlotType ULoadoutComponent::GetPrimarySlotType() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimarySlotType] No InventoryComponent found on owner"));
        return EPrimarySlotType::Weapon;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return EPrimarySlotType::Weapon;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].PrimarySlotType;
}

ESecondarySlotType ULoadoutComponent::GetSecondarySlotType() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondarySlotType] No InventoryComponent found on owner"));
        return ESecondarySlotType::None;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return ESecondarySlotType::None;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].SecondarySlotType;
}

bool ULoadoutComponent::IsShowingPrimary() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::IsShowingPrimary] No InventoryComponent found on owner"));
        return true;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return true;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].bShowPrimary;
}

void ULoadoutComponent::ToggleEquipment()
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ToggleEquipment] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    Loadout.bShowPrimary = !Loadout.bShowPrimary;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Toggled equipment: bShowPrimary = %s"),
           Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
            {
                TurnManager->OnActorSpeedChanged(GetOwner());
            }
        }
    }
}

bool ULoadoutComponent::HasWeaponAccess() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::HasWeaponAccess] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::IsEvolved] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].PrimarySlotType == EPrimarySlotType::Evolution;
}
// ==================== EQUIPMENT STATE HELPERS ====================

bool ULoadoutComponent::HasSecondaryEquipment() const
{
    if (CharacterClass != ECharacterClass::Generic)
    {
        return false;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::HasSecondaryEquipment] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::HasRingInSecondary] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetCurrentStance] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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

    if (ShownWeapon && ShownWeapon->WeaponEntry.Weapon)
    {
        // Strict override: per-loadout StanceOverride wins over the weapon
        // asset's default WeaponStance. nullptr falls through to the asset.
        if (ShownWeapon->StanceOverride)
        {
            return ShownWeapon->StanceOverride;
        }
        if (ShownWeapon->WeaponEntry.Weapon->WeaponStance)
        {
            return ShownWeapon->WeaponEntry.Weapon->WeaponStance;
        }
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
    if (!WeaponEntry)
    {
        return nullptr;
    }

    // Per-loadout attack override takes precedence over the weapon asset's attack.
    if (WeaponEntry->OverrideAttack)
    {
        return WeaponEntry->OverrideAttack;
    }

    return WeaponEntry->WeaponEntry.Weapon ? WeaponEntry->WeaponEntry.Weapon->WeaponAttack : nullptr;
}

UAnimMontage *ULoadoutComponent::GetCurrentAttackMontage() const
{
    UWeaponAttackData *Attack = GetCurrentAttack();
    return Attack ? Attack->AttackMontage : nullptr;
}

// ==================== SPELL ACCESS ====================

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

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetRingResonateSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

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
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
        return nullptr;

    return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
}

const FWeaponLoadoutEntry *ULoadoutComponent::GetSecondaryWeaponLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondaryWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (CharacterClass != ECharacterClass::Generic)
        return nullptr;

    if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
        return nullptr;

    return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetPrimaryRingLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryRingLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
        return nullptr;

    return Loadout.PrimaryRing.RingEntry.Ring != nullptr ? &Loadout.PrimaryRing : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetActiveRingLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveRingLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (CharacterClass != ECharacterClass::Resonator)
        return nullptr;

    if (!Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
        return nullptr;

    const FRingLoadoutEntry &Entry = Loadout.RingLoadout[Loadout.ActiveRingIndex];
    return Entry.IsValid() ? &Entry : nullptr;
}

FCrystalInventoryEntry *ULoadoutComponent::FindCrystalEntryByHolderInternal(UObject *Holder)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::FindCrystalEntryByHolderInternal] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Holder || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Weapon holder — match by Weapon pointer across primary/secondary slots,
    // regardless of PrimarySlotType. A holder lookup is by identity, not slot.
    if (UWeaponData *Weapon = Cast<UWeaponData>(Holder))
    {
        if (Loadout.PrimaryWeapon.WeaponEntry.Weapon == Weapon)
        {
            return &Loadout.PrimaryWeapon.WeaponEntry.AttachedCrystal;
        }
        if (Loadout.SecondaryWeapon.WeaponEntry.Weapon == Weapon)
        {
            return &Loadout.SecondaryWeapon.WeaponEntry.AttachedCrystal;
        }
        return nullptr;
    }

    // Ring holder — primary ring slot (Generic/Caster), then Resonator
    // RingLoadout array.
    if (URingData *Ring = Cast<URingData>(Holder))
    {
        if (Loadout.PrimaryRing.RingEntry.Ring == Ring)
        {
            return &Loadout.PrimaryRing.RingEntry.AttachedCrystal;
        }
        for (FRingLoadoutEntry &Entry : Loadout.RingLoadout)
        {
            if (Entry.RingEntry.Ring == Ring)
            {
                return &Entry.RingEntry.AttachedCrystal;
            }
        }
        return nullptr;
    }

    return nullptr;
}

FCrystalInventoryEntry ULoadoutComponent::GetCrystalEntryByHolder(UObject *Holder) const
{
    // const_cast: the internal helper is non-const because it's shared with
    // the mutating wrappers below. Get does not mutate — it takes a copy.
    FCrystalInventoryEntry *Entry = const_cast<ULoadoutComponent *>(this)->FindCrystalEntryByHolderInternal(Holder);
    return Entry ? *Entry : FCrystalInventoryEntry();
}

void ULoadoutComponent::ResetCrystalEntryByHolder(UObject *Holder)
{
    if (FCrystalInventoryEntry *Entry = FindCrystalEntryByHolderInternal(Holder))
    {
        *Entry = FCrystalInventoryEntry();
    }
}

int32 ULoadoutComponent::RepairCrystalEntryByHolder(UObject *Holder, int32 Amount)
{
    FCrystalInventoryEntry *Entry = FindCrystalEntryByHolderInternal(Holder);
    if (!Entry)
    {
        return -1;
    }
    Entry->RepairBetweenCombats(Amount);
    return Entry->CurrentDurability;
}

bool ULoadoutComponent::ApplyWearToCrystalEntryByHolder(UObject *Holder, int32 Amount)
{
    FCrystalInventoryEntry *Entry = FindCrystalEntryByHolderInternal(Holder);
    if (!Entry)
    {
        return false;
    }
    return Entry->ApplyWear(Amount);
}

void ULoadoutComponent::SetActiveRingIndex(int32 NewIndex)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::SetActiveRingIndex] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (!Loadout.RingLoadout.IsValidIndex(NewIndex))
    {
        return;
    }

    Loadout.ActiveRingIndex = NewIndex;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Set ActiveRingIndex to %d"), NewIndex);

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
            {
                TurnManager->OnActorSpeedChanged(GetOwner());
            }
        }
    }
}

void ULoadoutComponent::DebugLogLoadout()
{
    UE_LOG(LogTemp, Log, TEXT("=== LoadoutComponent Debug ==="));
    UE_LOG(LogTemp, Log, TEXT("CharacterClass: %s"), *UEnum::GetValueAsString(CharacterClass));
    UE_LOG(LogTemp, Log, TEXT("bIsReadyForBattle: %s"), bIsReadyForBattle ? TEXT("true") : TEXT("false"));

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DebugLogLoadout] No InventoryComponent found on owner"));
        UE_LOG(LogTemp, Log, TEXT("=============================="));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("ActiveLoadoutIndex: %d"), Inv->ActiveLoadoutIndex);
    UE_LOG(LogTemp, Log, TEXT("SavedLoadouts: %d"), Inv->SavedLoadouts.Num());

    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        UE_LOG(LogTemp, Log, TEXT("Active Loadout: %s"), *Loadout.LoadoutName);
        UE_LOG(LogTemp, Log, TEXT("  PrimarySlotType: %s"), *UEnum::GetValueAsString(Loadout.PrimarySlotType));
        UE_LOG(LogTemp, Log, TEXT("  bShowPrimary: %s"), Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));
    }

    UE_LOG(LogTemp, Log, TEXT("=============================="));
}

bool ULoadoutComponent::ShouldUseWeaponParry() const
{
    // Read the per-weapon loadout-entry flag for the active weapon.
    if (const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout())
    {
        return WeaponEntry->bUseWeaponParryAnimation;
    }
    return false;
}

UAnimMontage *ULoadoutComponent::GetDodgeLeftMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->DodgeLeftMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetDodgeRightMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->DodgeRightMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetBlockMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->BlockMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetParryMontage() const
{
    // Weapon-entry preference for the weapon's own parry animation.
    if (ShouldUseWeaponParry())
    {
        if (UWeaponData *Weapon = GetActiveWeapon())
        {
            if (Weapon->ParryMontage)
            {
                return Weapon->ParryMontage;
            }
        }
    }

    // Fall back to the character's parry montage from the Cosmetics asset.
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->ParryMontage : nullptr;
}

UStanceData *ULoadoutComponent::GetUnarmedStance() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->UnarmedStance : nullptr;
}

UInfusionDisplayData *ULoadoutComponent::GetInfusionDisplay() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->InfusionDisplay : nullptr;
}

UAnimMontage *ULoadoutComponent::GetRingSwitchMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->RingSwitchMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetItemUseAnimation(bool bIsSelfTarget) const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    if (!CharData->Cosmetics)
    {
        return nullptr;
    }
    return bIsSelfTarget ? CharData->Cosmetics->ItemUseSelfMontage : CharData->Cosmetics->ItemUseTargetMontage;
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

UInventoryComponent *ULoadoutComponent::GetInventoryComponent() const
{
    if (!GetOwner())
    {
        return nullptr;
    }
    return GetOwner()->FindComponentByClass<UInventoryComponent>();
}

// ==================== EQUIPMENT STAT QUERIES ====================

namespace
{
    /** Field-wise accumulator for FEquipmentStatBonus. The result is a
     *  read-only snapshot consumed by stat queries, not a roll target. */
    void AccumulateBonus(FEquipmentStatBonus &Out, const FEquipmentStatBonus &In)
    {
        Out.BonusRawDamage             += In.BonusRawDamage;
        Out.BonusSpellDamage           += In.BonusSpellDamage;
        Out.BonusEfficiency            += In.BonusEfficiency;
        Out.BonusStatusMultiplier      += In.BonusStatusMultiplier;
        Out.BonusCritChance            += In.BonusCritChance;
        Out.BonusSpellSpeed            += In.BonusSpellSpeed;
        Out.BonusDefense               += In.BonusDefense;
        Out.BonusActionSpeed           += In.BonusActionSpeed;
        Out.BonusMaxHP                 += In.BonusMaxHP;
        Out.BonusMaxEnergy             += In.BonusMaxEnergy;
        Out.BonusResistance            += In.BonusResistance;
        Out.BonusTurnSpeed             += In.BonusTurnSpeed;
        Out.BonusLuck                  += In.BonusLuck;
        Out.BonusMindModifierPercent   += In.BonusMindModifierPercent;
        Out.BonusBodyModifierPercent   += In.BonusBodyModifierPercent;
        Out.BonusSpiritModifierPercent += In.BonusSpiritModifierPercent;
    }
}

FEquipmentStatBonus ULoadoutComponent::GetActiveStatBonus(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    FEquipmentStatBonus Combined;

    const FCombatLoadout Loadout = GetActiveLoadout();

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
    {
        const bool bHasSecondaryWeapon =
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid();

        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (bHasSecondaryWeapon)
            {
                // Dual weapon — bShowPrimary picks which one is active.
                if (const FWeaponLoadoutEntry *Active = GetActiveWeaponLoadout())
                {
                    Combined = Active->WeaponEntry.StatBonus;
                }
            }
            else if (Loadout.PrimaryWeapon.IsValid())
            {
                Combined = Loadout.PrimaryWeapon.WeaponEntry.StatBonus;
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            // Ring primary; weapon-secondary (if any) sums in.
            if (Loadout.PrimaryRing.IsValid())
            {
                AccumulateBonus(Combined, Loadout.PrimaryRing.RingEntry.StatBonus);
            }
            if (bHasSecondaryWeapon)
            {
                AccumulateBonus(Combined, Loadout.SecondaryWeapon.WeaponEntry.StatBonus);
            }
        }
        // Evolution primary slots have no StatBonus on the crystal itself —
        // their contribution flows through GetActivePrimaryEvolutionCrystal.
        break;
    }

    case ECharacterClass::Resonator:
    {
        // Active ring (from RingLoadout) + primary weapon (if equipped).
        if (const FRingLoadoutEntry *ActiveRing = GetActiveRingLoadout())
        {
            AccumulateBonus(Combined, ActiveRing->RingEntry.StatBonus);
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            AccumulateBonus(Combined, Loadout.PrimaryWeapon.WeaponEntry.StatBonus);
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            Combined = Loadout.PrimaryWeapon.WeaponEntry.StatBonus;
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring &&
                 Loadout.PrimaryRing.IsValid())
        {
            Combined = Loadout.PrimaryRing.RingEntry.StatBonus;
        }
        break;
    }
    }

    return Combined;
}

TArray<FSkillEffect> ULoadoutComponent::GetActiveEffects(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    TArray<FSkillEffect> Result;

    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
    {
        if (UWeaponData *Weapon = GetActiveWeapon())
        {
            Result.Append(Weapon->Effects);
        }
        break;
    }

    case ECharacterClass::Resonator:
    {
        if (URingData *Ring = GetActiveRing())
        {
            Result.Append(Ring->Effects);
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        // Primary slot only (weapon OR ring, not both).
        if (UWeaponData *Weapon = GetPrimaryWeapon())
        {
            Result.Append(Weapon->Effects);
        }
        else if (URingData *Ring = GetPrimaryRing())
        {
            Result.Append(Ring->Effects);
        }
        break;
    }
    }

    return Result;
}

UItemData *ULoadoutComponent::GetActivePrimaryEvolutionCrystal(AActor *Actor) const
{
    (void)Actor;

    const FCombatLoadout Loadout = GetActiveLoadout();
    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
    {
        return nullptr;
    }
    if (!Loadout.PrimaryWeapon.IsValid())
    {
        return nullptr;
    }

    UItemData *Crystal = Loadout.PrimaryWeapon.WeaponEntry.AttachedCrystal.Crystal;
    if (!Crystal || !Crystal->bIsEvolutionCrystal)
    {
        return nullptr;
    }
    return Crystal;
}
