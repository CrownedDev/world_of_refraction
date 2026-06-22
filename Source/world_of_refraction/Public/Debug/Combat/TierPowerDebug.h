// TierPowerDebug.h
// Debug utilities for tier-power scaling (TierPowerConstants.h) — the own-tier
// multiplier applied to skills (damage/status/cost) and to gear per-point
// conversion at aggregation. Mirrors UTierGapDamageDebug's BlueprintFunctionLibrary
// pattern: BlueprintCallable statics invoked from a debug Blueprint/widget (and
// callable directly in C++). Additive inspection only — touches no live system.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/ItemTier.h"
#include "TierPowerDebug.generated.h"

class ULoadoutComponent;

/**
 * Debug utilities for the own-tier power multiplier (TierPowerConstants.h).
 */
UCLASS()
class WORLD_OF_REFRACTION_API UTierPowerDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Dump the full F..S TIER_POWER curve (1.00 … 4.80) — one line per tier. */
    UFUNCTION(BlueprintCallable, Category = "Debug|Damage")
    static void PrintCurve();

    /** Formatted line for one tier's skill multiplier
     *  (e.g. "S-Tier -> x4.80 on damage/status/cost (effects excluded)"). */
    UFUNCTION(BlueprintPure, Category = "Debug|Damage")
    static FString GetSkillPowerString(EItemTier Tier);

    /** Per-equipped-item tier-power gear contribution for the loadout's active set:
     *  item name, tier, GetTierPowerMultiplier(tier), and each non-zero substat
     *  raw -> tier-scaled (so the 3a flat-budget × 3c per-point effect is visible,
     *  including a cursed negative scaling harder at higher tier), then the
     *  authoritative aggregated (tier-weighted) Combined totals the game reads.
     *  Null-guards the loadout and each item pointer. */
    UFUNCTION(BlueprintCallable, Category = "Debug|Damage")
    static void PrintGearContribution(ULoadoutComponent *Loadout);
};
