// SkillDataBase.h
// Shared base class for all skill-shaped data assets (abilities, spells, weapon attacks).
// Carries fields that every skill has in common.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Loadout/LoadoutConstants.h"
#include "Combat/Defense/DefenseDifficulty.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SkillDataBase.generated.h"

/**
 * One authored per-hit damage exception (D1). HitNumber is 1-based.
 * Hits without an entry share the remaining percent evenly.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDamageSplitEntry
{
    GENERATED_BODY()

    /** Which hit this entry applies to (1 = first hit). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Split", meta = (ClampMin = "1"))
    int32 HitNumber = 1;

    /** Percent of total damage this hit carries (0-100 scale). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Split", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Percent = 0.0f;

    /** This hit's per-defense-type difficulty (parry/dodge/block). Defaults all-Inherit →
     *  resolves to the skill-level DefaultDifficulty, then Easy (×1.0). Merged onto the damage
     *  entry so a hit's damage and its defense difficulty are authored together — they can no
     *  longer drift apart. Resolved via ResolveImpactDifficulty (reads the same array). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Split")
    FDefenseDifficultyTriple Difficulty;

    /** If true, this component scales with the OPPOSITE stat to its action type — a physical hit scales
     *  with SpellDamage instead of RawDamage (a "fire punch" that scales off Spell). Default false =
     *  natural scaling (physical -> RawDamage). Stat-only: changes which stat scales, nothing else. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
    bool bOverrideStatScaling = false;
};

/** Resolve authored split entries into a full per-hit percent table (length
 *  HitCount, 0-100 scale, summing to 100). Empty Split → even 100/N per hit —
 *  identical to the legacy even split. Invalid entries (out-of-range /
 *  duplicate / non-positive) warn and are skipped; a table that can't sum to
 *  100 warns and is normalized (redistribute, never amplify). Shared by the
 *  runtime resolution (FinalizeDamageInputs) and UDamageSplitDebug so the
 *  debug output is exactly what executes. */
WORLD_OF_REFRACTION_API TArray<float> ResolveDamageSplit(int32 HitCount, const TArray<FDamageSplitEntry> &Split);

/** Resolve the per-hit difficulty (carried on the DamageSplit entries) into a full per-impact
 *  table (length HitCount). For each impact ordinal: use the DamageSplit entry with the matching
 *  HitNumber if present (its Difficulty; .Percent is ignored here), else Default; then resolve each
 *  field Inherit → Default → Easy. The resolved table carries NO Inherit values (every field is
 *  concrete Easy/Medium/Hard/Impossible) — clean for cluster 3/4 to consume. Reads the SAME array as
 *  ResolveDamageSplit so per-hit damage and difficulty can't drift; invalid/duplicate entries warn
 *  and are skipped (first wins). */
WORLD_OF_REFRACTION_API TArray<FDefenseDifficultyTriple> ResolveImpactDifficulty(
	int32 HitCount, const TArray<FDamageSplitEntry> &Split, const FDefenseDifficultyTriple &Default);

struct FSkillCastEntry;

/** Resolve each cast-entry's authored Difficulty into a dense per-cast-entry table (length
 *  CastArray.Num()). Per field: entry tier -> Default -> Easy (no Inherit survives). Empty CastArray
 *  -> empty table (the cluster-4 reader guards with IsValidIndex -> default triple -> Easy x1.0, same
 *  shape as the melee reader). Spell analog of ResolveImpactDifficulty, keyed by cast-entry index. */
WORLD_OF_REFRACTION_API TArray<FDefenseDifficultyTriple> ResolveCastDifficulty(
	const TArray<FSkillCastEntry> &CastArray, const FDefenseDifficultyTriple &Default);

/**
 * USkillDataBase
 * Truly-shared fields for abilities, spells, and weapon attacks.
 * Subclasses extend with asset-specific data (cost, requirements, delivery, etc.).
 */
UCLASS(Abstract, BlueprintType)
class WORLD_OF_REFRACTION_API USkillDataBase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Name = TEXT("Unnamed Skill");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    // ==================== COMBAT ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1"))
    int32 HitCount = 1;

    /** Per-hit damage exceptions (D1). Empty = even split across HitCount —
     *  the current behavior. Authored hits take their Percent; unassigned hits
     *  share the remainder evenly. Resolved once at action start via
     *  ResolveDamageSplit; consumed by the fused-montage runner (Stage 12). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (TitleProperty = "HitNumber"))
    TArray<FDamageSplitEntry> DamageSplit;

    /** Skill-level defense-difficulty fallback for impacts whose DamageSplit entry leaves a field
     *  Inherit (and for hits with no DamageSplit entry at all). Inherit here → Easy (×1.0). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    FDefenseDifficultyTriple DefaultDifficulty;

    /** Raw mode: folds StatusBuildup into damage at the orchestrator boundary; status bar doesn't move. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bIsRawMode = false;

    /** Per-hit status buildup amount. Disabled in raw mode. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat",
              meta = (EditCondition = "!bIsRawMode", EditConditionHides, ClampMin = "0"))
    int32 StatusBuildup = 0;

    /** If true, orchestrator rejects this skill when an infusion source is selected. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bImmuneToInfusion = false;

    // ==================== EFFECTS ====================

    /**
     * Effects applied by this skill (max LoadoutConstants::MAX_SKILL_EFFECTS).
     * Each effect carries its own target, condition, and timing.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects",
              meta = (TitleProperty = "EffectType"))
    TArray<FSkillEffect> Effects;

    // ==================== EFFECT HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    TArray<FSkillEffect> GetEffectsForCondition(ESkillTrigger Condition) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDrainEffect() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasBuffEffects() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDebuffEffects() const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
#endif
};
