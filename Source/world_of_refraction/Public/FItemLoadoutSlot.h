// FItemLoadoutSlot.h
// Item loadout slot - single item slot with remaining uses during combat
//
// ARCHITECTURE:
// FItemCrystalInventory = What you OWN (all consumable crystals)
// FItemLoadoutSlot = What's EQUIPPED for this battle (crystal + remaining uses)
//
// Items are consumed from inventory after battle based on uses spent

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "FItemLoadoutSlot.generated.h"

class UItemData;

/**
 * FItemLoadoutSlot
 * Single item slot in combat loadout
 * 
 * Each slot holds one crystal type with 3 uses per battle.
 * Uses are tracked at runtime and consumed from inventory after battle.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FItemLoadoutSlot
{
    GENERATED_BODY()

    /** The item crystal in this slot */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    UItemData* Crystal = nullptr;

    /** Remaining uses this battle (starts at 3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 RemainingUses = InventoryConstants::ITEM_USES_PER_SLOT;

    /** Uses consumed this battle (for post-battle inventory update) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    int32 UsesConsumed = 0;

    // ==================== STATE QUERIES ====================

    /** Check if slot has a crystal */
    bool HasCrystal() const
    {
        return Crystal != nullptr;
    }

    /** Check if slot is empty (no crystal or no uses left) */
    bool IsEmpty() const
    {
        return !HasCrystal() || RemainingUses <= 0;
    }

    /** Check if slot can be used */
    bool CanUse() const
    {
        return HasCrystal() && RemainingUses > 0;
    }

    /** Get remaining uses */
    int32 GetRemainingUses() const
    {
        return HasCrystal() ? RemainingUses : 0;
    }

    // ==================== USAGE ====================

    /** Use one charge (returns false if empty) */
    bool UseOne()
    {
        if (!CanUse())
        {
            return false;
        }
        
        RemainingUses--;
        UsesConsumed++;
        return true;
    }

    /** Get number of items to remove from inventory after battle */
    int32 GetItemsToConsume() const
    {
        return UsesConsumed;
    }

    // ==================== INITIALIZATION ====================

    /** Initialize slot with crystal */
    void Initialize(UItemData* InCrystal)
    {
        Crystal = InCrystal;
        RemainingUses = InventoryConstants::ITEM_USES_PER_SLOT;
        UsesConsumed = 0;
    }

    /** Reset for new battle (restore uses) */
    void ResetForBattle()
    {
        RemainingUses = InventoryConstants::ITEM_USES_PER_SLOT;
        UsesConsumed = 0;
    }

    /** Clear slot */
    void Clear()
    {
        Crystal = nullptr;
        RemainingUses = 0;
        UsesConsumed = 0;
    }
};
