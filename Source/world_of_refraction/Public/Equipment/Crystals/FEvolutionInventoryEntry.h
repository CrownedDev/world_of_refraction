// FEvolutionInventoryEntry.h
// Owned (unslotted) evolution item with stable per-instance identity.
//
// Distinct from FEvolutionAttachment: attachments live on the holder
// (FWeaponInventoryEntry / FRingInventoryEntry) and carry per-instance
// CurrentDurability with no GUID — slotted evolution items are destroyed on
// removal per locked design, so no instance-retrieval pathway is required.
//
// FEvolutionInventoryEntry is the inventory side: evolution items the
// character owns but has not yet slotted. Each entry carries an FGuid so
// trading APIs can move a specific instance between owners unambiguously
// when multiple copies of the same item asset are owned.

#pragma once

#include "CoreMinimal.h"
#include "FEvolutionInventoryEntry.generated.h"

class UEvolutionItemData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionInventoryEntry
{
    GENERATED_BODY()

    /** Stable per-instance identity. Generated when added to inventory.
     *  Does NOT transfer to FEvolutionAttachment when the item is slotted. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Evolution")
    FGuid InstanceID;

    /** Evolution item asset. Null in default-constructed entries. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Evolution")
    UEvolutionItemData *Item = nullptr;

    FEvolutionInventoryEntry() = default;

    explicit FEvolutionInventoryEntry(UEvolutionItemData *InItem)
        : InstanceID(FGuid::NewGuid()), Item(InItem) {}
};
