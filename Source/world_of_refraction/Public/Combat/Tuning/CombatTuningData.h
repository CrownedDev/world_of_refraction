// CombatTuningData.h
// B-218 (StatMathsRedesign.md §3, Q-146, Q-218): combat tuning numbers designers change without a
// rebuild. Ladders are FScalableFloat curve-table rows (constant/step keys at each integer);
// single values are fields whose defaults are today's C++ constants, so a new asset changes no
// number. The game reads the one asset at COMBAT_TUNING_ASSET_PATH through WoRTuning (CombatTuning.h
// for ladders, WoRTuning::Get() for single values); with no asset it reads this class's defaults.
// Debug: the Details-panel buttons; UCombatTuningDebug (wor.PrintCombatTuning, wor.CompareCombatTuning).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ScalableFloat.h"
#include "Combat/Tuning/CombatTuning.h"
#include "CombatTuningData.generated.h"

class UCurveTable;

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCombatTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCombatTuningData();

	// ---- Ladders (a curve row each, times Value (starts 1); no curve or a missing row = the C++ ladder) ----

	/** Power of a thing of its own tier, keyed F = 0 .. S = 6. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders")
	FScalableFloat TierPower;

	/** Damage by tier gap (action - channel), keyed -4 .. +4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders")
	FScalableFloat TierGapDamage;

	/** Cost by tier gap, keyed -4 .. +4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders")
	FScalableFloat TierGapCost;

	/** Per-pillar requirement gap (required - character level), keyed -5 .. +5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders")
	FScalableFloat RequirementGap;

	/** Scaling grade coefficient, keyed S = 0 .. F = 6. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders")
	FScalableFloat ScalingGrade;

	// B-224 (BaselineNumbers.md "Decided"): the combat numbers baseline, C++ tables in CombatBaselineConstants.h.

	/** The pillar soft cap a weapon's scaling grades read, keyed by live pillar level 0 .. 7. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Baseline")
	FScalableFloat PillarCurve;

	/** Reference weapon Power of a tier, keyed F = 0 .. S = 6 (authoring guide and debug, not read by damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Baseline")
	FScalableFloat WeaponReferencePower;

	/** Main-pillar requirement of a weapon of its tier, keyed F = 0 .. S = 6 (authoring guide and debug). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Baseline")
	FScalableFloat WeaponRequirementByTier;

	/** Enemy max HP multiplier by encounter rank, keyed by EEncounterRank value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Baseline")
	FScalableFloat EnemyRankHealth;

	/** B-223 (D-102): a normal enemy's chance to drop a pillar level, keyed by its tier F = 0 .. S = 6 (an elite doubles it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Level drops")
	FScalableFloat LevelDropChance;

	/** B-223 (D-102): pillar levels a mini-boss or boss always gives, keyed by its tier F = 0 .. S = 6. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladders|Level drops")
	FScalableFloat MiniBossLevels;

	/** The curve table FillLadderTable writes into (create an empty Curve Table and pick it here). */
	UPROPERTY(EditAnywhere, Category = "Ladders")
	TObjectPtr<UCurveTable> LadderTable;

	// ---- Weapon requirement (B-224) ----

	/** OBSOLETE (B-326, D-119): the old requirement ladder's floor. Nothing reads it now: an unmet weapon
	 *  requirement is WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR. Delete after Crown's PIE pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = 0, ClampMax = 1))
	float WeaponReqFactorMin;

	// ---- Infusion charge bands (damage or status multiplier at charge level 1 / 2) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeFocusL1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeFocusL2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeOffFocusL1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeOffFocusL2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeBalancedL1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
	float ChargeBalancedL2;

	// ---- Broken Darkness strain ----

	/** Strain points per point of requirement deficit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	float StrainPerDeficitPoint;
	/** Break threshold = MaxEP x this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	float StrainThresholdPerEP;
	/** Deficit an infused spell adds at charge level 1 / 2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	int32 StrainInfusionDeficitL1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	int32 StrainInfusionDeficitL2;
	/** Cap on (1 + spell damage fraction). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	float StrainPowerFactorMax;
	/** Floor on (1 - defence fraction). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broken Darkness")
	float StrainControlFactorMin;

	// ---- AI (B-345, D-136): the enemy / ally AI's scoring weights and thresholds (AIDecisionConstants.h) ----

	/** Target score added when the best action can kill the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	int32 AIKillPotentialScore;
	/** Target score per missing HP fraction (a target at 0% HP adds the whole weight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	float AIHPMissingWeight;
	/** Target score per point of the target's threat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	float AIThreatWeight;
	/** Threat per point of Raw Damage, of Status Multiplier and of Spell Damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	float AIRawDamageThreatMult;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	float AIStatusMultiplierThreatMult;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Target")
	float AISpellPowerThreatMult;

	/** A skill's score = its damage estimate + its status score x this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Skill score")
	float AIStatusScoreWeight;
	/** Status score when the hit tips the bar or the bar is near its trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Skill score")
	float AIStatusScoreTrigger;
	/** Status score for buildup that doesn't tip the bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Skill score")
	float AIStatusScoreContribute;
	/** Status score when the target already carries a dangerous status. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Skill score")
	float AIStatusScoreRedundant;
	/** Status bar fill at or above which the AI reads the bar as near its trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Skill score", meta = (ClampMin = 0, ClampMax = 1))
	float AIStatusBarNearTrigger;

	/** EP fraction below which the AI saves energy (no level 2 infusion; reaches for an EP item). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Thresholds", meta = (ClampMin = 0, ClampMax = 1))
	float AIEnergyConservationThreshold;
	/** EP fraction above which Hard and Expert infuse to push a near-full status bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Thresholds", meta = (ClampMin = 0, ClampMax = 1))
	float AIEnergyAbundantThreshold;
	/** HP fraction at or below which Hard and Expert heal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Thresholds", meta = (ClampMin = 0, ClampMax = 1))
	float AIHealHPThresholdHard;
	/** HP fraction at or below which Easy and Medium heal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Thresholds", meta = (ClampMin = 0, ClampMax = 1))
	float AIHealHPThresholdEasy;
	/** HP fraction at or below which the AI wards itself with a Last Stand item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Thresholds", meta = (ClampMin = 0, ClampMax = 1))
	float AILastStandHPThreshold;

	/** Emerald, enemy target: the DoT-secured kill is worth the target's HP x this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Emerald")
	float AIKillSecureFactor;
	/** Emerald, enemy target: turns of the target's threat subtracted for its one free action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Emerald")
	float AIFreeActionFactor;
	/** Emerald, self target: a held action must outscore the best affordable one by this factor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Emerald")
	float AIStarveMargin;
	/** Emerald, self target: a held action's value x 1 / (1 + this x delay turns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Emerald")
	float AIDelayDecay;

	/** Chance the AI tries to defend when a defence window opens, per AI tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Defence", meta = (ClampMin = 0, ClampMax = 1))
	float AIDefenseAttemptEasy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Defence", meta = (ClampMin = 0, ClampMax = 1))
	float AIDefenseAttemptMedium;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Defence", meta = (ClampMin = 0, ClampMax = 1))
	float AIDefenseAttemptHard;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Defence", meta = (ClampMin = 0, ClampMax = 1))
	float AIDefenseAttemptExpert;

	/** The asset at COMBAT_TUNING_ASSET_PATH, or null while none exists. */
	static const UCombatTuningData *GetActive();

	/** The ladder's value at Key: its curve when it has one, else CodeValue. */
	float GetLadder(ECombatLadder Ladder, int32 Key, float CodeValue) const;

	/** Writes every ladder's C++ values into Table as constant-key rows named after the ladder and
	 *  points each FScalableFloat at its row (Value 1). False when Table holds simple curves. */
	bool FillLadderRows(UCurveTable *Table);

	/** The C++ value of a ladder at Key (the fallback, and what FillLadderRows writes). */
	static float GetCodeLadderValue(ECombatLadder Ladder, int32 Key);

	/** Lowest and highest key a ladder row holds. */
	static void GetLadderKeyRange(ECombatLadder Ladder, int32 &OutFirst, int32 &OutLast);

	/** Every ladder value and single value, one line each, then the caps that stay C++ until B-042 (Q-218). */
	FString GetCombatTuningString() const;

	/** Lines for every value that differs from the C++ constants (empty when none do). */
	FString GetDifferencesFromCode() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintTuning() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void CompareWithCode() const;

	/** Fills LadderTable from the C++ ladders (see FillLadderRows). */
	UFUNCTION(CallInEditor, Category = "Debug")
	void FillLadderTable();

private:
	const FScalableFloat &LadderField(ECombatLadder Ladder) const;
};

namespace WoRTuning
{
	/** The active tuning asset, else the class defaults (= today's C++ constants). */
	WORLD_OF_REFRACTION_API const UCombatTuningData &Get();
}
