// BreakCalculator.h
// Utility functions for calculating break chances

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemTier.h"
#include "BreakCalculator.generated.h"

class URingData;
class USpellData;
class UUltimateData;

// Constants for break calculations
namespace BreakConstants
{
	constexpr float BASE_BREAK_CHANCE_PER_TIER = 0.15f;
	constexpr float INFUSION_BREAK_BONUS = 0.10f;
	constexpr float S_TIER_INFUSION_BREAK = 0.05f;
	constexpr float CUSTOM_SPELL_BREAK_BONUS = 0.05f;
	constexpr float LOW_DURABILITY_THRESHOLD = 0.25f;
	constexpr float MED_DURABILITY_THRESHOLD = 0.50f;
	constexpr float LOW_DURABILITY_BREAK_BONUS = 0.10f;
	constexpr float MED_DURABILITY_BREAK_BONUS = 0.05f;
}

/**
 * Detailed breakdown of break chance calculation
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBreakCalculationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float TotalBreakChance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float TierMismatchChance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float InfusionBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float CustomSpellBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	float DurabilityBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	int32 TierGap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	bool bGuaranteedBreak = false;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	FString RiskLevel;
};

/**
 * Break Calculator - Utility functions for equipment break mechanics
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBreakCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== CORE CALCULATIONS ====================

	/** Calculate break chance from tier mismatch + infusion */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static float CalculateBreakChance(EItemTier EquipmentTier, EItemTier ActionTier, bool bWasInfused);

	/** Get detailed breakdown of break calculation */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FBreakCalculationResult CalculateBreakChanceDetailed(
		EItemTier EquipmentTier,
		EItemTier ActionTier,
		bool bWasInfused,
		bool bIsCustomSpell = false,
		float DurabilityPercent = 1.0f);

	// ==================== RING-SPECIFIC ====================

	/** Calculate break chance for ring casting a spell */
	UFUNCTION(BlueprintPure, Category = "Break Calculator|Ring")
	static float GetRingSpellBreakChance(URingData* Ring, USpellData* Spell, bool bInfused);

	/** Calculate break chance for ring casting an ultimate */
	UFUNCTION(BlueprintPure, Category = "Break Calculator|Ring")
	static float GetRingUltimateBreakChance(URingData* Ring, UUltimateData* Ultimate, bool bInfused);

	// ==================== ROLL FUNCTIONS ====================

	/** Roll against break chance, returns true if broken */
	UFUNCTION(BlueprintCallable, Category = "Break Calculator")
	static bool RollForBreak(float BreakChance);

	/** Roll with seed for deterministic testing */
	UFUNCTION(BlueprintCallable, Category = "Break Calculator")
	static bool RollForBreakSeeded(float BreakChance, int32 Seed);

	// ==================== UTILITY ====================

	/** Get human-readable risk level string */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FString GetRiskLevelString(float BreakChance);

	/** Get color for risk level (for UI) */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static FLinearColor GetRiskLevelColor(float BreakChance);

	/** Is this break chance considered dangerous? (>= 25%) */
	UFUNCTION(BlueprintPure, Category = "Break Calculator")
	static bool IsDangerousBreakChance(float BreakChance)
	{
		return BreakChance >= 0.25f;
	}
};
