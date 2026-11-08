// TrialMapLook.cpp

#include "Trial/TrialMapLook.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	/** Each sun's intensity as the map authored it, so applying a look twice (wor.Trial.Look) doesn't stack. */
	TMap<TWeakObjectPtr<UDirectionalLightComponent>, float> GAuthoredSunIntensity;

	struct FElementLookRow
	{
		ESpellElement Element;
		FLinearColor SkyTint;
		FLinearColor FogColour;
		float FogDensity;
		FLinearColor SunColour;
		float SunIntensityScale;
	};

	// TODO: tune with Crown (placeholder values, B-189). Sky tint replaces the sky atmosphere's Rayleigh scattering.
	const FElementLookRow TRIAL_MAP_LOOKS[] = {
		{ESpellElement::Fire,      FLinearColor(0.30f, 0.12f, 0.06f), FLinearColor(0.85f, 0.40f, 0.20f), 0.035f, FLinearColor(1.00f, 0.70f, 0.50f), 1.10f},
		{ESpellElement::Water,     FLinearColor(0.10f, 0.22f, 0.45f), FLinearColor(0.35f, 0.55f, 0.80f), 0.045f, FLinearColor(0.80f, 0.90f, 1.00f), 0.85f},
		{ESpellElement::Earth,     FLinearColor(0.18f, 0.16f, 0.08f), FLinearColor(0.55f, 0.48f, 0.30f), 0.030f, FLinearColor(1.00f, 0.90f, 0.70f), 1.00f},
		{ESpellElement::Wind,      FLinearColor(0.16f, 0.30f, 0.30f), FLinearColor(0.75f, 0.90f, 0.85f), 0.015f, FLinearColor(0.95f, 1.00f, 0.95f), 1.05f},
		{ESpellElement::Light,     FLinearColor(0.30f, 0.30f, 0.22f), FLinearColor(1.00f, 0.95f, 0.80f), 0.020f, FLinearColor(1.00f, 0.98f, 0.85f), 1.30f},
		{ESpellElement::Darkness,  FLinearColor(0.05f, 0.03f, 0.10f), FLinearColor(0.15f, 0.10f, 0.25f), 0.060f, FLinearColor(0.55f, 0.45f, 0.80f), 0.45f},
		{ESpellElement::Lightning, FLinearColor(0.12f, 0.10f, 0.30f), FLinearColor(0.45f, 0.45f, 0.75f), 0.040f, FLinearColor(0.85f, 0.85f, 1.00f), 0.80f},
		{ESpellElement::Void,      FLinearColor(0.04f, 0.04f, 0.06f), FLinearColor(0.20f, 0.18f, 0.28f), 0.070f, FLinearColor(0.60f, 0.55f, 0.70f), 0.40f},
		{ESpellElement::Reality,   FLinearColor(0.20f, 0.12f, 0.28f), FLinearColor(0.60f, 0.45f, 0.75f), 0.025f, FLinearColor(0.95f, 0.85f, 1.00f), 1.00f},
	};
}

FTrialMapLook TrialMapLook::ForElement(ESpellElement Element)
{
	FTrialMapLook Look;
	for (const FElementLookRow &Row : TRIAL_MAP_LOOKS)
	{
		if (Row.Element == Element)
		{
			Look.bApplies = true;
			Look.SkyTint = Row.SkyTint;
			Look.FogColour = Row.FogColour;
			Look.FogDensity = Row.FogDensity;
			Look.SunColour = Row.SunColour;
			Look.SunIntensityScale = Row.SunIntensityScale;
			break;
		}
	}
	return Look;
}

FTrialMapLookResult TrialMapLook::ApplyTo(UWorld *World, ESpellElement Element)
{
	FTrialMapLookResult Result;
	const FTrialMapLook Look = ForElement(Element);
	if (!World || !Look.bApplies)
	{
		return Result;
	}
	for (auto It = GAuthoredSunIntensity.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent(); // a map that unloaded
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UDirectionalLightComponent *> Suns(*It);
		for (UDirectionalLightComponent *Sun : Suns)
		{
			if (Sun->Mobility == EComponentMobility::Static)
			{
				continue; // a baked sun keeps its light
			}
			const float Authored = GAuthoredSunIntensity.FindOrAdd(Sun, Sun->Intensity);
			Sun->SetLightColor(Look.SunColour);
			Sun->SetIntensity(Authored * Look.SunIntensityScale);
			++Result.Suns;
		}
		TInlineComponentArray<UExponentialHeightFogComponent *> Fogs(*It);
		for (UExponentialHeightFogComponent *Fog : Fogs)
		{
			Fog->SetFogInscatteringColor(Look.FogColour);
			Fog->SetFogDensity(Look.FogDensity);
			++Result.Fogs;
		}
		TInlineComponentArray<USkyAtmosphereComponent *> Skies(*It);
		for (USkyAtmosphereComponent *Sky : Skies)
		{
			Sky->SetRayleighScattering(Look.SkyTint);
			++Result.Skies;
		}
	}
	UE_LOG(LogWoR, Log, TEXT("[TrialMapLook] %s look on %s: %d sun(s), %d fog(s), %d sky(ies)"), *UEnum::GetValueAsString(Element),
		   *World->GetMapName(), Result.Suns, Result.Fogs, Result.Skies);
	return Result;
}

FString TrialMapLook::GetLookString(TOptional<ESpellElement> Element)
{
	FString Out = TEXT("[TrialMapLook] element: sky / fog (density) / sun (x intensity)");
	for (const FElementLookRow &Row : TRIAL_MAP_LOOKS)
	{
		if (Element.IsSet() && Element.GetValue() != Row.Element)
		{
			continue;
		}
		Out += FString::Printf(TEXT("\n  %s: %s / %s (%.3f) / %s (x%.2f)"), *UEnum::GetValueAsString(Row.Element), *Row.SkyTint.ToString(),
							   *Row.FogColour.ToString(), Row.FogDensity, *Row.SunColour.ToString(), Row.SunIntensityScale);
	}
	return Out;
}

static FAutoConsoleCommandWithWorldAndArgs GTrialLookCommand(
	TEXT("wor.Trial.Look"),
	TEXT("B-189: wor.Trial.Look <Element> previews that element's map look on this map (Fire, Water, ...); no argument prints the table."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogWoR, Display, TEXT("%s"), *TrialMapLook::GetLookString());
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogWoR, Warning, TEXT("[TrialMapLook] wor.Trial.Look previews in a game world only (PIE), never the editor's level"));
			return;
		}
		const int64 Value = StaticEnum<ESpellElement>()->GetValueByNameString(Args[0]);
		if (Value == INDEX_NONE)
		{
			UE_LOG(LogWoR, Warning, TEXT("[TrialMapLook] Unknown element '%s'"), *Args[0]);
			return;
		}
		TrialMapLook::ApplyTo(World, static_cast<ESpellElement>(Value));
	}));
