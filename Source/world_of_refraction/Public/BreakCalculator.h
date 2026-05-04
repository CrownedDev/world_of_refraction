// BreakCalculator.h
// Utility functions for calculating break chances

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemTier.h"
#include "DurabilityConstants.h"
#include "BreakCalculator.generated.h"

class URingData;
class USpellData;

// Constants for break calculations
namespace BreakConstants
{
	constexpr float BASE_BREAK_CHANCE_PER_TIER = 0.15f;
	constexpr float INFUSION_BREAK_BONUS = 0.10f;
	constexpr float S_TIER_INFUSION_BREAK = 0.05f;
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
	float DurabilityBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	int32 TierGap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	bool bGuaranteedBreak = false;

	UPROPERTY(BlueprintReadOnly, Category = "Break")
	FString RiskLevel;
};

/**
 * Detailed breakdown of durability wear calculation.
 * Parallel to FBreakCalculationResult but for the new durability model.
 * Replaces % chance with deterministic wear amounts.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDurabilityWearResult
{
	GENERATED_BODY()

	/** Total wear to apply to the crystal */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 TotalWear = 0;

	/** Wear from action being higher tier than crystal */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 TierMismatchWear = 0;

	/** Wear from infusion level (L1 or L2, ability or spell) */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 InfusionWear = 0;

	/** Tier difference (action tier - crystal tier). Negative if action is lower tier. */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 TierGap = 0;
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
		float DurabilityPercent = 1.0f);

	// ==================== RING-SPECIFIC ====================

	/** Calculate break chance for ring casting a spell */
	UFUNCTION(BlueprintPure, Category = "Break Calculator|Ring")
	static float GetRingSpellBreakChance(URingData *Ring, USpellData *Spell, bool bInfused);

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

	// ==================== DURABILITY WEAR (new model) ====================

	/**
	 * Calculate durability wear from a single action.
	 * Replaces the % break chance model with deterministic wear amounts.
	 *
	 * Wear stacks: tier mismatch + infusion wear.
	 * Example: D-tier crystal, B-tier L2 spell = 6 (mismatch x2) + 12 (spell L2) = 18 wear.
	 *
	 * @param CrystalTier   Tier of the slotted crystal taking the wear
	 * @param ActionTier    Tier of the spell/ability/attack being cast
	 * @param InfusionLevel 0 = no infusion, 1 = L1, 2 = L2
	 * @param bIsSpell      True if the action is a spell, false if ability/attack
	 * @return Total wear to apply to the crystal
	 */
	UFUNCTION(BlueprintPure, Category = "Durability")
	static int32 CalculateDurabilityWear(
		EItemTier CrystalTier,
		EItemTier ActionTier,
		int32 InfusionLevel,
		bool bIsSpell);

	/**
	 * Get detailed breakdown of wear calculation.
	 * Same calculation as CalculateDurabilityWear but returns each component
	 * for UI display / debugging.
	 */
	UFUNCTION(BlueprintPure, Category = "Durability")
	static FDurabilityWearResult CalculateDurabilityWearDetailed(
		EItemTier CrystalTier,
		EItemTier ActionTier,
		int32 InfusionLevel,
		bool bIsSpell);

	/**
	 * Check if applying wear would break the crystal.
	 * Useful for UI preview ("this action will break the crystal!") and
	 * commit-time confirmation prompts.
	 */
	UFUNCTION(BlueprintPure, Category = "Durability")
	static bool WouldBreakCrystal(int32 CurrentDurability, int32 ProposedWear);
};
