// WoRActionCosts.cpp

#include "GAS/WoRActionCosts.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "HAL/IConsoleManager.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRLog.h"

namespace
{
	/** A cost GE has no level: the amount is the SetByCaller. */
	constexpr float COST_EFFECT_LEVEL = 1.0f;

	int32 GWoRCostsUseGAS = 0;
	FAutoConsoleVariableRef CVarWoRCostsUseGAS(
		TEXT("wor.Costs.UseGAS"), GWoRCostsUseGAS,
		TEXT("1: a skill's EP cost and infusion HP backlash are paid as Instant GameplayEffects (B-043a); needs wor.Effects.UseGAS 1. 0: the old calls (default)."),
		ECVF_Default);

	FGameplayEffectSpecHandle MakeCostSpec(UAbilitySystemComponent *ASC, TSubclassOf<UGameplayEffect> Class,
										   const FGameplayTag &DataTag, int32 Amount)
	{
		if (!ASC)
		{
			return FGameplayEffectSpecHandle();
		}
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Class, COST_EFFECT_LEVEL, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DataTag, -static_cast<float>(Amount));
		}
		return Spec;
	}

	bool TryPay(AActor *Payer, int32 Amount, TSubclassOf<UGameplayEffect> Class, const FGameplayTag &DataTag,
				const TCHAR *What)
	{
		if (Amount <= 0)
		{
			return true;
		}
		if (!WoRCosts::PaysThroughGAS(Payer))
		{
			return false;
		}
		UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Payer);
		const FGameplayEffectSpecHandle Spec = MakeCostSpec(ASC, Class, DataTag, Amount);
		if (!Spec.IsValid())
		{
			UE_LOG(LogWoR, Log, TEXT("[Costs] %s: no %s spec; paying the old way"), *GetNameSafe(Payer), What);
			return false;
		}
		if (!ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied())
		{
			// Something blocked the GE (a tag requirement, an immunity): pay the old way, never cast free.
			UE_LOG(LogWoR, Log, TEXT("[Costs] %s: the %s cost GE did not apply; paying the old way"), *GetNameSafe(Payer), What);
			return false;
		}
		UE_LOG(LogWoR, Verbose, TEXT("[Costs] %s paid %d %s through GAS"), *GetNameSafe(Payer), Amount, What);
		return true;
	}
}

bool WoRCosts::PaysThroughGASEnabled()
{
	return GWoRCostsUseGAS != 0;
}

void WoRCosts::SetPaysThroughGAS(bool bEnabled)
{
	GWoRCostsUseGAS = bEnabled ? 1 : 0;
}

bool WoRCosts::PaysThroughGAS(const AActor *Payer)
{
	if (!PaysThroughGASEnabled() || !UWoREffectMirrorComponent::ReadsFromGAS() || !Payer || !Payer->HasAuthority())
	{
		return false;
	}
	const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Payer);
	const UCharacterDataComponent *Char = Payer->FindComponentByClass<UCharacterDataComponent>();
	// The Vitals set reads the character from the ASC's owner, so a cost is paid through GAS only when that is Payer.
	return ASC && ASC->GetOwner() == Payer && ASC->GetSet<UWoRAttributeSet_Vitals>() && Char && Char->CharacterData;
}

FGameplayEffectSpecHandle WoRCosts::MakeEnergyCostSpec(UAbilitySystemComponent *ASC, int32 Amount)
{
	return MakeCostSpec(ASC, UWoRGE_EnergyCost::StaticClass(), WoRTags::SetByCaller_Cost_Energy, Amount);
}

FGameplayEffectSpecHandle WoRCosts::MakeBacklashSpec(UAbilitySystemComponent *ASC, int32 Amount)
{
	return MakeCostSpec(ASC, UWoRGE_HPBacklash::StaticClass(), WoRTags::SetByCaller_Cost_Health, Amount);
}

bool WoRCosts::TryPayEnergy(AActor *Payer, int32 Amount)
{
	return TryPay(Payer, Amount, UWoRGE_EnergyCost::StaticClass(), WoRTags::SetByCaller_Cost_Energy, TEXT("EP"));
}

bool WoRCosts::TryPayBacklash(AActor *Payer, int32 Amount)
{
	return TryPay(Payer, Amount, UWoRGE_HPBacklash::StaticClass(), WoRTags::SetByCaller_Cost_Health, TEXT("HP"));
}
