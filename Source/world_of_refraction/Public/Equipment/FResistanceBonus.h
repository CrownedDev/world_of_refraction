// FResistanceBonus.h
// Per-instance gear status-buildup resistance roll: a zero-sum pool across the
// 12 resistance categories (9 elements + 3 physical types), percent
// (+resist / -weak). Distinct from FEquipmentStatBonus (the stat substat pool) —
// resistance has its OWN per-tier budget and does not compete with stats.
//
// Field order mirrors ClassInnateResistanceTable::FResistanceRow so the
// column-read logic is shareable (CLUSTER C). Lives template->instance like
// FEquipmentStatBonus: asset BaseResistance + GeneratedResistance -> copied to
// the inventory entry at CreateFrom* time (CLUSTER C).
//
// NOTE: the ClampMin/ClampMax meta literals below must mirror
// CombatConstants::RESISTANCE_BONUS_MIN / RESISTANCE_BONUS_MAX (UHT meta tags
// require literals, not named constants).

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"
#include "FResistanceBonus.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FResistanceBonus
{
	GENERATED_BODY()

	// ==================== ELEMENT CATEGORIES (9) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Fire = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Water = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Earth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Wind = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Light = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Darkness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Lightning = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Void = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Reality = 0.0f;

	// ==================== PHYSICAL CATEGORIES (3) ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Slash = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Pierce = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Impact = 0.0f;

	// ==================== BUDGET / ROLL ====================

	/** Tier -> resistance point budget (own pool). Returns 0 for an unmapped tier. */
	static int32 GetResistanceBudget(EItemTier Tier);

	/** Wipe the 12 fields and redistribute one full tier resistance budget using
	 *  the shared zero-sum broken-stick (flat 12-slot pool, no pillar split).
	 *  Per-category |delta| capped at RESISTANCE_CATEGORY_CAP; final value clamped
	 *  to RESISTANCE_BONUS_MIN/MAX. Returns the budget redistributed (0 on no-op).
	 *  Delegates to the explicit-budget overload with the tier's budget. */
	int32 RerollResistance(EItemTier Tier);

	/** Explicit-budget reroll — same wipe + distribution, caller-supplied budget
	 *  (the per-instance roll consumes the instance's STORED ResistanceMaxPool;
	 *  per-asset overrides also land here). */
	int32 RerollResistance(int32 Budget);

	/** Field-wise add Other onto this (12 categories, unclamped). Composes the
	 *  authored Base with a fresh per-instance roll at acquisition. */
	void Accumulate(const FResistanceBonus &Other);
};
