#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryConstants.h"

UEvolutionInventoryComponent::UEvolutionInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEvolutionInventoryComponent::AddInstance(UEvolutionItemData *Item)
{
    if (!Item || Entries.Num() >= InventoryConstants::MAX_EVOLUTION_ITEMS)
    {
        return false;
    }
    Entries.Add(FEvolutionInventoryEntry(Item));
    return true;
}

bool UEvolutionInventoryComponent::HasInstance(UEvolutionItemData *Item) const
{
    if (!Item)
    {
        return false;
    }
    for (const FEvolutionInventoryEntry &Entry : Entries)
    {
        if (Entry.Item == Item)
        {
            return true;
        }
    }
    return false;
}
