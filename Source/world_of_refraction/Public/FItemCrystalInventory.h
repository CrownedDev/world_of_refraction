// FItemCrystalInventory.h
// Item crystal inventory with tiered capacity
//
// ARCHITECTURE: Runtime inventory state for consumable crystals
// ItemData = immutable template (crystal type, tier, effects)
// FItemCrystalInventory = mutable runtime storage with capacity limits

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "ItemTier.h"
#include "CrystalType.h"
#include "FItemCrystalInventory.generated.h"

class UItemData;

/**
 * FItemCrystalInventory
 * Manages consumable item crystals with tiered capacity limits (runtime state)
 * S-tier: 3, A-tier: 5, B-tier: 8, C-tier: 12, D-tier: 15, E-tier: 20, F-tier: 25
 * Total: 88 crystals maximum
 *
 * Crystals are consumed permanently when used in combat.
 *
 * NOTE: Using separate arrays per tier because UHT doesn't support TMap<Enum, TArray<>>
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FItemCrystalInventory
{
    GENERATED_BODY()

    // ==================== STORAGE (Per-Tier Arrays) ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|F-Tier")
    TArray<UItemData *> CrystalsF; // Max 25

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|E-Tier")
    TArray<UItemData *> CrystalsE; // Max 20

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|D-Tier")
    TArray<UItemData *> CrystalsD; // Max 15

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|C-Tier")
    TArray<UItemData *> CrystalsC; // Max 12

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|B-Tier")
    TArray<UItemData *> CrystalsB; // Max 8

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|A-Tier")
    TArray<UItemData *> CrystalsA; // Max 5

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items|S-Tier")
    TArray<UItemData *> CrystalsS; // Max 3

    // ==================== CAPACITY ====================

    /** Get capacity for a specific tier */
    int32 GetCapacityForTier(EItemTier Tier) const
    {
        switch (Tier)
        {
        case EItemTier::S_Tier:
            return InventoryConstants::ITEM_CAPACITY_S;
        case EItemTier::A_Tier:
            return InventoryConstants::ITEM_CAPACITY_A;
        case EItemTier::B_Tier:
            return InventoryConstants::ITEM_CAPACITY_B;
        case EItemTier::C_Tier:
            return InventoryConstants::ITEM_CAPACITY_C;
        case EItemTier::D_Tier:
            return InventoryConstants::ITEM_CAPACITY_D;
        case EItemTier::E_Tier:
            return InventoryConstants::ITEM_CAPACITY_E;
        case EItemTier::F_Tier:
            return InventoryConstants::ITEM_CAPACITY_F;
        default:
            return 0;
        }
    }

    /** Get current count for a tier */
    int32 GetCountForTier(EItemTier Tier) const
    {
        return GetArrayForTier(Tier).Num();
    }

    /** Get remaining capacity for a tier */
    int32 GetRemainingCapacityForTier(EItemTier Tier) const
    {
        return GetCapacityForTier(Tier) - GetCountForTier(Tier);
    }

    /** Check if tier has room */
    bool CanAddToTier(EItemTier Tier) const
    {
        return GetCountForTier(Tier) < GetCapacityForTier(Tier);
    }

    /** Get total crystals across all tiers */
    int32 GetTotalCount() const
    {
        return CrystalsF.Num() + CrystalsE.Num() + CrystalsD.Num() +
               CrystalsC.Num() + CrystalsB.Num() + CrystalsA.Num() + CrystalsS.Num();
    }

    // ==================== ADD/REMOVE ====================

    /** Check if crystal can be added (validates tier capacity) */
    bool CanAddCrystal(UItemData *Crystal) const;

    /** Add crystal to inventory (returns false if at capacity) */
    bool AddCrystal(UItemData *Crystal);

    /** Remove crystal from inventory (returns false if not found) */
    bool RemoveCrystal(UItemData *Crystal);

    // ==================== QUERIES ====================

    /** Check if crystal is in inventory */
    bool HasCrystal(UItemData *Crystal) const;

    /** Get all crystals of a specific tier */
    TArray<UItemData *> GetCrystalsOfTier(EItemTier Tier) const
    {
        return GetArrayForTier(Tier);
    }

    /** Get all crystals of a specific type (across all tiers) */
    TArray<UItemData *> GetCrystalsOfType(ECrystalType Type) const;

    /** Get all evolution crystals across all tiers */
    TArray<UItemData *> GetEvolutionCrystals() const;

    /** Get all non-evolution (item) crystals across all tiers */
    TArray<UItemData *> GetItemCrystals() const;

    // ==================== CLEAR ====================

    /** Remove all crystals */
    void Clear()
    {
        CrystalsF.Empty();
        CrystalsE.Empty();
        CrystalsD.Empty();
        CrystalsC.Empty();
        CrystalsB.Empty();
        CrystalsA.Empty();
        CrystalsS.Empty();
    }

    /** Remove all crystals of a specific tier */
    void ClearTier(EItemTier Tier)
    {
        GetMutableArrayForTier(Tier).Empty();
    }

private:
    /** Get array for tier (const) */
    const TArray<UItemData *> &GetArrayForTier(EItemTier Tier) const
    {
        switch (Tier)
        {
        case EItemTier::S_Tier:
            return CrystalsS;
        case EItemTier::A_Tier:
            return CrystalsA;
        case EItemTier::B_Tier:
            return CrystalsB;
        case EItemTier::C_Tier:
            return CrystalsC;
        case EItemTier::D_Tier:
            return CrystalsD;
        case EItemTier::E_Tier:
            return CrystalsE;
        case EItemTier::F_Tier:
        default:
            return CrystalsF;
        }
    }

    /** Get array for tier (mutable) */
    TArray<UItemData *> &GetMutableArrayForTier(EItemTier Tier)
    {
        switch (Tier)
        {
        case EItemTier::S_Tier:
            return CrystalsS;
        case EItemTier::A_Tier:
            return CrystalsA;
        case EItemTier::B_Tier:
            return CrystalsB;
        case EItemTier::C_Tier:
            return CrystalsC;
        case EItemTier::D_Tier:
            return CrystalsD;
        case EItemTier::E_Tier:
            return CrystalsE;
        case EItemTier::F_Tier:
        default:
            return CrystalsF;
        }
    }
};
