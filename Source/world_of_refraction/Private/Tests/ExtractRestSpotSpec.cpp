// ExtractRestSpotSpec.cpp
// B-448 (D-186): Extract only at a boss's rest spot. A rest spot in the wild has no Extract (LeaveRun refuses there and
// the run goes on); the rest spot a beaten mini-boss left offers it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 EXTRACT_TEST_SEED = 448;
	const FName EXTRACT_TEST_MINI_BOSS(TEXT("ExtractMiniBossTeam"));
	const FName EXTRACT_TEST_WILD_SPOT(TEXT("WildRestSpot"));
}

BEGIN_DEFINE_SPEC(FExtractRestSpotSpec, "WoR.Trial.ExtractRestSpot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
END_DEFINE_SPEC(FExtractRestSpotSpec)

void FExtractRestSpotSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), EXTRACT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		}
	});

	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("refuses Extract at a rest spot in the wild, and the run goes on (D-186)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->NoteRestSpotUsed(EXTRACT_TEST_WILD_SPOT);
		TestFalse(TEXT("no Extract here"), WoRRestSpot::CanExtractHere(TestWorld->GetWorld()));
		TestFalse(TEXT("leaving is refused"), WoRRestSpot::LeaveRun(TestWorld->GetWorld()));
		TestTrue(TEXT("still in the run"), Run->IsRunActive());
	});

	It("offers Extract at the rest spot a beaten mini-boss left (ER-09c)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(EXTRACT_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->NoteRestSpotUsed(EXTRACT_TEST_MINI_BOSS);
		TestTrue(TEXT("Extract here"), WoRRestSpot::CanExtractHere(TestWorld->GetWorld()));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
