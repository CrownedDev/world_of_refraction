// FEquipmentStatBonus.h
// Shared stat-bonus struct used by weapons and rings.
//
// Lives on:
//  - UWeaponData::DefaultStatBonus      (asset-side roll template)
//  - URingData::DefaultStatBonus        (asset-side roll template)
//  - FWeaponInventoryEntry::StatBonus   (per-instance runtime copy)
//  - FRingInventoryEntry::StatBonus     (per-instance runtime copy)
//
// At inventory creation (FWeaponInventoryEntry::CreateFromWeapon /
// FRingInventoryEntry::CreateFromRing), the asset's DefaultStatBonus is
// copied into the entry's StatBonus, and bStatBonusLocked is propagated
// into bLocked. The per-instance copy is what should drive runtime stat
// queries; the asset-side template is the starting state for new entries.

#pragma once

#include "CoreMinimal.h"
#include "ItemTier.h"
#include "FEquipmentStatBonus.generated.h"

/**
 * FEquipmentStatBonus
 * Bag of bonus fields plus a pending-points / capped-roll budget.
 *
 * Tier budgets (capacity points): F=6, E=10, D=15, C=21, B=28, A=36, S=45.
 * Per-stat cap (planned): 40% of tier budget.
 * Designed for a capped broken-stick distribution in SpendPendingPoints.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEquipmentStatBonus
{
    GENERATED_BODY()

    // ==================== BONUS FIELDS (13) ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusRawDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusSpellDamage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusEfficiency = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusStatusMultiplier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0.0"))
    float BonusCritChance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusSpellSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusDefense = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusActionSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusMaxHP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusMaxEnergy = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusResistance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusTurnSpeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonuses", meta = (ClampMin = "0"))
    int32 BonusLuck = 0;

    // ==================== MASTERY / ROLL STATE ====================

    /** Unspent points awaiting SpendPendingPoints() distribution. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mastery", meta = (ClampMin = "0"))
    int32 PendingPoints = 0;

    /** When true, SpendPendingPoints is a no-op — bonuses are fixed. Mirrors
     *  the asset-side bStatBonusLocked, propagated at CreateFromWeapon /
     *  CreateFromRing time. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mastery")
    bool bLocked = false;

    // ==================== BUDGET / CAPACITY ====================

    /** Capacity points available for a given tier. Inline table —
     *  F=6, E=10, D=15, C=21, B=28, A=36, S=45. */
    static int32 GetBudget(EItemTier Tier);

    /** Sum of all 13 bonus fields. BonusCritChance is rounded to int for
     *  this sum — caller should treat the result as "capacity points
     *  spent", not as a damage / chance number. */
    int32 GetTotalSpent() const;

    /** Budget minus GetTotalSpent. May be negative if a designer over-allocates
     *  in the editor — callers should check >= 0 before using as a slot count. */
    int32 GetRemainingCapacity(EItemTier Tier) const;

    /** True when GetTotalSpent() >= GetBudget(Tier). */
    bool IsAtCap(EItemTier Tier) const;

    /** Queue points for a later SpendPendingPoints. Clamped to >= 0. */
    void AddPendingPoints(int32 Amount);

    /** Distribute PendingPoints across the 13 bonus fields using the
     *  capped broken-stick algorithm (40% per-stat cap, one reroll per
     *  call when at cap and PendingPoints >= GetBudget(Tier)). Resets
     *  PendingPoints to zero on return.
     *
     *  No-op when bLocked is true.
     *
     *  IMPLEMENTATION DEFERRED: algorithm parameters (RNG source, cap
     *  rounding, reroll behavior, locked-field handling) are unresolved.
     *  Body intentionally not provided in this pass — see
     *  FEquipmentStatBonus.cpp. */
    void SpendPendingPoints(EItemTier Tier);
};
