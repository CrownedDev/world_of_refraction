// DamageCalculator.h
// Centralized damage calculation system
// All damage formulas flow through here for consistency

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EActionType.h"
#include "ESpellElement.h"
#include "EInfusionSourceOption.h"
#include "ActionStatModifiers.h"
#include "DamageCalculator.generated.h"

class UCharacterData;
class UWeaponAttackData;
class USkillEffectManager;
class UBrokenDarknessManager;
class UCombatGridSubsystem;
/**
 * Damage calculation constants
 */
namespace DamageConstants
{
	// Critical hits
	constexpr float CRIT_MULTIPLIER = 1.5f;
	constexpr float BASE_CRIT_CHANCE = 0.05f; // 5%
	constexpr float MAX_CRIT_CHANCE = 0.60f;  // 60%

	// Defense
	constexpr float MAX_RESISTANCE = 0.50f; // 50% cap
	constexpr int32 MIN_DAMAGE = 1;			// Always deal at least 1 damage

	// Infusion
	// ELEMENT_INFUSION_PENALTY removed per locked cost matrix — see commit message.
	constexpr float POWER_INFUSION_L1_MULT = 1.3f;
	constexpr float POWER_INFUSION_L2_MULT = 1.6f;
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

	/** Element type (if elemental) */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	ESpellElement Element = ESpellElement::Generic;

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
	 *  DamageCalculator consumes StatusMultiplier / SpellDamage / RawDamage / CritChance from this. */
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

	/** Damage blocked by flat defense */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DamageBlockedByDefense = 0;

	/** Element multiplier applied (weakness/resistance) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float ElementMultiplier = 1.0f;

	/** Status buildup to apply */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 StatusBuildup = 0;

	/** Effective element of the attack */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	ESpellElement EffectiveElement = ESpellElement::Generic;

	// Breakdown for debugging
	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	float AttackerDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	float CritMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Debug")
	int32 DefenderFlatDefense = 0;

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
		UWeaponAttackData *Attack,
		bool bIsInfused = false);

	// ==================== COMPONENT CALCULATIONS ====================

	/**
	 * Get attacker's damage multiplier
	 * Includes Effect Damage (elemental) or Raw Damage (physical)
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetAttackerDamageMultiplier(AActor *Attacker, EActionType ActionType) const;

	/**
	 * Get defender's flat defense value
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	int32 GetDefenderFlatDefense(AActor *Defender) const;

	/**
	 * Get critical hit chance for attacker
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetCriticalChance(AActor *Attacker) const;

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

	// ==================== UTILITY ====================

	/**
	 * Get infusion damage multiplier
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Utility")
	static float GetInfusionDamageMultiplier(int32 InfusionLevel);

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

	/** Apply status effect modifiers to damage */
	float GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender) const;

	/** Skill-effect-driven crit damage multiplier — returns 1.0 + ModifyCritDamage% / 100. */
	float GetCritDamageMultiplier(AActor *Attacker) const;

	/** Get CombatGridSubsystem */
	UCombatGridSubsystem *GetCombatGridSubsystem() const;

	/** Cached references */
	UPROPERTY()
	mutable USkillEffectManager *CachedSkillEffectManager = nullptr;

	UPROPERTY()
	mutable UCombatGridSubsystem *CachedCombatGridSubsystem = nullptr;
};