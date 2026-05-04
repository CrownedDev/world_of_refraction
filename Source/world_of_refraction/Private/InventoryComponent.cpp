// InventoryComponent.cpp
// Character inventory storage and management implementation

#include "InventoryComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "WeaponData.h"
#include "RingData.h"
#include "ItemData.h"
#include "LoadoutData.h"
#include "CrystalType.h"
#include "CharacterData.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ==================== SPELL OPERATIONS ====================

bool UInventoryComponent::LearnSpell(USpellData *Spell)
{
    return Spells.LearnSpell(Spell);
}

bool UInventoryComponent::UnlearnSpell(USpellData *Spell)
{
    return Spells.UnlearnSpell(Spell);
}

bool UInventoryComponent::HasSpell(USpellData *Spell) const
{
    return Spells.HasSpell(Spell);
}

TArray<USpellData *> UInventoryComponent::GetSpellsByElement(ESpellElement Element) const
{
    return Spells.GetSpellsByElement(Element);
}

// ==================== ABILITY OPERATIONS ====================

bool UInventoryComponent::LearnAbility(UAbilityData *Ability)
{
    return Abilities.LearnAbility(Ability);
}

bool UInventoryComponent::UnlearnAbility(UAbilityData *Ability)
{
    return Abilities.UnlearnAbility(Ability);
}

bool UInventoryComponent::HasAbility(UAbilityData *Ability) const
{
    return Abilities.HasAbility(Ability);
}

TArray<UAbilityData *> UInventoryComponent::GetAbilitiesForWeaponType(EWeaponType WeaponType) const
{
    return Abilities.GetAbilitiesForWeaponType(WeaponType);
}

// ==================== WEAPON OPERATIONS ====================

bool UInventoryComponent::AddWeapon(UWeaponData *Weapon, bool bCopyDefaultCrystal)
{
    if (!Weapon)
    {
        return false;
    }

    // Check capacity
    if (!CanAddWeapon(Weapon))
    {
        return false;
    }

    FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon, bCopyDefaultCrystal);
    Weapons.Add(Entry);
    return true;
}

bool UInventoryComponent::RemoveWeapon(int32 WeaponIndex)
{
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return false;
    }

    Weapons.RemoveAt(WeaponIndex);
    return true;
}

FWeaponInventoryEntry UInventoryComponent::GetWeaponAt(int32 Index) const
{
    if (Weapons.IsValidIndex(Index))
    {
        return Weapons[Index];
    }
    return FWeaponInventoryEntry();
}

int32 UInventoryComponent::GetWeaponSlotCostTotal() const
{
    int32 Total = 0;
    for (const FWeaponInventoryEntry &Entry : Weapons)
    {
        Total += Entry.GetSlotCost();
    }
    return Total;
}

int32 UInventoryComponent::GetRemainingWeaponCapacity() const
{
    return InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS - GetWeaponSlotCostTotal();
}

bool UInventoryComponent::CanAddWeapon(UWeaponData *Weapon) const
{
    if (!Weapon)
    {
        return false;
    }

    // Base weapon costs 1 slot
    int32 Cost = InventoryConstants::WEAPON_BASE_SLOT_COST;
    return GetRemainingWeaponCapacity() >= Cost;
}

// ==================== WEAPON CRYSTAL OPERATIONS ====================

bool UInventoryComponent::AttachCrystalToWeapon(int32 WeaponIndex, UItemData *Crystal)
{
    if (!Weapons.IsValidIndex(WeaponIndex) || !Crystal || !Crystal->CanBeSlotted())
    {
        return false;
    }

    FWeaponInventoryEntry &Entry = Weapons[WeaponIndex];

    // Check if adding crystal would exceed capacity
    int32 CurrentCost = Entry.GetSlotCost();
    Entry.AttachCrystal(Crystal);
    int32 NewCost = Entry.GetSlotCost();

    int32 CostDelta = NewCost - CurrentCost;
    if (GetRemainingWeaponCapacity() < CostDelta)
    {
        // Revert
        return false;
    }

    return true;
}

UItemData *UInventoryComponent::RemoveCrystalFromWeapon(int32 WeaponIndex)
{
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return nullptr;
    }

    UItemData *OldCrystal = Weapons[WeaponIndex].AttachedCrystal.Crystal;
    Weapons[WeaponIndex].RemoveCrystal();
    return OldCrystal;
}

bool UInventoryComponent::ApplyEvolutionToWeapon(int32 WeaponIndex, UItemData *EvolutionCrystal)
{
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return false;
    }

    if (!EvolutionCrystal || !EvolutionCrystal->CanBeSlotted() || !EvolutionCrystal->GrantsEvolution())
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplyEvolutionToWeapon: Crystal must be refined and grant evolution"));
        return false;
    }

    FWeaponInventoryEntry &Entry = Weapons[WeaponIndex];

    int32 CurrentCost = Entry.GetSlotCost();
    FCrystalInventoryEntry OldCrystalEntry = Entry.AttachedCrystal;

    Entry.AttachCrystal(EvolutionCrystal);
    int32 NewCost = Entry.GetSlotCost();

    int32 CostDelta = NewCost - CurrentCost;
    if (GetRemainingWeaponCapacity() < CostDelta)
    {
        Entry.AttachedCrystal = OldCrystalEntry;
        return false;
    }

    return true;
}

// ==================== RING OPERATIONS ====================

bool UInventoryComponent::AddRing(URingData *Ring, bool bCopyDefaultCrystal)
{
    if (!Ring)
    {
        return false;
    }

    // Check capacity
    if (!CanAddRing(Ring))
    {
        return false;
    }

    FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring, bCopyDefaultCrystal);
    Rings.Add(Entry);
    return true;
}

bool UInventoryComponent::RemoveRing(int32 RingIndex)
{
    if (!Rings.IsValidIndex(RingIndex))
    {
        return false;
    }

    Rings.RemoveAt(RingIndex);
    return true;
}

FRingInventoryEntry UInventoryComponent::GetRingAt(int32 Index) const
{
    if (Rings.IsValidIndex(Index))
    {
        return Rings[Index];
    }
    return FRingInventoryEntry();
}

int32 UInventoryComponent::GetRingSlotCostTotal() const
{
    int32 Total = 0;
    for (const FRingInventoryEntry &Entry : Rings)
    {
        Total += Entry.GetSlotCost();
    }
    return Total;
}

int32 UInventoryComponent::GetRemainingRingCapacity() const
{
    return InventoryConstants::MAX_RING_INVENTORY_SLOTS - GetRingSlotCostTotal();
}

bool UInventoryComponent::CanAddRing(URingData *Ring) const
{
    if (!Ring)
    {
        return false;
    }

    // Base ring costs 1 slot
    int32 Cost = InventoryConstants::RING_BASE_SLOT_COST;
    return GetRemainingRingCapacity() >= Cost;
}

// ==================== RING CRYSTAL OPERATIONS ====================

bool UInventoryComponent::AttachCrystalToRing(int32 RingIndex, UItemData *Crystal)
{
    if (!Rings.IsValidIndex(RingIndex) || !Crystal || !Crystal->CanBeSlotted())
    {
        return false;
    }

    Rings[RingIndex].AttachCrystal(Crystal);
    return true;
}

UItemData *UInventoryComponent::RemoveCrystalFromRing(int32 RingIndex)
{
    if (!Rings.IsValidIndex(RingIndex))
    {
        return nullptr;
    }

    UItemData *OldCrystal = Rings[RingIndex].AttachedCrystal.Crystal;
    Rings[RingIndex].RemoveCrystal();
    return OldCrystal;
}

bool UInventoryComponent::ApplyEvolutionToRing(int32 RingIndex, UItemData *EvolutionCrystal)
{
    if (!Rings.IsValidIndex(RingIndex))
    {
        return false;
    }

    if (!EvolutionCrystal || !EvolutionCrystal->CanBeSlotted() || !EvolutionCrystal->GrantsEvolution())
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplyEvolutionToRing: Crystal must be refined and grant evolution"));
        return false;
    }

    FRingInventoryEntry &Entry = Rings[RingIndex];

    int32 CurrentCost = Entry.GetSlotCost();
    FCrystalInventoryEntry OldCrystalEntry = Entry.AttachedCrystal;

    Entry.AttachCrystal(EvolutionCrystal);
    int32 NewCost = Entry.GetSlotCost();

    int32 CostDelta = NewCost - CurrentCost;
    if (GetRemainingRingCapacity() < CostDelta)
    {
        Entry.AttachedCrystal = OldCrystalEntry;
        return false;
    }

    return true;
}

// ==================== ITEM OPERATIONS ====================

bool UInventoryComponent::AddItem(UItemData *Item)
{
    return Items.AddCrystal(Item);
}

bool UInventoryComponent::RemoveItem(UItemData *Item)
{
    return Items.RemoveCrystal(Item);
}

bool UInventoryComponent::HasItem(UItemData *Item) const
{
    return Items.HasCrystal(Item);
}

TArray<UItemData *> UInventoryComponent::GetItemsByType(ECrystalType Type) const
{
    return Items.GetCrystalsOfType(Type);
}

TArray<UItemData *> UInventoryComponent::GetItemsByTier(EItemTier Tier) const
{
    return Items.GetCrystalsOfTier(Tier);
}

// ==================== EVOLUTION HELPERS ====================

TArray<UItemData *> UInventoryComponent::GetEvolutionCrystals() const
{
    return Items.GetEvolutionCrystals();
}

TArray<UItemData *> UInventoryComponent::GetEvolutionCrystalsByElement(ESpellElement Element) const
{
    TArray<UItemData *> Result;

    TArray<UItemData *> AllEvolutions = GetEvolutionCrystals();
    for (UItemData *Crystal : AllEvolutions)
    {
        if (Crystal && Crystal->GetAssociatedElement() == Element)
        {
            Result.Add(Crystal);
        }
    }

    return Result;
}

// ==================== UTILITY ====================

void UInventoryComponent::ClearAll()
{
    Spells.Clear();
    Abilities.Clear();
    Weapons.Empty();
    Rings.Empty();
    Items.Clear();
}

FString UInventoryComponent::GetInventorySummary() const
{
    int32 EvolutionCount = GetEvolutionCrystals().Num();

    return FString::Printf(
        TEXT("Inventory Summary:\n")
            TEXT("  Spells: %d/%d\n")
                TEXT("  Abilities: %d/%d\n")
                    TEXT("  Weapons: %d (cost %d/%d)\n")
                        TEXT("  Rings: %d (cost %d/%d)\n")
                            TEXT("  Items: %d/%d\n")
                                TEXT("  Evolution Crystals: %d"),
        Spells.GetCount(), InventoryConstants::MAX_LEARNED_SPELLS,
        Abilities.GetCount(), InventoryConstants::MAX_LEARNED_ABILITIES,
        Weapons.Num(), GetWeaponSlotCostTotal(), InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS,
        Rings.Num(), GetRingSlotCostTotal(), InventoryConstants::MAX_RING_INVENTORY_SLOTS,
        Items.GetTotalCount(), InventoryConstants::ITEM_CAPACITY_TOTAL,
        EvolutionCount);
}

void UInventoryComponent::InitializeFromCharacterData(UCharacterData *CharacterData)
{
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeFromCharacterData: Null CharacterData"));
        return;
    }

    // Clear existing inventory
    Spells.LearnedSpells.Empty();
    Abilities.LearnedAbilities.Empty();
    Weapons.Empty();
    Rings.Empty();
    Items.Clear();

    // Add weapons
    // Lines 425-437 - Replace entire section with:
    ULoadoutData *DefaultLoadoutAsset = CharacterData->DefaultLoadout;
    if (DefaultLoadoutAsset)
    {
        if (DefaultLoadoutAsset->PrimaryWeapon)
        {
            AddWeapon(DefaultLoadoutAsset->PrimaryWeapon, true);
        }
        if (DefaultLoadoutAsset->SecondaryWeapon)
        {
            AddWeapon(DefaultLoadoutAsset->SecondaryWeapon, true);
        }
        if (DefaultLoadoutAsset->PrimaryRing)
        {
            AddRing(DefaultLoadoutAsset->PrimaryRing, true);
        }
    }

    if (DefaultLoadoutAsset && DefaultLoadoutAsset->PrimaryWeapon)
    {
        for (UAbilityData *Ability : DefaultLoadoutAsset->PrimaryWeapon->PresetAbilities)
        {
            if (Ability)
            {
                Abilities.LearnAbility(Ability);
            }
        }
    }
    if (DefaultLoadoutAsset && DefaultLoadoutAsset->SecondaryWeapon)
    {
        for (UAbilityData *Ability : DefaultLoadoutAsset->SecondaryWeapon->PresetAbilities)
        {
            if (Ability)
            {
                Abilities.LearnAbility(Ability);
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Initialized inventory from %s: %d weapons, %d rings, %d spells, %d abilities"),
           *CharacterData->CharacterName,
           Weapons.Num(),
           Rings.Num(),
           Spells.GetCount(),
           Abilities.GetCount());
}

#if WITH_EDITOR
void UInventoryComponent::DebugLogInventory()
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *GetInventorySummary());

    // Log weapons
    UE_LOG(LogTemp, Display, TEXT("--- Weapons ---"));
    for (int32 i = 0; i < Weapons.Num(); ++i)
    {
        const FWeaponInventoryEntry &W = Weapons[i];
        FString WeaponName = W.Weapon ? W.Weapon->WeaponName : TEXT("NULL");
        FString CrystalStr = W.HasCrystal() ? TEXT("Crystal") : TEXT("None");
        FString EvoStr = W.IsEvolved() ? TEXT("Evolved") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s %s) Cost:%d"),
               i, *WeaponName, *CrystalStr, *EvoStr, W.GetSlotCost());
    }

    // Log rings
    UE_LOG(LogTemp, Display, TEXT("--- Rings ---"));
    for (int32 i = 0; i < Rings.Num(); ++i)
    {
        const FRingInventoryEntry &R = Rings[i];
        FString RingName = R.Ring ? R.Ring->RingName : TEXT("NULL");
        FString CrystalStr = R.HasCrystal() ? TEXT("Crystal") : TEXT("None");
        FString EvoStr = R.IsEvolved() ? TEXT("Evolved") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s %s) Cost:%d"),
               i, *RingName, *CrystalStr, *EvoStr, R.GetSlotCost());
    }
}
#endif
