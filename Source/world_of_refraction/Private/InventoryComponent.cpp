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
#include "InventoryData.h"
#include "FSavedLoadout.h"

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

    // Both-set authoring diagnostic: Inventory wins; DefaultLoadout will be ignored.
    if (CharacterData->Inventory && CharacterData->DefaultLoadout)
    {
        UE_LOG(LogTemp, Display,
               TEXT("[InventoryComponent] Character '%s' has both Inventory and DefaultLoadout set — Inventory takes precedence; DefaultLoadout ignored"),
               *CharacterData->Name);
    }

    // New path (UInventoryData) — takes over the entire init if set.
    if (CharacterData->Inventory)
    {
        InitializeFromInventoryAsset(CharacterData);
        return;
    }

    // Legacy path — migration warning fires once per init until the character
    // is migrated to UInventoryData (and the warning self-removes in commit 5).
    UE_LOG(LogTemp, Warning,
           TEXT("[InventoryComponent] Character '%s' falling back to DefaultLoadout — needs migration to UInventoryData"),
           *CharacterData->Name);

    // Clear existing inventory
    Spells.LearnedSpells.Empty();
    Abilities.LearnedAbilities.Empty();
    Weapons.Empty();
    Rings.Empty();
    Items.Clear();

    ULoadoutData *DefaultLoadoutAsset = CharacterData->DefaultLoadout;
    if (!DefaultLoadoutAsset)
    {
        UE_LOG(LogTemp, Display, TEXT("InitializeFromCharacterData(%s): No DefaultLoadout set"),
               *CharacterData->Name);
        return;
    }

    // ---------- Weapons ----------
    if (DefaultLoadoutAsset->PrimaryWeapon)
    {
        AddWeapon(DefaultLoadoutAsset->PrimaryWeapon, true);
    }
    if (DefaultLoadoutAsset->SecondaryWeapon)
    {
        AddWeapon(DefaultLoadoutAsset->SecondaryWeapon, true);
    }

    // ---------- Rings ----------
    if (DefaultLoadoutAsset->PrimaryRing)
    {
        AddRing(DefaultLoadoutAsset->PrimaryRing, true);
    }
    for (const FResonatorRingSlot &Slot : DefaultLoadoutAsset->EquippedRings)
    {
        if (Slot.Ring)
        {
            AddRing(Slot.Ring, true);
        }
    }

    // ---------- Items ----------
    // Consumables first, evolution crystal last so it lands at a predictable
    // tail position in its tier array. AddItem fails only on tier-cap overflow
    // (no dedup), so a false return after the null filter is a capacity drop.
    for (UItemData *Item : DefaultLoadoutAsset->EquippedItems)
    {
        if (Item && !AddItem(Item))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeFromCharacterData(%s): item capacity full, dropping %s"),
                   *CharacterData->Name, *Item->ItemName);
        }
    }
    if (DefaultLoadoutAsset->PrimaryEvolution && !AddItem(DefaultLoadoutAsset->PrimaryEvolution))
    {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeFromCharacterData(%s): item capacity full, dropping evolution crystal %s"),
               *CharacterData->Name, *DefaultLoadoutAsset->PrimaryEvolution->ItemName);
    }

    // ---------- Spells ----------
    // Innate → Evolution → BD pools. LearnSpell short-circuits on null,
    // capacity, OR duplicate; HasSpell discriminates dedup (silent) from
    // capacity drop (warn).
    for (USpellData *Spell : DefaultLoadoutAsset->InnateSpells)
    {
        if (Spell && !LearnSpell(Spell) && !HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeFromCharacterData(%s): spell capacity full, dropping %s"),
                   *CharacterData->Name, *Spell->Name);
        }
    }
    for (USpellData *Spell : DefaultLoadoutAsset->EvolutionSpells)
    {
        if (Spell && !LearnSpell(Spell) && !HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeFromCharacterData(%s): spell capacity full, dropping %s"),
                   *CharacterData->Name, *Spell->Name);
        }
    }
    for (const FBDElementSpellPool &Pool : DefaultLoadoutAsset->BDSpellPools)
    {
        for (USpellData *Spell : Pool.Spells)
        {
            if (Spell && !LearnSpell(Spell) && !HasSpell(Spell))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("InitializeFromCharacterData(%s): spell capacity full, dropping %s"),
                       *CharacterData->Name, *Spell->Name);
            }
        }
    }

    // ---------- Abilities ----------
    // Loadout-listed abilities before weapon PresetAbilities so overlap dedup
    // logs read as "PresetAbilities subset already known" rather than the
    // reverse. Same false-return-and-not-known capacity discrimination as
    // spells above.
    for (UAbilityData *Ability : DefaultLoadoutAsset->PrimaryWeaponAbilities)
    {
        if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeFromCharacterData(%s): ability capacity full, dropping %s"),
                   *CharacterData->Name, *Ability->Name);
        }
    }
    for (UAbilityData *Ability : DefaultLoadoutAsset->SecondaryWeaponAbilities)
    {
        if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeFromCharacterData(%s): ability capacity full, dropping %s"),
                   *CharacterData->Name, *Ability->Name);
        }
    }
    if (DefaultLoadoutAsset->PrimaryWeapon)
    {
        for (UAbilityData *Ability : DefaultLoadoutAsset->PrimaryWeapon->PresetAbilities)
        {
            if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("InitializeFromCharacterData(%s): ability capacity full, dropping %s"),
                       *CharacterData->Name, *Ability->Name);
            }
        }
    }
    if (DefaultLoadoutAsset->SecondaryWeapon)
    {
        for (UAbilityData *Ability : DefaultLoadoutAsset->SecondaryWeapon->PresetAbilities)
        {
            if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("InitializeFromCharacterData(%s): ability capacity full, dropping %s"),
                       *CharacterData->Name, *Ability->Name);
            }
        }
    }

    UE_LOG(LogTemp, Display,
           TEXT("Initialized inventory from %s: %d weapons, %d rings, %d items, %d spells, %d abilities"),
           *CharacterData->Name,
           Weapons.Num(),
           Rings.Num(),
           Items.GetTotalCount(),
           Spells.GetCount(),
           Abilities.GetCount());
}

void UInventoryComponent::InitializeFromInventoryAsset(UCharacterData *CharacterData)
{
    UInventoryData *InventoryAsset = CharacterData ? CharacterData->Inventory : nullptr;
    if (!InventoryAsset)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset: null Inventory asset (CharacterData=%s)"),
               CharacterData ? *CharacterData->Name : TEXT("null"));
        return;
    }

    // Validate before population — surfaces ownership/loadout cross-check
    // failures alongside the init log rather than as downstream symptoms.
    for (const FString &Error : InventoryAsset->GetValidationErrors())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryData] '%s' validation: %s"),
               *InventoryAsset->GetName(), *Error);
    }

    // Clear ownership lists (same shape as legacy path).
    Spells.LearnedSpells.Empty();
    Abilities.LearnedAbilities.Empty();
    Weapons.Empty();
    Rings.Empty();
    Items.Clear();

    // ---------- Weapons ----------
    for (UWeaponData *Weapon : InventoryAsset->Weapons)
    {
        if (Weapon)
        {
            AddWeapon(Weapon, true);
        }
    }

    // ---------- Rings ----------
    for (URingData *Ring : InventoryAsset->Rings)
    {
        if (Ring)
        {
            AddRing(Ring, true);
        }
    }

    // ---------- Items (Crystals + consumables share FItemCrystalInventory) ----------
    // Mirror the legacy path's capacity-drop warning so authoring mistakes
    // surface in the log instead of silently dropping at runtime.
    for (UItemData *Crystal : InventoryAsset->Crystals)
    {
        if (Crystal && !AddItem(Crystal))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): item capacity full, dropping crystal %s"),
                   *CharacterData->Name, *Crystal->ItemName);
        }
    }
    for (UItemData *Item : InventoryAsset->Items)
    {
        if (Item && !AddItem(Item))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): item capacity full, dropping %s"),
                   *CharacterData->Name, *Item->ItemName);
        }
    }

    // ---------- Spells ----------
    // LearnSpell short-circuits on null/capacity/duplicate; HasSpell
    // discriminates dedup (silent) from capacity drop (warn).
    for (USpellData *Spell : InventoryAsset->Spells)
    {
        if (Spell && !LearnSpell(Spell) && !HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): spell capacity full, dropping %s"),
                   *CharacterData->Name, *Spell->Name);
        }
    }

    // ---------- Abilities ----------
    // Explicit-only — no implicit seeding from weapon PresetAbilities per
    // locked decision. Designer must list every ability in
    // UInventoryData::Abilities.
    for (UAbilityData *Ability : InventoryAsset->Abilities)
    {
        if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): ability capacity full, dropping %s"),
                   *CharacterData->Name, *Ability->Name);
        }
    }

    // ---------- SavedLoadouts ----------
    // Inflate each FSavedLoadout into a runtime FCombatLoadout. Stance
    // overrides flow through CreateFromSavedLoadout (commit 1.5).
    SavedLoadouts.Empty();
    for (const FSavedLoadout &SavedLoadout : InventoryAsset->SavedLoadouts)
    {
        SavedLoadouts.Add(FCombatLoadout::CreateFromSavedLoadout(SavedLoadout));
    }

    // Active index — clamp to valid range. Empty SavedLoadouts is soft-fail
    // (legacy parity); LoadoutComponent will seed an empty default downstream.
    if (SavedLoadouts.Num() > 0)
    {
        ActiveLoadoutIndex = FMath::Clamp(InventoryAsset->DefaultActiveLoadoutIndex, 0, SavedLoadouts.Num() - 1);
    }
    else
    {
        ActiveLoadoutIndex = 0;
    }

    UE_LOG(LogTemp, Display,
           TEXT("[InventoryComponent] Initialized inventory from %s (new path): %d weapons, %d rings, %d items, %d spells, %d abilities, %d loadouts (active=%d)"),
           *InventoryAsset->GetName(),
           Weapons.Num(),
           Rings.Num(),
           Items.GetTotalCount(),
           Spells.GetCount(),
           Abilities.GetCount(),
           SavedLoadouts.Num(),
           ActiveLoadoutIndex);
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
        FString WeaponName = W.Weapon ? W.Weapon->Name : TEXT("NULL");
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
        FString RingName = R.Ring ? R.Ring->Name : TEXT("NULL");
        FString CrystalStr = R.HasCrystal() ? TEXT("Crystal") : TEXT("None");
        FString EvoStr = R.IsEvolved() ? TEXT("Evolved") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s %s) Cost:%d"),
               i, *RingName, *CrystalStr, *EvoStr, R.GetSlotCost());
    }
}
#endif
