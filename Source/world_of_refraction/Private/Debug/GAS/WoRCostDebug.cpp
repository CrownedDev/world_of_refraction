// WoRCostDebug.cpp

#include "Debug/GAS/WoRCostDebug.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "GAS/WoRActionCosts.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Tags/WoRGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float COST_DEBUG_SCREEN_SECONDS = 8.0f;
	/** The amount the probe spec is built with; any positive cost shows the sign. */
	constexpr int32 COST_DEBUG_PROBE_AMOUNT = 10;

	FAutoConsoleCommandWithWorld GPrintCostsCmd(
		TEXT("wor.Costs.Print"),
		TEXT("DEBUG: how each combatant's skill costs are paid (GAS cost GEs or the old calls, B-043a)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UWoRCostDebug::PrintCosts(World); }));

	const TCHAR *YesNo(bool bValue) { return bValue ? TEXT("yes") : TEXT("no"); }
}

FString UWoRCostDebug::GetCostString(AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return FString::Printf(TEXT("%s: no character data"), *GetNameSafe(Actor));
	}
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const bool bVitals = ASC && ASC->GetSet<UWoRAttributeSet_Vitals>();
	FString Line = FString::Printf(
		TEXT("%s: costs paid %s (wor.Costs.UseGAS %d, wor.Effects.UseGAS %d, authority %s, ASC %s, Vitals %s); EP %d/%d, HP %d/%d"),
		*Actor->GetName(), WoRCosts::PaysThroughGAS(Actor) ? TEXT("through GAS") : TEXT("the old way"),
		WoRCosts::PaysThroughGASEnabled() ? 1 : 0, UWoREffectMirrorComponent::ReadsFromGAS() ? 1 : 0,
		YesNo(Actor->HasAuthority()), YesNo(ASC != nullptr), YesNo(bVitals), Char->CurrentEP, Char->MaxEP, Char->CurrentHP,
		Char->MaxHP);
	if (bVitals)
	{
		Line += FString::Printf(TEXT(" (Energy %.0f, Health %.0f)"),
								ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetEnergyAttribute()),
								ASC->GetNumericAttribute(UWoRAttributeSet_Vitals::GetHealthAttribute()));
	}
	const FGameplayEffectSpecHandle Probe = WoRCosts::MakeEnergyCostSpec(ASC, COST_DEBUG_PROBE_AMOUNT);
	if (Probe.IsValid())
	{
		Line += FString::Printf(TEXT("; a %d EP cost spec changes Energy by %.0f"), COST_DEBUG_PROBE_AMOUNT,
								Probe.Data->GetSetByCallerMagnitude(WoRTags::SetByCaller_Cost_Energy, false, 0.0f));
	}
	return Line;
}

int32 UWoRCostDebug::PrintCosts(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	int32 ThroughGAS = 0;
	int32 Checked = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UCharacterDataComponent>())
		{
			continue;
		}
		++Checked;
		ThroughGAS += WoRCosts::PaysThroughGAS(*It) ? 1 : 0;
		UE_LOG(LogWoR, Display, TEXT("[CostDebug] %s"), *GetCostString(*It));
	}
	const FString Summary = FString::Printf(TEXT("wor.Costs.Print: %d of %d combatant(s) pay through GAS"), ThroughGAS, Checked);
	UE_LOG(LogWoR, Display, TEXT("[CostDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, COST_DEBUG_SCREEN_SECONDS, FColor::Cyan, Summary);
	}
	return ThroughGAS;
}
