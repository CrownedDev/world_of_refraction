// SkillDataBase.h
// Shared base class for all skill-shaped data assets (abilities, spells, weapon attacks).
// Carries fields that every skill has in common.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Loadout/LoadoutConstants.h"
#include "Combat/Defense/DefenseDifficulty.h"
#include "Combat/TargetType.h"
#include "Inventory/ItemTier.h"
#include "Skills/Definitions/WorldStatRequirements.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Skills/Definitions/SkillVFXEntry.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "Combat/Actions/EAbilityExecutionType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SkillDataBase.generated.h"

class UTexture2D;
class UCharacterData;
class UAnimMontage;

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

    /** This hit deals no damage (a pure interaction moment — e.g. a grab-connect). The defense window
     *  still opens (defendable/interruptable); only damage is suppressed. Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
    bool bIgnoreDamage = false;

    /** This hit applies no status buildup. Independent of bIgnoreDamage (all four combinations
     *  authorable). Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
    bool bIgnoreStatus = false;

    /** If this hit is successfully parried OR dodged (by ALL targets), the rest of the attack aborts
     *  (montage stops, no further hits/casts; plays the return montage if authored). Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
    bool bInterruptable = false;
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

    // ==================== PRESENTATION ====================

    /** Display icon for UI (slot bars, menus). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    TObjectPtr<UTexture2D> Icon = nullptr;

    // ==================== EXECUTION ====================

    /** DESCRIPTIVE tag (D3): how this skill executes, for UI / AI filtering /
     *  categorization — uniform across abilities, spells, and weapon attacks.
     *  Movement is owned by the runner's montage chain + warp (W3); this tag no
     *  longer gates an approach. IsMelee() still reads it for the melee
     *  ExecutionRange (warp striking offset). Default Melee preserves existing
     *  ability assets (serialize-by-name); USpellData overrides to Ranged.
     *  Folded from UCastableSkillDataBase at the base merge. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    EAbilityExecutionType ExecutionType = EAbilityExecutionType::Melee;

    /** Melee warp-in stop distance: how far short of the target the motion-warp stops for a melee strike.
     *  Only consumed by montages with a Motion Warping window (displaced/lunge attacks); in-place content
     *  ignores it. Meaningful only for melee execution (the GetExecutionRange IsMelee gate + the editor
     *  EditCondition below). Hoisted from UWeaponAttackData (100) + UAbilityData (150) to the root at
     *  step 2 of the attack/ability merge; reconciled default 100. The Melee EditCondition was restored
     *  at the base merge — ExecutionType now lives on the same class, so it can reference it again. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
              meta = (EditCondition = "ExecutionType == EAbilityExecutionType::Melee", EditConditionHides, ClampMin = "0.0"))
    float ExecutionRange = 100.0f;

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

    // ============================================================
    // Folded from UCastableSkillDataBase at the base merge — all three
    // leaves (attack/ability/spell) were its only subclasses, so the
    // intermediate had zero polymorphic purpose.
    // ============================================================

    // ==================== IDENTITY (cast) ====================

    /** Tier for break calculations and difficulty scaling. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    // ==================== TARGETING ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    ETargetType TargetType = ETargetType::SingleEnemy;

    // ==================== COMBAT (cast) ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    int32 BaseDamage = 0;

    /** If true, this skill's PHYSICAL damage scales with SpellDamage instead of RawDamage (a "fire punch"
     *  that scales off Spell). Attack-wide (the whole physical attack). Stat-only: changes which stat scales,
     *  nothing else. Default false. (Spell casts have their own per-cast bOverrideStatScaling on
     *  FSkillCastEntry.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    bool bOverrideStatScaling = false;

    /** Default 0 means free. Attacks are free unless designers set a cost. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    int32 BaseEnergyCost = 0;

    /** Turns before this skill fires after arming. 0 = fires this turn (no
     *  deferral). N = arms now, fires at the start of the turn N global turns
     *  ahead (the deferral mechanism is D8 Stage 8b/8c). FIFO if multiple land
     *  the same turn. Renamed/hoisted from SpellData.TurnCost (CoreRedirect);
     *  old default-1 spell assets self-migrate to 0 via delta serialization. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
    int32 ActivationDelay = 0;

    // ==================== DELIVERY ====================

    /** DEPRECATED (D6, meta'd Stage 12 SC7): load-only — delivery is
     *  per-entry on CastArray; only the migration, the empty-CastArray
     *  fallback dispatch, and the async-decision fallback still read this.
     *  Hard-delete at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Delivery", meta = (DeprecatedProperty))
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    /** DEPRECATED (D6, meta'd Stage 12 SC7): load-only — per-entry on
     *  CastArray; only the migration + empty-CastArray fallback still read
     *  this. Hard-delete at the post-SC8 resave bake. */
    UPROPERTY(BlueprintReadOnly, Category = "Delivery", meta = (DeprecatedProperty))
    float ProjectileSpeed = 1500.0f;

    /** Cast deliveries (D6). INDEX-ORDERED: a UCombatNotify (Family=Cast,
     *  Index=N) fires entry N — array position IS identity. Each entry is one
     *  self-contained delivery (fireball-then-pillar = two entries: Projectile
     *  + AOE). Consumed by the fused-montage runner (Stage 12); populated via
     *  PostLoad migration from the loose delivery fields. All three skill
     *  types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (TitleProperty = "Label"))
    TArray<FSkillCastEntry> CastArray;

    // ==================== ANIMATION ====================

    /** The unified skill montage (D2) — the field the fused-montage runner
     *  plays at Stage 12. Populated via PostLoad migration from the leaf
     *  montage fields (CastAnimation / ExecutionMontage / AttackMontage) until
     *  readers switch; the leaf fields stay runtime-authoritative until then. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *SkillMontage = nullptr;

    /** Optional wind-up montage played BEFORE SkillMontage (the cast lead-in).
     *  Null = skip straight to SkillMontage (opt-in, like ReturnMontage —
     *  presence is the trigger, no flag/duration gate). Carries its own
     *  UCombatNotify notifies like any montage. All three types inherit it.
     *  Chain: [RitualCastMontage] → SkillMontage → [ReturnMontage]. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *RitualCastMontage = nullptr;

    /** Optional post-cast return montage — the runner plays it after
     *  SkillMontage IFF set (null = no return leg). Plays in-place this
     *  stage; warp-to-origin movement is the deferred movement arc. All
     *  three skill types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage *ReturnMontage = nullptr;

    /** Montage play-rate scalar (D7) — uniform across all three skill types;
     *  stat scaling layers on top. 1.0 = no change (the regression guard).
     *  Hoisted from WeaponAttackData; the runner plays SkillMontage at this
     *  rate (Stage 12). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseAnimSpeed = 1.0f;

    // ==================== VISUALS ====================

    /** Role-classified VFX entries (D5). INDEX-ORDERED: a UCombatNotify
     *  (Family=VFX, Index=N) selects entry N — array position IS identity.
     *  Role drives code-spawned visuals (e.g. projectile impact fires
     *  Impact-role entries). Consumed by the fused-montage runner (Stage 12);
     *  populated via PostLoad migration from the loose spell VFX fields
     *  (Stage 10B). All three skill types inherit it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (TitleProperty = "Label"))
    TArray<FSkillVFXEntry> VFXArray;

    // ==================== REQUIREMENTS ====================

    // ShowOnlyInnerProperties: inline the struct's fields under the category
    // header — without it the panel shows "Requirements" twice (category +
    // identically-named member row). Inherited by all three leaves.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements",
              meta = (ShowOnlyInnerProperties))
    FWorldStatRequirements Requirements;

    // ==================== EFFECT HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    TArray<FSkillEffect> GetEffectsForCondition(ESkillTrigger Condition) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDrainEffect() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasBuffEffects() const;

    UFUNCTION(BlueprintPure, Category = "Skill|Effects")
    bool HasDebuffEffects() const;

    // ==================== REQUIREMENT QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    bool MeetsRequirements(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    int32 GetTotalDeficit(const UCharacterData *Character) const;

    UFUNCTION(BlueprintPure, Category = "Skill|Requirements")
    float CalculateRequirementPenalty(const UCharacterData *Character) const;

    // ==================== DAMAGE / ENERGY (shared) ====================
    // Hoisted from UAbilityData at the attack/ability merge (Cluster 1) so the merged
    // FAction.SkillData (USkillDataBase*) path can compute damage/energy for BOTH attacks and
    // abilities — the formula is identical across the leaves. Deliberately NON-UFUNCTION (a
    // sibling, USpellData, already declares same-named, differently-signatured methods; making
    // these virtual/UFUNCTION would trip name-hiding / Blueprint-overload errors). UAbilityData
    // keeps its own UFUNCTION versions for Blueprint exposure; their bodies are identical.

    /** Attacker-side base damage: BaseDamage * (1 - requirement penalty). The RawDamage multiplier
     *  is applied once downstream by DamageCalculator. bIsInfused is accepted for call-site symmetry
     *  but no longer affects the value (the element-infusion damage penalty was removed). */
    int32 CalculateDamage(const UCharacterData *Character, bool bIsInfused) const;

    /** Energy cost: BaseEnergyCost * (1 + requirement penalty), * INFUSION_ENERGY_MULTIPLIER when
     *  infused. Matches the UAbilityData normal/infused split. */
    int32 CalculateEnergyCost(const UCharacterData *Character, bool bIsInfused) const;

    // ==================== DISCRIMINATOR ====================

    /** True if this is a basic weapon attack (vs an ability/spell). Overridden per leaf. Used to
     *  discriminate attacks from abilities once they share the merged FAction.SkillData pointer.
     *  Correct in both eras: UWeaponAttackData → true today; post-reparent attacks are UAbilityData
     *  with bIsAttack=true → true. */
    virtual bool IsAttack() const { return false; }

    // ==================== DISPLAY ====================

    UFUNCTION(BlueprintPure, Category = "Skill|Display")
    FString GetTierString() const;

    /** One-line stat summary (hits / buildup / energy / speed). Hoisted from UWeaponAttackData at the
     *  attack/ability merge (step 5a) — reads only base fields, so it serves every skill leaf. Stays a
     *  UFUNCTION (no leaf declares GetAttackSummary anymore, so no Blueprint name-collision). */
    UFUNCTION(BlueprintPure, Category = "Skill|Display")
    FString GetAttackSummary() const;

    // ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent) override;
#endif
};
