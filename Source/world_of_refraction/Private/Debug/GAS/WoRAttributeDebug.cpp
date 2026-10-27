// WoRAttributeDebug.cpp

#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/TurnManager.h"
#include "Engine/GameInstance.h"
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

	/** A sources-composed attribute: its base is ExpectedBase and exactly one SourcesClass effect is on it. */
	void AddSourcesMismatches(const UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute, float ExpectedBase,
							  const TCHAR *BaseLabel, TSubclassOf<UGameplayEffect> SourcesClass, const FString &LayersText,
							  TArray<FString> &Mismatches)
	{
		const float Base = ASC.GetNumericAttributeBase(Attribute);
		if (!FMath::IsNearlyEqual(Base, ExpectedBase, ATTRIBUTE_DEBUG_TOLERANCE))
		{
			Mismatches.Add(FString::Printf(TEXT("%s: base %.5f, %s %.5f"), *Attribute.GetName(), Base, BaseLabel, ExpectedBase));
		}
		FGameplayEffectQuery Query;
		Query.EffectDefinition = SourcesClass;
		const int32 Sources = ASC.GetActiveEffects(Query).Num();
		if (Sources != 1)
		{
			Mismatches.Add(FString::Printf(TEXT("%s: %d stat-sources effects, expected 1 (%s)"), *Attribute.GetName(), Sources, *LayersText));
		}
	}

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

void UWoRAttributeDebug::AddResistanceSourceMismatches(const AActor *Actor, const UAbilitySystemComponent &ASC,
														const FGameplayAttribute &Attribute, TArray<FString> &Mismatches)
{
	const float Base = ASC.GetNumericAttributeBase(Attribute);
	if (!FMath::IsNearlyZero(Base, ATTRIBUTE_DEBUG_TOLERANCE))
	{
		Mismatches.Add(FString::Printf(TEXT("%s: base %.5f, expected 0 (the resistance sources add it all)"), *Attribute.GetName(), Base));
	}
	// Once per combatant: each source effect applied once, carrying its own bucket per tag.
	if (Attribute != UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute())
	{
		return;
	}
	for (const TPair<TSubclassOf<UGameplayEffect>, TMap<FGameplayTag, float>> &Source :
		 UWoRAttributeSyncComponent::GetResistanceSourceMagnitudes(const_cast<AActor *>(Actor)))
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = Source.Key;
		const TArray<FActiveGameplayEffectHandle> Handles = ASC.GetActiveEffects(Query);
		if (Handles.Num() != 1)
		{
			Mismatches.Add(FString::Printf(TEXT("%s: %d effects, expected 1"), *Source.Key->GetName(), Handles.Num()));
			continue;
		}
		const FActiveGameplayEffect *Active = ASC.GetActiveGameplayEffect(Handles[0]);
		for (const TPair<FGameplayTag, float> &Expected : Source.Value)
		{
			const float Carried = Active ? Active->Spec.GetSetByCallerMagnitude(Expected.Key, false) : 0.0f;
			if (!FMath::IsNearlyEqual(Carried, Expected.Value, ATTRIBUTE_DEBUG_TOLERANCE))
			{
				Mismatches.Add(FString::Printf(TEXT("%s %s: carries %.5f, source %.5f"), *Source.Key->GetName(),
											   *Expected.Key.ToString(), Carried, Expected.Value));
			}
		}
	}
}

void UWoRAttributeDebug::AddComposedMismatches(const AActor *Actor, const UAbilitySystemComponent &ASC,
											   const FGameplayAttribute &Attribute, TArray<FString> &Mismatches)
{
	if (!Actor->FindComponentByClass<UWoRAttributeSyncComponent>())
	{
		return;
	}
	// The resistance sources are synced with or without CharacterData.
	if (UWoRGE_ResistanceSources::CoversAttribute(Attribute))
	{
		AddResistanceSourceMismatches(Actor, ASC, Attribute, Mismatches);
		return;
	}
	const UCharacterDataComponent *Char = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Char)
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
	if (Attribute == UWoRAttributeSet_Resistance::GetStatusResistanceAttribute())
	{
		FWoRStatLayers ResistLayers;
		if (WoRResistance::GetGeneralStatusResistanceLayers(const_cast<AActor *>(Actor), ResistLayers))
		{
			AddSourcesMismatches(ASC, Attribute, ResistLayers.StatBase, TEXT("capped stat"),
								 UWoRGE_StatSources_StatusResistance::StaticClass(), ResistLayers.ToString(), Mismatches);
		}
		return;
	}
	if (const TSubclassOf<UGameplayEffect> PoolClass = UWoRGE_PoolSources::GetClassFor(Attribute))
	{
		FWoRPoolLayers PoolLayers;
		const bool bMaxHealth = Attribute == UWoRAttributeSet_Vitals::GetMaxHealthAttribute();
		if (bMaxHealth ? Char->GetMaxHPLayers(PoolLayers) : Char->GetMaxEPLayers(PoolLayers))
		{
			AddSourcesMismatches(ASC, Attribute, PoolLayers.StatPortion, TEXT("stat portion"), PoolClass, PoolLayers.ToString(), Mismatches);
			return;
		}
		// Mirrored (a max set another way): no pool effect may be left on it.
		FGameplayEffectQuery Query;
		Query.EffectDefinition = PoolClass;
		const int32 Leftover = ASC.GetActiveEffects(Query).Num();
		if (Leftover != 0)
		{
			Mismatches.Add(FString::Printf(TEXT("%s: mirrored, but %d pool sources effects left"), *Attribute.GetName(), Leftover));
		}
		return;
	}
	ESubStat Stat;
	EWorldPillar Pillar;
	FWoRStatLayers Layers;
	TSubclassOf<UGameplayEffect> SourcesClass;
	const TCHAR *BaseLabel = TEXT("capped stat");
	if (UWoRAttributeSet_Stats::GetComposedStat(Attribute, Stat) && Char->GetStatLayers(Stat, Layers))
	{
		SourcesClass = UWoRGE_StatSources::GetClassFor(Stat);
	}
	else if (UWoRAttributeSet_Stats::GetComposedPillar(Attribute, Pillar) && Char->GetPillarLayers(Pillar, Layers))
	{
		SourcesClass = UWoRGE_StatSources::GetClassFor(Pillar);
		BaseLabel = TEXT("world base");
	}
	else if (Attribute == UWoRAttributeSet_Stats::GetTurnSpeedAttribute())
	{
		const UGameInstance *GI = Actor->GetWorld() ? Actor->GetWorld()->GetGameInstance() : nullptr;
		const UTurnManager *Turns = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
		if (!Turns || !Turns->GetTurnSpeedLayers(Actor, Layers))
		{
			return;
		}
		SourcesClass = UWoRGE_StatSources::GetClassFor(ESubStat::TurnSpeed);
		BaseLabel = TEXT("cached turn speed");
	}
	else
	{
		return;
	}
	AddSourcesMismatches(ASC, Attribute, Layers.StatBase, BaseLabel, SourcesClass, Layers.ToString(), Mismatches);
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
