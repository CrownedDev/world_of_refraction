// WoRBuildupExecution.cpp

#include "GAS/WoRBuildupExecution.h"
#include "AbilitySystemComponent.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoREffectContext.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float BUILDUP_EFFECT_LEVEL = 1.0f;
}

UWoRGE_Buildup::UWoRGE_Buildup()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UWoRBuildupExecution::StaticClass();
	Executions.Add(Execution);
}

FBuildupMathInputs UWoRBuildupExecution::ReadAttributes(const FWoRBuildupRequest &Request, const UAbilitySystemComponent *Source,
														const UAbilitySystemComponent *Target)
{
	FBuildupMathInputs Inputs = Request.Inputs;
	if (Request.bHasSource && !Request.bSkipBaseStatAmp)
	{
		UWoRAttributeSyncComponent::ReadSyncedAttribute(Source, UWoRAttributeSet_Stats::GetStatusMultiplierAttribute(),
														Inputs.SourceStatusMultiplier);
	}

	// WoRResistance::GetHitTypeResistance's rule on the attributes: the element's type (None and
	// Generic read Resist_NonElemental), plus the physical subtype when the hit has one.
	float General = 0.0f;
	float HitType = 0.0f;
	float Bonus = 0.0f;
	if (UWoRAttributeSyncComponent::ReadSyncedAttribute(Target, UWoRAttributeSet_Resistance::GetStatusResistanceAttribute(), General) &&
		UWoRAttributeSyncComponent::ReadSyncedAttribute(
			Target, UWoRAttributeSet_Resistance::GetTypeAttribute(WoRDamageType::ElementToDamageType(Request.Element)), HitType) &&
		UWoRAttributeSyncComponent::ReadSyncedAttribute(Target, UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute(), Bonus))
	{
		float Physical = 0.0f;
		if (Request.PhysicalType != EPhysicalDamageType::None &&
			UWoRAttributeSyncComponent::ReadSyncedAttribute(
				Target, UWoRAttributeSet_Resistance::GetTypeAttribute(WoRDamageType::PhysicalToDamageType(Request.PhysicalType)), Physical))
		{
			HitType += Physical;
		}
		Inputs.GeneralStatusResistance = General;
		Inputs.HitTypeResistance = HitType;
		Inputs.StatusResistBonus = Bonus;
	}
	return Inputs;
}

void UWoRBuildupExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
												  FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const
{
	const FWoREffectContext *Context = FWoREffectContext::Get(ExecutionParams.GetOwningSpec().GetContext());
	if (!Context || !Context->BuildupRequest)
	{
		UE_LOG(LogWoR, Warning, TEXT("[BuildupExecution] spec has no buildup request; no buildup added"));
		return;
	}
	const UAbilitySystemComponent *Target = ExecutionParams.GetTargetAbilitySystemComponent();
	const FWoRBuildupRequest &Request = *Context->BuildupRequest;
	const float Buildup = WoRBuildupMath::Compute(ReadAttributes(Request, ExecutionParams.GetSourceAbilitySystemComponent(), Target));

	const FGameplayAttribute BuildupAttribute = UWoRAttributeSet_Vitals::GetBuildupAttribute();
	if (Buildup != 0.0f && Target && Target->HasAttributeSetForAttribute(BuildupAttribute))
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(BuildupAttribute, EGameplayModOp::Additive, Buildup));
	}
	UE_LOG(LogWoR, Verbose, TEXT("[BuildupExecution] %s: +%.2f buildup\n%s"), *GetNameSafe(Target ? Target->GetOwner() : nullptr), Buildup,
		   *GetRequestString(Request));
}

FWoRBuildupRequest UWoRBuildupExecution::MakeRequest(const UStatusBuildupManager &Buildup, AActor *Source, AActor *Target, float Amount,
													 ESpellElement Element, EPhysicalDamageType PhysicalType, bool bSkipBaseStatAmp)
{
	FWoRBuildupRequest Request;
	Request.Inputs = Buildup.GatherBuildupMathInputs(Source, Target, Amount, Element, PhysicalType, bSkipBaseStatAmp);
	Request.Element = Element;
	Request.PhysicalType = PhysicalType;
	Request.bHasSource = Source != nullptr;
	Request.bSkipBaseStatAmp = bSkipBaseStatAmp;
	return Request;
}

FGameplayEffectSpecHandle UWoRBuildupExecution::MakeBuildupSpec(UAbilitySystemComponent &Source, const FWoRBuildupRequest &Request)
{
	FGameplayEffectContextHandle Context = Source.MakeEffectContext();
	if (FWoREffectContext *WoRContext = FWoREffectContext::GetMutable(Context))
	{
		WoRContext->BuildupRequest = MakeShared<const FWoRBuildupRequest>(Request);
	}
	return Source.MakeOutgoingSpec(UWoRGE_Buildup::StaticClass(), BUILDUP_EFFECT_LEVEL, Context);
}

FString UWoRBuildupExecution::GetRequestString(const FWoRBuildupRequest &Request)
{
	return FString::Printf(TEXT("element %s, physical %s, source=%d, skip source multiplier=%d\n%s"),
						   *UEnum::GetValueAsString(Request.Element), *UEnum::GetValueAsString(Request.PhysicalType),
						   Request.bHasSource, Request.bSkipBaseStatAmp, *WoRBuildupMath::GetInputsString(Request.Inputs));
}
