// RestSpotSpec.cpp
// B-187 part 1: a rest spot rests the party only in a run and becomes the last rest spot; leaving there pays the
// leave share of the tally. Healing and reviving members are UPartySessionSubsystem's (covered by its own specs);
// the menu is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGameMode.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 REST_SPOT_TEST_SEED = 11;
}

BEGIN_DEFINE_SPEC(FRestSpotSpec, "WoR.Trial.RestSpot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
END_DEFINE_SPEC(FRestSpotSpec)

void FRestSpotSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
	});
	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("brings every player back beside the rest stand, alternating sides (B-187d)", [this]()
	{
		const FTransform Stand(FRotator::ZeroRotator, FVector(100.f, 0.f, 0.f)); // facing +X: right is +Y
		const float Spacing = TrialReturn::RETURN_SPACING_CM;
		TestTrue(TEXT("first: the stand"), TrialReturn::GetReturnStand(Stand, 0).GetLocation().Equals(Stand.GetLocation()));
		TestTrue(TEXT("second: right"), TrialReturn::GetReturnStand(Stand, 1).GetLocation().Equals(FVector(100.f, Spacing, 0.f)));
		TestTrue(TEXT("third: left"), TrialReturn::GetReturnStand(Stand, 2).GetLocation().Equals(FVector(100.f, -Spacing, 0.f)));
		TestTrue(TEXT("fourth: further right"), TrialReturn::GetReturnStand(Stand, 3).GetLocation().Equals(FVector(100.f, 2.f * Spacing, 0.f)));
		TestTrue(TEXT("same facing"), TrialReturn::GetReturnStand(Stand, 3).GetRotation().Equals(Stand.GetRotation()));
	});

	It("rests only in a run, and becomes the run's last rest spot", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run))
		{
			return;
		}
		TestEqual(TEXT("outside a run: nothing"), WoRRestSpot::Rest(TestWorld->GetWorld(), TEXT("SpotA")), INDEX_NONE);
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REST_SPOT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestTrue(TEXT("in a run: rested"), WoRRestSpot::Rest(TestWorld->GetWorld(), TEXT("SpotA")) >= 0);
		TestEqual(TEXT("the last rest spot"), Run->GetLastRestSpot(), FName(TEXT("SpotA")));
	});

	It("pays the leave share of the tally when the party leaves at a rest spot", [this]()
	{
		const FRunPayout Tally = RunRewards::FightTally(EEncounterRank::Boss);
		const FRunPayout Full = RunRewards::ScalePayout(Tally, ETrialDifficulty::Medium, false, RunRewards::BASE_PAYOUT_MULTIPLIER);
		const FRunPayout Left = RunRewards::ScalePayout(Tally, ETrialDifficulty::Medium, false, RunRewards::LEAVE_EARLY_SHARE);
		TestTrue(TEXT("less than the full payout"), Left.HubGold < Full.HubGold);
		TestTrue(TEXT("but something"), Left.HubGold > 0);
		TestFalse(TEXT("the confirm says how to get full rewards"), WoRRestSpot::GetLeaveConfirmText().IsEmpty());
	});
}

#endif
