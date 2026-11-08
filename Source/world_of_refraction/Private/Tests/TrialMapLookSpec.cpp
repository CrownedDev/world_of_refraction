// TrialMapLookSpec.cpp
// B-189 (RunStructure §4): a trial's element sets its map's sky, fog and sun from one table; Generic changes nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/TrialMapLook.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "WoRTestWorld.h"

namespace
{
	const ESpellElement LOOK_TEST_ELEMENTS[] = {ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth, ESpellElement::Wind,
												ESpellElement::Light, ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void,
												ESpellElement::Reality};
}

BEGIN_DEFINE_SPEC(FTrialMapLookSpec, "WoR.Trial.MapLook", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FTrialMapLookSpec)

void FTrialMapLookSpec::Define()
{
	It("gives every element a look, and two elements look different", [this]()
	{
		for (const ESpellElement Element : LOOK_TEST_ELEMENTS)
		{
			TestTrue(FString::Printf(TEXT("%s has a look"), *UEnum::GetValueAsString(Element)), TrialMapLook::ForElement(Element).bApplies);
		}
		const FTrialMapLook Fire = TrialMapLook::ForElement(ESpellElement::Fire);
		const FTrialMapLook Water = TrialMapLook::ForElement(ESpellElement::Water);
		TestFalse(TEXT("Fire and Water: different fog"), Fire.FogColour.Equals(Water.FogColour));
		TestFalse(TEXT("Fire and Water: different sun"), Fire.SunColour.Equals(Water.SunColour));
	});

	It("leaves an untyped (Generic) trial's map as authored", [this]()
	{
		TestFalse(TEXT("Generic"), TrialMapLook::ForElement(ESpellElement::Generic).bApplies);
		TestFalse(TEXT("None"), TrialMapLook::ForElement(ESpellElement::None).bApplies);
	});

	Describe("on a map", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
		});
		AfterEach([this]() { TestWorld.Reset(); });

		It("sets the sun and the fog to the element's look", [this]()
		{
			UWorld *World = TestWorld->GetWorld();
			ADirectionalLight *Sun = World ? World->SpawnActor<ADirectionalLight>() : nullptr;
			AExponentialHeightFog *Fog = World ? World->SpawnActor<AExponentialHeightFog>() : nullptr;
			if (!TestTrue(TEXT("setup"), Sun && Fog))
			{
				return;
			}
			UDirectionalLightComponent *SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
			if (!TestNotNull(TEXT("sun component"), SunLight))
			{
				return;
			}
			SunLight->SetMobility(EComponentMobility::Movable);
			const float AuthoredIntensity = SunLight->Intensity;
			const FTrialMapLookResult Result = TrialMapLook::ApplyTo(World, ESpellElement::Darkness);
			const FTrialMapLook Look = TrialMapLook::ForElement(ESpellElement::Darkness);
			TestEqual(TEXT("one sun"), Result.Suns, 1);
			TestEqual(TEXT("one fog"), Result.Fogs, 1);
			TestTrue(TEXT("sun colour"), SunLight->GetLightColor().Equals(Look.SunColour, 0.02f)); // stored as an sRGB FColor
			TestEqual(TEXT("sun intensity scaled"), SunLight->Intensity, AuthoredIntensity * Look.SunIntensityScale, 0.01f);
			TestEqual(TEXT("fog density"), Fog->GetComponent()->FogDensity, Look.FogDensity, 0.0001f);
		});

		It("changes nothing for Generic", [this]()
		{
			UWorld *World = TestWorld->GetWorld();
			const FTrialMapLookResult Result = TrialMapLook::ApplyTo(World, ESpellElement::Generic);
			TestEqual(TEXT("no sun touched"), Result.Suns, 0);
			TestEqual(TEXT("no fog touched"), Result.Fogs, 0);
		});
	});
}

#endif
