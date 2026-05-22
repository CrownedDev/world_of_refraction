// FItemLoadoutSlot.h
// Runtime item slot inside FCombatLoadout::ItemSlots. Mirrors the (CrystalId,
// Quantity) shape of the designer-authored FEquippedItemSlot.
//
// Empty slots persist in the array with their CrystalId identity and
// Quantity = 0 so they can be refilled between combats. Cap enforcement
// happens at the loadout level (max 6 slots, max 3 quantity per slot, unique
// by ECrystalType).
//
// 9.5b migrates readers off the legacy (Crystal, RemainingUses, UsesConsumed)
// shape and replaces helpers (UseOne / CanUse / etc.) with direct Quantity
// reads/writes at the call sites.

#pragma once

#include "CoreMinimal.h"
#include "FCrystalId.h"
#include "FItemLoadoutSlot.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FItemLoadoutSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Slot")
    FCrystalId CrystalId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Slot")
    int32 Quantity = 0;

    bool IsEmpty() const { return CrystalId.Type == ECrystalType::None || Quantity <= 0; }
};
