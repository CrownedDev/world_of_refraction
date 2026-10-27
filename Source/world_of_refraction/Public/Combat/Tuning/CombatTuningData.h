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

	/** The curve table FillLadderTable writes into (create an empty Curve Table and pick it here). */
	UPROPERTY(EditAnywhere, Category = "Ladders")
	TObjectPtr<UCurveTable> LadderTable;

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

	/** Every ladder value and single value, one line each. */
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
