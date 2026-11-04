// PoolSubsystem.cpp
// Phase 1c (all owned types, INERT) implementation. See PoolSubsystem.h.

#include "Pool/PoolSubsystem.h"
#include "Equipment/Rings/RingData.h"

#include "Pool/PoolAccessors.h"             // the filter/translation layer
#include "Equipment/Crystals/CrystalType.h" // StaticEnum<ECrystalType> for the debug dump
#include "Equipment/Crystals/CrystalTypeHelpers.h" // IsGemType — item/refined gem/stone dispatch
#include "Inventory/ItemTier.h"             // TierHelpers::GetTierName
#include "Engine/Engine.h"                  // GEngine on-screen debug
#include "Misc/Paths.h"                     // FPaths::GetBaseFilename for the wearer dump
#include "Equipment/Crystals/ItemIdentity.h"  // item names for the slot dump

// --- Populate (authored → pool): the asset + the factories/types the populate dereferences ---
#include "Inventory/InventoryData.h"               // UInventoryData (the authored source)
#include "Skills/Definitions/SpellData.h"          // USpellData::Tier
#include "Skills/Definitions/AbilityData.h"        // UAbilityData::Tier / IsAttack
#include "Equipment/Crystals/EvolutionItemData.h"  // UEvolutionItemData::Tier / MaxDurability

// --- Debug console (wor.* commands): world → game instance → subsystem, + the played-character source ---
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Character/CharacterData.h"            // UCharacterData::Inventory
#include "Character/CharacterDataComponent.h"   // UCharacterDataComponent::CharacterData
#include "Inventory/InventoryComponent.h"       // wor.DrawFromPool target (step 2)

namespace
{
    constexpr float POOL_DEBUG_SCREEN_DURATION = 12.0f;

    FString EnumName(const UEnum *EnumPtr, int64 Value)
    {
        return EnumPtr ? EnumPtr->GetAuthoredNameStringByValue(Value) : FString::FromInt(static_cast<int32>(Value));
    }
}

// ==================== ARMOURY ====================

namespace
{
    const FGuid &WeaponIdOf(const FWeaponInventoryEntry &Entry) { return Entry.PersistentID; }
    const FGuid &AdapterIdOf(const FRingInventoryEntry &Entry) { return Entry.PersistentID; }
    const FGuid &EvolutionIdOf(const FEvolutionInventoryEntry &Entry) { return Entry.InstanceID; }
}

void UPoolSubsystem::RebuildGearIndex() const
{
    WeaponIndex.Reset();
    AdapterIndex.Reset();
    EvolutionIndex.Reset();
    // The first entry with an ID wins, as the old scan found it; a second with the same ID is a data fault.
    const auto AddFirst = [](TMap<FGuid, int32> &Map, const FGuid &ID, int32 Index, const TCHAR *Kind)
    {
        if (Map.Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Pool] two %ss share the ID %s; lookups find the first"), Kind, *ID.ToString());
            return;
        }
        Map.Add(ID, Index);
    };
    for (int32 Index = 0; Index < OwnedWeapons.Num(); ++Index)
    {
        AddFirst(WeaponIndex, OwnedWeapons[Index].PersistentID, Index, TEXT("weapon"));
    }
    for (int32 Index = 0; Index < OwnedRings.Num(); ++Index)
    {
        AddFirst(AdapterIndex, OwnedRings[Index].PersistentID, Index, TEXT("adapter"));
    }
    for (int32 Index = 0; Index < OwnedEvolutions.Num(); ++Index)
    {
        AddFirst(EvolutionIndex, OwnedEvolutions[Index].InstanceID, Index, TEXT("evolution"));
    }
    bGearIndexDirty = false;
}

template <class TEntry, class FIdOf>
int32 UPoolSubsystem::FindIndexed(const TArray<TEntry> &Entries, const TMap<FGuid, int32> &Index, const FGuid &ID, FIdOf IdOf) const
{
    if (!ID.IsValid())
    {
        return INDEX_NONE;
    }
    if (bGearIndexDirty)
    {
        RebuildGearIndex();
    }
    const int32 *Found = Index.Find(ID);
    if (!Found)
    {
        return INDEX_NONE; // the index is clean (every add and removal marks it dirty), so a miss is a real miss
    }
    if (Entries.IsValidIndex(*Found) && IdOf(Entries[*Found]) == ID)
    {
        return *Found;
    }
    // A stale hit (an entry changed under the index): rebuild once and look again.
    RebuildGearIndex();
    Found = Index.Find(ID);
    return Found && Entries.IsValidIndex(*Found) ? *Found : INDEX_NONE;
}

void UPoolSubsystem::AddWeaponToPool(const FWeaponInventoryEntry &Entry)
{
    OwnedWeapons.Add(Entry);
    MarkGearIndexDirty();
}

FWeaponInventoryEntry *UPoolSubsystem::FindWeaponInPool(const FGuid &PersistentID)
{
    const int32 Index = FindIndexed(OwnedWeapons, WeaponIndex, PersistentID, WeaponIdOf);
    return Index != INDEX_NONE ? &OwnedWeapons[Index] : nullptr;
}

const FWeaponInventoryEntry *UPoolSubsystem::FindWeaponInPool(const FGuid &PersistentID) const
{
    return const_cast<UPoolSubsystem *>(this)->FindWeaponInPool(PersistentID);
}

void UPoolSubsystem::AddRingToPool(const FRingInventoryEntry &Entry)
{
    OwnedRings.Add(Entry);
    MarkGearIndexDirty();
}

FRingInventoryEntry *UPoolSubsystem::FindRingInPool(const FGuid &PersistentID)
{
    const int32 Index = FindIndexed(OwnedRings, AdapterIndex, PersistentID, AdapterIdOf);
    return Index != INDEX_NONE ? &OwnedRings[Index] : nullptr;
}

const FRingInventoryEntry *UPoolSubsystem::FindRingInPool(const FGuid &PersistentID) const
{
    return const_cast<UPoolSubsystem *>(this)->FindRingInPool(PersistentID);
}

void UPoolSubsystem::AddEvolutionToPool(const FEvolutionInventoryEntry &Entry)
{
    OwnedEvolutions.Add(Entry);
    MarkGearIndexDirty();
}

FEvolutionInventoryEntry *UPoolSubsystem::FindEvolutionInPool(const FGuid &InstanceID)
{
    const int32 Index = FindIndexed(OwnedEvolutions, EvolutionIndex, InstanceID, EvolutionIdOf);
    return Index != INDEX_NONE ? &OwnedEvolutions[Index] : nullptr;
}

const FEvolutionInventoryEntry *UPoolSubsystem::FindEvolutionInPool(const FGuid &InstanceID) const
{
    return const_cast<UPoolSubsystem *>(this)->FindEvolutionInPool(InstanceID);
}

FPoolGearView UPoolSubsystem::GetOwnedGearOf(FName Owner) const
{
    FPoolGearView View;
    if (Owner.IsNone())
    {
        return View;
    }
    for (const FWeaponInventoryEntry &Entry : OwnedWeapons)
    {
        if (GetGearOwner(Entry.PersistentID) == Owner)
        {
            View.Weapons.Add(&Entry);
        }
    }
    for (const FRingInventoryEntry &Entry : OwnedRings)
    {
        if (GetGearOwner(Entry.PersistentID) == Owner)
        {
            View.Adapters.Add(&Entry);
        }
    }
    for (const FEvolutionInventoryEntry &Entry : OwnedEvolutions)
    {
        if (GetGearOwner(Entry.InstanceID) == Owner)
        {
            View.Evolutions.Add(&Entry);
        }
    }
    return View;
}

FString UPoolSubsystem::GetStoreString() const
{
    TSet<FName> Owners;
    for (const TPair<FGuid, FName> &Owned : GearOwners)
    {
        Owners.Add(Owned.Value);
    }
    FString Out = FString::Printf(TEXT("[Pool] Store (B-312): %d weapon(s), %d adapter(s), %d evolution(s), %d owner(s)"), OwnedWeapons.Num(),
                                  OwnedRings.Num(), OwnedEvolutions.Num(), Owners.Num());
    for (const FName Owner : Owners)
    {
        const FPoolGearView View = GetOwnedGearOf(Owner);
        Out += FString::Printf(TEXT("\n  %s: %d weapon(s), %d adapter(s), %d evolution(s)"), *FPaths::GetBaseFilename(Owner.ToString()),
                               View.Weapons.Num(), View.Adapters.Num(), View.Evolutions.Num());
        for (const FWeaponInventoryEntry *Weapon : View.Weapons)
        {
            Out += FString::Printf(TEXT("\n    weapon %s %s"), *GetNameSafe(Weapon->Weapon), *Weapon->PersistentID.ToString());
        }
        for (const FRingInventoryEntry *Adapter : View.Adapters)
        {
            Out += FString::Printf(TEXT("\n    adapter %s %s"), *GetNameSafe(Adapter->Ring), *Adapter->PersistentID.ToString());
        }
        for (const FEvolutionInventoryEntry *Evolution : View.Evolutions)
        {
            Out += FString::Printf(TEXT("\n    evolution %s %s"), *GetNameSafe(Evolution->Item), *Evolution->InstanceID.ToString());
        }
    }
    const int32 Unowned = OwnedWeapons.Num() + OwnedRings.Num() + OwnedEvolutions.Num() - GearOwners.Num();
    if (Unowned > 0)
    {
        Out += FString::Printf(TEXT("\n  about %d piece(s) with no owner yet (AssignUnownedGear gives them out at the next draw)"), Unowned);
    }
    return Out;
}

// ==================== SHARED GEAR (B-091) ====================

FName UPoolSubsystem::GetCharacterKey(const UCharacterData *CharacterData)
{
    return CharacterData ? FName(*CharacterData->GetPathName()) : NAME_None;
}

bool UPoolSubsystem::BankStartingGear(FName CharacterKey, const TArray<FWeaponInventoryEntry> &Weapons,
                                      const TArray<FRingInventoryEntry> &Rings,
                                      const TArray<FEvolutionInventoryEntry> &Evolutions)
{
    if (CharacterKey.IsNone() || BankedCharacters.Contains(CharacterKey))
    {
        return false;
    }
    BankedCharacters.Add(CharacterKey);
    // B-321 (D-117): a character's starting gear is its own (owner set first, so listeners see it).
    for (const FWeaponInventoryEntry &Entry : Weapons)
    {
        SetGearOwner(Entry.PersistentID, CharacterKey);
        UpdateWeapon(Entry);
    }
    for (const FRingInventoryEntry &Entry : Rings)
    {
        SetGearOwner(Entry.PersistentID, CharacterKey);
        UpdateRing(Entry);
    }
    for (const FEvolutionInventoryEntry &Entry : Evolutions)
    {
        SetGearOwner(Entry.InstanceID, CharacterKey);
        UpdateEvolution(Entry);
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Banked %s's starting gear: %d weapon(s), %d ring(s), %d evolution(s)"),
           *CharacterKey.ToString(), Weapons.Num(), Rings.Num(), Evolutions.Num());
    return true;
}

void UPoolSubsystem::UpdateWeapon(const FWeaponInventoryEntry &Entry)
{
    if (!Entry.PersistentID.IsValid())
    {
        return;
    }
    if (FWeaponInventoryEntry *Existing = FindWeaponInPool(Entry.PersistentID))
    {
        *Existing = Entry;
    }
    else
    {
        AddWeaponToPool(Entry);
    }
    OnPoolGearChanged.Broadcast(Entry.PersistentID);
}

void UPoolSubsystem::UpdateRing(const FRingInventoryEntry &Entry)
{
    if (!Entry.PersistentID.IsValid())
    {
        return;
    }
    if (FRingInventoryEntry *Existing = FindRingInPool(Entry.PersistentID))
    {
        *Existing = Entry;
    }
    else
    {
        AddRingToPool(Entry);
    }
    OnPoolGearChanged.Broadcast(Entry.PersistentID);
}

void UPoolSubsystem::UpdateEvolution(const FEvolutionInventoryEntry &Entry)
{
    if (!Entry.InstanceID.IsValid())
    {
        return;
    }
    if (FEvolutionInventoryEntry *Existing = FindEvolutionInPool(Entry.InstanceID))
    {
        *Existing = Entry;
    }
    else
    {
        AddEvolutionToPool(Entry);
    }
    OnPoolGearChanged.Broadcast(Entry.InstanceID);
}

void UPoolSubsystem::RemoveGear(const FGuid &GearID)
{
    const auto MatchesWeapon = [&GearID](const FWeaponInventoryEntry &E) { return E.PersistentID == GearID; };
    const auto MatchesRing = [&GearID](const FRingInventoryEntry &E) { return E.PersistentID == GearID; };
    const auto MatchesEvolution = [&GearID](const FEvolutionInventoryEntry &E) { return E.InstanceID == GearID; };
    const int32 Removed = OwnedWeapons.RemoveAll(MatchesWeapon) + OwnedRings.RemoveAll(MatchesRing) +
                          OwnedEvolutions.RemoveAll(MatchesEvolution);
    MarkGearIndexDirty();
    Wearers.Remove(GearID);
    GearOwners.Remove(GearID);
    if (Removed > 0)
    {
        OnPoolGearChanged.Broadcast(GearID);
    }
}

FName UPoolSubsystem::GetWearer(const FGuid &GearID) const
{
    const FName *Wearer = Wearers.Find(GearID);
    return Wearer ? *Wearer : NAME_None;
}

void UPoolSubsystem::SetWearer(const FGuid &GearID, FName CharacterKey)
{
    if (GearID.IsValid() && !CharacterKey.IsNone())
    {
        Wearers.Add(GearID, CharacterKey);
    }
}

void UPoolSubsystem::ReleaseWearer(const FGuid &GearID)
{
    Wearers.Remove(GearID);
}

FName UPoolSubsystem::GetGearOwner(const FGuid &GearID) const
{
    const FName *Owner = GearOwners.Find(GearID);
    return Owner ? *Owner : NAME_None;
}

void UPoolSubsystem::SetGearOwner(const FGuid &GearID, FName CharacterKey)
{
    if (GearID.IsValid() && !CharacterKey.IsNone())
    {
        GearOwners.Add(GearID, CharacterKey);
    }
}

FName UPoolSubsystem::GetChestKey()
{
    static const FName ChestKey(TEXT("SharedChest"));
    return ChestKey;
}

bool UPoolSubsystem::GiveGear(const FGuid &GearID, FName ToKey)
{
    // Evolutions wait for Q-323 (attached / primary-slot detach): refused here too, so no caller can leave a slot dangling.
    const bool bInPool = FindWeaponInPool(GearID) || FindRingInPool(GearID);
    if (!bInPool || ToKey.IsNone() || GetGearOwner(GearID) == ToKey)
    {
        return false;
    }
    const FName FromKey = GetGearOwner(GearID);
    SetGearOwner(GearID, ToKey);
    UE_LOG(LogTemp, Log, TEXT("[Pool] %s gave %s to %s"), *FPaths::GetBaseFilename(FromKey.ToString()), *GearID.ToString(),
           *FPaths::GetBaseFilename(ToKey.ToString()));
    OnPoolGearChanged.Broadcast(GearID);
    return true;
}

int32 UPoolSubsystem::AssignUnownedGear(FName Fallback)
{
    int32 Assigned = 0;
    const auto Assign = [this, Fallback, &Assigned](const FGuid &ID, const FString &Name)
    {
        if (!ID.IsValid() || GearOwners.Contains(ID))
        {
            return;
        }
        const FName Wearer = GetWearer(ID);
        const FName Owner = Wearer.IsNone() ? Fallback : Wearer;
        if (Owner.IsNone())
        {
            return;
        }
        GearOwners.Add(ID, Owner);
        ++Assigned;
        UE_LOG(LogTemp, Log, TEXT("[Pool] %s now belongs to %s (%s, D-117)"), *Name, *FPaths::GetBaseFilename(Owner.ToString()),
               Wearer.IsNone() ? TEXT("the party leader") : TEXT("its wearer"));
    };
    for (const FWeaponInventoryEntry &Entry : OwnedWeapons)
    {
        Assign(Entry.PersistentID, GetNameSafe(Entry.Weapon));
    }
    for (const FRingInventoryEntry &Entry : OwnedRings)
    {
        Assign(Entry.PersistentID, GetNameSafe(Entry.Ring));
    }
    for (const FEvolutionInventoryEntry &Entry : OwnedEvolutions)
    {
        Assign(Entry.InstanceID, GetNameSafe(Entry.Item));
    }
    return Assigned;
}

void UPoolSubsystem::NoteClashes(FName LoserKey, const TArray<FPoolGearClash> &NewClashes)
{
    Clashes.RemoveAll([LoserKey](const FPoolGearClash &Clash) { return Clash.LoserKey == LoserKey; });
    Clashes.Append(NewClashes);
}

void UPoolSubsystem::RestoreSharedGear(const TMap<FGuid, FName> &InWearers, const TArray<FName> &InBanked)
{
    Wearers = InWearers;
    BankedCharacters = TSet<FName>(InBanked);
}

// ==================== CUSTOM LOADOUTS ====================

void UPoolSubsystem::SetCustomLoadouts(FName CharacterKey, const TArray<FSavedLoadout> &Loadouts)
{
    if (Loadouts.IsEmpty())
    {
        CustomLoadouts.Remove(CharacterKey);
        return;
    }
    if (Loadouts.Num() > PoolConstants::MAX_CUSTOM_LOADOUTS)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pool] %s has %d custom loadouts; keeping the first %d (D-111)"),
               *FPaths::GetBaseFilename(CharacterKey.ToString()), Loadouts.Num(), PoolConstants::MAX_CUSTOM_LOADOUTS);
    }
    TArray<FSavedLoadout> &Kept = CustomLoadouts.FindOrAdd(CharacterKey).Loadouts;
    Kept = Loadouts;
    Kept.SetNum(FMath::Min(Kept.Num(), PoolConstants::MAX_CUSTOM_LOADOUTS));
}

const TArray<FSavedLoadout> *UPoolSubsystem::GetCustomLoadouts(FName CharacterKey) const
{
    const FPoolCustomLoadouts *Found = CustomLoadouts.Find(CharacterKey);
    return Found ? &Found->Loadouts : nullptr;
}

// ==================== KNOWLEDGE ====================

void UPoolSubsystem::AddSpellToPool(const FSpellInstance &Instance)
{
    OwnedSpells.Add(Instance);
}

FSpellInstance *UPoolSubsystem::FindSpellInPool(const FGuid &InstanceID)
{
    return OwnedSpells.FindByPredicate(
        [&InstanceID](const FSpellInstance &I) { return I.InstanceID == InstanceID; });
}

const FSpellInstance *UPoolSubsystem::FindSpellInPool(const FGuid &InstanceID) const
{
    return OwnedSpells.FindByPredicate(
        [&InstanceID](const FSpellInstance &I) { return I.InstanceID == InstanceID; });
}

void UPoolSubsystem::AddAbilityToPool(const FAbilityInstance &Instance)
{
    OwnedAbilities.Add(Instance);
}

FAbilityInstance *UPoolSubsystem::FindAbilityInPool(const FGuid &InstanceID)
{
    return OwnedAbilities.FindByPredicate(
        [&InstanceID](const FAbilityInstance &I) { return I.InstanceID == InstanceID; });
}

const FAbilityInstance *UPoolSubsystem::FindAbilityInPool(const FGuid &InstanceID) const
{
    return OwnedAbilities.FindByPredicate(
        [&InstanceID](const FAbilityInstance &I) { return I.InstanceID == InstanceID; });
}

// ==================== ITEMS ====================

void UPoolSubsystem::AddCrystalToPool(const FCrystalId &Id, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    // gem/stone by IsGemType (mirrors the run component's PoolFor).
    (CrystalTypeHelpers::IsGemType(Id.Type) ? Crystals : Stones).FindOrAdd(Id) += Count;
}

// ==================== POUCHES AND STORAGE (B-203, B-322) ====================

const FPoolPouch &UPoolSubsystem::GetPouch(FName CharacterKey) const
{
    static const FPoolPouch Empty;
    const FPoolPouch *Found = Pouches.Find(CharacterKey);
    return Found ? *Found : Empty;
}

FPoolPouch &UPoolSubsystem::FindOrAddPouch(FName CharacterKey)
{
    return Pouches.FindOrAdd(CharacterKey);
}

int32 UPoolSubsystem::GetCarriedCount(FName CharacterKey, const FCrystalId &Id) const
{
    const int32 *Found = GetPouch(CharacterKey).Stacks.Find(Id);
    return Found ? *Found : 0;
}

int32 UPoolSubsystem::GetCarryRoom(FName CharacterKey, const FCrystalId &Id) const
{
    if (Id.Type == ECrystalType::None || CharacterKey.IsNone())
    {
        return 0;
    }
    const FPoolPouch &Pouch = GetPouch(CharacterKey);
    const int32 *Stack = Pouch.Stacks.Find(Id);
    if (!Stack && Pouch.Stacks.Num() >= Pouch.SlotCount)
    {
        return 0;
    }
    return FMath::Max(0, InventoryConstants::ITEM_STACK_MAX - (Stack ? *Stack : 0));
}

int32 UPoolSubsystem::CarryOrStore(FName CharacterKey, const FCrystalId &Id, int32 Count)
{
    if (Count <= 0 || Id.Type == ECrystalType::None)
    {
        return 0;
    }
    // No character (an enemy, a debug add): everything goes to the shared storage.
    const int32 Carried = FMath::Min(Count, GetCarryRoom(CharacterKey, Id));
    if (Carried > 0)
    {
        FindOrAddPouch(CharacterKey).Stacks.FindOrAdd(Id) += Carried;
    }
    const int32 Stored = Count - Carried;
    if (Stored > 0)
    {
        AddCrystalToPool(Id, Stored);
        UE_LOG(LogTemp, Log, TEXT("[Pool] %d %s didn't fit %s's pouch: sent to storage"), Stored, *ItemIdentity::GetDisplayName(Id),
               *FPaths::GetBaseFilename(CharacterKey.ToString()));
    }
    OnPoolItemsChanged.Broadcast();
    return Stored;
}

int32 UPoolSubsystem::TakeCarried(FName CharacterKey, const FCrystalId &Id, int32 Count)
{
    FPoolPouch *Pouch = Pouches.Find(CharacterKey);
    int32 *Stack = Pouch ? Pouch->Stacks.Find(Id) : nullptr;
    if (!Stack || Count <= 0)
    {
        return 0;
    }
    const int32 Taken = FMath::Min(*Stack, Count);
    *Stack -= Taken;
    if (*Stack <= 0)
    {
        Pouch->Stacks.Remove(Id);
    }
    OnPoolItemsChanged.Broadcast();
    return Taken;
}

int32 UPoolSubsystem::GetStoredCount(const FCrystalId &Id) const
{
    const int32 *Found = (CrystalTypeHelpers::IsGemType(Id.Type) ? Crystals : Stones).Find(Id);
    return Found ? *Found : 0;
}

int32 UPoolSubsystem::TakeStored(const FCrystalId &Id, int32 Count)
{
    TMap<FCrystalId, int32> &Storage = CrystalTypeHelpers::IsGemType(Id.Type) ? Crystals : Stones;
    int32 *Stack = Storage.Find(Id);
    if (!Stack || Count <= 0)
    {
        return 0;
    }
    const int32 Taken = FMath::Min(*Stack, Count);
    *Stack -= Taken;
    if (*Stack <= 0)
    {
        Storage.Remove(Id);
    }
    return Taken;
}

bool UPoolSubsystem::MoveToStorage(FName CharacterKey, const FCrystalId &Id, int32 Count)
{
    if (Count <= 0 || GetCarriedCount(CharacterKey, Id) < Count)
    {
        return false;
    }
    // Into storage first, so listeners to TakeCarried's broadcast find the items there.
    AddCrystalToPool(Id, Count);
    TakeCarried(CharacterKey, Id, Count);
    return true;
}

int32 UPoolSubsystem::ReleasePouch(FName CharacterKey)
{
    FPoolPouch Released;
    if (!Pouches.RemoveAndCopyValue(CharacterKey, Released))
    {
        return 0;
    }
    int32 Moved = 0;
    for (const TPair<FCrystalId, int32> &Stack : Released.Stacks)
    {
        AddCrystalToPool(Stack.Key, Stack.Value);
        Moved += Stack.Value;
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] %s leaves: %d carried item(s) go to storage"), *CharacterKey.ToString(), Moved);
    OnPoolItemsChanged.Broadcast();
    return Moved;
}

bool UPoolSubsystem::MoveToCarried(FName CharacterKey, const FCrystalId &Id, int32 Count)
{
    if (Count <= 0 || GetStoredCount(Id) < Count || GetCarryRoom(CharacterKey, Id) < Count)
    {
        return false;
    }
    TakeStored(Id, Count);
    FindOrAddPouch(CharacterKey).Stacks.FindOrAdd(Id) += Count;
    OnPoolItemsChanged.Broadcast();
    return true;
}

bool UPoolSubsystem::BankStartingItems(FName CharacterKey, const TMap<FCrystalId, int32> &Items)
{
    if (CharacterKey.IsNone() || BankedItemCharacters.Contains(CharacterKey))
    {
        return false;
    }
    BankedItemCharacters.Add(CharacterKey);
    for (const TPair<FCrystalId, int32> &Pair : Items)
    {
        CarryOrStore(CharacterKey, Pair.Key, Pair.Value);
    }
    const FPoolPouch &Pouch = GetPouch(CharacterKey);
    UE_LOG(LogTemp, Log, TEXT("[Pool] Banked %s's starting items: %d of %d pouch slot(s) in use"),
           *FPaths::GetBaseFilename(CharacterKey.ToString()), Pouch.Stacks.Num(), Pouch.SlotCount);
    return true;
}

void UPoolSubsystem::RestorePouches(const TMap<FName, FPoolPouch> &InPouches, const TArray<FName> &InBanked)
{
    Pouches = InPouches;
    for (TPair<FName, FPoolPouch> &Pair : Pouches)
    {
        Pair.Value.SlotCount = FMath::Max(Pair.Value.SlotCount, InventoryConstants::ITEM_SLOTS_START);
    }
    BankedItemCharacters = TSet<FName>(InBanked);
    OnPoolItemsChanged.Broadcast();
}

bool UPoolSubsystem::GiveCarried(FName FromKey, FName ToKey, const FCrystalId &Id, int32 Count)
{
    if (Count <= 0 || FromKey == ToKey || ToKey.IsNone() || GetCarriedCount(FromKey, Id) < Count || GetCarryRoom(ToKey, Id) < Count)
    {
        return false;
    }
    FPoolPouch &From = FindOrAddPouch(FromKey);
    int32 &Stack = From.Stacks.FindChecked(Id);
    Stack -= Count;
    if (Stack <= 0)
    {
        From.Stacks.Remove(Id);
    }
    FindOrAddPouch(ToKey).Stacks.FindOrAdd(Id) += Count;
    UE_LOG(LogTemp, Log, TEXT("[Pool] %s gave %d %s to %s"), *FPaths::GetBaseFilename(FromKey.ToString()), Count,
           *ItemIdentity::GetDisplayName(Id), *FPaths::GetBaseFilename(ToKey.ToString()));
    OnPoolItemsChanged.Broadcast();
    return true;
}

FString UPoolSubsystem::GetItemSlotsString() const
{
    FString Out = FString::Printf(TEXT("[Pool] Pouches (B-322): %d character(s), %d banked"), Pouches.Num(), BankedItemCharacters.Num());
    for (const TPair<FName, FPoolPouch> &Pouch : Pouches)
    {
        Out += FString::Printf(TEXT("\n  %s: %d of %d slot(s)"), *FPaths::GetBaseFilename(Pouch.Key.ToString()), Pouch.Value.Stacks.Num(),
                               Pouch.Value.SlotCount);
        for (const TPair<FCrystalId, int32> &Pair : Pouch.Value.Stacks)
        {
            Out += FString::Printf(TEXT("\n    slot: %s x%d"), *ItemIdentity::GetDisplayName(Pair.Key), Pair.Value);
        }
    }
    // B-324: the chest's gear beside the stored items.
    int32 ChestGear = 0;
    for (const TPair<FGuid, FName> &Owner : GearOwners)
    {
        ChestGear += Owner.Value == GetChestKey() ? 1 : 0;
    }
    Out += FString::Printf(TEXT("\n  Chest gear: %d piece(s)"), ChestGear);
    Out += TEXT("\n  Storage:");
    for (const TMap<FCrystalId, int32> *Storage : {&Crystals, &Stones})
    {
        for (const TPair<FCrystalId, int32> &Pair : *Storage)
        {
            Out += FString::Printf(TEXT("\n    %s x%d"), *ItemIdentity::GetDisplayName(Pair.Key), Pair.Value);
        }
    }
    if (Crystals.Num() + Stones.Num() == 0)
    {
        Out += TEXT(" empty");
    }
    return Out;
}

// ==================== CATEGORY GETTERS ====================

FPoolKnowledgeView UPoolSubsystem::GetKnowledge(const FPoolFilter &Filter) const
{
    FPoolKnowledgeView View;
    for (const FSpellInstance &Spell : OwnedSpells)
    {
        if (PoolAccessors::MatchesFilter(Filter, Spell))
        {
            View.Spells.Add(Spell);
        }
    }
    for (const FAbilityInstance &Ability : OwnedAbilities)
    {
        if (PoolAccessors::MatchesFilter(Filter, Ability))
        {
            View.Abilities.Add(Ability);
        }
    }
    return View;
}

FPoolArmouryView UPoolSubsystem::GetArmoury(const FPoolFilter &Filter) const
{
    FPoolArmouryView View;
    for (const FWeaponInventoryEntry &Weapon : OwnedWeapons)
    {
        if (PoolAccessors::MatchesFilter(Filter, Weapon))
        {
            View.Weapons.Add(Weapon);
        }
    }
    for (const FRingInventoryEntry &Ring : OwnedRings)
    {
        if (PoolAccessors::MatchesFilter(Filter, Ring))
        {
            View.Rings.Add(Ring);
        }
    }
    for (const FEvolutionInventoryEntry &Evo : OwnedEvolutions)
    {
        if (PoolAccessors::MatchesFilter(Filter, Evo))
        {
            View.Evolutions.Add(Evo);
        }
    }
    return View;
}

TArray<FPoolItemStack> UPoolSubsystem::GetItems(const FPoolFilter &Filter) const
{
    TArray<FPoolItemStack> Out;

    // The item/refined axis is gone (gem-merge) — every stack tags bRefined=false. The dormant
    // FPoolFilter.bUseRefined / .bRefined and FPoolItemStack.bRefined fields are left in
    // PoolQueryTypes for a later cleanup; the browse path is still inert.
    auto Gather = [&](const TMap<FCrystalId, int32> &Pool)
    {
        for (const TPair<FCrystalId, int32> &Pair : Pool)
        {
            if (Filter.bUseBucket && PoolAccessors::GetBucket(Pair.Key) != Filter.Bucket)
            {
                continue; // browse-bucket narrowing (Crystal vs Augment Stone)
            }
            if (PoolAccessors::MatchesFilter(Filter, Pair.Key))
            {
                FPoolItemStack Stack;
                Stack.Id = Pair.Key;
                Stack.Count = Pair.Value;
                Stack.bRefined = false;
                Out.Add(Stack);
            }
        }
    };

    Gather(Crystals);
    Gather(Stones);
    return Out;
}

FPoolQueryResult UPoolSubsystem::QueryAll(const FPoolFilter &Filter) const
{
    FPoolQueryResult Result;
    Result.Knowledge = GetKnowledge(Filter);
    Result.Armoury = GetArmoury(Filter);
    Result.Items = GetItems(Filter);
    return Result;
}

// ==================== POPULATE (authored → pool) ====================

void UPoolSubsystem::PopulateFromInventoryAsset(UInventoryData *Asset, AActor *OwnerContext)
{
    // One-time per session (GI-scoped). A flag, not an is-empty check, so a legitimately-empty
    // authored asset can't re-trigger and double-bank.
    if (bPopulated)
    {
        UE_LOG(LogTemp, Log, TEXT("[Pool] PopulateFromInventoryAsset: already populated this session — skipping."));
        return;
    }
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pool] PopulateFromInventoryAsset: null Asset — nothing to populate."));
        return;
    }

    // ---------- Weapons ----------
    // Reuse the SAME factory the run uses: CreateFromWeapon copies the default crystal into
    // AttachedItem + seeds Tier/Quality; the acquisition mint adds a fresh PersistentID (the factory
    // deliberately leaves it invalid). NO pickup roll — authored banking is deterministic (the
    // toggle-off path keeps the factory's C_Quality placeholder).
    for (UWeaponData *Weapon : Asset->Weapons)
    {
        if (!Weapon)
        {
            continue;
        }
        FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
        Entry.PersistentID = FGuid::NewGuid();
        AddWeaponToPool(Entry);
    }

    // ---------- Rings ----------
    for (URingData *Ring : Asset->Rings)
    {
        if (!Ring)
        {
            continue;
        }
        FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
        Entry.PersistentID = FGuid::NewGuid();
        AddRingToPool(Entry);
    }

    // ---------- Spells ----------
    // Learn-time mint (mirrors FSpellCollection::LearnSpell): ctor mints InstanceID; seed Tier from
    // the asset + the C_Quality placeholder.
    for (USpellData *Spell : Asset->Spells)
    {
        if (!Spell)
        {
            continue;
        }
        FSpellInstance Instance(Spell);
        Instance.Tier = Spell->Tier;
        Instance.Quality = EItemQuality::C_Quality;
        AddSpellToPool(Instance);
    }

    // ---------- Abilities ----------
    // Mirror FAbilityCollection::LearnAbility: basic attacks live on the weapon, never the bar — skip.
    for (UAbilityData *Ability : Asset->Abilities)
    {
        if (!Ability || Ability->IsAttack())
        {
            continue;
        }
        FAbilityInstance Instance(Ability);
        Instance.Tier = Ability->Tier;
        Instance.Quality = EItemQuality::C_Quality;
        AddAbilityToPool(Instance);
    }

    // ---------- Crystals ----------
    // Seed from the authored CrystalStock (mixed gem+stone); AddCrystalToPool routes gem→Crystals /
    // stone→Stones by IsGemType. The asset's PostLoad already folded the deprecated ItemCrystals +
    // RefinedCrystals into CrystalStock (and emptied them), so this reads the merged stock — the same
    // source the run bulk-load reads, keeping pool-seed and run-seed consistent.
    for (const TPair<FCrystalId, int32> &Pair : Asset->CrystalStock)
    {
        AddCrystalToPool(Pair.Key, Pair.Value);
    }

    // ---------- Evolutions ----------
    // Mint an owned entry per authored item (mirrors UEvolutionInventoryComponent::AddInstance's seed:
    // InstanceID via ctor, Tier from asset, C_Quality placeholder, full starting durability). No pickup
    // roll (deterministic banking) and no run-cap gate — the pool is raw bank storage.
    for (UEvolutionItemData *Item : Asset->EvolutionEquipment)
    {
        if (!Item)
        {
            continue;
        }
        FEvolutionInventoryEntry Entry(Item);
        Entry.Tier = Item->Tier;
        Entry.Quality = EItemQuality::C_Quality;
        Entry.CurrentDurability = Item->MaxDurability;
        AddEvolutionToPool(Entry);
    }

    bPopulated = true;

    UE_LOG(LogTemp, Display,
           TEXT("[Pool] Populated from '%s' (owner=%s): Weapons=%d Rings=%d Spells=%d Abilities=%d Evolutions=%d"),
           *Asset->GetName(),
           OwnerContext ? *OwnerContext->GetName() : TEXT("none"),
           OwnedWeapons.Num(), OwnedRings.Num(), OwnedSpells.Num(), OwnedAbilities.Num(), OwnedEvolutions.Num());
}

// ==================== DEBUG ====================

FString UPoolSubsystem::GetPoolString() const
{
    const UEnum *CrystalEnum = StaticEnum<ECrystalType>();
    const UEnum *EssenceEnum = StaticEnum<EEssenceType>();

    auto SumStacks = [](const TMap<FCrystalId, int32> &Pool)
    {
        int32 Total = 0;
        for (const TPair<FCrystalId, int32> &Pair : Pool)
        {
            Total += Pair.Value;
        }
        return Total;
    };

    FString Out = TEXT("[Pool]");
    Out += FString::Printf(TEXT("\n  Armoury  : Weapons=%d Rings=%d Evolutions=%d"),
                           OwnedWeapons.Num(), OwnedRings.Num(), OwnedEvolutions.Num());
    Out += FString::Printf(TEXT("\n  Shared gear (B-091): %d worn, %d character(s) banked, %d clash(es); %d owned (B-321)"),
                           Wearers.Num(), BankedCharacters.Num(), Clashes.Num(), GearOwners.Num());
    for (const TPair<FGuid, FName> &Worn : Wearers)
    {
        Out += FString::Printf(TEXT("\n    %s worn by %s"), *Worn.Key.ToString(EGuidFormats::Short),
                               *FPaths::GetBaseFilename(Worn.Value.ToString()));
    }
    for (const TPair<FGuid, FName> &Owned : GearOwners)
    {
        Out += FString::Printf(TEXT("\n    %s owned by %s"), *Owned.Key.ToString(EGuidFormats::Short),
                               *FPaths::GetBaseFilename(Owned.Value.ToString()));
    }
    for (const FPoolGearClash &Clash : Clashes)
    {
        Out += FString::Printf(TEXT("\n    CLASH: %s worn by %s, taken out of %s's loadout"), *Clash.ItemName.ToString(),
                               *Clash.WearerName.ToString(), *Clash.LoserName.ToString());
    }
    for (const TPair<FName, FPoolCustomLoadouts> &Custom : CustomLoadouts)
    {
        Out += FString::Printf(TEXT("\n  Custom loadouts (B-311): %s has %d"), *FPaths::GetBaseFilename(Custom.Key.ToString()),
                               Custom.Value.Loadouts.Num());
    }
    Out += FString::Printf(TEXT("\n  Knowledge: Spells=%d Abilities=%d"),
                           OwnedSpells.Num(), OwnedAbilities.Num());
    Out += FString::Printf(TEXT("\n  Items    : Crystals=%d (%d), Stones=%d (%d)  [stacks (total)]"),
                           Crystals.Num(), SumStacks(Crystals),
                           Stones.Num(), SumStacks(Stones));

    // Crystal stacks grouped by BROWSE bucket (Crystal vs Augment Stone), each listing both pools.
    auto DumpBucket = [&](EItemBucket Bucket, const TCHAR *Label)
    {
        Out += FString::Printf(TEXT("\n    %s:"), Label);
        auto EmitPool = [&](const TMap<FCrystalId, int32> &Pool, const TCHAR *Tag)
        {
            for (const TPair<FCrystalId, int32> &Pair : Pool)
            {
                if (PoolAccessors::GetBucket(Pair.Key) != Bucket)
                {
                    continue;
                }
                Out += FString::Printf(TEXT("\n      [%s] %s %s x%d"), Tag,
                                       *EnumName(CrystalEnum, static_cast<int64>(Pair.Key.Type)),
                                       *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
            }
        };
        EmitPool(Crystals, TEXT("crystal"));
        EmitPool(Stones, TEXT("stone"));
    };
    DumpBucket(EItemBucket::Crystal, TEXT("Crystals"));
    DumpBucket(EItemBucket::AugmentStone, TEXT("Augment Stones"));

    // Wallet.
    Out += FString::Printf(TEXT("\n  Wallet   : Gold=%d Prisms=%d Diamond=%d Gear=%d Skill=%d"),
                           PoolGold, PoolPrisms, PoolDiamond, PoolGearEssence, PoolSkillEssence);
    for (const FCurrencyEntry &E : PoolEssence.Items)
    {
        if (E.Amount != 0)
        {
            Out += FString::Printf(TEXT("\n      Essence[%s]=%d"),
                                   *EnumName(EssenceEnum, static_cast<int64>(E.Key)), E.Amount);
        }
    }

    return Out;
}

void UPoolSubsystem::PrintPoolState() const
{
    const FString Dump = GetPoolString();
    UE_LOG(LogTemp, Log, TEXT("%s"), *Dump);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, POOL_DEBUG_SCREEN_DURATION, FColor::Cyan, Dump);
    }
}

// ==================== DEBUG CONSOLE COMMANDS ====================
// FAutoConsoleCommandWithWorld — NOT UFUNCTION(Exec). Exec only dispatches on PlayerController / Pawn /
// GameMode / GameInstance / CheatManager / HUD; a UGameInstanceSubsystem is NOT on that list, so an Exec
// here would silently no-op (the documented cause of "console commands never working" on the wrong class).
// Console commands work from any class. The "wor." prefix groups them and avoids engine-command clashes.
// Both resolve PIE-only state, so the whole world → game-instance → subsystem chain is null-checked.

#if !UE_BUILD_SHIPPING
namespace
{
    UPoolSubsystem *ResolvePoolFromWorld(UWorld *World)
    {
        if (!World)
        {
            return nullptr;
        }
        UGameInstance *GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    }

    static FAutoConsoleCommandWithWorld GPopulatePoolCmd(
        TEXT("wor.PopulatePool"),
        TEXT("Populate the account pool from the played character's authored inventory, then print it (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.PopulatePool: no pool subsystem (no PIE world?)."));
                    return;
                }

                // Source the InventoryData from the PLAYED character: first player controller's pawn →
                // CharacterDataComponent → CharacterData->Inventory. Cleanest testing source — it banks
                // whatever character is being played, with no hardcoded asset path to keep in sync.
                APlayerController *PC = World->GetFirstPlayerController();
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UCharacterDataComponent *CDC = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
                UCharacterData *CharData = CDC ? CDC->CharacterData : nullptr;
                UInventoryData *Inventory = CharData ? CharData->Inventory : nullptr;
                if (!Inventory)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.PopulatePool: could not resolve a played character's Inventory asset (pawn / CharacterData / Inventory missing)."));
                    return;
                }

                Pool->PopulateFromInventoryAsset(Inventory, Pawn);
                Pool->PrintPoolState();
            }));

    static FAutoConsoleCommandWithWorld GPrintStoreCmd(
        TEXT("wor.Pool.PrintStore"),
        TEXT("B-312: the gear store by owner (each character, the shared chest), one instance per item (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                if (const UPoolSubsystem *Pool = ResolvePoolFromWorld(World))
                {
                    UE_LOG(LogTemp, Log, TEXT("%s"), *Pool->GetStoreString());
                }
            }));

    static FAutoConsoleCommandWithWorld GPrintItemSlotsCmd(
        TEXT("wor.Inventory.PrintSlots"),
        TEXT("B-322: each character's pouch (one item type per slot) and the shared storage (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                if (const UPoolSubsystem *Pool = ResolvePoolFromWorld(World))
                {
                    UE_LOG(LogTemp, Log, TEXT("%s"), *Pool->GetItemSlotsString());
                }
            }));

    static FAutoConsoleCommandWithWorld GPrintPoolCmd(
        TEXT("wor.PrintPool"),
        TEXT("Dump the account pool (per-category counts, crystal stacks, wallet) to log + screen (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.PrintPool: no pool subsystem (no PIE world?)."));
                    return;
                }
                Pool->PrintPoolState();
            }));

    // Step 2 — the first NON-INERT flip: DRAW the run inventory from the pool. Uncalled-first
    // (not wired into InitializeFromCharacterData), so this debug command is the ONLY way to
    // trigger a pool draw. PIE stays byte-identical until you type wor.DrawFromPool manually.
    static FAutoConsoleCommandWithWorld GDrawFromPoolCmd(
        TEXT("wor.DrawFromPool"),
        TEXT("Draw the played character's RUN inventory FROM the account pool (whole-entry, preserves instance identity), then print the pool (PIE debug). Owned inventory only — SavedLoadouts stay authored."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.DrawFromPool: no pool subsystem (no PIE world?)."));
                    return;
                }

                APlayerController *PC = World->GetFirstPlayerController();
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UInventoryComponent *Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
                if (!Inv)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.DrawFromPool: could not resolve a played character's InventoryComponent (pawn / component missing)."));
                    return;
                }

                Inv->InitializeFromPool(Pool);
                Pool->PrintPoolState();
            }));

    // The DIFF tool: dump the played character's RUN inventory per-INSTANCE (tiers / quality /
    // attached crystals). Run after wor.PopulatePool + wor.DrawFromPool and compare against
    // wor.PrintPool — matching tiers/crystals prove the whole-entry draw was lossless.
    static FAutoConsoleCommandWithWorld GPrintInventoryCmd(
        TEXT("wor.PrintInventory"),
        TEXT("Dump the played character's run inventory per-instance (PersistentID/InstanceID, tier, quality, attached crystal) to log + screen (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UInventoryComponent *Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
                if (!Inv)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.PrintInventory: could not resolve a played character's InventoryComponent (pawn / component missing)."));
                    return;
                }

                const FString Dump = Inv->GetInventoryInstanceString();
                UE_LOG(LogTemp, Log, TEXT("%s"), *Dump);
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, POOL_DEBUG_SCREEN_DURATION, FColor::Yellow, Dump);
                }
            }));
}
#endif // !UE_BUILD_SHIPPING
