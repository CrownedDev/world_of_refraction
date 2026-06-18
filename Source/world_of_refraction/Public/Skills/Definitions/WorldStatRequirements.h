// WorldStatRequirements.h
// Reusable struct for world stat requirement checks

#pragma once

#include "CoreMinimal.h"
#include "Character/StatConstants.h"
#include "WorldStatRequirements.generated.h"

class UCharacterData;

/**
 * Reusable world stat requirements
 * Embed this in any DataAsset that needs progression gates
 * Eliminates code duplication across Spells, Abilities, Weapons, Items, etc.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWorldStatRequirements
{
    GENERATED_BODY()

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "0", ClampMax = "7"))
    int32 RequiredWorldMind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "0", ClampMax = "7"))
    int32 RequiredWorldBody = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ClampMin = "0", ClampMax = "7"))
    int32 RequiredWorldSpirit = 0;

    // ==================== UTILITY FUNCTIONS ====================

    // Check if character meets all requirements
    bool MeetsRequirements(const UCharacterData *Character) const;

    // True only if the character's world stats EXACTLY equal the requirements on all
    // three pillars — never above, never below. Any surplus or deficit on any pillar
    // (including a pillar the spell sets to 0) disqualifies.
    bool ExactlyMatchedBy(const UCharacterData *Character) const;

    // Check if there are any requirements at all
    bool HasRequirements() const
    {
        return RequiredWorldMind > 0 || RequiredWorldBody > 0 || RequiredWorldSpirit > 0;
    }

    // Get formatted requirement summary with character check
    FString GetRequirementsSummary(const UCharacterData *Character = nullptr) const;

    // Get minimum rank required (highest of the three stats)
    int32 GetMinimumRank() const
    {
        return FMath::Max3(RequiredWorldMind, RequiredWorldBody, RequiredWorldSpirit);
    }

    // Get simple requirement string for display
    FString GetSimpleString() const;

    // ==================== PENALTY CALCULATIONS ====================

    // Get total world stat deficit
    int32 GetTotalDeficit(const UCharacterData *Character) const;

    // Calculate requirement penalty (0.0 to 0.60, capped at 60%)
    float CalculatePenalty(const UCharacterData *Character) const;
};