// WoREffectDebug.cpp

#include "Debug/GAS/WoREffectDebug.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float EFFECT_DEBUG_SCREEN_SECONDS = 8.0f;
	/** Stacked values are copied, not recomputed, so anything above rounding noise is drift. */
	constexpr float EFFECT_DEBUG_TOLERANCE = 0.001f;

	FAutoConsoleCommandWithWorld GCompareEffectsCmd(
		TEXT("wor.CompareEffects"),
		TEXT("DEBUG: check every combatant's mirrored GameplayEffects read the same as its stored effects (B-042c)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UWoREffectDebug::CompareEffects(World); }));

	const TArray<ESkillEffectType> &PortedTypes()
	{
		static const TArray<ESkillEffectType> Types = []()
		{
			TArray<ESkillEffectType> Result;
			const UEnum *Enum = StaticEnum<ESkillEffectType>();
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
			{
				const ESkillEffectType Type = static_cast<ESkillEffectType>(Enum->GetValueByIndex(Index));
				if (FWoREffectSpecBuilder::GetEffectClass(Type))
				{
					Result.Add(Type);
				}
			}
			return Result;
		}();
		return Types;
	}
}

FString UWoREffectDebug::GetEffectMismatchString(AActor *Actor)
{
	const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return TEXT("no AbilitySystemComponent");
	}
	if (!Actor->FindComponentByClass<UWoREffectMirrorComponent>())
	{
		return TEXT("no EffectMirrorComponent");
	}

	// Read the stored effects directly, so the compare holds whatever wor.Effects.UseGAS says.
	const TArray<FWoRMirroredEffect> Stored = UWoREffectMirrorComponent::ReadEnumEffects(Actor);
	TArray<FString> Mismatches;
	for (const ESkillEffectType Type : PortedTypes())
	{
		const FString TypeName = UEnum::GetValueAsString(Type);
		const bool bStored = Stored.ContainsByPredicate([Type](const FWoRMirroredEffect &E) { return E.Identity.EffectType == Type; });
		const bool bMirrored = UWoREffectMirrorComponent::HasEffectOfType(*ASC, Type);
		if (bStored != bMirrored)
		{
			Mismatches.Add(FString::Printf(TEXT("%s: stored %d, GAS %d"), *TypeName, bStored, bMirrored));
		}
		if (!bStored && !bMirrored)
		{
			continue;
		}
		for (uint8 ElementIndex = 0; ElementIndex <= static_cast<uint8>(ESpellElement::None); ++ElementIndex)
		{
			const ESpellElement Element = static_cast<ESpellElement>(ElementIndex);
			float StoredTotal = 0.0f;
			for (const FWoRMirroredEffect &E : Stored)
			{
				if (E.Identity.EffectType == Type && E.Identity.Element == Element)
				{
					StoredTotal += E.Magnitude;
				}
			}
			const float MirroredTotal = FWoREffectSpecBuilder::SumMagnitudeForElement(*ASC, Type, Element);
			if (!FMath::IsNearlyEqual(StoredTotal, MirroredTotal, EFFECT_DEBUG_TOLERANCE))
			{
				Mismatches.Add(FString::Printf(TEXT("%s %s: stored %.3f, GAS %.3f"), *TypeName,
											   *UEnum::GetValueAsString(Element), StoredTotal, MirroredTotal));
			}
		}
	}
	return FString::Join(Mismatches, TEXT("\n"));
}

int32 UWoREffectDebug::CompareEffects(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	int32 Differing = 0;
	int32 Checked = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UWoREffectMirrorComponent *Mirror = It->FindComponentByClass<UWoREffectMirrorComponent>();
		if (!Mirror)
		{
			continue;
		}
		++Checked;
		const FString Mismatches = GetEffectMismatchString(*It);
		if (Mismatches.IsEmpty())
		{
			UE_LOG(LogWoREffects, Display, TEXT("[EffectDebug] %s: effects match (%d mirrored)"), *It->GetName(), Mirror->NumMirrored());
			continue;
		}
		++Differing;
		UE_LOG(LogWoREffects, Warning, TEXT("[EffectDebug] %s: effects DIFFER\n%s\nMirror:\n%s"), *It->GetName(), *Mismatches,
			   *Mirror->GetDebugString());
	}
	const FString Summary = FString::Printf(TEXT("wor.CompareEffects: %d of %d combatant(s) differ (UseGAS %d)"), Differing,
											Checked, UWoREffectMirrorComponent::ReadsFromGAS() ? 1 : 0);
	UE_LOG(LogWoREffects, Display, TEXT("[EffectDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, EFFECT_DEBUG_SCREEN_SECONDS, Differing > 0 ? FColor::Red : FColor::Green, Summary);
	}
	return Differing;
}
