// WoRAttributeDebug.cpp

#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float ATTRIBUTE_DEBUG_SCREEN_SECONDS = 8.0f;
	/** Mirrored floats are copied, not recomputed, so anything above rounding noise is drift. */
	constexpr float ATTRIBUTE_DEBUG_TOLERANCE = 0.001f;

	FAutoConsoleCommandWithWorld GDumpAttributesCmd(
		TEXT("wor.DumpAttributes"),
		TEXT("DEBUG: print every combatant's GAS attributes (B-041)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->FindComponentByClass<UCharacterDataComponent>())
				{
					UE_LOG(LogWoR, Display, TEXT("[AttributeDebug] %s\n%s"), *It->GetName(), *UWoRAttributeDebug::GetAttributesString(*It));
				}
			}
		}));

	FAutoConsoleCommandWithWorld GCompareAttributesCmd(
		TEXT("wor.CompareAttributes"),
		TEXT("DEBUG: check every combatant's GAS attributes equal the stat getters they mirror (B-041)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UWoRAttributeDebug::CompareAttributes(World); }));
}

FString UWoRAttributeDebug::GetAttributesString(AActor *Actor)
{
	const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return TEXT("no AbilitySystemComponent");
	}
	TArray<FString> Lines;
	for (const FWoRAttributeSourceValue &Source : UWoRAttributeSyncComponent::ReadSourceValues(Actor))
	{
		Lines.Add(FString::Printf(TEXT("%s=%.5f"), *Source.Attribute.GetName(), ASC->GetNumericAttribute(Source.Attribute)));
	}
	return Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : TEXT("no CharacterDataComponent");
}

FString UWoRAttributeDebug::GetAttributeMismatchString(AActor *Actor)
{
	const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return TEXT("no AbilitySystemComponent");
	}
	TArray<FString> Mismatches;
	for (const FWoRAttributeSourceValue &Source : UWoRAttributeSyncComponent::ReadSourceValues(Actor))
	{
		if (!ASC->HasAttributeSetForAttribute(Source.Attribute))
		{
			Mismatches.Add(FString::Printf(TEXT("%s: no attribute set"), *Source.Attribute.GetName()));
			continue;
		}
		const float Attribute = ASC->GetNumericAttribute(Source.Attribute);
		if (!FMath::IsNearlyEqual(Attribute, Source.Value, ATTRIBUTE_DEBUG_TOLERANCE))
		{
			Mismatches.Add(FString::Printf(TEXT("%s: attribute %.5f, getter %.5f"), *Source.Attribute.GetName(), Attribute, Source.Value));
		}
		AddComposedMismatches(Actor, *ASC, Source.Attribute, Mismatches);
	}
	return FString::Join(Mismatches, TEXT("\n"));
}

void UWoRAttributeDebug::AddComposedMismatches(const AActor *Actor, const UAbilitySystemComponent &ASC,
											   const FGameplayAttribute &Attribute, TArray<FString> &Mismatches)
{
	const UCharacterDataComponent *Char = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Char || !Actor->FindComponentByClass<UWoRAttributeSyncComponent>())
	{
		return;
	}
	if (const TSubclassOf<UGameplayEffect> DerivedClass = UWoRGE_DerivedStat::GetClassFor(Attribute))
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = DerivedClass;
		const int32 Derived = ASC.GetActiveEffects(Query).Num();
		if (Derived != 1)
		{
			Mismatches.Add(FString::Printf(TEXT("%s: %d derived-stat effects, expected 1"), *Attribute.GetName(), Derived));
		}
		return;
	}
	ESubStat Stat;
	FWoRStatLayers Layers;
	if (!UWoRAttributeSet_Stats::GetComposedStat(Attribute, Stat) || !Char->GetStatLayers(Stat, Layers))
	{
		return;
	}
	const float Base = ASC.GetNumericAttributeBase(Attribute);
	if (!FMath::IsNearlyEqual(Base, Layers.StatBase, ATTRIBUTE_DEBUG_TOLERANCE))
	{
		Mismatches.Add(FString::Printf(TEXT("%s: base %.5f, capped stat %.5f"), *Attribute.GetName(), Base, Layers.StatBase));
	}
	FGameplayEffectQuery Query;
	Query.EffectDefinition = UWoRGE_StatSources::GetClassFor(Stat);
	const int32 Sources = ASC.GetActiveEffects(Query).Num();
	if (Sources != 1)
	{
		Mismatches.Add(FString::Printf(TEXT("%s: %d stat-sources effects, expected 1 (%s)"), *Attribute.GetName(), Sources,
									   *Layers.ToString()));
	}
}

int32 UWoRAttributeDebug::CompareAttributes(const UObject *WorldContextObject)
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
		if (!It->FindComponentByClass<UCharacterDataComponent>())
		{
			continue;
		}
		++Checked;
		const FString Mismatches = GetAttributeMismatchString(*It);
		if (Mismatches.IsEmpty())
		{
			UE_LOG(LogWoR, Display, TEXT("[AttributeDebug] %s: attributes match"), *It->GetName());
			continue;
		}
		++Differing;
		UE_LOG(LogWoR, Warning, TEXT("[AttributeDebug] %s: attributes DIFFER\n%s"), *It->GetName(), *Mismatches);
	}
	const FString Summary = FString::Printf(TEXT("wor.CompareAttributes: %d of %d combatant(s) differ"), Differing, Checked);
	UE_LOG(LogWoR, Display, TEXT("[AttributeDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, ATTRIBUTE_DEBUG_SCREEN_SECONDS, Differing > 0 ? FColor::Red : FColor::Green, Summary);
	}
	return Differing;
}
