// WoRTickExecution.cpp

#include "GAS/WoRTickExecution.h"
#include "AbilitySystemComponent.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRDamageExecution.h"
#include "GAS/WoREffectContext.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "WoRLog.h"

namespace
{
	/** The holder's HealingReceivedBuff total as the enum path reads it (GetTotalStatModifier, which reads the GAS
	 *  mirror for that type when it is the read source), or false without an effect manager. */
	bool ReadHealingReceived(AActor *Holder, float &OutPercent)
	{
		const UWorld *World = Holder ? Holder->GetWorld() : nullptr;
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const USkillEffectManager *Effects = GameInstance ? GameInstance->GetSubsystem<USkillEffectManager>() : nullptr;
		if (!Effects)
		{
			return false;
		}
		OutPercent = Effects->GetTotalStatModifier(Holder, ESkillEffectType::HealingReceivedBuff);
		return true;
	}
}

FTickMathInputs UWoRTickExecution::ReadAttributes(const FWoRTickRequest &Request, const UAbilitySystemComponent *Holder)
{
	FTickMathInputs Inputs = Request.Inputs;
	if (Request.bHeal)
	{
		if (Holder)
		{
			ReadHealingReceived(Holder->GetAvatarActor(), Inputs.HealingReceivedPercent);
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
	// As the enum path: a DOT only ever damages, a HOT only ever heals.
	return Request.bHeal ? static_cast<float>(FMath::Max(0, WoRTickMath::GetHotTickHeal(Inputs)))
						 : -static_cast<float>(FMath::Max(0, WoRTickMath::GetDotTickDamage(Inputs)));
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
	UE_LOG(LogWoR, VeryVerbose, TEXT("[TickExecution] %s"), *GetRequestString(*Context->TickRequest));

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
	if (Request.bHeal)
	{
		ReadHealingReceived(Holder, Request.Inputs.HealingReceivedPercent);
	}
	else if (!Effect.bShareOfMaxHP)
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
