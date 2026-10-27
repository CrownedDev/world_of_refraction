// WoRDerivedStatMagnitudes.cpp

#include "GAS/WoRDerivedStatMagnitudes.h"
#include "Character/StatLayers.h"
#include "Combat/CombatConstants.h"
#include "GAS/WoRAttributeSets.h"
#include "WoRLog.h"

namespace
{
	/** Efficiency = 1 - the reduction. */
	constexpr float REDUCTION_SUBTRACTS = -1.0f;
}

void UWoRMMC_ClampedStatTerm::Init(const FGameplayAttribute &From, ESubStat Stat, float Coefficient)
{
	FromCapture = FGameplayEffectAttributeCaptureDefinition(From, EGameplayEffectAttributeCaptureSource::Target, false);
	SourceStat = Stat;
	TermCoefficient = Coefficient;
	RelevantAttributesToCapture.Add(FromCapture);
}

float UWoRMMC_ClampedStatTerm::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec &Spec) const
{
	FAggregatorEvaluateParameters Parameters;
	Parameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Parameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Source = 0.0f;
	if (!GetCapturedAttributeMagnitude(FromCapture, Spec, Parameters, Source))
	{
		UE_LOG(LogWoR, Warning, TEXT("[DerivedStat] %s: %s not captured; the term is 0"), *GetClass()->GetName(),
			   *FromCapture.AttributeToCapture.GetName());
	}
	return FWoRStatLayers::ForStat(SourceStat).Clamp(Source) * TermCoefficient;
}

UWoRMMC_CritChanceLuckTerm::UWoRMMC_CritChanceLuckTerm()
{
	Init(UWoRAttributeSet_Stats::GetLuckAttribute(), ESubStat::Luck, CombatConstants::CRIT_CHANCE_LUCK_BONUS);
}

UWoRMMC_EfficiencyReductionTerm::UWoRMMC_EfficiencyReductionTerm()
{
	Init(UWoRAttributeSet_Stats::GetEfficiencyReductionAttribute(), ESubStat::Efficiency, REDUCTION_SUBTRACTS);
}
