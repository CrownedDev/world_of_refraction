// HostHandOverScreensSpec.cpp
// B-459a: the lost-host signal and what the host-lost card and the Hand over host? confirm show. The card follows the
// hand-over's step (pips, lines, Stop waiting only while waiting on someone else); the confirm refuses off a listen
// server or alone; the signal fires when a hand-over starts and when the player stops waiting. The screens are the PIE
// sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Online/HostSuccession.h"
#include "UI/Coop/HostHandOverModel.h"

namespace
{
	constexpr int32 HANDOVER_TEST_TWO_PLAYERS = 2;
	constexpr int32 HANDOVER_TEST_SECOND_SEARCH = 2;
}

BEGIN_DEFINE_SPEC(FHostHandOverScreensSpec, "WoR.UI.HostHandOver", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FHostHandOverScreensSpec)

void FHostHandOverScreensSpec::Define()
{
	It("hides the card with no hand-over and follows each step", [this]()
	{
		TestFalse(TEXT("no hand-over: hidden"), WoRHostHandOverScreens::BuildCard(EHostHandOverStep::None, false, false, 0).bShown);

		const FHostLostCard Taking = WoRHostHandOverScreens::BuildCard(EHostHandOverStep::Hosting, false, true, 0);
		TestTrue(TEXT("shown"), Taking.bShown);
		TestEqual(TEXT("a drop's title"), Taking.Title.ToString(), FString(TEXT("The host left")));
		TestTrue(TEXT("this machine takes over"), Taking.HostLine.ToString().StartsWith(TEXT("You")));
		TestFalse(TEXT("the new host can't stop: the others wait on it"), Taking.bCanStop);

		const FHostLostCard First = WoRHostHandOverScreens::BuildCard(EHostHandOverStep::Rejoining, true, false, 0);
		const FHostLostCard Later = WoRHostHandOverScreens::BuildCard(EHostHandOverStep::Rejoining, true, false, HANDOVER_TEST_SECOND_SEARCH);
		TestEqual(TEXT("a planned hand-over's title"), First.Title.ToString(), FString(TEXT("Handing over host")));
		TestTrue(TEXT("a guest can stop waiting"), First.bCanStop);
		TestTrue(TEXT("more pips once searching"), Later.LitPips > First.LitPips && Later.LitPips < WoRHostHandOverScreens::CARD_PIPS);

		const FHostLostCard GaveUp = WoRHostHandOverScreens::BuildCard(EHostHandOverStep::GaveUp, false, false, 0);
		TestTrue(TEXT("gave up"), GaveUp.bGaveUp && GaveUp.bCanStop);
	});

	It("refuses the hand-over off a listen server or alone, and names the next host", [this]()
	{
		TestFalse(TEXT("a guest can't"), WoRHostHandOverScreens::BuildConfirm(NM_Client, HANDOVER_TEST_TWO_PLAYERS, FString()).bCanHandOver);
		const FHandOverConfirm Alone = WoRHostHandOverScreens::BuildConfirm(NM_ListenServer, 1, FString());
		TestFalse(TEXT("alone: no"), Alone.bCanHandOver);
		TestFalse(TEXT("with a reason"), Alone.Refusal.IsEmpty());
		const FHandOverConfirm Ready = WoRHostHandOverScreens::BuildConfirm(NM_ListenServer, HANDOVER_TEST_TWO_PLAYERS, TEXT("Kestrel"));
		TestTrue(TEXT("a host with a friend can"), Ready.bCanHandOver);
		TestEqual(TEXT("the next host by name"), Ready.NextHost.ToString(), FString(TEXT("Kestrel")));
	});

	It("signals when a hand-over starts and when the player stops waiting", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		UWoRHostHandOverSubsystem *HandOver = TestWorld.GetSubsystem<UWoRHostHandOverSubsystem>();
		if (!TestNotNull(TEXT("hand-over subsystem"), HandOver)) return;
		TArray<EHostHandOverStep> Seen;
		const FDelegateHandle Handle = HandOver->OnStepChanged.AddLambda([&Seen](EHostHandOverStep Step) { Seen.Add(Step); });
		// This machine (no net id here) is neither of these two, so it rejoins the second.
		HandOver->NoteHostOrder({TEXT("host"), TEXT("friend")});
		TestTrue(TEXT("the hand-over starts"), HandOver->BeginHandOver());
		TestTrue(TEXT("the signal: rejoining"), Seen.Num() == 1 && Seen[0] == EHostHandOverStep::Rejoining);
		TestTrue(TEXT("the card shows"), WoRHostHandOverScreens::BuildCard(TestWorld.GetWorld()).bShown);
		HandOver->StopWaiting();
		TestTrue(TEXT("the signal: none"), Seen.Num() == 2 && Seen[1] == EHostHandOverStep::None);
		TestFalse(TEXT("the card goes"), WoRHostHandOverScreens::BuildCard(TestWorld.GetWorld()).bShown);
		HandOver->OnStepChanged.Remove(Handle);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
