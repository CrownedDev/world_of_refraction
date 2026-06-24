// EvolutionInventoryComponent.h
// Instance-based inventory for evolution items the character owns but has
// not yet slotted onto a weapon, ring, or character.
//
// Hard cap MAX_EVOLUTION_ITEMS (5). Each entry carries an FGuid so trading
// APIs (later commits) can move a specific instance unambiguously when
// multiple copies of the same evolution asset are owned.
//
// Slotted evolution items live in FEvolutionAttachment on the holder
// (FWeaponInventoryEntry::AttachedItem / FRingInventoryEntry::AttachedItem),
// NOT here. Slotted evolutions are destroyed on removal per locked design
// — they do not return to this component.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "EvolutionInventoryComponent.generated.h"

class UEvolutionItemData;

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UEvolutionInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEvolutionInventoryComponent();

    /** Owned, unslotted evolution items. Order is insertion order. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Inventory|Evolution")
    TArray<FEvolutionInventoryEntry> Entries;

    /** Append Item as a new entry with a freshly-generated InstanceID.
     *  Returns false when Item is null or the hard cap is reached. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool AddInstance(UEvolutionItemData *Item);

    /** True iff an owned (unslotted) entry references Item (pointer identity).
     *  Returns false when Item is null. Ownership query for loadout validation
     *  and inventory inspection — replaces the legacy Items-pool HasItem read. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool HasInstance(UEvolutionItemData *Item) const;

    /** Remove the owned instance with this InstanceID. Returns true iff an entry was found and
     *  removed. The first evolution-inventory remove path — used by DismantleEvolution
     *  (remove-then-grant) and the primary-remove action. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool RemoveInstance(FGuid InstanceID);

    /** Current entry count. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Evolution")
    int32 Num() const { return Entries.Num(); }
};
