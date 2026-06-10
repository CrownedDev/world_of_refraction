#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/CombatConstants.h"
#include "Character/FPillarWeights.h"

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

    FEvolutionInventoryEntry Entry(Item);

    // U3a pickup roll — mirrors UInventoryComponent::ApplyPickupRoll for evolution:
    // when the asset opts in, the fresh OWNED instance rolls its Generated layers
    // from its own stored MaxPools (per-asset override or tier budget); pillar
    // percents roll alongside on the tier budget. The rolled state reaches the
    // slotted FEvolutionAttachment via the slot-time copy (U3b). Pools (charge
    // meters) start 0. Toggle-off: Generated* stay zero — authored Base only.
    if (Item->bRandomGenerateOnPickup)
    {
        Entry.StatMaxPool = (Item->StatMaxPoolOverride != CombatConstants::POOL_OVERRIDE_USE_TIER)
                                ? Item->StatMaxPoolOverride
                                : FEquipmentStatBonus::GetSubstatBudget(Item->Tier);
        Entry.ResistanceMaxPool = (Item->ResistanceMaxPoolOverride != CombatConstants::POOL_OVERRIDE_USE_TIER)
                                      ? Item->ResistanceMaxPoolOverride
                                      : FResistanceBonus::GetResistanceBudget(Item->Tier);

        Entry.GeneratedStatBonus.RerollSubstats(Entry.StatMaxPool, FPillarWeights());
        Entry.GeneratedStatBonus.RerollPillars(Item->Tier);
        Entry.GeneratedResistance.RerollResistance(Entry.ResistanceMaxPool);
    }

    Entries.Add(Entry);
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
