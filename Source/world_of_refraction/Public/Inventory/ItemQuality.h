// ItemQuality.h
// Per-instance item QUALITY grade (F through S) — ROLLED per drop and stamped on the
// item INSTANCE, NOT authored on the shared asset (the drop-grade-shift perk requires it
// to be roll-based). A SEPARATE axis from EItemTier: Tier = power/rarity (and the leveled
// axis); Quality = the rolled drop-grade that biases roll budget / loot value and is never
// mutated by leveling. Mirrors EItemTier's F..S shape.

#pragma once

#include "CoreMinimal.h"
#include "ItemQuality.generated.h"

/**
 * Universal per-instance drop-quality grade, weakest (F) to strongest (S).
 *
 * Rolled at drop time (weighted curve, Luck/perk-biased) and stamped on the item
 * instance — distinct from EItemTier (which is authored/seeded power and the axis leveling
 * mutates). Append-only — never reorder/remove (serialization-safe), exactly like EItemTier.
 */
UENUM(BlueprintType)
enum class EItemQuality : uint8
{
	F_Quality UMETA(DisplayName = "F-Quality (Weakest)"),
	E_Quality UMETA(DisplayName = "E-Quality"),
	D_Quality UMETA(DisplayName = "D-Quality"),
	C_Quality UMETA(DisplayName = "C-Quality"),
	B_Quality UMETA(DisplayName = "B-Quality"),
	A_Quality UMETA(DisplayName = "A-Quality"),
	S_Quality UMETA(DisplayName = "S-Quality (Strongest)")
};
