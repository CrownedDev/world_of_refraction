#include "EvolutionInventoryComponent.h"
#include "InventoryConstants.h"

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
