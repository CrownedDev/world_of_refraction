// WorldStatRequirements.h
// Reusable struct for world stat requirement checks

#pragma once

#include "CoreMinimal.h"
#include "Character/StatConstants.h"
#include "WorldStatRequirements.generated.h"

class UCharacterData;
class UCharacterDataComponent;

/** A character's Mind / Body / Spirit world levels as one value (B-217). In combat, read the live
 *  run levels (FromComponent: asset + head start + earned/bought); FromAsset is the authored base. */
struct WORLD_OF_REFRACTION_API FWorldPillarLevels
{
    int32 Mind = 0;
    int32 Body = 0;
    int32 Spirit = 0;

    static FWorldPillarLevels FromAsset(const UCharacterData &Character);
    static FWorldPillarLevels FromComponent(const UCharacterDataComponent &Component);
};

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

    /** A weapon's damage factor from its requirement (D-119, Elden Ring's single penalty): 1.0 when every pillar
     *  is met (being over gives nothing extra) or there is no character; any shortfall, of 1 level or 5, is
     *  REQUIREMENT_UNMET_DAMAGE_FACTOR, and that weapon gives no pillar scaling (WoRWeaponPower::GetWeaponPart).
     *  It scales the weapon part only (B-220); a skill's own requirement cuts the skill part (GetRequirementFactor). */
    float GetWeaponRequirementFactor(const UCharacterData *Character) const;

    // ==================== BY LEVELS (B-217: combat passes the live levels) ====================
    // The UCharacterData versions above read the asset's levels (FWorldPillarLevels::FromAsset).

    bool ExactlyMatchedBy(const FWorldPillarLevels &Levels) const;
    /** B-326 (D-119): every pillar at or over its requirement. */
    bool IsMetBy(const FWorldPillarLevels &Levels) const;
    /** B-326 (D-119): 1.0 when met, else WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR. The one penalty for
     *  weapons, abilities and spells alike; an unmet skill still works (nothing becomes uncastable). */
    float GetRequirementFactor(const FWorldPillarLevels &Levels) const;
    int32 GetTotalDeficit(const FWorldPillarLevels &Levels) const;
    float GetWeaponRequirementFactor(const FWorldPillarLevels &Levels) const;

    /** Per-pillar requirement-gap percents (whole-number, 5.0 = +5%) for AddPillarPercent. */
    void GetRequirementGapPillarPercents(const FWorldPillarLevels &Levels, float &OutMindPct, float &OutBodyPct,
                                         float &OutSpiritPct) const;
};

namespace WeaponRequirement
{
    /** B-326 (D-119, Crown 2026-11-03; replaces Q-293d (2)): a weapon, ability or spell whose requirement isn't met
     *  deals this share of its part, however far short, and gives no pillar scaling. Elden Ring's penalty. */
    constexpr float REQUIREMENT_UNMET_DAMAGE_FACTOR = 0.6f;
    /** OBSOLETE (B-326, D-119): the old ladder's floor; nothing reads it but UCombatTuningData::WeaponReqFactorMin's
     *  default. Delete with that field after Crown's PIE pass. */
    constexpr float WEAPON_REQ_FACTOR_MIN = 0.25f;
    constexpr float WEAPON_REQ_FACTOR_MAX = 1.0f;

    /** True for a factor GetWeaponRequirementFactor gives an unmet requirement (anything under full power). */
    inline bool IsUnmetFactor(float RequirementFactor) { return RequirementFactor < WEAPON_REQ_FACTOR_MAX; }
}