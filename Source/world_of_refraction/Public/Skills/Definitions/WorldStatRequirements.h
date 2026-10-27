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

    // Old √-deficit requirement penalty (0.0 to 0.60). Not applied to damage or cost any more
    // (requirement-gap scaling replaced it). No C++ reader: Blueprint-exposed through
    // USkillDataBase::CalculateRequirementPenalty. The BD strain reads GetTotalDeficit.
    float CalculatePenalty(const UCharacterData *Character) const;

    /** A weapon's damage factor from its requirement (D-082, Q-146c, Q-162): each pillar's
     *  requirement-gap ladder value clamped to [WEAPON_REQ_FACTOR_MIN, 1]; the worst pillar decides.
     *  1.0 when every pillar is met (being over gives nothing extra) or there is no character. */
    float GetWeaponRequirementFactor(const UCharacterData *Character) const;
};

namespace WeaponRequirement
{
    /** Floor on the weapon requirement factor. The ladder's lowest rung (gap +5) is 0.32, so this
     *  only bites if set above that. */
    constexpr float WEAPON_REQ_FACTOR_MIN = 0.25f; // TODO: tune
    constexpr float WEAPON_REQ_FACTOR_MAX = 1.0f;
}