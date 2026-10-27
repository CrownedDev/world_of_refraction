// WoRTickExecution.cpp

#include "GAS/WoRTickExecution.h"
#include "AbilitySystemComponent.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRDamageExecution.h"
#include "GAS/WoREffectContext.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "WoRLog.h"

FTickMathInputs UWoRTickExecution::ReadAttributes(const FWoRTickRequest &Request, const UAbilitySystemComponent *Holder)
{
	FTickMathInputs Inputs = Request.Inputs;
	if (Request.bHeal)
	{
		if (Holder)
		{
			Inputs.HealingReceivedPercent = FWoREffectSpecBuilder::SumMagnitude(*Holder, ESkillEffectType::HealingReceivedBuff);
		}
	}
	else if (!Inputs.bShareOfMaxHP)
	{
		UWoRDamageExecution::ReadElementResistance(Holder, Request.Element, Inputs.ElementResistance);
	}
	return Inputs;
}

float UWoRTickExecution::GetHealthChange(const FWoRTickRequest &Request, const FTickMathInputs &Inputs)
{
	return Request.bHeal ? static_cast<float>(WoRTickMath::GetHotTickHeal(Inputs))
						 : -static_cast<float>(WoRTickMath::GetDotTickDamage(Inputs));
}

void UWoRTickExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
											   FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const
{
	const FWoREffectContext *Context = FWoREffectContext::Get(ExecutionParams.GetOwningSpec().GetContext());
	if (!Context || !Context->TickRequest)
	{
		UE_LOG(LogWoR, Warning, TEXT("[TickExecution] spec has no tick request; no tick paid"));
		return;
	}
	const UAbilitySystemComponent *Holder = ExecutionParams.GetTargetAbilitySystemComponent();
	const float Change = GetHealthChange(*Context->TickRequest, ReadAttributes(*Context->TickRequest, Holder));

	const FGameplayAttribute Health = UWoRAttributeSet_Vitals::GetHealthAttribute();
	if (!FMath::IsNearlyZero(Change) && Holder && Holder->HasAttributeSetForAttribute(Health))
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Health, EGameplayModOp::Additive, Change));
	}
	UE_LOG(LogWoR, Verbose, TEXT("[TickExecution] %s: %s %.0f"), *GetNameSafe(Holder ? Holder->GetOwner() : nullptr),
		   Context->TickRequest->bHeal ? TEXT("HOT") : TEXT("DOT"), FMath::Abs(Change));
}

FWoRTickRequest UWoRTickExecution::MakeRequest(const FActiveSkillEffect &Effect, AActor *Holder)
{
	FWoRTickRequest Request;
	Request.bHeal = Effect.EffectType == ESkillEffectType::HealthRestore;
	Request.Element = Effect.Element;
	Request.Inputs.Value = Effect.GetStackedValue();
	Request.Inputs.bShareOfMaxHP = Effect.bShareOfMaxHP;
	if (!Request.bHeal && !Effect.bShareOfMaxHP)
	{
		Request.Inputs.ElementResistance = WoRResistance::GetElementDamageResistance(Holder, Effect.Element);
	}
	return Request;
}

FString UWoRTickExecution::GetRequestString(const FWoRTickRequest &Request)
{
	return FString::Printf(TEXT("%s, element %s\n%s"), Request.bHeal ? TEXT("HOT") : TEXT("DOT"),
						   *UEnum::GetValueAsString(Request.Element), *WoRTickMath::GetInputsString(Request.Inputs));
}
