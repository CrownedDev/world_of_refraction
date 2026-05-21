#include "CrystalInventoryComponent.h"
#include "InventoryConstants.h"

UCrystalInventoryComponent::UCrystalInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ==================== WRITE ====================

bool UCrystalInventoryComponent::AddItemCount(FCrystalId Id, int32 Count)
{
    if (!CanAddItemCount(Id, Count))
    {
        return false;
    }
    ItemCrystals.FindOrAdd(Id) += Count;
    return true;
}

bool UCrystalInventoryComponent::AddRefinedCount(FCrystalId Id, int32 Count)
{
    if (!CanAddRefinedCount(Id, Count))
    {
        return false;
    }
    RefinedCrystals.FindOrAdd(Id) += Count;
    return true;
}

// ==================== READ ====================

int32 UCrystalInventoryComponent::GetItemCount(FCrystalId Id) const
{
    const int32 *Found = ItemCrystals.Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetRefinedCount(FCrystalId Id) const
{
    const int32 *Found = RefinedCrystals.Find(Id);
    return Found ? *Found : 0;
}

int32 UCrystalInventoryComponent::GetItemCountForTier(EItemTier Tier) const
{
    int32 Total = 0;
    for (const TPair<FCrystalId, int32> &Pair : ItemCrystals)
    {
        if (Pair.Key.Tier == Tier)
        {
            Total += Pair.Value;
        }
    }
    return Total;
}

int32 UCrystalInventoryComponent::GetRefinedCountForTier(EItemTier Tier) const
{
    int32 Total = 0;
    for (const TPair<FCrystalId, int32> &Pair : RefinedCrystals)
    {
        if (Pair.Key.Tier == Tier)
        {
            Total += Pair.Value;
        }
    }
    return Total;
}

// ==================== CAPS ====================

bool UCrystalInventoryComponent::CanAddItemCount(FCrystalId Id, int32 Count) const
{
    if (Count <= 0)
    {
        return false;
    }
    return GetItemCountForTier(Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}

bool UCrystalInventoryComponent::CanAddRefinedCount(FCrystalId Id, int32 Count) const
{
    if (Count <= 0)
    {
        return false;
    }
    return GetRefinedCountForTier(Id.Tier) + Count <= InventoryConstants::CRYSTAL_PER_TIER_CAP;
}
