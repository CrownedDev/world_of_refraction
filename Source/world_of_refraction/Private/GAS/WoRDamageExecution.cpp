// WoRDamageExecution.cpp

#include "GAS/WoRDamageExecution.h"
#include "AbilitySystemComponent.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectContext.h"
#include "WoRLog.h"

namespace
{
	constexpr float DAMAGE_EFFECT_LEVEL = 1.0f;

	/** Out = the attribute, when ASC holds its set; otherwise Out keeps the gathered value. */
	bool ReadAttribute(const UAbilitySystemComponent *ASC, const FGameplayAttribute &Attribute, float &Out)
	{
		if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute))
		{
			return false;
		}
		Out = ASC->GetNumericAttribute(Attribute);
		return true;
	}
}

UWoRGE_Damage::UWoRGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UWoRDamageExecution::StaticClass();
	Executions.Add(Execution);
}

FDamageMathInputs UWoRDamageExecution::ReadAttributes(const FWoRDamageRequest &Request, const UAbilitySystemComponent *Source,
													  const UAbilitySystemComponent *Target)
{
	FDamageMathInputs Inputs = Request.Inputs;
	if (Inputs.bCanCrit)
	{
		if (!Request.bCritChanceOverridden)
		{
			ReadAttribute(Source, UWoRAttributeSet_Stats::GetCritChanceAttribute(), Inputs.CritChance);
		}
		ReadAttribute(Source, UWoRAttributeSet_Stats::GetCritDamageAttribute(), Inputs.CritDamageMultiplier);
	}
	if (Inputs.bApplyDefense)
	{
		ReadAttribute(Target, UWoRAttributeSet_Stats::GetDefenseAttribute(), Inputs.DefenderDefense);
	}

	// WoRResistance::GetElementDamageResistance's rule on the attributes: the element's own
	// resistance, plus Ice for Water (Q-145b); non-elemental hits read none (Q-145).
	const EWoRDamageType ElementType = WoRDamageType::ElementToDamageType(Inputs.Element);
	if (!Request.bIgnoreResistance && ElementType != EWoRDamageType::None)
	{
		float Resistance = 0.0f;
		if (ReadAttribute(Target, UWoRAttributeSet_Resistance::GetTypeAttribute(ElementType), Resistance))
		{
			float IceResistance = 0.0f;
			if (ElementType == EWoRDamageType::Water &&
				ReadAttribute(Target, UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Ice), IceResistance))
			{
				Resistance += IceResistance;
			}
			Inputs.ElementResistance = Resistance;
		}
	}
	return Inputs;
}

void UWoRDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
												 FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const
{
	const FWoREffectContext *Context = FWoREffectContext::Get(ExecutionParams.GetOwningSpec().GetContext());
	if (!Context || !Context->DamageRequest)
	{
		UE_LOG(LogWoR, Warning, TEXT("[DamageExecution] spec has no damage request; no damage dealt"));
		return;
	}
	const UAbilitySystemComponent *Target = ExecutionParams.GetTargetAbilitySystemComponent();
	const FDamageCalculationResult Result = WoRDamageMath::Compute(
		ReadAttributes(*Context->DamageRequest, ExecutionParams.GetSourceAbilitySystemComponent(), Target));

	const FGameplayAttribute Health = UWoRAttributeSet_Vitals::GetHealthAttribute();
	if (Result.FinalDamage > 0 && Target && Target->HasAttributeSetForAttribute(Health))
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(Health, EGameplayModOp::Additive, -static_cast<float>(Result.FinalDamage)));
	}
	UE_LOG(LogWoR, Verbose, TEXT("[DamageExecution] %s: %d damage%s"), *GetNameSafe(Target ? Target->GetOwner() : nullptr),
		   Result.FinalDamage, Result.bWasCritical ? TEXT(" (crit)") : TEXT(""));
}

FWoRDamageRequest UWoRDamageExecution::MakeRequest(const UDamageCalculator &Calculator, AActor *Attacker, AActor *Defender,
												   const FDamageCalculationInput &Input)
{
	FWoRDamageRequest Request;
	Request.Inputs = Calculator.GatherDamageMathInputs(Attacker, Defender, Input);
	Request.bCritChanceOverridden = Input.OverrideCritChance >= 0.0f;
	Request.bIgnoreResistance = Input.bIgnoreResistance;
	return Request;
}

FGameplayEffectSpecHandle UWoRDamageExecution::MakeDamageSpec(UAbilitySystemComponent &Source, const FWoRDamageRequest &Request)
{
	FGameplayEffectContextHandle Context = Source.MakeEffectContext();
	if (FWoREffectContext *WoRContext = FWoREffectContext::GetMutable(Context))
	{
		WoRContext->DamageRequest = MakeShared<const FWoRDamageRequest>(Request);
	}
	return Source.MakeOutgoingSpec(UWoRGE_Damage::StaticClass(), DAMAGE_EFFECT_LEVEL, Context);
}

FString UWoRDamageExecution::GetRequestString(const FWoRDamageRequest &Request)
{
	return FString::Printf(TEXT("crit chance overridden=%d, ignore resistance=%d\n%s"), Request.bCritChanceOverridden,
						   Request.bIgnoreResistance, *WoRDamageMath::GetInputsString(Request.Inputs));
}
