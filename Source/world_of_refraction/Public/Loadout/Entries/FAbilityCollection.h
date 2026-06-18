// FAbilityCollection.h
// Ability inventory collection for characters
//
// ARCHITECTURE: Runtime inventory state, separate from AbilityData assets
// AbilityData = immutable template, FAbilityCollection = mutable runtime ownership

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "FAbilityCollection.generated.h"

class UAbilityData;

/**
 * FAbilityCollection
 * Manages a character's learned ability inventory (runtime state)
 * Capacity: 50 abilities, supports learn/unlearn
 * Abilities are filtered by weapon type when assigning to loadout
 * 
 * NOTE: This tracks which abilities a character OWNS, not which are EQUIPPED.
 * Loadout system assigns owned abilities to weapon slots.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAbilityCollection
{
    GENERATED_BODY()

    /** Array of learned abilities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    TArray<UAbilityData*> LearnedAbilities;

    // ==================== CAPACITY ====================

    /** Check if collection has room for more abilities */
    bool CanLearn() const
    {
        return LearnedAbilities.Num() < InventoryConstants::MAX_LEARNED_ABILITIES;
    }

    /** Get remaining capacity */
    int32 GetRemainingCapacity() const
    {
        return InventoryConstants::MAX_LEARNED_ABILITIES - LearnedAbilities.Num();
    }

    /** Get current count */
    int32 GetCount() const
    {
        return LearnedAbilities.Num();
    }

    /** Get max capacity */
    int32 GetMaxCapacity() const
    {
        return InventoryConstants::MAX_LEARNED_ABILITIES;
    }

    // ==================== LEARN/UNLEARN ====================

    /** Learn a new ability (returns false if at capacity, already known, or a basic attack). Body in
     *  FAbilityCollection.cpp so it can call IsAttack() on UAbilityData (only forward-declared here). */
    bool LearnAbility(UAbilityData* Ability);

    /** Unlearn an ability (returns false if not known) */
    bool UnlearnAbility(UAbilityData* Ability)
    {
        if (!Ability)
        {
            return false;
        }
        return LearnedAbilities.Remove(Ability) > 0;
    }

    // ==================== QUERIES ====================

    /** Check if ability is in collection */
    bool HasAbility(UAbilityData* Ability) const
    {
        return Ability && LearnedAbilities.Contains(Ability);
    }

    /** Get abilities filtered by required weapon type */
    TArray<UAbilityData*> GetAbilitiesForWeaponType(EWeaponType WeaponType) const;

    /** Count abilities for a specific weapon type */
    int32 GetAbilityCountForWeaponType(EWeaponType WeaponType) const;

    // ==================== CLEAR ====================

    /** Remove all abilities */
    void Clear()
    {
        LearnedAbilities.Empty();
    }
};
