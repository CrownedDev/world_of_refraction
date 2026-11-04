// CombatTuningData.cpp

#include "Combat/Tuning/CombatTuningData.h"
#include "AI/AIDecisionConstants.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Tuning/CombatBaselineConstants.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Infusion/InfusionConstants.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Skills/Definitions/WorldStatRequirements.h"
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

	/** A ladder's FScalableFloat multiplies its curve by Value; 1 keeps the curve's own numbers. */
	constexpr float LADDER_CURVE_SCALE = 1.0f;

	const ECombatLadder TUNING_LADDERS[] = {
		ECombatLadder::TierPower, ECombatLadder::TierGapDamage, ECombatLadder::TierGapCost,
		ECombatLadder::RequirementGap, ECombatLadder::ScalingGrade,
		ECombatLadder::PillarCurve, ECombatLadder::WeaponReferencePower, ECombatLadder::WeaponRequirementByTier,
		ECombatLadder::EnemyRankHealth, ECombatLadder::LevelDropChance, ECombatLadder::MiniBossLevels};

	const TCHAR *TuningLadderName(ECombatLadder Ladder)
	{
		switch (Ladder)
		{
		case ECombatLadder::TierPower:      return TEXT("TierPower");
		case ECombatLadder::TierGapDamage:  return TEXT("TierGapDamage");
		case ECombatLadder::TierGapCost:    return TEXT("TierGapCost");
		case ECombatLadder::RequirementGap: return TEXT("RequirementGap");
		case ECombatLadder::ScalingGrade:   return TEXT("ScalingGrade");
		case ECombatLadder::PillarCurve:          return TEXT("PillarCurve");
		case ECombatLadder::WeaponReferencePower: return TEXT("WeaponReferencePower");
		case ECombatLadder::WeaponRequirementByTier:    return TEXT("WeaponRequirementByTier");
		case ECombatLadder::EnemyRankHealth:      return TEXT("EnemyRankHealth");
		case ECombatLadder::LevelDropChance:      return TEXT("LevelDropChance");
		case ECombatLadder::MiniBossLevels:       return TEXT("MiniBossLevels");
		default:                            return TEXT("?");
		}
	}

	/** Ladders whose keys are indices: outside the range the C++ value (no scaling) applies. The
	 *  gap ladders instead hold their end values past the range, as the C++ clamp does. */
	bool TuningLadderIsIndexed(ECombatLadder Ladder)
	{
		return Ladder != ECombatLadder::TierGapDamage && Ladder != ECombatLadder::TierGapCost &&
			   Ladder != ECombatLadder::RequirementGap;
	}
}

UCombatTuningData::UCombatTuningData()
	: TierPower(LADDER_CURVE_SCALE)
	, TierGapDamage(LADDER_CURVE_SCALE)
	, TierGapCost(LADDER_CURVE_SCALE)
	, RequirementGap(LADDER_CURVE_SCALE)
	, ScalingGrade(LADDER_CURVE_SCALE)
	, PillarCurve(LADDER_CURVE_SCALE)
	, WeaponReferencePower(LADDER_CURVE_SCALE)
	, WeaponRequirementByTier(LADDER_CURVE_SCALE)
	, EnemyRankHealth(LADDER_CURVE_SCALE)
	, LevelDropChance(LADDER_CURVE_SCALE)
	, MiniBossLevels(LADDER_CURVE_SCALE)
	, WeaponReqFactorMin(WeaponRequirement::WEAPON_REQ_FACTOR_MIN)
	, ChargeFocusL1(InfusionConstants::CHARGE_FOCUS_L1)
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
	, AIKillPotentialScore(AIConstants::KILL_POTENTIAL_SCORE)
	, AIHPMissingWeight(AIConstants::HP_MISSING_WEIGHT)
	, AIThreatWeight(AIConstants::THREAT_WEIGHT)
	, AIRawDamageThreatMult(AIConstants::RAW_DAMAGE_THREAT_MULT)
	, AIStatusMultiplierThreatMult(AIConstants::STATUS_MULTIPLIER_THREAT_MULT)
	, AISpellPowerThreatMult(AIConstants::SPELL_POWER_THREAT_MULT)
	, AIStatusScoreWeight(AIConstants::STATUS_SCORE_WEIGHT)
	, AIStatusScoreTrigger(AIConstants::STATUS_SCORE_TRIGGER)
	, AIStatusScoreContribute(AIConstants::STATUS_SCORE_CONTRIBUTE)
	, AIStatusScoreRedundant(AIConstants::STATUS_SCORE_REDUNDANT)
	, AIStatusBarNearTrigger(AIConstants::STATUS_BAR_NEAR_TRIGGER_THRESHOLD)
	, AIEnergyConservationThreshold(AIConstants::ENERGY_CONSERVATION_THRESHOLD)
	, AIEnergyAbundantThreshold(AIConstants::ENERGY_ABUNDANT_THRESHOLD)
	, AIHealHPThresholdHard(AIConstants::HARDEXPERT_HEAL_HP_THRESHOLD)
	, AIHealHPThresholdEasy(AIConstants::EASYMED_HEAL_HP_THRESHOLD)
	, AILastStandHPThreshold(AIConstants::LAST_STAND_HP_THRESHOLD)
	, AIKillSecureFactor(AIConstants::KILL_SECURE_FACTOR)
	, AIFreeActionFactor(AIConstants::FREE_ACTION_FACTOR)
	, AIStarveMargin(AIConstants::STARVE_MARGIN)
	, AIDelayDecay(AIConstants::DELAY_DECAY)
	, AIDefenseAttemptEasy(AIConstants::EASY_DEFENSE_ATTEMPT)
	, AIDefenseAttemptMedium(AIConstants::MEDIUM_DEFENSE_ATTEMPT)
	, AIDefenseAttemptHard(AIConstants::HARD_DEFENSE_ATTEMPT)
	, AIDefenseAttemptExpert(AIConstants::EXPERT_DEFENSE_ATTEMPT)
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
	// The statics and the load are game-thread only; elsewhere the C++ values apply.
	if (bMissing || !UObjectInitialized() || !IsInGameThread())
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
	case ECombatLadder::PillarCurve:          return PillarCurve;
	case ECombatLadder::WeaponReferencePower: return WeaponReferencePower;
	case ECombatLadder::WeaponRequirementByTier:    return WeaponRequirementByTier;
	case ECombatLadder::EnemyRankHealth:      return EnemyRankHealth;
	case ECombatLadder::LevelDropChance:      return LevelDropChance;
	case ECombatLadder::MiniBossLevels:       return MiniBossLevels;
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
	case ECombatLadder::PillarCurve:
		OutFirst = 0;
		OutLast = CombatBaseline::PILLAR_CURVE_MAX_LEVEL;
		return;
	case ECombatLadder::EnemyRankHealth:
		OutFirst = 0;
		OutLast = CombatBaseline::ENEMY_RANK_COUNT - 1;
		return;
	case ECombatLadder::TierPower:
	case ECombatLadder::ScalingGrade:
	case ECombatLadder::WeaponReferencePower:
	case ECombatLadder::WeaponRequirementByTier:
	case ECombatLadder::LevelDropChance:
	case ECombatLadder::MiniBossLevels:
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
	case ECombatLadder::PillarCurve:          return CombatBaseline::CodePillarCurve(Key);
	case ECombatLadder::WeaponReferencePower: return CombatBaseline::CodeWeaponReferencePower(Key);
	case ECombatLadder::WeaponRequirementByTier:    return static_cast<float>(CombatBaseline::CodeWeaponRequirement(Key));
	case ECombatLadder::EnemyRankHealth:      return CombatBaseline::CodeEnemyRankHealth(Key);
	case ECombatLadder::LevelDropChance:      return CombatBaseline::CodeLevelDropChance(Key);
	case ECombatLadder::MiniBossLevels:       return static_cast<float>(CombatBaseline::CodeMiniBossLevels(Key));
	case ECombatLadder::TierPower:
	default:
		return TierPowerScaling::CodeTierPower(Key);
	}
}

float UCombatTuningData::GetLadder(ECombatLadder Ladder, int32 Key, float CodeValue) const
{
	const FScalableFloat &Field = LadderField(Ladder);
	if (!Field.Curve.CurveTable || Field.Curve.RowName.IsNone())
	{
		return CodeValue;
	}
	int32 First, Last;
	GetLadderKeyRange(Ladder, First, Last);
	if (TuningLadderIsIndexed(Ladder) && (Key < First || Key > Last))
	{
		return CodeValue;
	}
	// A row that isn't in the table keeps the C++ value (FScalableFloat alone would return Value).
	float Out = CodeValue;
	const FRealCurve *Found = nullptr;
	if (!Field.EvaluateCurveAtLevel(Out, Found, static_cast<float>(FMath::Clamp(Key, First, Last)), GetName(), false) || !Found)
	{
		return CodeValue;
	}
	return Out;
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
		Field.Value = LADDER_CURVE_SCALE;
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
	Out += FString::Printf(TEXT("  Weapon req unmet x%.2f, no scaling (D-119; the old floor %.3f is obsolete)\n"),
						   WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR, WeaponReqFactorMin);
	Out += FString::Printf(TEXT("  Charge bands    focus %.3f/%.3f  off-focus %.3f/%.3f  balanced %.3f/%.3f\n"),
						   ChargeFocusL1, ChargeFocusL2, ChargeOffFocusL1, ChargeOffFocusL2, ChargeBalancedL1, ChargeBalancedL2);
	Out += FString::Printf(TEXT("  BD strain       per deficit %.3f  threshold/EP %.3f  infusion deficit %d/%d  power max %.3f  control min %.3f"),
						   StrainPerDeficitPoint, StrainThresholdPerEP, StrainInfusionDeficitL1, StrainInfusionDeficitL2,
						   StrainPowerFactorMax, StrainControlFactorMin);
	Out += FString::Printf(TEXT("\n  AI target       kill %d  HP missing %.2f  threat %.2f (raw %.2f, status %.2f, spell %.2f)"),
						   AIKillPotentialScore, AIHPMissingWeight, AIThreatWeight,
						   AIRawDamageThreatMult, AIStatusMultiplierThreatMult, AISpellPowerThreatMult);
	Out += FString::Printf(TEXT("\n  AI skill score  status x%.2f  trigger %.1f  contribute %.1f  redundant %.1f  bar near %.2f"),
						   AIStatusScoreWeight, AIStatusScoreTrigger, AIStatusScoreContribute, AIStatusScoreRedundant, AIStatusBarNearTrigger);
	Out += FString::Printf(TEXT("\n  AI thresholds   EP save %.2f  EP abundant %.2f  heal Hard+ %.2f  heal Easy/Med %.2f  last stand %.2f"),
						   AIEnergyConservationThreshold, AIEnergyAbundantThreshold, AIHealHPThresholdHard,
						   AIHealHPThresholdEasy, AILastStandHPThreshold);
	Out += FString::Printf(TEXT("\n  AI Emerald      kill secure %.2f  free action %.2f  starve %.2f  delay decay %.2f"),
						   AIKillSecureFactor, AIFreeActionFactor, AIStarveMargin, AIDelayDecay);
	Out += FString::Printf(TEXT("\n  AI defend try   Easy %.2f  Medium %.2f  Hard %.2f  Expert %.2f"),
						   AIDefenseAttemptEasy, AIDefenseAttemptMedium, AIDefenseAttemptHard, AIDefenseAttemptExpert);
	// C++ only until B-042 moves the caps into data (Q-218); listed so one print shows the whole baseline.
	Out += FString::Printf(TEXT("\n  C++ caps        defense ceiling %.2f  crit base x%.2f  crit stat cap x%.2f  crit gear ceiling x%.2f"),
						   CombatConstants::DEFENSE_REDUCTION_CEILING, CombatConstants::CRIT_DMG_BASE,
						   CombatConstants::CRIT_DAMAGE_STAT_CAP, CombatConstants::CRIT_DAMAGE_GEAR_CEILING);
	return Out;
}

FString UCombatTuningData::GetDifferencesFromCode() const
{
	FString Out;
	for (const ECombatLadder Ladder : TUNING_LADDERS)
	{
		// A table without the ladder's row is silent at lookup (it keeps the C++ value); report it here.
		const FScalableFloat &Field = LadderField(Ladder);
		if (Field.Curve.CurveTable && !Field.Curve.CurveTable->FindCurve(Field.Curve.RowName, GetName(), false))
		{
			Out += FString::Printf(TEXT("%s: row '%s' is not in %s (C++ values used)\n"), TuningLadderName(Ladder),
								   *Field.Curve.RowName.ToString(), *Field.Curve.CurveTable->GetName());
		}
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
	Single(TEXT("WeaponReqFactorMin"), WeaponReqFactorMin, WeaponRequirement::WEAPON_REQ_FACTOR_MIN);
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
	// B-345: the AI weights and thresholds.
	Single(TEXT("AIKillPotentialScore"), static_cast<float>(AIKillPotentialScore), static_cast<float>(AIConstants::KILL_POTENTIAL_SCORE));
	Single(TEXT("AIHPMissingWeight"), AIHPMissingWeight, AIConstants::HP_MISSING_WEIGHT);
	Single(TEXT("AIThreatWeight"), AIThreatWeight, AIConstants::THREAT_WEIGHT);
	Single(TEXT("AIRawDamageThreatMult"), AIRawDamageThreatMult, AIConstants::RAW_DAMAGE_THREAT_MULT);
	Single(TEXT("AIStatusMultiplierThreatMult"), AIStatusMultiplierThreatMult, AIConstants::STATUS_MULTIPLIER_THREAT_MULT);
	Single(TEXT("AISpellPowerThreatMult"), AISpellPowerThreatMult, AIConstants::SPELL_POWER_THREAT_MULT);
	Single(TEXT("AIStatusScoreWeight"), AIStatusScoreWeight, AIConstants::STATUS_SCORE_WEIGHT);
	Single(TEXT("AIStatusScoreTrigger"), AIStatusScoreTrigger, AIConstants::STATUS_SCORE_TRIGGER);
	Single(TEXT("AIStatusScoreContribute"), AIStatusScoreContribute, AIConstants::STATUS_SCORE_CONTRIBUTE);
	Single(TEXT("AIStatusScoreRedundant"), AIStatusScoreRedundant, AIConstants::STATUS_SCORE_REDUNDANT);
	Single(TEXT("AIStatusBarNearTrigger"), AIStatusBarNearTrigger, AIConstants::STATUS_BAR_NEAR_TRIGGER_THRESHOLD);
	Single(TEXT("AIEnergyConservationThreshold"), AIEnergyConservationThreshold, AIConstants::ENERGY_CONSERVATION_THRESHOLD);
	Single(TEXT("AIEnergyAbundantThreshold"), AIEnergyAbundantThreshold, AIConstants::ENERGY_ABUNDANT_THRESHOLD);
	Single(TEXT("AIHealHPThresholdHard"), AIHealHPThresholdHard, AIConstants::HARDEXPERT_HEAL_HP_THRESHOLD);
	Single(TEXT("AIHealHPThresholdEasy"), AIHealHPThresholdEasy, AIConstants::EASYMED_HEAL_HP_THRESHOLD);
	Single(TEXT("AILastStandHPThreshold"), AILastStandHPThreshold, AIConstants::LAST_STAND_HP_THRESHOLD);
	Single(TEXT("AIKillSecureFactor"), AIKillSecureFactor, AIConstants::KILL_SECURE_FACTOR);
	Single(TEXT("AIFreeActionFactor"), AIFreeActionFactor, AIConstants::FREE_ACTION_FACTOR);
	Single(TEXT("AIStarveMargin"), AIStarveMargin, AIConstants::STARVE_MARGIN);
	Single(TEXT("AIDelayDecay"), AIDelayDecay, AIConstants::DELAY_DECAY);
	Single(TEXT("AIDefenseAttemptEasy"), AIDefenseAttemptEasy, AIConstants::EASY_DEFENSE_ATTEMPT);
	Single(TEXT("AIDefenseAttemptMedium"), AIDefenseAttemptMedium, AIConstants::MEDIUM_DEFENSE_ATTEMPT);
	Single(TEXT("AIDefenseAttemptHard"), AIDefenseAttemptHard, AIConstants::HARD_DEFENSE_ATTEMPT);
	Single(TEXT("AIDefenseAttemptExpert"), AIDefenseAttemptExpert, AIConstants::EXPERT_DEFENSE_ATTEMPT);
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
