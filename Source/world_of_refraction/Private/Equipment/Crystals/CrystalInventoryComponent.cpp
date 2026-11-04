#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Inventory/InventoryConstants.h"
#include "Pool/PoolSubsystem.h"

UCrystalInventoryComponent::UCrystalInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ==================== POOL DISPATCH ====================
// IsGemType(Type) selects the gem vs stone pool (gem → Crystals, stone → Stones). One
// dispatch, in one place — the former item/refined axis is gone (crystals are dual-purpose).

TMap<FCrystalId, int32> &UCrystalInventoryComponent::PoolFor(ECrystalType Type)
{
    return CrystalTypeHelpers::IsGemType(Type) ? Crystals : Stones;
}

const TMap<FCrystalId, int32> &UCrystalInventoryComponent::PoolFor(ECrystalType Type) const
{
    return CrystalTypeHelpers::IsGemType(Type) ? Crystals : Stones;
}

int32 UCrystalInventoryComponent::SumAtTier(const TMap<FCrystalId, int32> &Pool, EItemTier Tier)
{
    int32 Total = 0;
    for (const TPair<FCrystalId, int32> &Pair : Pool)
    {
        if (Pair.Key.Tier == Tier)
        {
            Total += Pair.Value;
        }
    }
    return Total;
}

// ==================== WRITE ====================

bool UCrystalInventoryComponent::AddCount(FCrystalId Id, int32 Count)
{
    Id = StatStone::Normalize(Id); // B-353 (D-142): an old single-stat stone is a StatStone
    if (Id.Type == ECrystalType::None)
    {
        return false;
    }
    if (!CanAddCount(Id, Count))
    {
        return false;
    }
    if (UPoolSubsystem *Pool = SharedPool.Get())
    {
        Pool->CarryOrStore(PouchKey, Id, Count); // OnPoolItemsChanged refreshes this mirror
        return true;
    }
    PoolFor(Id.Type).FindOrAdd(Id) += Count;
    return true;
}

int32 UCrystalInventoryComponent::RemoveCount(FCrystalId Id, int32 Count)
{
    Id = StatStone::Normalize(Id); // B-353 (D-142): an old single-stat stone is a StatStone
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    if (Count <= 0)
    {
        return 0;
    }
    if (UPoolSubsystem *Shared = SharedPool.Get())
    {
        return Shared->TakeCarried(PouchKey, Id, Count);
    }
    TMap<FCrystalId, int32> &Pool = PoolFor(Id.Type);
    int32 *Found = Pool.Find(Id);
    if (!Found)
    {
        return 0;
    }
    const int32 Removed = FMath::Min(*Found, Count);
    *Found -= Removed;
    if (*Found <= 0)
    {
        Pool.Remove(Id);
    }
    return Removed;
}

// ==================== READ ====================

int32 UCrystalInventoryComponent::GetCount(FCrystalId Id) const
{
    Id = StatStone::Normalize(Id); // B-353 (D-142): an old single-stat stone is a StatStone
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    const TMap<FCrystalId, int32> *Pouch = GetPouchStacks();
    const int32 *Found = (Pouch ? *Pouch : PoolFor(Id.Type)).Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetCountForTier(EItemTier Tier) const
{
    // Combined gem + stone count at Tier — the aggregate display total. Per-pool cap
    // checks use SumAtTier on a single pool, not this sum.
    if (const TMap<FCrystalId, int32> *Pouch = GetPouchStacks())
    {
        return SumAtTier(*Pouch, Tier); // the pouch holds gems and stones in one map
    }
    return SumAtTier(Crystals, Tier) + SumAtTier(Stones, Tier);
}

int32 UCrystalInventoryComponent::GetTotalCount() const
{
    int32 Total = 0;
    for (const TPair<FCrystalId, int32> &Pair : GetHeldStacks())
    {
        Total += Pair.Value;
    }
    return Total;
}

int32 UCrystalInventoryComponent::GetStackCount() const
{
    const TMap<FCrystalId, int32> *Pouch = GetPouchStacks();
    return Pouch ? Pouch->Num() : Crystals.Num() + Stones.Num();
}

TMap<FCrystalId, int32> UCrystalInventoryComponent::GetHeldStacks() const
{
    if (const TMap<FCrystalId, int32> *Pouch = GetPouchStacks())
    {
        return *Pouch;
    }
    TMap<FCrystalId, int32> Held = Crystals;
    Held.Append(Stones);
    return Held;
}

const TMap<FCrystalId, int32> *UCrystalInventoryComponent::GetPouchStacks() const
{
    const UPoolSubsystem *Pool = SharedPool.Get();
    return Pool ? &Pool->GetCarriedItems(PouchKey) : nullptr;
}

void UCrystalInventoryComponent::ClearAll()
{
    // A re-init refills from the authored asset: that must not reach the party's shared items.
    UnbindSharedItems();
    Crystals.Empty();
    Stones.Empty();
}

// ==================== CAPS ====================
// Per-pool: the cap check sums ONLY the pool the Id routes to (Crystals or Stones), so
// gems and stones each get a full CRYSTAL_PER_TIER_CAP per tier, independently.

bool UCrystalInventoryComponent::CanAddCount(FCrystalId Id, int32 Count) const
{
    Id = StatStone::Normalize(Id); // B-353 (D-142): an old single-stat stone is a StatStone
    if (Count <= 0)
    {
        return false;
    }
    // D-097: shared items never run out of room; what doesn't fit the slots is stored.
    if (SharedPool.IsValid())
    {
        return true;
    }
    return SumAtTier(PoolFor(Id.Type), Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}

// ==================== THE CHARACTER'S POUCH (B-203, B-322) ====================

void UCrystalInventoryComponent::BindSharedItems(UPoolSubsystem *Pool, FName CharacterKey)
{
    if (!Pool)
    {
        return;
    }
    SharedPool = Pool;
    PouchKey = CharacterKey;
    Pool->OnPoolItemsChanged.AddUniqueDynamic(this, &UCrystalInventoryComponent::HandlePoolItemsChanged);
    HandlePoolItemsChanged();
}

void UCrystalInventoryComponent::UnbindSharedItems()
{
    if (UPoolSubsystem *Pool = SharedPool.Get())
    {
        Pool->OnPoolItemsChanged.RemoveDynamic(this, &UCrystalInventoryComponent::HandlePoolItemsChanged);
    }
    SharedPool.Reset();
    PouchKey = NAME_None;
}

void UCrystalInventoryComponent::HandlePoolItemsChanged()
{
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool)
    {
        return;
    }
    Crystals.Empty();
    Stones.Empty();
    for (const TPair<FCrystalId, int32> &Pair : Pool->GetCarriedItems(PouchKey))
    {
        PoolFor(Pair.Key.Type).Add(Pair.Key, Pair.Value);
    }
}

void UCrystalInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindSharedItems();
    Super::EndPlay(EndPlayReason);
}
