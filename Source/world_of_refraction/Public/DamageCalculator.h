// DamageCalculator.h
// Centralized damage calculation system
// All damage formulas flow through here for consistency

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SpellElement.h"
#include "EInfusionSourceOption.h"
#include "DamageCalculator.generated.h"

class UCharacterData;
class USpellData;
class UAbilityData;
class UWeaponAttackData;
class UStatusEffectManager;
class UBrokenDarknessManager;

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

	// Element interactions
	constexpr float WEAKNESS_MULTIPLIER = 1.5f;	  // 50% more damage
	constexpr float RESISTANCE_MULTIPLIER = 0.5f; // 50% less damage
	constexpr float NEUTRAL_MULTIPLIER = 1.0f;

	// Infusion
	constexpr float ELEMENT_INFUSION_PENALTY = 0.7f; // 30% damage penalty for casters
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

	/** Is this elemental damage? */
	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	bool bIsElemental = false;

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

	/** Damage reduced by resistance (percentage) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DamageReducedByResistance = 0;

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
	float DefenderResistance = 0.0f;

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
	 * Calculate spell damage
	 * Convenience wrapper for spell attacks
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Spell")
	FDamageCalculationResult CalculateSpellDamage(
		AActor *Caster,
		AActor *Target,
		USpellData *Spell,
		bool bUseElementalMode = true,
		int32 InfusionLevel = 0);

	/**
	 * Calculate ability damage
	 * Convenience wrapper for ability attacks
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Ability")
	FDamageCalculationResult CalculateAbilityDamage(
		AActor *User,
		AActor *Target,
		UAbilityData *Ability,
		bool bIsInfused = false,
		int32 PowerInfusionLevel = 0);

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
	float GetAttackerDamageMultiplier(AActor *Attacker, bool bIsElemental) const;

	/**
	 * Get defender's flat defense value
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	int32 GetDefenderFlatDefense(AActor *Defender) const;

	/**
	 * Get defender's elemental resistance
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetDefenderResistance(AActor *Defender) const;

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

	/**
	 * Get element interaction multiplier
	 * Currently always returns 1.0 - no elemental advantage system
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Components")
	float GetElementInteractionMultiplier(ESpellElement AttackElement, ESpellElement DefenderElement) const;

	// ==================== STATUS EFFECT CALCULATIONS ====================

	/**
	 * Calculate status effect buildup
	 * Includes BD stack multiplier if applicable
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Status")
	int32 CalculateStatusBuildup(
		AActor *Attacker,
		AActor *Target,
		int32 BaseBuildup,
		ESpellElement Element);

	/**
	 * Get BD stack multiplier for status effects
	 * Returns 1.0 if not BD or element doesn't match
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Status")
	float GetBDStackStatusMultiplier(AActor *Attacker, ESpellElement Element) const;

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
	 * Apply defense to damage (flat + resistance)
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Utility")
	int32 ApplyDefenseToValue(
		int32 Damage,
		int32 FlatDefense,
		float Resistance,
		bool bIsElemental) const;

	/**
	 * Get infusion damage multiplier
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Utility")
	static float GetInfusionDamageMultiplier(int32 InfusionLevel);

	/**
	 * Check if element A is weak to element B
	 * Currently returns false - no elemental weakness system
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Utility")
	static bool IsWeakTo(ESpellElement Defender, ESpellElement Attacker);

	/**
	 * Check if element A resists element B
	 * Currently returns false - no elemental resistance system
	 */
	UFUNCTION(BlueprintPure, Category = "Damage Calculator|Utility")
	static bool ResistsElement(ESpellElement Defender, ESpellElement Attacker);

	// ==================== DEBUG ====================

	UFUNCTION(BlueprintCallable, Category = "Damage Calculator|Debug", CallInEditor)
	void DebugPrintCalculation(const FDamageCalculationResult &Result) const;

private:
	/** Get CharacterData from actor */
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Get StatusEffectManager */
	UStatusEffectManager *GetStatusEffectManager() const;

	/** Get BrokenDarknessManager from actor */
	UBrokenDarknessManager *GetBrokenDarknessManager(AActor *Actor) const;

	/** Apply status effect modifiers to damage */
	float GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender, bool bIsElemental) const;

	/** Cached reference */
	UPROPERTY()
	mutable UStatusEffectManager *CachedStatusManager = nullptr;
};
