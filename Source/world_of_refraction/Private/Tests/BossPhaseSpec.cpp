// BossPhaseSpec.cpp
// B-193 (D-098, Q-193): a boss reaches phases from an HP point or a hidden condition, keeps a
// phase when it heals, can pass several phases in one hit, and grows harder (weather, AI, status
// resistance) as it goes.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr float BOSS_TEST_FIRST_POINT = 50.0f;
	constexpr float BOSS_TEST_LAST_POINT = 25.0f;
	constexpr float BOSS_TEST_FIRST_WEATHER = 0.5f;
	constexpr float BOSS_TEST_LAST_WEATHER = 0.8f;
	constexpr float BOSS_TEST_RESIST_STEP = 0.1f;
	constexpr int32 BOSS_TEST_HIDDEN_COUNT = 2;
	constexpr float BOSS_TEST_TOLERANCE = 0.001f;
	constexpr float BOSS_TEST_MARGIN = 10.0f;

	FBossPhase BossTestPhase(FName Id, EBossPhaseConditionKind Kind, float Value, float Weather)
	{
		FBossPhase Phase;
		Phase.PhaseId = Id;
		Phase.Conditions.Add({Kind, Value});
		Phase.AggressionSteps = 1;
		Phase.WeatherIntensity = Weather;
		return Phase;
	}
}

BEGIN_DEFINE_SPEC(FBossPhaseSpec, "WoR.Boss.Phases",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *BossActor = nullptr;
	UCharacterDataComponent *BossHP = nullptr;

	/** The boss with Phases authored (empty: Style's preset). */
	UBossPhaseComponent *SpawnBoss(const TArray<FBossPhase> &Phases, EBossStyle Style = EBossStyle::None)
	{
		UBossData *Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = FWoRTestWorld::NewCharacterData();
		Data->Style = Style;
		Data->Phases = Phases;
		BossActor = TestWorld->SpawnCombatant(Data->Boss, /*bAIControlled=*/true);
		BossHP = BossActor->FindComponentByClass<UCharacterDataComponent>();
		UBossPhaseComponent *Boss = NewObject<UBossPhaseComponent>(BossActor);
		BossActor->AddInstanceComponent(Boss);
		Boss->RegisterComponent();
		Boss->Setup(Data);
		return Boss;
	}

	void SetBossHPPercent(float Percent)
	{
		BossHP->ServerSetHP(FMath::FloorToInt32(BossHP->MaxHP * Percent / 100.0f));
	}
END_DEFINE_SPEC(FBossPhaseSpec)

void FBossPhaseSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		BossActor = nullptr;
		BossHP = nullptr;
		TestWorld.Reset();
	});

	It("reaches phase 2 from an HP point and keeps it when healed back above the line", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss({BossTestPhase(TEXT("Two"), EBossPhaseConditionKind::HPBelowPercent,
															  BOSS_TEST_FIRST_POINT, BOSS_TEST_FIRST_WEATHER)});
		TestTrue(TEXT("The boss has HP"), BossHP->MaxHP > 0);
		SetBossHPPercent(BOSS_TEST_FIRST_POINT + BOSS_TEST_MARGIN);
		TestEqual(TEXT("Above the line: no phase"), Boss->GetReachedPhaseCount(), 0);
		SetBossHPPercent(BOSS_TEST_FIRST_POINT - BOSS_TEST_MARGIN);
		TestEqual(TEXT("Below the line: phase 2"), Boss->GetReachedPhaseCount(), 1);
		BossHP->ServerHeal(BossHP->MaxHP);
		TestEqual(TEXT("Healed to full: still phase 2"), Boss->GetReachedPhaseCount(), 1);
	});

	It("reaches no phase on the killing blow", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss({BossTestPhase(TEXT("Two"), EBossPhaseConditionKind::HPBelowPercent,
															  BOSS_TEST_FIRST_POINT, BOSS_TEST_FIRST_WEATHER)});
		BossHP->ServerTakeDamage(BossHP->MaxHP);
		TestEqual(TEXT("Dead at once: no phase"), Boss->GetReachedPhaseCount(), 0);
	});

	It("reaches a phase from hidden conditions (status breaks, own turns)", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss({BossTestPhase(TEXT("Breaks"), EBossPhaseConditionKind::StatusBreaks, BOSS_TEST_HIDDEN_COUNT, 0.0f),
											   BossTestPhase(TEXT("Turns"), EBossPhaseConditionKind::TurnCount, BOSS_TEST_HIDDEN_COUNT, 0.0f)});
		Boss->RecordStatusBreak();
		TestEqual(TEXT("One break: not yet"), Boss->GetReachedPhaseCount(), 0);
		Boss->RecordStatusBreak();
		TestEqual(TEXT("Two breaks: phase reached"), Boss->GetReachedPhaseCount(), 1);

		UTurnManager *Turns = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("Turn manager"), Turns);
		if (!Turns)
		{
			return;
		}
		AActor *Other = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Turns->OnTurnStarted.Broadcast(Other, 1);
		Turns->OnTurnStarted.Broadcast(BossActor, 1);
		TestEqual(TEXT("Another's turn doesn't count"), Boss->GetReachedPhaseCount(), 1);
		Turns->OnTurnStarted.Broadcast(BossActor, 2);
		TestEqual(TEXT("Two own turns: next phase"), Boss->GetReachedPhaseCount(), 2);
	});

	It("passes two phases in one big hit and the weather pull rises", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss(
			{BossTestPhase(TEXT("Two"), EBossPhaseConditionKind::HPBelowPercent, BOSS_TEST_FIRST_POINT, BOSS_TEST_FIRST_WEATHER),
			 BossTestPhase(TEXT("Three"), EBossPhaseConditionKind::HPBelowPercent, BOSS_TEST_LAST_POINT, BOSS_TEST_LAST_WEATHER)});
		TestEqual(TEXT("No weather pull before a phase"), Boss->GetWeatherIntensity(), 0.0f, BOSS_TEST_TOLERANCE);
		SetBossHPPercent(BOSS_TEST_FIRST_POINT - BOSS_TEST_MARGIN);
		TestEqual(TEXT("Phase 2: first pull"), Boss->GetWeatherIntensity(), BOSS_TEST_FIRST_WEATHER, BOSS_TEST_TOLERANCE);

		UBossPhaseComponent *Fresh = SpawnBoss(
			{BossTestPhase(TEXT("Two"), EBossPhaseConditionKind::HPBelowPercent, BOSS_TEST_FIRST_POINT, BOSS_TEST_FIRST_WEATHER),
			 BossTestPhase(TEXT("Three"), EBossPhaseConditionKind::HPBelowPercent, BOSS_TEST_LAST_POINT, BOSS_TEST_LAST_WEATHER)});
		SetBossHPPercent(BOSS_TEST_LAST_POINT - BOSS_TEST_MARGIN);
		TestEqual(TEXT("One hit past both lines: both phases"), Fresh->GetReachedPhaseCount(), 2);
		TestEqual(TEXT("Strongest pull"), Fresh->GetWeatherIntensity(), BOSS_TEST_LAST_WEATHER, BOSS_TEST_TOLERANCE);
	});

	It("raises status resistance with each break and plays harder each phase", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss({BossTestPhase(TEXT("Two"), EBossPhaseConditionKind::HPBelowPercent,
															  BOSS_TEST_FIRST_POINT, 0.0f)});
		UStatusBuildupManager *Status = TestWorld->GetSubsystem<UStatusBuildupManager>();
		UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		TestNotNull(TEXT("Status manager"), Status);
		TestNotNull(TEXT("AI manager"), AI);
		if (!Status || !AI)
		{
			return;
		}
		const float Before = Status->GetTotalStatusResistance(BossActor, ESpellElement::Fire, EPhysicalDamageType::None);
		Boss->RecordStatusBreak();
		const float After = Status->GetTotalStatusResistance(BossActor, ESpellElement::Fire, EPhysicalDamageType::None);
		TestEqual(TEXT("One break: one step more resistance"), After - Before, BOSS_TEST_RESIST_STEP, BOSS_TEST_TOLERANCE);

		const EAIDifficulty Base = AI->GetDifficultyFor(BossActor);
		SetBossHPPercent(BOSS_TEST_FIRST_POINT - BOSS_TEST_MARGIN);
		TestEqual(TEXT("Phase 2: one AI step harder"), static_cast<int32>(AI->GetDifficultyFor(BossActor)),
				  FMath::Min(static_cast<int32>(Base) + 1, static_cast<int32>(EAIDifficulty::Expert)));
	});

	It("uses the style preset with no authored phases, and memories skip or add phases", [this]()
	{
		UBossPhaseComponent *Boss = SpawnBoss({}, EBossStyle::Timing);
		TestEqual(TEXT("Timing preset: two phases"), Boss->GetPhaseCount(), UBossData::GetPresetPhases(EBossStyle::Timing).Num());
		TestTrue(TEXT("The preset has phases"), Boss->GetPhaseCount() > 0);
		const int32 PresetCount = Boss->GetPhaseCount();
		Boss->SkipNextPhase();
		TestEqual(TEXT("Skipped one"), Boss->GetPhaseCount(), PresetCount - 1);
		Boss->AddPhase(UBossData::GetMemoryAddedPhase());
		TestEqual(TEXT("Added the last stand"), Boss->GetPhaseCount(), PresetCount);
	});
}

#endif
