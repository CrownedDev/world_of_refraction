// WoRDerivedStatMagnitudes.h
// B-042s (Q-042s): the derived-stat terms as magnitude calculations. Each captures its source attribute
// live (Target, not snapshot) and clamps the captured value with the source's compose clamp
// (FWoRStatLayers::ForStat) before scaling it: a capture reads the source's aggregator, which the
// attribute set's PreAttributeChange clamp never reaches, while the getters read the clamped value.
// Debug: wor.CompareAttributes (the derived attributes against their getters).

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "WoRDerivedStatMagnitudes.generated.h"

enum class ESubStat : uint8;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRMMC_ClampedStatTerm : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec &Spec) const override;

protected:
	/** Constructor only: the magnitude is ForStat(Stat).Clamp(From) x Coefficient. */
	void Init(const FGameplayAttribute &From, ESubStat Stat, float Coefficient);

private:
	FGameplayEffectAttributeCaptureDefinition FromCapture;
	ESubStat SourceStat;
	float TermCoefficient = 0.0f;
};

/** Crit chance's Luck term: clamped Luck x CRIT_CHANCE_LUCK_BONUS. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRMMC_CritChanceLuckTerm : public UWoRMMC_ClampedStatTerm
{
	GENERATED_BODY()

public:
	UWoRMMC_CritChanceLuckTerm();
};

/** Efficiency's term: minus the clamped EfficiencyReduction. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRMMC_EfficiencyReductionTerm : public UWoRMMC_ClampedStatTerm
{
	GENERATED_BODY()

public:
	UWoRMMC_EfficiencyReductionTerm();
};
