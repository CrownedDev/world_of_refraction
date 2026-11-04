// WeatherBaselineSpec.cpp
// B-359 (D-149, Q-359): the battle's weather is the trial's element; a boss phase steps it toward the boss's element
// and raises its intensity (D-098); the leaders' HP no longer moves it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 WEATHER_TEST_SEED = 4242;
	constexpr float WEATHER_TEST_PHASE_POINT = 50.0f;
	constexpr float WEATHER_TEST_PHASE_INTENSITY = 0.6f;
	constexpr float WEATHER_TEST_BELOW_POINT = 40.0f;
	constexpr float WEATHER_TEST_PERCENT = 100.0f;
	constexpr int32 WEATHER_TEST_LOW_HP = 1;
	constexpr float WEATHER_TEST_TOLERANCE = 0.001f;
	const ESpellElement WEATHER_TEST_TRIAL_ELEMENT = ESpellElement::Fire;
	const ESpellElement WEATHER_TEST_BOSS_ELEMENT = ESpellElement::Water;
}

BEGIN_DEFINE_SPEC(FWeatherBaselineSpec, "WoR.Weather.Baseline", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UWeatherStateManager *Weather = nullptr;
	AActor *Hero = nullptr;
	AActor *BossActor = nullptr;
	UBossPhaseComponent *Boss = nullptr;
END_DEFINE_SPEC(FWeatherBaselineSpec)

void FWeatherBaselineSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Weather = TestWorld->GetSubsystem<UWeatherStateManager>();
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Weather"), Weather) || !TestNotNull(TEXT("Trial run"), TrialRun)) return;
		UTrialData *Trial = NewObject<UTrialData>(GetTransientPackage());
		Trial->Element = WEATHER_TEST_TRIAL_ELEMENT;
		TrialRun->RestoreRun(Trial, WEATHER_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});

		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBossData *Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = FWoRTestWorld::NewCharacterData();
		Data->Boss->InnateElement = WEATHER_TEST_BOSS_ELEMENT;
		FBossPhase Phase;
		Phase.PhaseId = TEXT("Two");
		Phase.Conditions.Add({EBossPhaseConditionKind::HPBelowPercent, WEATHER_TEST_PHASE_POINT});
		Phase.WeatherIntensity = WEATHER_TEST_PHASE_INTENSITY;
		Data->Phases = {Phase};
		BossActor = TestWorld->SpawnCombatant(Data->Boss, /*bAIControlled=*/true);
		Boss = NewObject<UBossPhaseComponent>(BossActor);
		BossActor->AddInstanceComponent(Boss);
		Boss->RegisterComponent();
		Boss->Setup(Data);
		Weather->InitialiseLeaders({Hero}, {BossActor});
	});

	AfterEach([this]()
	{
		if (Weather)
		{
			Weather->EndCombat();
		}
		Weather = nullptr;
		Hero = nullptr;
		BossActor = nullptr;
		Boss = nullptr;
		TestWorld.Reset();
	});

	It("starts the battle's weather at the trial's element", [this]()
	{
		if (!Weather || !Hero || !BossActor) return;
		TestTrue(TEXT("The baseline is the trial's element"), Weather->GetBaselineElement() == WEATHER_TEST_TRIAL_ELEMENT);
		TestTrue(TEXT("The battle's weather is the trial's element"), Weather->GetBattleWeatherElement() == WEATHER_TEST_TRIAL_ELEMENT);
		TestEqual(TEXT("No intensity before a phase"), Weather->GetBattleWeatherIntensity(), 0.0f, WEATHER_TEST_TOLERANCE);
	});

	It("steps the weather at a boss phase: the boss's element, its phase intensity", [this]()
	{
		if (!Weather || !Boss) return;
		UCharacterDataComponent *BossHP = BossActor->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Boss HP"), BossHP)) return;
		BossHP->ServerSetHP(FMath::FloorToInt32(BossHP->MaxHP * WEATHER_TEST_BELOW_POINT / WEATHER_TEST_PERCENT));
		if (!TestEqual(TEXT("Phase reached"), Boss->GetReachedPhaseCount(), 1)) return;
		TestTrue(TEXT("Toward the boss's element"), Weather->GetBattleWeatherElement() == WEATHER_TEST_BOSS_ELEMENT);
		TestEqual(TEXT("At the phase's intensity"), Weather->GetBattleWeatherIntensity(), WEATHER_TEST_PHASE_INTENSITY, WEATHER_TEST_TOLERANCE);
	});

	It("no longer moves with the leaders' HP", [this]()
	{
		if (!Weather || !Hero) return;
		const float BlendBefore = Weather->GetLastBlendValue();
		UCharacterDataComponent *HeroHP = Hero->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Hero HP"), HeroHP)) return;
		HeroHP->ServerSetHP(WEATHER_TEST_LOW_HP);
		TestEqual(TEXT("The blend didn't move"), Weather->GetLastBlendValue(), BlendBefore, WEATHER_TEST_TOLERANCE);
		TestTrue(TEXT("Still the trial's element"), Weather->GetBattleWeatherElement() == WEATHER_TEST_TRIAL_ELEMENT);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
