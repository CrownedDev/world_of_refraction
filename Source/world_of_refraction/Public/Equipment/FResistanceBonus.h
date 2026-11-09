// FResistanceBonus.h
// Per-instance gear status-buildup resistance roll: a zero-sum pool across the
// 10 resistance categories (9 elements + one Physical, B-358: the Slash / Pierce / Impact fields hold its one
// value, read and written through ResistanceColumns), percent
// (+resist / -weak). Distinct from FEquipmentStatBonus (the stat substat pool) —
// resistance has its OWN per-tier budget and does not compete with stats.
//
// Field order mirrors ClassInnateResistanceTable::FResistanceRow so the
// column-read logic is shareable. Flow (per-instance roll model): the asset's
// authored BaseResistance is copied to the inventory entry at CreateFrom* time;
// the instance's rolled layer is generated at acquisition (toggle-ON assets) —
// the asset's GeneratedResistance is designer PREVIEW only, never gameplay-read.
//
// NOTE: the ClampMin/ClampMax meta literals below must mirror
// CombatConstants::RESISTANCE_BONUS_MIN / RESISTANCE_BONUS_MAX (UHT meta tags
// require literals, not named constants).
//
// B-389 (D-168, Crown 2026-11-08): defence is resistance by damage type, in one family of scoped lines (the
// deckbuilder's Def lines): All (today's Defence, BonusDefense: its own step and 75% cap, ED-20), each element (the
// columns above), and two damage-only scope lines appended below, Physical (a hit that is not a spell) and Spell. The
// element and scope lines that match a hit are summed into one resistance under the ±50% cap (ED-27). The two new lines
// are not resistance columns (ResistanceColumns::Types), so the buildup roll, the GAS columns and status buildup are
// unchanged; old data reads 0.

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

	// ==================== DAMAGE SCOPE LINES (B-389, D-168) ====================

	/** A cut to every hit that is not a spell (attacks and abilities, infused or not), percent. Damage only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float PhysicalDamage = 0.0f;

	/** A cut to every spell hit, percent. Damage only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float SpellDamage = 0.0f;

	// ==================== ICE (B-064c, D-029) ====================

	/** Ice resistance, percent: resists Ice hits and Water hits too (Q-145b). A resistance column (last in
	 *  ResistanceColumns::Types) but never rolled (Q-064c): authored lines only. Appended: old assets and saves read 0
	 *  (the save copies this struct by tagged property, so no save version bump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float Ice = 0.0f;

	/** B-389: the scope line a hit reads (SpellDamage for a spell, else PhysicalDamage), percent. */
	float GetScopeLine(bool bHitIsSpell) const { return bHitIsSpell ? SpellDamage : PhysicalDamage; }

	// ==================== BUDGET / ROLL ====================

	/** Tier -> resistance point budget (own pool). Returns 0 for an unmapped tier. */
	static int32 GetResistanceBudget(EItemTier Tier);

	/** Wipe the columns and redistribute one full tier resistance budget using
	 *  the shared zero-sum broken-stick (flat 10-slot pool, no pillar split).
	 *  Per-category |delta| capped at RESISTANCE_CATEGORY_CAP; final value clamped
	 *  to RESISTANCE_BONUS_MIN/MAX. Returns the budget redistributed (0 on no-op).
	 *  Delegates to the explicit-budget overload with the tier's budget. */
	int32 RerollResistance(EItemTier Tier); // OBSOLETE (B-335, D-127; delete in the D-124 sweep)

	/** Explicit-budget reroll — same wipe + distribution, caller-supplied budget
	 *  (the per-instance roll consumes the instance's STORED ResistanceMaxPool;
	 *  per-asset overrides also land here). */
	int32 RerollResistance(int32 Budget); // OBSOLETE (B-335, D-127; delete in the D-124 sweep)

	/** Field-wise add Other onto this (every column and the B-389 scope lines, unclamped). Composes the
	 *  authored Base with a fresh per-instance roll at acquisition, and every gear source into one total. */
	void Accumulate(const FResistanceBonus &Other);
};
