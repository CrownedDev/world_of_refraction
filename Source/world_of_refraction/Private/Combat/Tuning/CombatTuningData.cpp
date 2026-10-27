// CombatTuningData.cpp

#include "Combat/Tuning/CombatTuningData.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Infusion/InfusionConstants.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Engine/CurveTable.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "WoRLog.h"

namespace
{
	// TODO: cook rule for this path (Asset Manager primary asset type) before the first packaged build.
	const TCHAR *COMBAT_TUNING_ASSET_PATH = TEXT("/Game/Data/Combat/DA_CombatTuning_Default.DA_CombatTuning_Default");

	/** Float noise allowed when comparing asset values with the C++ constants. */
	constexpr float TUNING_COMPARE_TOLERANCE = 1.0e-6f;

	const ECombatLadder TUNING_LADDERS[] = {
		ECombatLadder::TierPower, ECombatLadder::TierGapDamage, ECombatLadder::TierGapCost,
		ECombatLadder::RequirementGap, ECombatLadder::ScalingGrade};

	const TCHAR *TuningLadderName(ECombatLadder Ladder)
	{
		switch (Ladder)
		{
		case ECombatLadder::TierPower:      return TEXT("TierPower");
		case ECombatLadder::TierGapDamage:  return TEXT("TierGapDamage");
		case ECombatLadder::TierGapCost:    return TEXT("TierGapCost");
		case ECombatLadder::RequirementGap: return TEXT("RequirementGap");
		case ECombatLadder::ScalingGrade:   return TEXT("ScalingGrade");
		default:                            return TEXT("?");
		}
	}

	/** Ladders whose keys are indices: outside the range the C++ value (no scaling) applies. The
	 *  gap ladders instead hold their end values past the range, as the C++ clamp does. */
	bool TuningLadderIsIndexed(ECombatLadder Ladder)
	{
		return Ladder == ECombatLadder::TierPower || Ladder == ECombatLadder::ScalingGrade;
	}
}

UCombatTuningData::UCombatTuningData()
	: ChargeFocusL1(InfusionConstants::CHARGE_FOCUS_L1)
	, ChargeFocusL2(InfusionConstants::CHARGE_FOCUS_L2)
	, ChargeOffFocusL1(InfusionConstants::CHARGE_OFFFOCUS_L1)
	, ChargeOffFocusL2(InfusionConstants::CHARGE_OFFFOCUS_L2)
	, ChargeBalancedL1(InfusionConstants::CHARGE_BALANCED_L1)
	, ChargeBalancedL2(InfusionConstants::CHARGE_BALANCED_L2)
	, StrainPerDeficitPoint(BrokenDarknessConstants::STRAIN_PER_DEFICIT_POINT)
	, StrainThresholdPerEP(BrokenDarknessConstants::STRAIN_THRESHOLD_PER_EP)
	, StrainInfusionDeficitL1(BrokenDarknessConstants::STRAIN_INFUSION_DEFICIT_L1)
	, StrainInfusionDeficitL2(BrokenDarknessConstants::STRAIN_INFUSION_DEFICIT_L2)
	, StrainPowerFactorMax(BrokenDarknessConstants::STRAIN_POWER_FACTOR_MAX)
	, StrainControlFactorMin(BrokenDarknessConstants::STRAIN_CONTROL_FACTOR_MIN)
{
}

const UCombatTuningData *UCombatTuningData::GetActive()
{
	// Same lookup as UResistanceProfileData::GetActive: a miss is remembered until the next world
	// starts; outside the editor the asset stays loaded so a GC never forces a load mid-fight.
	static TWeakObjectPtr<const UCombatTuningData> Cached;
	static bool bMissing = false;
	static bool bResetBound = false;
	if (!bResetBound)
	{
		bResetBound = true;
		FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld *, const UWorld::InitializationValues) { bMissing = false; });
	}
	if (Cached.IsValid())
	{
		return Cached.Get();
	}
	if (bMissing || !UObjectInitialized())
	{
		return nullptr;
	}
	const UCombatTuningData *Loaded = Cast<UCombatTuningData>(FSoftObjectPath(COMBAT_TUNING_ASSET_PATH).TryLoad());
	if (!Loaded)
	{
		bMissing = true;
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatTuning] No asset at %s; using the C++ tuning values"), COMBAT_TUNING_ASSET_PATH);
		return nullptr;
	}
	if (!GIsEditor)
	{
		const_cast<UCombatTuningData *>(Loaded)->AddToRoot();
	}
	Cached = Loaded;
	return Loaded;
}

const FScalableFloat &UCombatTuningData::LadderField(ECombatLadder Ladder) const
{
	switch (Ladder)
	{
	case ECombatLadder::TierGapDamage:  return TierGapDamage;
	case ECombatLadder::TierGapCost:    return TierGapCost;
	case ECombatLadder::RequirementGap: return RequirementGap;
	case ECombatLadder::ScalingGrade:   return ScalingGrade;
	case ECombatLadder::TierPower:
	default:
		return TierPower;
	}
}

void UCombatTuningData::GetLadderKeyRange(ECombatLadder Ladder, int32 &OutFirst, int32 &OutLast)
{
	// The gap ladders' C++ tables are flat beyond +-4 (tier gap) and +-5 (requirement gap).
	constexpr int32 TIER_GAP_LAST_STEP = 4;
	constexpr int32 REQUIREMENT_GAP_LAST_STEP = 5;
	constexpr int32 TIER_INDEX_LAST = 6;
	switch (Ladder)
	{
	case ECombatLadder::TierGapDamage:
	case ECombatLadder::TierGapCost:
		OutFirst = -TIER_GAP_LAST_STEP;
		OutLast = TIER_GAP_LAST_STEP;
		return;
	case ECombatLadder::RequirementGap:
		OutFirst = -REQUIREMENT_GAP_LAST_STEP;
		OutLast = REQUIREMENT_GAP_LAST_STEP;
		return;
	case ECombatLadder::TierPower:
	case ECombatLadder::ScalingGrade:
	default:
		OutFirst = 0;
		OutLast = TIER_INDEX_LAST;
		return;
	}
}

float UCombatTuningData::GetCodeLadderValue(ECombatLadder Ladder, int32 Key)
{
	switch (Ladder)
	{
	case ECombatLadder::TierGapDamage:  return TierGapDamage::CodeTierGapDamage(Key);
	case ECombatLadder::TierGapCost:    return TierGapDamage::CodeTierGapCost(Key);
	case ECombatLadder::RequirementGap: return TierGapDamage::CodeRequirementGap(Key);
	case ECombatLadder::ScalingGrade:
	{
		int32 First, Last;
		GetLadderKeyRange(Ladder, First, Last);
		return (Key >= First && Key <= Last) ? CodeScalingTierCoefficient(static_cast<EScalingTier>(Key)) : 0.0f;
	}
	case ECombatLadder::TierPower:
	default:
		return TierPowerScaling::CodeTierPower(Key);
	}
}

float UCombatTuningData::GetLadder(ECombatLadder Ladder, int32 Key, float CodeValue) const
{
	const FScalableFloat &Field = LadderField(Ladder);
	if (!Field.Curve.CurveTable)
	{
		return CodeValue;
	}
	int32 First, Last;
	GetLadderKeyRange(Ladder, First, Last);
	if (TuningLadderIsIndexed(Ladder) && (Key < First || Key > Last))
	{
		return CodeValue;
	}
	return Field.GetValueAtLevel(static_cast<float>(FMath::Clamp(Key, First, Last)));
}

bool UCombatTuningData::FillLadderRows(UCurveTable *Table)
{
	if (!Table || Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatTuning] FillLadderRows needs an empty or rich-curve table"));
		return false;
	}
	Modify();
	Table->Modify();
	for (const ECombatLadder Ladder : TUNING_LADDERS)
	{
		const FName Row(TuningLadderName(Ladder));
		Table->RemoveRow(Row);
		FRichCurve &Curve = Table->AddRichCurve(Row);
		int32 First, Last;
		GetLadderKeyRange(Ladder, First, Last);
		for (int32 Key = First; Key <= Last; ++Key)
		{
			const FKeyHandle Handle = Curve.AddKey(static_cast<float>(Key), GetCodeLadderValue(Ladder, Key));
			Curve.SetKeyInterpMode(Handle, RCIM_Constant);
		}
		FScalableFloat &Field = const_cast<FScalableFloat &>(LadderField(Ladder));
		Field.Value = 1.0f;
		Field.Curve.CurveTable = Table;
		Field.Curve.RowName = Row;
	}
	UCurveTable::InvalidateAllCachedCurves();
	return true;
}

FString UCombatTuningData::GetCombatTuningString() const
{
	FString Out = FString::Printf(TEXT("=== Combat tuning (%s) ===\n"), *GetName());
	for (const ECombatLadder Ladder : TUNING_LADDERS)
	{
		int32 First, Last;
		GetLadderKeyRange(Ladder, First, Last);
		Out += FString::Printf(TEXT("  %-15s %s:"), TuningLadderName(Ladder), LadderField(Ladder).Curve.CurveTable ? TEXT("curve") : TEXT("C++  "));
		for (int32 Key = First; Key <= Last; ++Key)
		{
			Out += FString::Printf(TEXT(" %d=%.3f"), Key, GetLadder(Ladder, Key, GetCodeLadderValue(Ladder, Key)));
		}
		Out += TEXT("\n");
	}
	Out += FString::Printf(TEXT("  Charge bands    focus %.3f/%.3f  off-focus %.3f/%.3f  balanced %.3f/%.3f\n"),
						   ChargeFocusL1, ChargeFocusL2, ChargeOffFocusL1, ChargeOffFocusL2, ChargeBalancedL1, ChargeBalancedL2);
	Out += FString::Printf(TEXT("  BD strain       per deficit %.3f  threshold/EP %.3f  infusion deficit %d/%d  power max %.3f  control min %.3f"),
						   StrainPerDeficitPoint, StrainThresholdPerEP, StrainInfusionDeficitL1, StrainInfusionDeficitL2,
						   StrainPowerFactorMax, StrainControlFactorMin);
	return Out;
}

FString UCombatTuningData::GetDifferencesFromCode() const
{
	FString Out;
	for (const ECombatLadder Ladder : TUNING_LADDERS)
	{
		int32 First, Last;
		GetLadderKeyRange(Ladder, First, Last);
		for (int32 Key = First; Key <= Last; ++Key)
		{
			const float Code = GetCodeLadderValue(Ladder, Key);
			const float Data = GetLadder(Ladder, Key, Code);
			if (!FMath::IsNearlyEqual(Data, Code, TUNING_COMPARE_TOLERANCE))
			{
				Out += FString::Printf(TEXT("%s[%d]: data %.4f, C++ %.4f\n"), TuningLadderName(Ladder), Key, Data, Code);
			}
		}
	}
	auto Single = [&Out](const TCHAR *Name, float Data, float Code)
	{
		if (!FMath::IsNearlyEqual(Data, Code, TUNING_COMPARE_TOLERANCE))
		{
			Out += FString::Printf(TEXT("%s: data %.4f, C++ %.4f\n"), Name, Data, Code);
		}
	};
	Single(TEXT("ChargeFocusL1"), ChargeFocusL1, InfusionConstants::CHARGE_FOCUS_L1);
	Single(TEXT("ChargeFocusL2"), ChargeFocusL2, InfusionConstants::CHARGE_FOCUS_L2);
	Single(TEXT("ChargeOffFocusL1"), ChargeOffFocusL1, InfusionConstants::CHARGE_OFFFOCUS_L1);
	Single(TEXT("ChargeOffFocusL2"), ChargeOffFocusL2, InfusionConstants::CHARGE_OFFFOCUS_L2);
	Single(TEXT("ChargeBalancedL1"), ChargeBalancedL1, InfusionConstants::CHARGE_BALANCED_L1);
	Single(TEXT("ChargeBalancedL2"), ChargeBalancedL2, InfusionConstants::CHARGE_BALANCED_L2);
	Single(TEXT("StrainPerDeficitPoint"), StrainPerDeficitPoint, BrokenDarknessConstants::STRAIN_PER_DEFICIT_POINT);
	Single(TEXT("StrainThresholdPerEP"), StrainThresholdPerEP, BrokenDarknessConstants::STRAIN_THRESHOLD_PER_EP);
	Single(TEXT("StrainInfusionDeficitL1"), StrainInfusionDeficitL1, BrokenDarknessConstants::STRAIN_INFUSION_DEFICIT_L1);
	Single(TEXT("StrainInfusionDeficitL2"), StrainInfusionDeficitL2, BrokenDarknessConstants::STRAIN_INFUSION_DEFICIT_L2);
	Single(TEXT("StrainPowerFactorMax"), StrainPowerFactorMax, BrokenDarknessConstants::STRAIN_POWER_FACTOR_MAX);
	Single(TEXT("StrainControlFactorMin"), StrainControlFactorMin, BrokenDarknessConstants::STRAIN_CONTROL_FACTOR_MIN);
	return Out;
}

void UCombatTuningData::PrintTuning() const
{
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *GetCombatTuningString());
}

void UCombatTuningData::CompareWithCode() const
{
	const FString Differences = GetDifferencesFromCode();
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatTuning] %s: %s"), *GetName(),
		   Differences.IsEmpty() ? TEXT("every value equals the C++ constants") : *(TEXT("differs from the C++ constants\n") + Differences));
}

void UCombatTuningData::FillLadderTable()
{
	if (FillLadderRows(LadderTable))
	{
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatTuning] %s: ladders written to %s; save both assets"), *GetName(), *GetNameSafe(LadderTable));
	}
}

namespace WoRTuning
{
	const UCombatTuningData &Get()
	{
		const UCombatTuningData *Active = UCombatTuningData::GetActive();
		return Active ? *Active : *GetDefault<UCombatTuningData>();
	}

	float GetLadderValue(ECombatLadder Ladder, int32 Key, float CodeValue)
	{
		const UCombatTuningData *Active = UCombatTuningData::GetActive();
		return Active ? Active->GetLadder(Ladder, Key, CodeValue) : CodeValue;
	}
}
