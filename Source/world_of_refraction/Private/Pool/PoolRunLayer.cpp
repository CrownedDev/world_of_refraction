// PoolRunLayer.cpp
// B-314 (D-111, D-097, Q-314): the run layer over the gear store. UPoolSubsystem's run-layer half, kept out of
// PoolSubsystem.cpp. A home piece taken on the run keeps its home attachment (and wear) in its run mark and gets it back
// at run end; gear found or bought in the run is run-only: dropped at run end with its tier remembered by definition.

#include "Pool/PoolSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
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

    /** B-509 (D-219): a home piece's socket after a run that keeps the run's sockets: the run's item at full wear (wear
     *  resets at run end, D-093), or the home state when it is the same item. */
    FRuntimeAttachedItem KeptRunAttachment(const FRuntimeAttachedItem &Now, const FRuntimeAttachedItem &Home)
    {
        if (Now == Home)
        {
            return Home;
        }
        FRuntimeAttachedItem Kept = Now;
        Kept.RepairBetweenCombats(Kept.GetMaxDurability());
        return Kept;
    }

    /** B-510: Entry's tier after a lost run (never below F). True when it changed. */
    template <class TEntry>
    bool DropLostRunTier(TEntry &Entry)
    {
        const EItemTier Lowered = TierHelpers::GetTierFromValue(TierHelpers::GetTierValue(Entry.Tier) - GearTierLoss::RANKS_LOST_ON_LOST_RUN);
        if (Lowered == Entry.Tier)
        {
            return false;
        }
        Entry.Tier = Lowered;
        return true;
    }

    /** B-525 (D-224, D-219): settles one run skill list into the hub's. A run skill slotted at the end that the hub
     *  doesn't know (a found one) joins it when bKeepsFound and there is room; the hub's copy of every slotted skill
     *  (owned, or just kept) drops tier when bDropsTier. TInstance is FSpellInstance or FAbilityInstance. */
    template <class TInstance, class TGetSkill>
    void SettleRunSkillList(TArray<TInstance> &Hub, const TArray<TInstance> &Run, const TSet<const UObject *> &Slotted, bool bKeepsFound,
                            bool bDropsTier, int32 Capacity, TGetSkill GetSkill, int32 &OutKept, int32 &OutLowered)
    {
        for (const TInstance &Instance : Run)
        {
            const UObject *Skill = GetSkill(Instance);
            if (!Skill || !Slotted.Contains(Skill))
            {
                continue; // not slotted: goes back to the pool, as found gear left in the inventory
            }
            int32 Index = Hub.IndexOfByPredicate([&](const TInstance &I) { return GetSkill(I) == Skill; });
            if (Index == INDEX_NONE)
            {
                if (!bKeepsFound || Hub.Num() >= Capacity)
                {
                    continue;
                }
                Index = Hub.Add(Instance);
                ++OutKept;
            }
            OutLowered += bDropsTier && DropLostRunTier(Hub[Index]) ? 1 : 0;
        }
    }

    /** The evolution instance in a socket, or an unset ID. */
    FGuid SocketedEvolutionID(const FRuntimeAttachedItem &Attached)
    {
        return Attached.IsEvolution() ? Attached.Evolution.InstanceID : FGuid();
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
    RunStartWornGear.Reset(); // B-508: SnapshotRunStartGear fills it
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
    RunStartWornGear.Reset();
    bRunLayerActive = false;
}

int32 UPoolSubsystem::RestoreRunLayer(const TMap<FGuid, FPoolRunMark> &SavedMarks)
{
    RunMarks.Reset();
    // B-439b: a save from before the counts were kept has none, so what is held now stands in (the save's restore
    // replaces it with the saved counts, RestoreRunStartItems).
    RunStartItems = CountHeldItems();
    RunStartWornGear.Reset(); // B-508: the save's restore puts its copy back (RestoreRunStartWornGear)
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

FPoolRunEndResult UPoolSubsystem::EndRunLayer(bool bKeepUpgrades, bool bKeepRunSockets, const TSet<FGuid> &KeepAsHubGear)
{
    FPoolRunEndResult Result;
    PutBackRunStartSkills(); // B-523 (D-224): the hub lists come back on every ending
    if (!bRunLayerActive)
    {
        return Result;
    }
    // Closed before any change is heard: a listener that pushes a piece back mid-way finds no open layer to mark it in.
    const TMap<FGuid, FPoolRunMark> Marks = MoveTemp(RunMarks);
    RunMarks.Reset();
    RunStartItems.Reset(); // B-439b
    RunStartWornGear.Reset(); // B-508: CloseRun takes its copy first
    bRunLayerActive = false;

    // B-509: a kept piece keeps the found evolution in its socket, as does a home piece keeping the run's sockets on
    // an ending that keeps found gear (CloseRun passes a non-empty KeepAsHubGear only for a Kept ending).
    TSet<FGuid> Keep;
    for (const TPair<FGuid, FPoolRunMark> &Pair : Marks)
    {
        const bool bKeptHere = Pair.Value.bRunOnly && KeepAsHubGear.Contains(Pair.Key);
        if (!bKeptHere && !(bKeepRunSockets && Pair.Value.bOnRun && KeepAsHubGear.Num() > 0))
        {
            continue;
        }
        if (bKeptHere)
        {
            Keep.Add(Pair.Key);
        }
        const FWeaponInventoryEntry *Weapon = FindWeaponInPool(Pair.Key);
        const FRingInventoryEntry *Adapter = Weapon ? nullptr : FindRingInPool(Pair.Key);
        const FGuid Socketed = Weapon ? SocketedEvolutionID(Weapon->AttachedItem) : Adapter ? SocketedEvolutionID(Adapter->AttachedItem) : FGuid();
        if (Socketed.IsValid())
        {
            Keep.Add(Socketed);
        }
    }
    // A home socket keeps the run's item unless it is a found evolution that leaves (a run out of wipes).
    const auto KeepsRunSocket = [bKeepRunSockets, &Marks, &Keep](const FRuntimeAttachedItem &Now)
    {
        const FGuid Socketed = SocketedEvolutionID(Now);
        const FPoolRunMark *SocketedMark = Socketed.IsValid() ? Marks.Find(Socketed) : nullptr;
        return bKeepRunSockets && !(SocketedMark && SocketedMark->bRunOnly && !Keep.Contains(Socketed));
    };

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
                if (Keep.Contains(ID)) // B-509 (D-221): worn at an extract or a boss win: hub gear
                {
                    HubGear.Add(ID);
                    ++Result.KeptAsHubGear;
                    continue;
                }
                RemoveGear(ID);
                ++Result.Dropped;
            }
            continue;
        }
        // A home piece: copies, since each update can move the arrays the pointers point into.
        if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(ID))
        {
            // B-098 (Q-098): only the attachment goes home; a weapon's spells are not tracked (deleted).
            const FRuntimeAttachedItem After = KeepsRunSocket(Weapon->AttachedItem) ? KeptRunAttachment(Weapon->AttachedItem, Mark.HomeAttached) : Mark.HomeAttached;
            if (RunLayerAttachmentDiffers(Weapon->AttachedItem, After))
            {
                FWeaponInventoryEntry Home = *Weapon;
                Home.AttachedItem = After;
                UpdateWeapon(Home);
                ++Result.Restored;
            }
        }
        else if (const FRingInventoryEntry *Adapter = FindRingInPool(ID))
        {
            const bool bKeepsSocket = KeepsRunSocket(Adapter->AttachedItem); // B-509: its spells ride with its socket
            const FRuntimeAttachedItem After = bKeepsSocket ? KeptRunAttachment(Adapter->AttachedItem, Mark.HomeAttached) : Mark.HomeAttached;
            const TArray<USpellData *> &Spells = bKeepsSocket ? Adapter->AssignedSpells : Mark.HomeSpells;
            if (RunLayerAttachmentDiffers(Adapter->AttachedItem, After) || Adapter->AssignedSpells != Spells)
            {
                FRingInventoryEntry Home = *Adapter;
                Home.AttachedItem = After;
                Home.AssignedSpells = Spells;
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
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer closed (%s): %d home piece(s) changed, %d run-only piece(s) dropped, %d kept as hub gear, %d tier(s) remembered"),
           bKeepRunSockets ? TEXT("the run's sockets stay") : TEXT("home states back"), Result.Restored, Result.Dropped,
           Result.KeptAsHubGear, Result.Remembered);
    return Result;
}

void UPoolSubsystem::SnapshotRunStartGear(const TSet<FName> &CharacterKeys)
{
    RunStartWornGear.Reset();
    for (const FName &Key : CharacterKeys)
    {
        if (const FWornGear *Worn = WornGear.Find(Key))
        {
            RunStartWornGear.Add(Key, *Worn);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run layer: %d worn-gear record(s) kept as the run found them; a quit puts them back (D-219)"),
           RunStartWornGear.Num());
}

void UPoolSubsystem::CloseRun(EPoolRunEnding Ending)
{
    if (!bRunLayerActive)
    {
        return;
    }
    const bool bKept = Ending == EPoolRunEnding::Kept;
    if (Ending != EPoolRunEnding::Quit)
    {
        // B-509: the run's sockets stay, so an item a home socket held at the start and gave up in the run is the
        // player's own, not a find. shortcut: an item swapped out (destroyed, CL-28b) also counts, so one find of that
        // item can stay; count destroyed items if that ever matters.
        for (const TPair<FGuid, FPoolRunMark> &Pair : RunMarks)
        {
            const FRuntimeAttachedItem &Home = Pair.Value.HomeAttached;
            const FWeaponInventoryEntry *Weapon = Pair.Value.bOnRun ? FindWeaponInPool(Pair.Key) : nullptr;
            const FRingInventoryEntry *Adapter = Pair.Value.bOnRun && !Weapon ? FindRingInPool(Pair.Key) : nullptr;
            const FRuntimeAttachedItem *Now = Weapon ? &Weapon->AttachedItem : Adapter ? &Adapter->AttachedItem : nullptr;
            if (Now && (Home.IsCrystal() || Home.IsAugmentStone()) && !(*Now == Home))
            {
                ++RunStartItems.FindOrAdd(StatStone::Normalize(Home.Crystal.Id)); // CountHeldItems' keys
            }
        }
    }
    // D-250: a kept run's finds stay where they are, carried. StoreRunFoundItems (B-238b: to home storage) is
    // OBSOLETE and no longer called.
    if (!bKept)
    {
        TakeRunFoundItems(); // B-439b (Q-439 point 2, D-176) and B-451 (D-189): a quit or a lost run loses its finds
    }
    const TMap<FName, FWornGear> RunStart = MoveTemp(RunStartWornGear);
    if (!bKept)
    {
        // B-510 (D-219): before anything goes back, so it is what the party wears at the end (a closed game: as saved).
        for (const TPair<FName, FWornGear> &Start : RunStart)
        {
            ApplyLostRunTierDrop(Start.Key);
        }
    }
    SettleRunSkills(Ending); // B-525: while the records are still what the party wears at the end
    // B-509 (D-219, D-221): every ending but a quit keeps the run's sockets; only a kept ending keeps worn found gear.
    TSet<FGuid> WornNow;
    if (bKept)
    {
        for (const TPair<FName, FWornGear> &Record : WornGear)
        {
            WornNow.Append(Record.Value.GetPieceIDs());
        }
    }
    // B-451: a lost or quit run keeps no upgrade levels.
    EndRunLayer(/*bKeepUpgrades=*/bKept, /*bKeepRunSockets=*/Ending != EPoolRunEnding::Quit, WornNow);
    if (Ending != EPoolRunEnding::Quit)
    {
        return;
    }
    // D-219: a quit's progress doesn't count, so each record goes back to how the run found it (pick included).
    int32 PutBack = 0;
    for (const TPair<FName, FWornGear> &Start : RunStart)
    {
        const FWornGear *Now = WornGear.Find(Start.Key);
        if (Now && Now->SameAs(Start.Value))
        {
            continue;
        }
        WornGear.Add(Start.Key, Start.Value);
        OnWornGearChanged.Broadcast(Start.Key);
        ++PutBack;
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Run quit: %d worn-gear record(s) put back as the run found them (D-219)"), PutBack);
}

int32 UPoolSubsystem::SettleRunSkills(EPoolRunEnding Ending)
{
    const bool bKeepsFound = Ending != EPoolRunEnding::Quit;
    const bool bDropsTier = Ending != EPoolRunEnding::Kept;
    int32 Kept = 0;
    int32 Lowered = 0;
    for (TPair<FName, FPoolSkills> &Pair : RunStartSkills)
    {
        const FPoolSkills *Run = CharacterSkills.Find(Pair.Key);
        const FWornGear *Worn = WornGear.Find(Pair.Key);
        if (!Run || !Worn)
        {
            continue;
        }
        // Slotted: a pick in a slot of a worn entry (B-517), of a skill in the run list. Innate, Broken Darkness and
        // evolution spells in their own places are not gear picks; one also picked into a gear slot is (Q-525).
        TSet<const UObject *> Slotted;
        for (const FLoadoutSlot &Slot : Worn->Equipment)
        {
            for (const UAbilityData *Ability : Slot.AssignedAbilities)
            {
                Slotted.Add(Ability);
            }
            for (const USpellData *Spell : Slot.SpellOverrides)
            {
                Slotted.Add(Spell);
            }
        }
        Slotted.Remove(nullptr);
        SettleRunSkillList(Pair.Value.Spells, Run->Spells, Slotted, bKeepsFound, bDropsTier, InventoryConstants::MAX_LEARNED_SPELLS,
                           [](const FSpellInstance &I) -> const UObject * { return I.Spell; }, Kept, Lowered);
        SettleRunSkillList(Pair.Value.Abilities, Run->Abilities, Slotted, bKeepsFound, bDropsTier, InventoryConstants::MAX_LEARNED_ABILITIES,
                           [](const FAbilityInstance &I) -> const UObject * { return I.Ability; }, Kept, Lowered);
    }
    if (!RunStartSkills.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[Pool] Run skills settled (D-224): %d slotted found skill(s) kept, %d slotted skill(s) drop %d tier(s)"),
               Kept, Lowered, GearTierLoss::RANKS_LOST_ON_LOST_RUN);
    }
    return Kept;
}

int32 UPoolSubsystem::ApplyLostRunTierDrop(FName CharacterKey)
{
    const FWornGear *Worn = WornGear.Find(CharacterKey);
    if (!Worn)
    {
        return 0;
    }
    int32 Dropped = 0;
    for (const FGuid &ID : Worn->GetPieceIDs())
    {
        // Copies, as EndRunLayer: each update broadcasts, and a listener may move the arrays.
        if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(ID))
        {
            FWeaponInventoryEntry Lowered = *Weapon;
            if (DropLostRunTier(Lowered))
            {
                UpdateWeapon(Lowered);
                ++Dropped;
            }
        }
        else if (const FRingInventoryEntry *Adapter = FindRingInPool(ID))
        {
            FRingInventoryEntry Lowered = *Adapter;
            if (DropLostRunTier(Lowered))
            {
                UpdateRing(Lowered);
                ++Dropped;
            }
        }
        else if (const FEvolutionInventoryEntry *Evolution = FindEvolutionInPool(ID))
        {
            FEvolutionInventoryEntry Lowered = *Evolution;
            if (DropLostRunTier(Lowered))
            {
                UpdateEvolution(Lowered);
                ++Dropped;
            }
        }
        else if (FApparelEntry *Apparel = OwnedApparel.FindByPredicate([&ID](const FApparelEntry &E) { return E.PersistentID == ID; }))
        {
            if (DropLostRunTier(*Apparel))
            {
                OnPoolGearChanged.Broadcast(ID);
                ++Dropped;
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Lost run: %s's %d worn piece(s) drop %d tier(s), never below F (D-219)"),
           *FPaths::GetBaseFilename(CharacterKey.ToString()), Dropped, GearTierLoss::RANKS_LOST_ON_LOST_RUN);
    return Dropped;
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
    for (const TPair<FName, FWornGear> &Start : RunStartWornGear) // B-508
    {
        Out += FString::Printf(TEXT("\n  run start: %s wore %d piece(s) (put back if the run is quit)"),
                               *FPaths::GetBaseFilename(Start.Key.ToString()), Start.Value.GetPieceIDs().Num());
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
