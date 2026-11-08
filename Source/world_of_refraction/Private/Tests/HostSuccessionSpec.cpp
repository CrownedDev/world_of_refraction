// HostSuccessionSpec.cpp
// B-234b part 1 (Q-234b): who hosts after the host drops (the next player in join order), which failures start a
// hand-over, and a new host adopting the checkpoint copy (its own slot is never written). The drop itself, the take-over
// and the rejoin are the 3-player PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Online/HostSuccession.h"
#include "Save/CoopCheckpoint.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"

namespace
{
	const FString SUCCESSION_TEST_HOST(TEXT("host-id"));
	const FString SUCCESSION_TEST_SECOND(TEXT("second-id"));
	const FString SUCCESSION_TEST_THIRD(TEXT("third-id"));
	constexpr int32 SUCCESSION_TEST_USER = 0;
	/** Review: the spec's own slot, never the real checkpoint copy (WoRCheckpoint::SLOT_NAME) a player keeps. */
	const FString SUCCESSION_TEST_SLOT(TEXT("WoRTestHandOverCopy"));
}

BEGIN_DEFINE_SPEC(FHostSuccessionSpec, "WoR.Coop.HostSuccession", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FHostSuccessionSpec)

void FHostSuccessionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		UGameplayStatics::DeleteGameInSlot(SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER);
	});

	It("picks the next player in join order after the old host", [this]()
	{
		const TArray<FString> Order = {SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD};
		TestEqual(TEXT("the second joiner"), WoRHostSuccession::PickNextHost(Order, SUCCESSION_TEST_HOST), SUCCESSION_TEST_SECOND);
		TestEqual(TEXT("the old host is skipped wherever it is"), WoRHostSuccession::PickNextHost({SUCCESSION_TEST_SECOND, SUCCESSION_TEST_HOST}, SUCCESSION_TEST_SECOND), SUCCESSION_TEST_HOST);
		TestTrue(TEXT("nobody left"), WoRHostSuccession::PickNextHost({SUCCESSION_TEST_HOST}, SUCCESSION_TEST_HOST).IsEmpty());
		TestEqual(TEXT("an empty id is skipped"), WoRHostSuccession::PickNextHost({FString(), SUCCESSION_TEST_THIRD}, SUCCESSION_TEST_HOST), SUCCESSION_TEST_THIRD);
	});

	It("only a lost connection or a timeout starts a hand-over", [this]()
	{
		TestTrue(TEXT("lost"), WoRHostSuccession::IsHostLoss(ENetworkFailure::ConnectionLost));
		TestTrue(TEXT("timeout"), WoRHostSuccession::IsHostLoss(ENetworkFailure::ConnectionTimeout));
		TestFalse(TEXT("a refused join"), WoRHostSuccession::IsHostLoss(ENetworkFailure::PendingConnectionFailure));
		TestFalse(TEXT("a version mismatch"), WoRHostSuccession::IsHostLoss(ENetworkFailure::OutdatedClient));
	});

	It("a client that isn't next rejoins the next host; with nobody else known nothing starts", [this]()
	{
		UWoRHostHandOverSubsystem *HandOver = TestWorld->GetSubsystem<UWoRHostHandOverSubsystem>();
		if (!TestNotNull(TEXT("hand-over subsystem"), HandOver))
		{
			return;
		}
		HandOver->NoteHostOrder({SUCCESSION_TEST_HOST});
		TestFalse(TEXT("alone with the host: nothing to hand over"), HandOver->BeginHandOver());
		// The headless world's player id is none of these, so it rejoins the second joiner.
		HandOver->NoteHostOrder({SUCCESSION_TEST_HOST, SUCCESSION_TEST_SECOND, SUCCESSION_TEST_THIRD});
		TestTrue(TEXT("started"), HandOver->BeginHandOver());
		TestEqual(TEXT("next host"), HandOver->GetNextHost(), SUCCESSION_TEST_SECOND);
		TestEqual(TEXT("rejoining"), HandOver->GetStep(), EHostHandOverStep::Rejoining);
		TestTrue(TEXT("debug lists the order"), HandOver->GetSuccessionString().Contains(SUCCESSION_TEST_THIRD));
	});

	It("a new host adopts a copy holding a run, and refuses without one", [this]()
	{
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("save subsystem"), Save))
		{
			return;
		}
		TestFalse(TEXT("no copy kept"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		UWoRSaveGame *Copy = Cast<UWoRSaveGame>(UGameplayStatics::CreateSaveGameObject(UWoRSaveGame::StaticClass()));
		if (!TestNotNull(TEXT("copy"), Copy))
		{
			return;
		}
		Copy->HandlePreSave(); // stamps the save version, as a real save does (else the load check wipes it, D-060)
		TestTrue(TEXT("a copy without a run written"), UGameplayStatics::SaveGameToSlot(Copy, SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER));
		TestFalse(TEXT("a copy without a run: refused"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		Copy->Data.Run.bHasRun = true;
		TestTrue(TEXT("a copy with a run written"), UGameplayStatics::SaveGameToSlot(Copy, SUCCESSION_TEST_SLOT, SUCCESSION_TEST_USER));
		TestTrue(TEXT("adopted"), Save->AdoptCheckpointFrom(SUCCESSION_TEST_SLOT));
		TestTrue(TEXT("playing the host's copy"), Save->IsPlayingHostCopy());
		TestTrue(TEXT("its run waits to resume"), Save->GetData().Run.bHasRun);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
