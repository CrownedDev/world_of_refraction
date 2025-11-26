// BreakCalculator.h
// Utility functions for calculating break chances based on tier mismatches

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ETier.h"
#include "BreakCalculator.generated.h"

// Forward declarations
class URingData;
class USpellData;
class UUltimateData;

/**
 * Break calculation constants
 */
namespace BreakConstants
{
	/** Base break chance per tier gap (15%) */
	constexpr float BASE_BREAK_CHANCE_PER_TIER = 0.15f;
	
	/** Additional break chance when action is infused (+10%) */
	constexpr float INFUSION_BREAK_BONUS = 0.10f;
	
	/** S-tier equipment can still break when infused (5%) */
	constexpr float S_TIER_INFUSION_BREAK = 0.05f;
	
	/** Custom spells add break risk (+5%) */
	constexpr float CUSTOM_SPELL_BREAK_BONUS = 0.05f;
	
	/** Low durability threshold (<25%) adds break risk */
	constexpr float LOW_DURABILITY_BREAK_BONUS = 0.10f;
	
	/** Medium durability threshold (<50%) adds break risk */
	constexpr float MED_DURABILITY_BREAK_BONUS = 0.05f;
}

/**
 * Result of a break calculation
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBreakCalculationResult
{
	GENERATED_BODY()

	/** Final break chance (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float TotalBreakChance = 0.0f;

	/** Base break chance from tier mismatch */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float TierMismatchChance = 0.0f;

	/** Bonus from infusion */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float InfusionBonus = 0.0f;

	/** Bonus from custom spell */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float CustomSpellBonus = 0.0f;

	/** Bonus from low durability */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float DurabilityBonus = 0.0f;

	/** Tier gap (positive = action higher than equipment) */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	int32 TierGap = 0;

	/** Is this a guaranteed break? */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	bool bGuaranteedBreak = false;

	/** Risk level description */
	UPROPERTY(BlueprintReadOnly, Category = "Break")
	FString RiskLevel = TEXT("Safe");
};

/**
 * Break Calculator - Utility functions for break chance calculations
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBreakCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== CORE CALCULATION ====================

	/**
	 * Calculate break chance from tier mismatch
	 * @param EquipmentTier - Tier of the equipment (ring, weapon)
	 * @param ActionTier - Tier of the action being performed (spell, ultimate)
	 * @param bWasInfused - Was the action infused?
	 * @return Break chance from 0.0 to 1.0
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static float CalculateBreakChance(
		ETier EquipmentTier,
		ETier ActionTier,
		bool bWasInfused);

	/**
	 * Detailed break calculation with breakdown
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FBreakCalculationResult CalculateBreakChanceDetailed(
		ETier EquipmentTier,
		ETier ActionTier,
		bool bWasInfused,
		bool bIsCustomSpell = false,
		float DurabilityPercent = 1.0f);

	// ==================== RING-SPECIFIC ====================

	/**
	 * Calculate break chance for a ring casting a spell
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static float GetRingSpellBreakChance(
		URingData* Ring,
		USpellData* Spell,
		bool bInfused);

	/**
	 * Calculate break chance for a ring using an ultimate
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static float GetRingUltimateBreakChance(
		URingData* Ring,
		UUltimateData* Ultimate,
		bool bInfused);

	// ==================== ROLL FUNCTIONS ====================

	/**
	 * Roll for break based on chance
	 * @param BreakChance - Chance from 0.0 to 1.0
	 * @return True if break occurs
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static bool RollForBreak(float BreakChance);

	/**
	 * Roll with seed for deterministic testing
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static bool RollForBreakSeeded(float BreakChance, int32 Seed);

	// ==================== UTILITY ====================

	/**
	 * Get risk level description based on break chance
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FString GetRiskLevelString(float BreakChance);

	/**
	 * Get color for risk level (for UI)
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FLinearColor GetRiskLevelColor(float BreakChance);

	/**
	 * Is this break chance considered dangerous?
	 */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static bool IsDangerousBreakChance(float BreakChance);
};
