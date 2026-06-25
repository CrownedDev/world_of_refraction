#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Inventory/InventoryConstants.h"

UCrystalInventoryComponent::UCrystalInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ==================== POOL DISPATCH ====================
// IsGemType(Type) selects the gem vs stone container; the item/refined axis is
// fixed by which overload the caller is in. Every Id-keyed method below routes
// through these, so the gem/stone branch lives in exactly one place per axis.

TMap<FCrystalId, int32> &UCrystalInventoryComponent::ItemPoolFor(ECrystalType Type)
{
    return CrystalTypeHelpers::IsGemType(Type) ? GemItem : StoneItem;
}

const TMap<FCrystalId, int32> &UCrystalInventoryComponent::ItemPoolFor(ECrystalType Type) const
{
    return CrystalTypeHelpers::IsGemType(Type) ? GemItem : StoneItem;
}

TMap<FCrystalId, int32> *UCrystalInventoryComponent::RefinedPoolFor(ECrystalType Type)
{
    // Gem-only: stones have no refined form → nullptr (callers reject).
    return CrystalTypeHelpers::IsGemType(Type) ? &GemRefined : nullptr;
}

const TMap<FCrystalId, int32> *UCrystalInventoryComponent::RefinedPoolFor(ECrystalType Type) const
{
    return CrystalTypeHelpers::IsGemType(Type) ? &GemRefined : nullptr;
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

bool UCrystalInventoryComponent::AddItemCount(FCrystalId Id, int32 Count)
{
    if (Id.Type == ECrystalType::None)
    {
        return false;
    }
    if (!CanAddItemCount(Id, Count))
    {
        return false;
    }
    ItemPoolFor(Id.Type).FindOrAdd(Id) += Count;
    return true;
}

bool UCrystalInventoryComponent::AddRefinedCount(FCrystalId Id, int32 Count)
{
    if (Id.Type == ECrystalType::None)
    {
        return false;
    }
    if (!CanAddRefinedCount(Id, Count))
    {
        return false;
    }
    TMap<FCrystalId, int32> *Pool = RefinedPoolFor(Id.Type);
    if (!Pool) // stones have no refined pool — reject
    {
        return false;
    }
    Pool->FindOrAdd(Id) += Count;
    return true;
}

int32 UCrystalInventoryComponent::RemoveItemCount(FCrystalId Id, int32 Count)
{
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    if (Count <= 0)
    {
        return 0;
    }
    TMap<FCrystalId, int32> &Pool = ItemPoolFor(Id.Type);
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

int32 UCrystalInventoryComponent::RemoveRefinedCount(FCrystalId Id, int32 Count)
{
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    if (Count <= 0)
    {
        return 0;
    }
    TMap<FCrystalId, int32> *Pool = RefinedPoolFor(Id.Type);
    if (!Pool) // stones have no refined pool — nothing to remove
    {
        return 0;
    }
    int32 *Found = Pool->Find(Id);
    if (!Found)
    {
        return 0;
    }
    const int32 Removed = FMath::Min(*Found, Count);
    *Found -= Removed;
    if (*Found <= 0)
    {
        Pool->Remove(Id);
    }
    return Removed;
}

// ==================== READ ====================

int32 UCrystalInventoryComponent::GetItemCount(FCrystalId Id) const
{
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    const int32 *Found = ItemPoolFor(Id.Type).Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetRefinedCount(FCrystalId Id) const
{
    if (Id.Type == ECrystalType::None)
    {
        return 0;
    }
    const TMap<FCrystalId, int32> *Pool = RefinedPoolFor(Id.Type);
    if (!Pool) // stones have no refined pool
    {
        return 0;
    }
    const int32 *Found = Pool->Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetItemCountForTier(EItemTier Tier) const
{
    // Combined gem+stone item count at Tier — the aggregate display total. The
    // per-container cap check uses SumAtTier on a single pool, not this sum.
    return SumAtTier(GemItem, Tier) + SumAtTier(StoneItem, Tier);
}

int32 UCrystalInventoryComponent::GetRefinedCountForTier(EItemTier Tier) const
{
    return SumAtTier(GemRefined, Tier); // refined is gem-only (stones have no refined form)
}

int32 UCrystalInventoryComponent::GetTotalCount() const
{
    int32 Total = 0;
    for (const TMap<FCrystalId, int32> *Pool : {&GemItem, &GemRefined, &StoneItem})
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
    return GemItem.Num() + GemRefined.Num() + StoneItem.Num();
}

void UCrystalInventoryComponent::ClearAll()
{
    GemItem.Empty();
    GemRefined.Empty();
    StoneItem.Empty();
}

// ==================== CAPS ====================
// Per-container: the cap check sums ONLY the pool the Id routes to (gem item, gem
// refined, stone item, or stone refined), so gems and stones each get a full
// CRYSTAL_PER_TIER_CAP per tier, independently.

bool UCrystalInventoryComponent::CanAddItemCount(FCrystalId Id, int32 Count) const
{
    if (Count <= 0)
    {
        return false;
    }
    return SumAtTier(ItemPoolFor(Id.Type), Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}

bool UCrystalInventoryComponent::CanAddRefinedCount(FCrystalId Id, int32 Count) const
{
    if (Count <= 0)
    {
        return false;
    }
    const TMap<FCrystalId, int32> *Pool = RefinedPoolFor(Id.Type);
    if (!Pool) // stones have no refined pool — cannot add a refined stone
    {
        return false;
    }
    return SumAtTier(*Pool, Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}
