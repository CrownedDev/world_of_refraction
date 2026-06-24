#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/CombatConstants.h"
#include "Character/FPillarWeights.h"
#include "Character/CharacterDataComponent.h" // owner Luck for the pickup quality roll
#include "Currency/EconomyYield.h"            // RollQuality (§11 weighted drop curve)
#include "Inventory/InventoryComponent.h"     // run weapons/rings for the gear-locked evo count
#include "Loadout/Entries/FWeaponInventoryEntry.h" // AttachedItem.IsEvolution()
#include "Loadout/Entries/FRingInventoryEntry.h"

namespace
{
    /** Owner's normalized Luck for the pickup quality roll (0 when no character data resolves).
     *  Mirrors UInventoryComponent's helper of the same name — same GetEquipmentModifiedLuck read. */
    float ResolveEvolutionOwnerLuck(const UActorComponent *Comp)
    {
        if (Comp)
        {
            if (const AActor *Owner = Comp->GetOwner())
            {
                if (const UCharacterDataComponent *CDC = Owner->FindComponentByClass<UCharacterDataComponent>())
                {
                    return CDC->GetEquipmentModifiedLuck();
                }
            }
        }
        return 0.0f;
    }
}

UEvolutionInventoryComponent::UEvolutionInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEvolutionInventoryComponent::AddInstance(UEvolutionItemData *Item)
{
    // Cap is the RUN total (owned entries + authored gear-locked evolutions), not just Entries —
    // can't acquire a 6th. Escape valves (DismantleEvolution/RemoveInstance, future sell) free a slot.
    if (!Item || CountRunEvolutions() >= InventoryConstants::MAX_EVOLUTION_ITEMS)
    {
        return false;
    }

    FEvolutionInventoryEntry Entry(Item);
    // Cluster 1c (tier-on-instance foundation): seed per-instance Tier from the asset + placeholder
    // Quality (weighted roll lands later). Written-but-unread — reads still use Item->Tier until the
    // cluster-2 repoint, so this is a behavioural no-op.
    Entry.Tier = Item->Tier;
    Entry.Quality = EItemQuality::C_Quality;
    Entry.CurrentDurability = Item->MaxDurability; // fresh evo starts at full durability (gear-durability)

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

        // Fresh pickup → roll a per-instance Quality (§11), Luck-biased. Toggle-OFF acquisitions
        // keep the C_Quality placeholder seeded above.
        Entry.Quality = EconomyYield::RollQuality(ResolveEvolutionOwnerLuck(this));
    }

    Entries.Add(Entry);
    return true;
}

bool UEvolutionInventoryComponent::RemoveInstance(FGuid InstanceID)
{
    const int32 Index = Entries.IndexOfByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
    if (Index == INDEX_NONE)
    {
        return false;
    }
    Entries.RemoveAt(Index);
    return true;
}

int32 UEvolutionInventoryComponent::CountRunEvolutions() const
{
    // Owned total: bag + primary-slotted + player-gear-attached are ALL FEvolutionInventoryEntry
    // (attach = reference, not move — §5.3b — so they persist in Entries), counted once here.
    int32 Count = Entries.Num();

    // PLUS authored/LOCKED evolutions baked into run gear (the weapon/ring asset's AttachedItem) —
    // these are NOT owned entries, so they are additional. A gear evolution counts here ONLY when its
    // AttachedItem.Evolution.InstanceID is INVALID (authored-locked — no owned-entry link). A VALID
    // InstanceID means the evo is PLAYER-attached (references an owned Entry, already counted above) —
    // skip it, or it would double-count (gear-i / gear-cap; the reference model, §5.3b).
    if (const AActor *Owner = GetOwner())
    {
        if (const UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>())
        {
            for (const FWeaponInventoryEntry &W : Inv->Weapons)
            {
                if (W.AttachedItem.IsEvolution() && !W.AttachedItem.Evolution.InstanceID.IsValid())
                {
                    ++Count;
                }
            }
            for (const FRingInventoryEntry &R : Inv->Rings)
            {
                if (R.AttachedItem.IsEvolution() && !R.AttachedItem.Evolution.InstanceID.IsValid())
                {
                    ++Count;
                }
            }
        }
    }
    return Count;
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
