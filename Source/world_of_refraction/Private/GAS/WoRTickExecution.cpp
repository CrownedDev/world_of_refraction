// WoRTickExecution.cpp

#include "GAS/WoRTickExecution.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
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
		const USkillEffectManager *Effects = WoRFight::Get<USkillEffectManager>(GameInstance);
		if (!Effects)
		{
			return false;
		}
		OutPercent = Effects->GetTotalStatModifier(Holder, ESkillEffectType::HealingReceivedBuff);
		return true;
	}

	/** A gain as the game's ServerHeal / ServerGainEnergy take it: nothing below 1. */
	float TickGain(int32 Amount)
	{
		return static_cast<float>(FMath::Max(0, Amount));
	}

	/** A loss as ServerTakeDamage / ServerSpendEnergy take it: nothing below 1. */
	float TickLoss(int32 Amount)
	{
		return -TickGain(Amount);
	}

	/** Percent of a max pool, rounded, as the enum path's percent restores. */
	int32 TickShareOf(int32 MaxPool, float Percent)
	{
		return FMath::RoundToInt(MaxPool * Percent / CombatConstants::STAT_PERCENT_DIVISOR);
	}
}

bool UWoRTickExecution::IsPaidByTick(ESkillEffectType Type)
{
	return USkillEffectManager::IsPayoutType(Type);
}

FTickMathInputs UWoRTickExecution::ReadAttributes(const FWoRTickRequest &Request, const UAbilitySystemComponent *Holder)
{
	FTickMathInputs Inputs = Request.Inputs;
	if (Request.IsHeal())
	{
		if (Holder)
		{
			ReadHealingReceived(Holder->GetAvatarActor(), Inputs.HealingReceivedPercent);
		}
	}
	else if (Request.PayoutType == ESkillEffectType::DOT && !Inputs.bShareOfMaxHP)
	{
		UWoRDamageExecution::ReadElementResistance(Holder, Request.Element, Inputs.ElementResistance);
	}
	return Inputs;
}

FWoRTickPools UWoRTickExecution::ReadPools(const UAbilitySystemComponent *Holder)
{
	FWoRTickPools Pools;
	const AActor *Avatar = Holder ? Holder->GetAvatarActor() : nullptr;
	if (const UCharacterDataComponent *Char = Avatar ? Avatar->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		Pools.MaxHP = Char->MaxHP;
		Pools.MaxEP = Char->MaxEP;
	}
	else if (Holder && Holder->HasAttributeSetForAttribute(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()))
	{
		Pools.MaxHP = FMath::RoundToInt(Holder->GetNumericAttribute(UWoRAttributeSet_Vitals::GetMaxHealthAttribute()));
		Pools.MaxEP = FMath::RoundToInt(Holder->GetNumericAttribute(UWoRAttributeSet_Vitals::GetMaxEnergyAttribute()));
	}
	return Pools;
}

float UWoRTickExecution::GetHealthChange(const FWoRTickRequest &Request, const FTickMathInputs &Inputs)
{
	// As the enum path: a DOT only ever damages, a HOT only ever heals.
	return Request.IsHeal() ? static_cast<float>(FMath::Max(0, WoRTickMath::GetHotTickHeal(Inputs)))
						 : -static_cast<float>(FMath::Max(0, WoRTickMath::GetDotTickDamage(Inputs)));
}

FWoRTickPayout UWoRTickExecution::GetPayout(const FWoRTickRequest &Request, const FTickMathInputs &Inputs, const FWoRTickPools &Pools)
{
	const FGameplayAttribute Health = UWoRAttributeSet_Vitals::GetHealthAttribute();
	const FGameplayAttribute Energy = UWoRAttributeSet_Vitals::GetEnergyAttribute();
	const int32 Flat = FMath::RoundToInt(Inputs.Value);
	switch (Request.PayoutType)
	{
	case ESkillEffectType::DOT:
	case ESkillEffectType::HealthRestore:
		return {Health, GetHealthChange(Request, Inputs)};
	case ESkillEffectType::DrainHP:
	case ESkillEffectType::SelfDamage:
		return {Health, TickLoss(Flat)};
	case ESkillEffectType::RestoreHPPercent:
	{
		// As the enum path: the share is rounded, then the healing received amplifies it and rounds again.
		if (Pools.MaxHP <= 0)
		{
			return {Health, 0.0f};
		}
		const int32 Share = TickShareOf(Pools.MaxHP, Inputs.Value);
		return {Health, TickGain(WoRTickMath::AmplifyHeal(Share, Inputs.HealingReceivedPercent))};
	}
	case ESkillEffectType::EnergyRestore:
		return {Energy, TickGain(Flat)};
	case ESkillEffectType::EnergyDrain:
	case ESkillEffectType::DrainEnergy:
		return {Energy, TickLoss(Flat)};
	case ESkillEffectType::RestoreEnergyPercent:
		return {Energy, Pools.MaxEP > 0 ? TickGain(TickShareOf(Pools.MaxEP, Inputs.Value)) : 0.0f};
	default:
		return {Health, 0.0f};
	}
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
	const FWoRTickRequest &Request = *Context->TickRequest;
	const FWoRTickPayout Payout = GetPayout(Request, ReadAttributes(Request, Holder), ReadPools(Holder));
	UE_LOG(LogWoR, VeryVerbose, TEXT("[TickExecution] %s"), *GetRequestString(Request));

	if (!FMath::IsNearlyZero(Payout.Change) && Holder && Holder->HasAttributeSetForAttribute(Payout.Attribute))
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Payout.Attribute, EGameplayModOp::Additive, Payout.Change));
	}
	UE_LOG(LogWoR, Verbose, TEXT("[TickExecution] %s: %s %s %+.0f"), *GetNameSafe(Holder ? Holder->GetOwner() : nullptr),
		   *UEnum::GetValueAsString(Request.PayoutType), *Payout.Attribute.GetName(), Payout.Change);
}

FWoRTickRequest UWoRTickExecution::MakeRequest(const FActiveSkillEffect &Effect, AActor *Holder)
{
	FWoRTickRequest Request;
	Request.PayoutType = Effect.EffectType;
	Request.Element = Effect.Element;
	Request.Inputs.Value = Effect.GetStackedValue();
	Request.Inputs.bShareOfMaxHP = Effect.bShareOfMaxHP;
	if (Request.IsHeal())
	{
		ReadHealingReceived(Holder, Request.Inputs.HealingReceivedPercent);
	}
	else if (Request.PayoutType == ESkillEffectType::DOT && !Effect.bShareOfMaxHP)
	{
		Request.Inputs.ElementResistance = WoRResistance::GetElementDamageResistance(Holder, Effect.Element);
	}
	return Request;
}

FString UWoRTickExecution::GetRequestString(const FWoRTickRequest &Request)
{
	return FString::Printf(TEXT("%s, element %s\n%s"), *UEnum::GetValueAsString(Request.PayoutType),
						   *UEnum::GetValueAsString(Request.Element), *WoRTickMath::GetInputsString(Request.Inputs));
}
