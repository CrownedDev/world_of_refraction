// TierGapDamageDebug.h
// Debug utilities for tier-gap damage multipliers

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/ItemTier.h"
#include "TierGapDamageDebug.generated.h"

/**
 * Debug utilities for the tier-gap damage multiplier (TierGapConstants.h).
 */
UCLASS()
class WORLD_OF_REFRACTION_API UTierGapDamageDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Print the full F..S x F..S multiplier matrix (rows: action tier, cols: channel tier). */
    UFUNCTION(BlueprintCallable, Category = "Debug|Damage")
    static void PrintTable();

    /** Get formatted multiplier string for one action/channel pairing. */
    UFUNCTION(BlueprintPure, Category = "Debug|Damage")
    static FString GetMultiplierString(EItemTier ActionTier, EItemTier ChannelTier);
};
