// PoolRunLayer.cpp
// B-314 (D-111, D-097, Q-314): the run layer over the gear store. UPoolSubsystem's run-layer half, kept out of
// PoolSubsystem.cpp. A home piece taken on the run keeps its home attachment (and wear) in its run mark and gets it back
// at run end; gear found or bought in the run is run-only: dropped at run end with its tier remembered by definition.

#include "Pool/PoolSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/ItemTier.h"
#include "Misc/Paths.h"

namespace
{
    /** The definition (asset) a pool entry is an instance of. */
    const UObject *RunLayerDefinitionOf(const FWeaponInventoryEntry &Entry) { return Entry.Weapon; }
    const UObject *RunLayerDefinitionOf(const FRingInventoryEntry &Entry) { return Entry.Ring; }
    const UObject *RunLayerDefinitionOf(const FEvolutionInventoryEntry &Entry) { return Entry.Item; }

    /** Raises Entry's tier to its definition's remembered one (never lowers it). */
    template <class TEntry>
    bool RaiseToRememberedTier(const UPoolSubsystem &Pool, TEntry &Entry)
    {
        EItemTier Remembered = EItemTier::F_Tier;
        if (!Pool.FindRememberedTier(RunLayerDefinitionOf(Entry), Remembered) ||
            TierHelpers::GetTierValue(Remembered) <= TierHelpers::GetTierValue(Entry.Tier))
        {
            return false;
        }
        UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: %s arrives at %s, the tier an earlier run left it at (D-097)"),
               *GetNameSafe(RunLayerDefinitionOf(Entry)), *TierHelpers::GetTierName(Remembered));
        Entry.Tier = Remembered;
        return true;
    }

    /** True when Now isn't the home attachment: another item, or the same one worn (operator== compares identity
     *  only, not durability). */
    bool RunLayerAttachmentDiffers(const FRuntimeAttachedItem &Now, const FRuntimeAttachedItem &Home)
    {
        return !(Now == Home) || Now.GetCurrentDurability() != Home.GetCurrentDurability();
    }
}

void UPoolSubsystem::BeginRunLayer()
{
    if (bRunLayerActive)
    {
        // The store may have been reloaded under it (a resumed save), so its home states can be stale: not applied.
        UE_LOG(LogTemp, Warning, TEXT("[Pool] Run layer: a new run starts while the last one is still open - its marks are discarded"));
        DiscardRunLayer();
    }
    RunMarks.Reset();
    RunStartItems = CountHeldItems(); // B-439b
    bRunLayerActive = true;
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer open"));
}

void UPoolSubsystem::DiscardRunLayer()
{
    if (bRunLayerActive)
    {
        UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer discarded: %d mark(s) dropped, nothing put back"), RunMarks.Num());
    }
    RunMarks.Reset();
    RunStartItems.Reset();
    bRunLayerActive = false;
}

int32 UPoolSubsystem::RestoreRunLayer(const TMap<FGuid, FPoolRunMark> &SavedMarks)
{
    RunMarks.Reset();
    // B-439b: a save from before the counts were kept has none, so what is held now stands in (the save's restore
    // replaces it with the saved counts, RestoreRunStartItems).
    RunStartItems = CountHeldItems();
    bRunLayerActive = true;
    int32 Dropped = 0;
    for (const TPair<FGuid, FPoolRunMark> &Saved : SavedMarks)
    {
        if (FindWeaponInPool(Saved.Key) || FindRingInPool(Saved.Key) || FindEvolutionInPool(Saved.Key))
        {
            RunMarks.Add(Saved.Key, Saved.Value);
        }
        else
        {
            ++Dropped;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer restored from the save: %d mark(s) kept"), RunMarks.Num());
    if (Dropped > 0)
    {
        // A dropped home mark's piece won't be put back to its home state at run end.
        UE_LOG(LogTemp, Warning, TEXT("[Pool] Run layer restore: %d saved mark(s) dropped (their gear is no longer in the store)"), Dropped);
    }
    return RunMarks.Num();
}

bool UPoolSubsystem::TakeOnRun(const FGuid &GearID)
{
    if (!bRunLayerActive || !GearID.IsValid() || RunMarks.Contains(GearID))
    {
        return false;
    }
    FPoolRunMark Mark;
    Mark.bOnRun = true;
    if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(GearID))
    {
        Mark.HomeAttached = Weapon->AttachedItem; // B-098 (Q-098): a weapon's spells are deleted, so none are marked
    }
    else if (const FRingInventoryEntry *Adapter = FindRingInPool(GearID))
    {
        Mark.HomeAttached = Adapter->AttachedItem;
        Mark.HomeSpells = Adapter->AssignedSpells;
    }
    else if (const FEvolutionInventoryEntry *Evolution = FindEvolutionInPool(GearID))
    {
        Mark.HomeDurability = Evolution->CurrentDurability;
    }
    else
    {
        return false;
    }
    RunMarks.Add(GearID, Mark);
    return true;
}

int32 UPoolSubsystem::TakeHomeGearOnRun(const TSet<FName> &WearerKeys)
{
    TArray<FGuid> WornIDs;
    const auto IsWorn = [this, &WearerKeys](const FGuid &ID) { return WearerKeys.Contains(GetWearer(ID)); };
    const auto AddWorn = [&WornIDs, &IsWorn](const FGuid &ID, const FRuntimeAttachedItem &Attached)
    {
        if (!IsWorn(ID))
        {
            return;
        }
        WornIDs.Add(ID);
        // The evolution on a worn piece goes along with it (its owned entry keeps the durability).
        if (Attached.IsEvolution() && Attached.Evolution.InstanceID.IsValid())
        {
            WornIDs.Add(Attached.Evolution.InstanceID);
        }
    };
    for (const FWeaponInventoryEntry &Entry : OwnedWeapons)
    {
        AddWorn(Entry.PersistentID, Entry.AttachedItem);
    }
    for (const FRingInventoryEntry &Entry : OwnedRings)
    {
        AddWorn(Entry.PersistentID, Entry.AttachedItem);
    }
    for (const FEvolutionInventoryEntry &Entry : OwnedEvolutions)
    {
        if (IsWorn(Entry.InstanceID)) // an evolution worn in a primary slot
        {
            WornIDs.Add(Entry.InstanceID);
        }
    }
    int32 Taken = 0;
    for (const FGuid &ID : WornIDs)
    {
        Taken += TakeOnRun(ID) ? 1 : 0;
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: the worn loadout goes on the run, free (D-137): %d piece(s)"), Taken);
    return Taken;
}

bool UPoolSubsystem::MarkRunOnly(const FGuid &GearID)
{
    if (!bRunLayerActive || !GearID.IsValid())
    {
        return false;
    }
    FPoolRunMark &Mark = RunMarks.FindOrAdd(GearID);
    if (Mark.bOnRun)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pool] Run layer: %s is a home piece on the run - not made run-only"), *GearID.ToString());
        return false;
    }
    Mark.bRunOnly = true;
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: %s found in the run (run-only)"), *GearID.ToString());
    return true;
}

bool UPoolSubsystem::IsRunOnly(const FGuid &GearID) const
{
    const FPoolRunMark *Mark = RunMarks.Find(GearID);
    return Mark && Mark->bRunOnly;
}

bool UPoolSubsystem::IsOnRun(const FGuid &GearID) const
{
    const FPoolRunMark *Mark = RunMarks.Find(GearID);
    return Mark && Mark->bOnRun;
}

FPoolRunEndResult UPoolSubsystem::EndRunLayer(bool bKeepUpgrades)
{
    FPoolRunEndResult Result;
    if (!bRunLayerActive)
    {
        return Result;
    }
    // Closed before any change is heard: a listener that pushes a piece back mid-way finds no open layer to mark it in.
    const TMap<FGuid, FPoolRunMark> Marks = MoveTemp(RunMarks);
    RunMarks.Reset();
    RunStartItems.Reset(); // B-439b
    bRunLayerActive = false;

    for (const TPair<FGuid, FPoolRunMark> &Pair : Marks)
    {
        const FGuid &ID = Pair.Key;
        const FPoolRunMark &Mark = Pair.Value;
        if (Mark.bRunOnly)
        {
            // A piece sold or dismantled during the run is no longer here: its tier was remembered at the removal.
            const bool bFound = FindWeaponInPool(ID) || FindRingInPool(ID) || FindEvolutionInPool(ID);
            if (bFound)
            {
                Result.Remembered += bKeepUpgrades && RememberRunOnlyTier(ID) ? 1 : 0;
                RemoveGear(ID);
                ++Result.Dropped;
            }
            continue;
        }
        // A home piece: copies, since each update can move the arrays the pointers point into.
        if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(ID))
        {
            // B-098 (Q-098): only the attachment goes home; a weapon's spells are not tracked (deleted).
            if (RunLayerAttachmentDiffers(Weapon->AttachedItem, Mark.HomeAttached))
            {
                FWeaponInventoryEntry Home = *Weapon;
                Home.AttachedItem = Mark.HomeAttached;
                UpdateWeapon(Home);
                ++Result.Restored;
            }
        }
        else if (const FRingInventoryEntry *Adapter = FindRingInPool(ID))
        {
            if (RunLayerAttachmentDiffers(Adapter->AttachedItem, Mark.HomeAttached) || Adapter->AssignedSpells != Mark.HomeSpells)
            {
                FRingInventoryEntry Home = *Adapter;
                Home.AttachedItem = Mark.HomeAttached;
                Home.AssignedSpells = Mark.HomeSpells;
                UpdateRing(Home);
                ++Result.Restored;
            }
        }
        else if (const FEvolutionInventoryEntry *Evolution = FindEvolutionInPool(ID))
        {
            if (Evolution->CurrentDurability != Mark.HomeDurability)
            {
                FEvolutionInventoryEntry Home = *Evolution;
                Home.CurrentDurability = Mark.HomeDurability;
                UpdateEvolution(Home);
                ++Result.Restored;
            }
        }
        // A home piece on the run can't be sold or dismantled (UEconomyService refuses, Q-314b); one removed another
        // way stays gone.
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer closed: %d home piece(s) back to their home state, %d run-only piece(s) dropped, %d tier(s) remembered"),
           Result.Restored, Result.Dropped, Result.Remembered);
    return Result;
}

TMap<FCrystalId, int32> UPoolSubsystem::CountHeldItems() const
{
    TMap<FCrystalId, int32> Held;
    for (const TMap<FCrystalId, int32> *Stacks : {&Crystals, &Stones})
    {
        for (const TPair<FCrystalId, int32> &Stack : *Stacks)
        {
            Held.FindOrAdd(Stack.Key) += Stack.Value;
        }
    }
    for (const TPair<FName, FPoolPouch> &Pouch : Pouches)
    {
        for (const TPair<FCrystalId, int32> &Stack : Pouch.Value.Stacks)
        {
            Held.FindOrAdd(Stack.Key) += Stack.Value;
        }
    }
    return Held;
}

int32 UPoolSubsystem::TakeRunFoundItems()
{
    if (!bRunLayerActive)
    {
        return 0;
    }
    // shortcut: counted for the whole party, so which pouch loses a found item is by pouch order, not by who found
    // it; keep a count per pouch if that ever matters.
    TArray<FName> PouchKeys;
    Pouches.GetKeys(PouchKeys); // a copy: TakeCarried's broadcast reaches listeners
    int32 Taken = 0;
    bool bStorageChanged = false;
    for (const TPair<FCrystalId, int32> &Held : CountHeldItems())
    {
        int32 Found = Held.Value - RunStartItems.FindRef(Held.Key);
        for (int32 Index = 0; Index < PouchKeys.Num() && Found > 0; ++Index)
        {
            const int32 FromPouch = TakeCarried(PouchKeys[Index], Held.Key, Found);
            Found -= FromPouch;
            Taken += FromPouch;
        }
        if (Found > 0)
        {
            const int32 FromStorage = TakeStored(Held.Key, Found);
            Taken += FromStorage;
            bStorageChanged |= FromStorage > 0;
        }
    }
    if (bStorageChanged)
    {
        OnPoolItemsChanged.Broadcast();
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: the run was quit, %d item(s) found in it are taken back (D-176)"), Taken);
    return Taken;
}

int32 UPoolSubsystem::StoreRunFoundItems()
{
    if (!bRunLayerActive)
    {
        return 0;
    }
    // shortcut: counted for the whole party, as TakeRunFoundItems; a find that already went to storage (a full pouch)
    // still counts, so an item carried from before the run may go to storage in its place (never lost). Keep a count
    // per place if that ever matters.
    TArray<FName> PouchKeys;
    Pouches.GetKeys(PouchKeys); // a copy: MoveToStorage's broadcast reaches listeners
    int32 Moved = 0;
    for (const TPair<FCrystalId, int32> &Held : CountHeldItems())
    {
        int32 Found = Held.Value - RunStartItems.FindRef(Held.Key);
        for (int32 Index = 0; Index < PouchKeys.Num() && Found > 0; ++Index)
        {
            const int32 Carried = FMath::Min(Found, GetCarriedCount(PouchKeys[Index], Held.Key));
            if (Carried > 0 && MoveToStorage(PouchKeys[Index], Held.Key, Carried))
            {
                Found -= Carried;
                Moved += Carried;
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: the run ended, %d item(s) found in it go to home storage (D-097)"), Moved);
    return Moved;
}

FName UPoolSubsystem::GetDefinitionKey(const UObject *Definition)
{
    return Definition ? FName(*Definition->GetPathName()) : NAME_None;
}

bool UPoolSubsystem::FindRememberedTier(const UObject *Definition, EItemTier &OutTier) const
{
    const EItemTier *Remembered = UpgradeMemory.Find(GetDefinitionKey(Definition));
    if (!Remembered)
    {
        return false;
    }
    OutTier = *Remembered;
    return true;
}

bool UPoolSubsystem::RememberTier(const UObject *Definition, EItemTier Tier)
{
    const FName Key = GetDefinitionKey(Definition);
    if (Key.IsNone())
    {
        return false;
    }
    EItemTier *Remembered = UpgradeMemory.Find(Key);
    if (Remembered && TierHelpers::GetTierValue(*Remembered) >= TierHelpers::GetTierValue(Tier))
    {
        return false;
    }
    UpgradeMemory.Add(Key, Tier);
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: %s remembered at %s for the next run (D-097)"), *GetNameSafe(Definition),
           *TierHelpers::GetTierName(Tier));
    return true;
}

bool UPoolSubsystem::RememberRunOnlyTier(const FGuid &GearID)
{
    if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(GearID))
    {
        return RememberTier(Weapon->Weapon, Weapon->Tier);
    }
    if (const FRingInventoryEntry *Adapter = FindRingInPool(GearID))
    {
        return RememberTier(Adapter->Ring, Adapter->Tier);
    }
    if (const FEvolutionInventoryEntry *Evolution = FindEvolutionInPool(GearID))
    {
        return RememberTier(Evolution->Item, Evolution->Tier);
    }
    return false;
}

void UPoolSubsystem::NoteRunGearRemoved(const FGuid &GearID)
{
    const FPoolRunMark *Mark = RunMarks.Find(GearID);
    if (!Mark)
    {
        return;
    }
    // Q-314c: a run-only piece sold or dismantled before run end still counts as reached.
    if (Mark->bRunOnly)
    {
        RememberRunOnlyTier(GearID);
    }
    RunMarks.Remove(GearID);
}

bool UPoolSubsystem::ApplyUpgradeMemory(FWeaponInventoryEntry &Entry) const
{
    return RaiseToRememberedTier(*this, Entry);
}

bool UPoolSubsystem::ApplyUpgradeMemory(FRingInventoryEntry &Entry) const
{
    return RaiseToRememberedTier(*this, Entry);
}

bool UPoolSubsystem::ApplyUpgradeMemory(FEvolutionInventoryEntry &Entry) const
{
    return RaiseToRememberedTier(*this, Entry);
}

FString UPoolSubsystem::GetRunLayerString() const
{
    int32 OnRun = 0;
    int32 RunOnly = 0;
    for (const TPair<FGuid, FPoolRunMark> &Pair : RunMarks)
    {
        OnRun += Pair.Value.bOnRun ? 1 : 0;
        RunOnly += Pair.Value.bRunOnly ? 1 : 0;
    }
    FString Out = FString::Printf(TEXT("[Pool] Run layer (B-314): %s; %d home piece(s) on the run, %d run-only piece(s); %d definition(s) remembered"),
                                  bRunLayerActive ? TEXT("open") : TEXT("closed"), OnRun, RunOnly, UpgradeMemory.Num());
    const auto NameOf = [this](const FGuid &ID) -> FString
    {
        if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(ID))
        {
            return FString::Printf(TEXT("weapon %s %s"), *GetNameSafe(Weapon->Weapon), *TierHelpers::GetTierName(Weapon->Tier));
        }
        if (const FRingInventoryEntry *Adapter = FindRingInPool(ID))
        {
            return FString::Printf(TEXT("adapter %s %s"), *GetNameSafe(Adapter->Ring), *TierHelpers::GetTierName(Adapter->Tier));
        }
        if (const FEvolutionInventoryEntry *Evolution = FindEvolutionInPool(ID))
        {
            return FString::Printf(TEXT("evolution %s %s"), *GetNameSafe(Evolution->Item), *TierHelpers::GetTierName(Evolution->Tier));
        }
        return TEXT("<no longer in the store>");
    };
    for (const TPair<FGuid, FPoolRunMark> &Pair : RunMarks)
    {
        if (Pair.Value.bRunOnly)
        {
            Out += FString::Printf(TEXT("\n  run-only: %s %s (dropped at run end)"), *NameOf(Pair.Key), *Pair.Key.ToString());
        }
    }
    // B-439b: what a quit would take back now.
    for (const TPair<FCrystalId, int32> &Held : CountHeldItems())
    {
        const int32 Found = bRunLayerActive ? Held.Value - RunStartItems.FindRef(Held.Key) : 0;
        if (Found > 0)
        {
            Out += TEXT("\n") + FString::Printf(TEXT("  found in the run: %d %s (to storage at run end; taken back if the run is quit)"), Found, *ItemIdentity::GetDisplayName(Held.Key));
        }
    }
    for (const TPair<FName, EItemTier> &Remembered : UpgradeMemory)
    {
        Out += FString::Printf(TEXT("\n  remembered: %s at %s"), *FPaths::GetBaseFilename(Remembered.Key.ToString()),
                               *TierHelpers::GetTierName(Remembered.Value));
    }
    return Out;
}

#if !UE_BUILD_SHIPPING
namespace
{
    static FAutoConsoleCommandWithWorld GRunPrintKeptCmd(
        TEXT("wor.Run.PrintKept"),
        TEXT("B-314: the run layer (home pieces on the run, run-only gear dropped at run end) and the tiers remembered per gear definition (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.Run.PrintKept: no pool subsystem (no PIE world?)."));
                    return;
                }
                UE_LOG(LogTemp, Log, TEXT("%s"), *Pool->GetRunLayerString());
            }));
}
#endif // !UE_BUILD_SHIPPING
