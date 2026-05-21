// FEquippedItemSlot.h
// Designer-authored entry inside FSavedLoadout::EquippedItems. Holds a
// FCrystalId identity that persists even when Quantity reaches 0 (the empty
// slot stays in the array so it can be refilled between combats).
//
// Slot uniqueness across a loadout is enforced by ECrystalType in
// FSavedLoadout::GetValidationErrors — no two slots may share a CrystalId.Type
// regardless of tier.

#pragma once

#include "CoreMinimal.h"
#include "FCrystalId.h"
#include "InventoryConstants.h"
#include "FEquippedItemSlot.generated.h"

/**
 * FEquippedItemSlot
 * One designer-authored item slot. Pairs a crystal identity with a starting
 * quantity. Quantity is capped at MAX_QUANTITY_PER_ITEM_SLOT (3).
 *
 * Note on EditCondition: ideally Quantity would be hidden in the editor when
 * the parent FSavedLoadout's bAutoEquipItemsOnCombatStart is true, but UE's
 * cross-struct EditCondition resolution (Outer.* syntax) is unreliable in
 * 5.7. Quantity is always editable; runtime ignores it when auto-equip is
 * true (logic lands with the equip API in a later commit).
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEquippedItemSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipped Item")
    FCrystalId CrystalId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipped Item",
              meta = (ClampMin = "0", ClampMax = "3"))
    int32 Quantity = 0;

    FEquippedItemSlot() = default;

    FEquippedItemSlot(const FCrystalId &InId, int32 InQuantity)
        : CrystalId(InId), Quantity(InQuantity) {}

    bool IsEmpty() const { return Quantity <= 0; }
};
