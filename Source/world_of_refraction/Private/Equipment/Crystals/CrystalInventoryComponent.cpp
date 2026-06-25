#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Inventory/InventoryConstants.h"

UCrystalInventoryComponent::UCrystalInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ==================== POOL DISPATCH ====================
// IsGemType(Type) selects the gem vs stone pool (gem → Gems, stone → Stones). One
// dispatch, in one place — the former item/refined axis is gone (gems are dual-purpose).

TMap<FCrystalId, int32> &UCrystalInventoryComponent::PoolFor(ECrystalType Type)
{
    return CrystalTypeHelpers::IsGemType(Type) ? Gems : Stones;
}

const TMap<FCrystalId, int32> &UCrystalInventoryComponent::PoolFor(ECrystalType Type) const
{
    return CrystalTypeHelpers::IsGemType(Type) ? Gems : Stones;
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
    if (Id.Type == ECrystalType::None)
    {
        return false;
    }
    if (!CanAddCount(Id, Count))
    {
        return false;
    }
    PoolFor(Id.Type).FindOrAdd(Id) += Count;
    return true;
}

int32 UCrystalInventoryComponent::RemoveCount(FCrystalId Id, int32 Count)
{
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    if (Count <= 0)
    {
        return 0;
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
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    const int32 *Found = PoolFor(Id.Type).Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetCountForTier(EItemTier Tier) const
{
    // Combined gem + stone count at Tier — the aggregate display total. Per-pool cap
    // checks use SumAtTier on a single pool, not this sum.
    return SumAtTier(Gems, Tier) + SumAtTier(Stones, Tier);
}

int32 UCrystalInventoryComponent::GetTotalCount() const
{
    int32 Total = 0;
    for (const TMap<FCrystalId, int32> *Pool : {&Gems, &Stones})
    {
        for (const TPair<FCrystalId, int32> &Pair : *Pool)
        {
            Total += Pair.Value;
        }
    }
    return Total;
}

int32 UCrystalInventoryComponent::GetStackCount() const
{
    return Gems.Num() + Stones.Num();
}

void UCrystalInventoryComponent::ClearAll()
{
    Gems.Empty();
    Stones.Empty();
}

// ==================== CAPS ====================
// Per-pool: the cap check sums ONLY the pool the Id routes to (Gems or Stones), so
// gems and stones each get a full CRYSTAL_PER_TIER_CAP per tier, independently.

bool UCrystalInventoryComponent::CanAddCount(FCrystalId Id, int32 Count) const
{
    if (Count <= 0)
    {
        return false;
    }
    return SumAtTier(PoolFor(Id.Type), Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}
