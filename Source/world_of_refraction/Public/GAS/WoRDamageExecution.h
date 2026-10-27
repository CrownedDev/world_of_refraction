// WoRDamageExecution.h
// GAS stage 3 (B-042e, StatMathsRedesign.md §4 step 5): the damage execution wraps B-219's core
// (WoRDamageMath::Compute). The inputs that are attributes come from the ability system components:
// the attacker's CritChance (unless the action overrides it) and CritDamage, the defender's Defense
// and its resistance to the hit's element (Resist_<type>, Ice also resists Water, Q-145b). The rest
// (base damage, action mods, scaling, grid, status, the crit roll and flags) travel in the effect
// context as an FWoRDamageRequest, gathered by UDamageCalculator as today. The output is Health -=
// the final damage. Nothing in play applies it yet: UDamageCalculator stays the damage path until
// the getters read attributes (B-042h); the spec proves both give the same number.
// Debug: GetRequestString; WoRDamageMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Combat/Damage/DamageMath.h"
#include "GAS/WoRGameplayEffects.h"
#include "WoRDamageExecution.generated.h"

class UAbilitySystemComponent;
class UDamageCalculator;
struct FDamageCalculationInput;

/** What a damage execution needs besides attributes: UDamageCalculator's gathered inputs. */
struct FWoRDamageRequest
{
	FDamageMathInputs Inputs;
	/** The action set its own crit chance (OverrideCritChance): keep it, don't read the attribute. */
	bool bCritChanceOverridden = false;
	/** The action ignores resistance (bIgnoreResistance): keep ElementResistance at 0. */
	bool bIgnoreResistance = false;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
										FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const override;

	/** Request's inputs with the attribute-backed fields read from Source (attacker) and Target
	 *  (defender). A side without an ASC, or without the set, keeps the gathered value. */
	static FDamageMathInputs ReadAttributes(const FWoRDamageRequest &Request, const UAbilitySystemComponent *Source,
											const UAbilitySystemComponent *Target);

	/** The request for one hit, gathered by Calculator as CalculateDamage does (rolls the crit once). */
	static FWoRDamageRequest MakeRequest(const UDamageCalculator &Calculator, AActor *Attacker, AActor *Defender,
										 const FDamageCalculationInput &Input);

	/** Build the damage spec on Source with Request in its context. Invalid without a Source ASC. */
	static FGameplayEffectSpecHandle MakeDamageSpec(UAbilitySystemComponent &Source, const FWoRDamageRequest &Request);

	static FString GetRequestString(const FWoRDamageRequest &Request);
};

/** One hit: Instant, runs UWoRDamageExecution. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Damage : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Damage();
};
