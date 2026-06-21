// DamageCalculator.h
// Centralized damage calculation system
// All damage formulas flow through here for consistency

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Actions/EActionType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Skills/Definitions/EScalingTier.h"
#include "DamageCalculator.generated.h"

class UCharacterData;
class USkillDataBase;
class USkillEffectManager;
class UBrokenDarknessManager;
class UCombatGridSubsystem;
/**
 * Damage calculation constants
 */
namespace DamageConstants
{
	// Critical hits — crit DAMAGE is now a variable stat (CombatConstants::CRIT_DMG_BASE + CritDamage
	// stat + gear, via UDamageCalculator::GetCritDamageMultiplier). The old fixed CRIT_MULTIPLIER (1.5)
	// was retired in cluster 5e-D once the AI estimator stopped reading it.
	constexpr float BASE_CRIT_CHANCE = 0.05f; // 5%

	// Defense
	constexpr float MAX_RESISTANCE = 0.50f; // 50% cap
	constexpr int32 MIN_DAMAGE = 1;			// Always deal at least 1 damage

	// Infusion
	// ELEMENT_INFUSION_PENALTY removed per locked cost matrix — see commit message.
	// POWER_INFUSION_L1/L2_MULT removed in the tier-power arc — they fed only the
	// orphaned GetInfusionDamageMultiplier (dead code). Live infusion damage rides
	// ActionExecutor::GetChargeDamageMultiplier (1.15 / 1.30).
}

/**
 * Input data for damage calculation
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDamageCalculationInput
{
	GENERATED_BODY()

	/** Base damage before any modifiers */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	int32 BaseDamage = 0;

	/** Action category — drives damage-stat selection.
	 *  Spell → SpellDamage, Ability/Attack → RawDamage. None defaults to RawDamage branch. */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	EActionType ActionType = EActionType::None;

	/** Element type (if elemental); None = non-elemental damage instance */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	ESpellElement Element = ESpellElement::None;

	/** Can this attack critically hit? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bCanCrit = true;

	/** Was this attack infused? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bWasInfused = false;

	/** Infusion level (0-2) */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	int32 InfusionLevel = 0;

	/** Per-action stat modifiers accumulated from all active sources
	 *  (Reality innate/slotted/infused, Evolution slotted/infused, future buffs).
	 *  DamageCalculator consumes StatusMultiplier / SpellDamage / RawDamage / CritDamage from this. */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	FActionStatModifiers ActionMods;

	/** Is this Raw (physical) mode for spell? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bIsRawMode = false;

	/** Number of hits (for multi-hit) */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	int32 HitCount = 1;

	/** Override crit chance (negative = use default) */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	float OverrideCritChance = -1.0f;

	/** Skip defense calculation? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bIgnoreDefense = false;

	/** Skip resistance calculation? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bIgnoreResistance = false;

	/** Authored per-skill stat-scaling tiers (stage b2). Each entry adds
	 *  GetScalingTierCoefficient(grade) × StatFraction(attacker's effective stat) to the attacker
	 *  multiplier (additive). EMPTY = no tier scaling → byte-identical to pre-b2. Populated from the
	 *  source skill's USkillDataBase::StatScaling at the Input-build sites; sites left unpopulated stay
	 *  empty (safe no-op). */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	TArray<FStatScaling> StatScaling;
};

/**
 * Result of damage calculation
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDamageCalculationResult
{
	GENERATED_BODY()

	/** Final damage after all modifiers */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 FinalDamage = 0;

	/** Damage before defense/resistance */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DamageBeforeDefense = 0;

	/** Was this a critical hit? */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bWasCritical = false;

	/** HP removed by the Defense % reduction (DamageBeforeDefense − post-reduction damage). */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DamageBlockedByDefense = 0;

	/** Element multiplier applied (weakness/resistance) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float ElementMultiplier = 1.0f;

	/** Status buildup to apply */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 StatusBuildup = 0;

	/** Effective element of the attack; None = non-elemental */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	ESpellElement EffectiveElement = ESpellElement::None;

	// Breakdown for debugging
	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	float AttackerDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	float CritMultiplier = 1.0f;

	/** Defense REDUCTION fraction applied [0, 0.5] (cluster 4: was the flat-int blocked amount). */
	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	float DefenderFlatDefense = 0.0f;

	/** Selected infusion source - determines if weapon stats apply */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;
};

/**
 * Centralized damage calculation subsystem
 * All damage should flow through this for consistency
 */
UCLASS()
class WORLD_OF_REFRACTION_API UDamageCalculator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;

	// ==================== MAIN CALCULATION ====================

	/**
	 * Calculate damage with full input struct
	 * Most flexible - use for complex scenarios
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator")
	FDamageCalculationResult CalculateDamage(
		AActor *Attacker,
		AActor *Defender,
		const FDamageCalculationInput &Input);

	/**
	 * Calculate weapon attack damage
	 * Convenience wrapper for basic attacks
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Attack")
	FDamageCalculationResult CalculateAttackDamage(
		AActor *Attacker,
		AActor *Target,
		USkillDataBase *Attack,
		bool bIsInfused = false);

	// ==================== COMPONENT CALCULATIONS ====================

	/**
	 * Get attacker's damage multiplier
	 * Includes Effect Damage (elemental) or Raw Damage (physical)
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetAttackerDamageMultiplier(AActor *Attacker, EActionType ActionType) const;

	/**
	 * Get defender's defense REDUCTION fraction [0, 0.5] (cluster 4: flat-int -> capped %).
	 * TODO: rename to GetDefenderDefenseReduction in a Blueprint-aware pass.
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetDefenderFlatDefense(AActor *Defender) const;

	/**
	 * Get critical hit chance for attacker
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetCriticalChance(AActor *Attacker) const;

	/**
	 * Get the attacker's full crit-DAMAGE multiplier (CRIT_DMG_BASE x1.0 + CritDamage stat ramp to
	 * x1.5, then BonusCritDamage gear + ModifyCritDamage transient toward x2.0). The same value the
	 * live crit path applies; public so the AI scorer can value crits at the attacker's real crit damage.
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetCritDamageMultiplier(AActor *Attacker) const;

	/**
	 * Roll for critical hit
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Components")
	bool RollCriticalHit(AActor *Attacker, float OverrideChance = -1.0f) const;

	// ==================== STATUS EFFECT CALCULATIONS ====================
	// CalculateStatusBuildup was dead code (zero callers) and was removed in
	// feature/integration-gaps-sweep-3. Live buildup amplification +
	// resistance reduction live on UStatusBuildupManager::AddStatusBuildup —
	// that's the path every consumer (ActionExecutor, CombatOrchestrator,
	// ItemExecutor) actually calls.
	//
	// GetBDStackStatusMultiplier also removed (feature/fix-bd-stack-multiplier):
	// it was a thin gate over BDManager methods that lost its only caller when
	// CalculateStatusBuildup was deleted, leaving BD stacks dead. The element-
	// gated accessor now lives on the manager itself —
	// UBrokenDarknessManager::GetElementStackStatusMultiplier(Element) — and is
	// consumed by UStatusBuildupManager::AddStatusBuildup as step 5c.

	// ==================== HEALING CALCULATIONS ====================

	/**
	 * Calculate healing amount
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Healing")
	int32 CalculateHealing(
		AActor *Healer,
		AActor *Target,
		int32 BaseHealing);

	// ==================== DEBUG ====================

	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Debug", CallInEditor)
	void DebugPrintCalculation(const FDamageCalculationResult &Result) const;

private:
	/** Get CharacterData from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Get SkillEffectManager */
	USkillEffectManager *GetSkillEffectManager() const;

	/** Get BrokenDarknessManager from actor */
	UBrokenDarknessManager *GetBrokenDarknessManager(AActor *Actor) const;

	/** Apply status effect modifiers to damage. ActionType gates the
	 *  physical-only RawDamageBuff/Debuff term (Spell actions skip it). */
	float GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender, EActionType ActionType) const;

	/** Attacker's EFFECTIVE (composed, crystal-aware) value for an arbitrary substat, used by the
	 *  per-skill scaling-tier term. RawDamage/SpellDamage map to the SAME GetEvolutionModified* getters
	 *  the baseline AttackerMult uses (so the tier term is consistent with the baseline). Returns 0 for
	 *  ESubStat::None / no attacker / no component. */
	float GetEffectiveStatForScaling(AActor *Attacker, ESubStat Stat) const;

	/** Get CombatGridSubsystem */
	UCombatGridSubsystem *GetCombatGridSubsystem() const;

	/** Cached references */
	UPROPERTY()
	mutable USkillEffectManager *CachedSkillEffectManager = nullptr;

	UPROPERTY()
	mutable UCombatGridSubsystem *CachedCombatGridSubsystem = nullptr;
};