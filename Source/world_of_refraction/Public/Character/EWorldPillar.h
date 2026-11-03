// EWorldPillar.h
// The world-stat pillar key, split out of CharacterDataComponent.h (B-265) so headers that only
// name a pillar don't pull in the whole component.

#pragma once

#include "CoreMinimal.h"
#include "EWorldPillar.generated.h"

/** The three world-stat pillars — the shared, BlueprintType pillar key for the world-stat
 *  earn/buy/query ops (replaces the prior file-local non-UENUM EPillar/ECrystalPillar, which
 *  weren't usable across the C-series). UEconomyService (C4) passes this to BuyWorldStat. */
UENUM(BlueprintType)
enum class EWorldPillar : uint8
{
    Mind,
    Body,
    Spirit
};
