// BreakCalculator.h
// Durability wear calculations for equipment crystals

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/ItemTier.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "BreakCalculator.generated.h"

class URingData;
class USpellData;
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
 * Detailed breakdown of substat-modified wear.
 * Layered on top of FDurabilityWearResult — surfaces the power/control factors
 * so designers can sanity-check the modifier vs the paper formula.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDurabilityWearWithSubstatsResult
{
	GENERATED_BODY()

	/** Wear from CalculateDurabilityWear before substat modulation */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 BaseWear = 0;

	/** clamp(PF_MIN, 1 + (spell_damage% + status_mult%) * AMP, +inf) */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	float PowerFactor = 1.0f;

	/** clamp(CF_MIN, 1 + (efficiency% + resistance%) * AMP, CF_MAX) */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	float ControlFactor = 1.0f;

	/** Tier difference (action tier - crystal tier). Negative if action is lower tier. */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 TierGap = 0;

	/** Final wear after floor — what callers should apply to the crystal */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	int32 FinalWear = 0;

	/** Authored wear as a FRACTION of max durability (0-1; 0.07 = 7%), BEFORE substat
	 *  power/control modulation and the min-1 floor. Tier-independent (same on any crystal
	 *  tier). For UI / WouldBreakCrystal preview. Applied amount after substats is FinalWear. */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	float WearPercentOfMax = 0.0f;

	/** True when the min-1 mismatch floor raised FinalWear to 1 (a real TierGap>0 cast whose
	 *  modulated wear would otherwise have rounded to 0). For debug / preview visibility. */
	UPROPERTY(BlueprintReadOnly, Category = "Wear")
	bool bMinFloored = false;
};

/**
 * Break Calculator - Durability wear calculations for equipment crystals
 */
UCLASS()
class WORLD_OF_REFRACTION_API UBreakCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
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
	 * Authored wear as a fraction of the crystal's max durability (0-1), BEFORE substat
	 * power/control modulation and the min-1 floor. Single source of the wear percent —
	 * CalculateDurabilityWear and the detailed/substat results all derive from this, so the
	 * amount and the percent can never drift. For UI/preview. Tier-independent.
	 */
	UFUNCTION(BlueprintPure, Category = "Durability")
	static float CalculateWearPercentOfMax(
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

	// ==================== SUBSTAT-MODIFIED WEAR ====================
	// Wraps CalculateDurabilityWear with a caster power/control modifier:
	//   final = base x power_factor / control_factor
	// with a floor of FLOOR_FRAC x base and (for tier_gap < ONE_SHOT_GAP) a
	// ceiling of CEIL_FRAC x max_durability. Pure — no state, no CharacterData ref.
	// Callers pass already-normalized fractions. Efficiency inversion: the live
	// caller passes EfficiencyFrac = 1 - GetEffectiveEfficiencyMultiplier() — the
	// COMPOSED multiplier (innate + equipment BonusEfficiency + attached
	// EfficiencyStone + transients), so geared efficiency DOES reduce wear.

	UFUNCTION(BlueprintPure, Category = "Durability")
	static int32 CalculateDurabilityWearWithSubstats(
		EItemTier CrystalTier,
		EItemTier ActionTier,
		int32 InfusionLevel,
		bool bIsSpell,
		float SpellDamageFrac,
		float StatusMultiplierFrac,
		float EfficiencyFrac,
		float ResistanceFrac);

	UFUNCTION(BlueprintPure, Category = "Durability")
	static FDurabilityWearWithSubstatsResult CalculateDurabilityWearWithSubstatsDetailed(
		EItemTier CrystalTier,
		EItemTier ActionTier,
		int32 InfusionLevel,
		bool bIsSpell,
		float SpellDamageFrac,
		float StatusMultiplierFrac,
		float EfficiencyFrac,
		float ResistanceFrac);
};
