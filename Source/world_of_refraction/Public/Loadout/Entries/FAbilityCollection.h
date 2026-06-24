// FAbilityCollection.h
// Ability inventory collection for characters
//
// ARCHITECTURE: Runtime inventory state, separate from AbilityData assets
// AbilityData = immutable template, FAbilityCollection = mutable runtime ownership

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "FAbilityCollection.generated.h"

class UAbilityData;

/**
 * FAbilityInstance
 * Per-instance owned-ability record (cluster i of the spell-instance arc — the ability twin of
 * FSpellInstance). Wraps the ability asset with the per-instance Tier/Quality/InstanceID that
 * leveling + leveled dismantle will read.
 *
 * INERT until the equip-chain threading (cluster ii): the equip arrays + cast path still copy
 * .Ability OUT as a bare UAbilityData*, so combat reads the ASSET tier. Tier/Quality/InstanceID
 * are written at learn time but UNREAD by combat for now. Invalid InstanceID = legacy/asset-fallback.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAbilityInstance
{
    GENERATED_BODY()

    /** The owned ability asset. The equip chain copies this out as a bare pointer (cluster i is inert). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ability")
    UAbilityData *Ability = nullptr;

    /** Per-instance tier — seeded from the asset at learn time, mutated by leveling (deferred).
     *  UNREAD by combat until cluster ii threads it through the equip chain. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ability")
    EItemTier Tier = EItemTier::F_Tier;

    /** Per-instance quality — placeholder until the weighted-roll cluster; never mutated by leveling. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ability")
    EItemQuality Quality = EItemQuality::C_Quality;

    /** Stable per-instance identity, minted at learn time. Invalid (default) = legacy/asset-fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ability")
    FGuid InstanceID;

    FAbilityInstance() = default;

    /** Acquisition ctor — mints the InstanceID. Tier/Quality are seeded by the caller (LearnAbility),
     *  which has the full UAbilityData type to read Ability->Tier. */
    explicit FAbilityInstance(UAbilityData *InAbility)
        : Ability(InAbility), InstanceID(FGuid::NewGuid()) {}
};

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

    /** Array of learned abilities (per-instance — wraps the asset with Tier/Quality/InstanceID). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    TArray<FAbilityInstance> LearnedAbilities;

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

    /** Unlearn an ability (returns false if not known). Matches on the wrapped asset (.Ability). */
    bool UnlearnAbility(UAbilityData* Ability)
    {
        if (!Ability)
        {
            return false;
        }
        return LearnedAbilities.RemoveAll([Ability](const FAbilityInstance &I) { return I.Ability == Ability; }) > 0;
    }

    // ==================== QUERIES ====================

    /** Check if ability is in collection (by asset). */
    bool HasAbility(UAbilityData* Ability) const
    {
        return Ability && LearnedAbilities.ContainsByPredicate([Ability](const FAbilityInstance &I) { return I.Ability == Ability; });
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
