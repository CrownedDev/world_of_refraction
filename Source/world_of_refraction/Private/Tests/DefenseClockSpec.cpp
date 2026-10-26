// DefenseClockSpec.cpp
// B-009: every defense timestamp runs on world time, so wall-clock time passing
// without a world tick (a pause) leaves the press window untouched.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "WoRTestWorld.h"
#include "Combat/Defense/DefenseSystem.h"

namespace
{
	constexpr float CLOCK_TEST_WINDOW_SECONDS = 0.5f;
	constexpr int32 CLOCK_TEST_BASE_DAMAGE = 10;
	constexpr float CLOCK_TEST_ATTACK_SIZE = 1.0f;
	/** Real time slept with no world tick — stands in for a pause. */
	constexpr float CLOCK_WALL_CLOCK_GAP_SECONDS = 0.05f;
	constexpr int32 CLOCK_WARMUP_FRAMES = 3;
	/** An AI press anchored before an impact that lands early in the world's life. */
	constexpr double CLOCK_NEGATIVE_PRESS_TIME = -0.2;
}

BEGIN_DEFINE_SPEC(FDefenseClockSpec, "WoR.Defense.Clock",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDefenseSystem *DefenseSystem = nullptr;
	AActor *Attacker = nullptr;
	AActor *Defender = nullptr;

	void OpenTestWindow()
	{
		DefenseSystem->OpenDefenseWindow(Attacker, Defender, CLOCK_TEST_ATTACK_SIZE, CLOCK_TEST_BASE_DAMAGE, CLOCK_TEST_WINDOW_SECONDS, false);
	}
END_DEFINE_SPEC(FDefenseClockSpec)

void FDefenseClockSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		DefenseSystem = TestWorld->GetSubsystem<UDefenseSystem>();
		TestNotNull(TEXT("DefenseSystem subsystem"), DefenseSystem);
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		TestWorld->Tick(CLOCK_WARMUP_FRAMES);
	});

	AfterEach([this]()
	{
		if (DefenseSystem && Defender)
		{
			DefenseSystem->CloseDefenseWindow(Defender);
		}
		DefenseSystem = nullptr;
		Attacker = nullptr;
		Defender = nullptr;
		TestWorld.Reset();
	});

	It("reads the world's game time", [this]()
	{
		const double WorldTime = TestWorld->GetWorld()->GetTimeSeconds();
		TestTrue(TEXT("World time has advanced past zero"), WorldTime > 0.0);
		TestEqual(TEXT("Clock equals world time"), UDefenseSystem::GetDefenseClockSeconds(DefenseSystem), WorldTime);
	});

	It("keeps the remaining window while only wall-clock time passes", [this]()
	{
		if (!DefenseSystem || !Defender)
		{
			return; // setup failure already recorded
		}
		OpenTestWindow();
		const float RemainingAtOpen = DefenseSystem->GetRemainingWindowTime(Defender);
		TestEqual(TEXT("Full window at open"), RemainingAtOpen, CLOCK_TEST_WINDOW_SECONDS);

		FPlatformProcess::Sleep(CLOCK_WALL_CLOCK_GAP_SECONDS);
		TestEqual(TEXT("No world tick → window unchanged"), DefenseSystem->GetRemainingWindowTime(Defender), RemainingAtOpen);

		TestWorld->Tick();
		TestTrue(TEXT("A world tick drains the window"), DefenseSystem->GetRemainingWindowTime(Defender) < RemainingAtOpen);
	});

	It("stamps a player press on the defense clock", [this]()
	{
		if (!DefenseSystem || !Defender)
		{
			return;
		}
		OpenTestWindow();
		DefenseSystem->SubmitDefenseInput(Defender, EDefenseType::Block);

		const FDefenseState State = DefenseSystem->GetDefenseState(Defender);
		if (TestEqual(TEXT("One buffered press"), State.InputBuffer.Num(), 1))
		{
			TestEqual(TEXT("Press time is the clock"), State.InputBuffer[0].InputTime, UDefenseSystem::GetDefenseClockSeconds(DefenseSystem));
		}
	});

	It("keeps an explicit negative press time", [this]()
	{
		if (!DefenseSystem || !Defender)
		{
			return;
		}
		OpenTestWindow();
		DefenseSystem->SubmitDefenseInput(Defender, EDefenseType::Parry, EDefenseDirection::None, CLOCK_NEGATIVE_PRESS_TIME);

		const FDefenseState State = DefenseSystem->GetDefenseState(Defender);
		if (TestEqual(TEXT("One buffered press"), State.InputBuffer.Num(), 1))
		{
			TestEqual(TEXT("Negative time kept, not replaced by now"), State.InputBuffer[0].InputTime, CLOCK_NEGATIVE_PRESS_TIME);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
