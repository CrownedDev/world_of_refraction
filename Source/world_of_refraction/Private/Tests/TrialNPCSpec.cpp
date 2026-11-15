// TrialNPCSpec.cpp
// B-449 (D-186, Q-449): V, the trial NPC who extracts the party. The NPC's prompt and defaults; Extract is offered in a
// run wherever V stands (not only where a beaten boss left a rest spot, B-448) and refused outside one.
// The run's end itself (the half share, the travel home) is UTrialRunSubsystem::LeaveRunEarly's, checked in PIE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialNPC.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"

namespace
{
	constexpr int32 TRIAL_NPC_TEST_SEED = 449;
	const FName TRIAL_NPC_TEST_WILD_SPOT(TEXT("WildRestSpot"));
}

BEGIN_DEFINE_SPEC(FTrialNPCSpec, "WoR.Trial.NPC", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FTrialNPCSpec)

void FTrialNPCSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("is V by default: the prompt says Talk and the name, and V extracts", [this]()
	{
		const ATrialNPC *V = TestWorld->GetWorld()->SpawnActor<ATrialNPC>();
		if (!TestNotNull(TEXT("spawned"), V)) return;
		TestEqual(TEXT("named V"), V->GetDisplayName().ToString(), FString(TEXT("V")));
		TestTrue(TEXT("extracts the party"), V->ExtractsParty());
		const FInteractPrompt Prompt = V->GetPrompt();
		TestTrue(TEXT("Talk"), Prompt.Verb == EInteractVerb::Talk);
		TestTrue(TEXT("the line names V"), WoRHubHUD::FormatPromptLine(Prompt).ToString().EndsWith(TEXT("Talk · V")));
		TestTrue(TEXT("listed by the debug string"), WoRTrialNPC::GetTrialNPCsString(TestWorld->GetWorld()).Contains(TEXT("V (")));
	});

	It("refuses Extract outside a run, and nothing is left", [this]()
	{
		TestFalse(TEXT("a reason"), WoRTrialNPC::GetExtractRefusal(TestWorld->GetWorld()).IsEmpty());
		TestFalse(TEXT("refused"), WoRTrialNPC::Extract(TestWorld->GetWorld()));
	});

	It("offers Extract in a run wherever V stands, even where the rest spot's Extract is refused (Q-449)", [this]()
	{
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), TRIAL_NPC_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		Run->NoteRestSpotUsed(TRIAL_NPC_TEST_WILD_SPOT);
		TestFalse(TEXT("no Extract at a rest spot in the wild (B-448)"), WoRRestSpot::CanExtractHere(TestWorld->GetWorld()));
		TestTrue(TEXT("V extracts all the same"), WoRTrialNPC::GetExtractRefusal(TestWorld->GetWorld()).IsEmpty());
	});

	// B-502 (D-214): the pillar-point seller moved to the shrine rest points (RestServiceSpec.cpp, WoR.Trial.RestService).
}

#endif // WITH_DEV_AUTOMATION_TESTS
