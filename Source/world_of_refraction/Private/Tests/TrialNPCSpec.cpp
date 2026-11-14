// TrialNPCSpec.cpp
// B-449 (D-186, Q-449): V, the trial NPC who extracts the party. The NPC's prompt and defaults; Extract is offered in a
// run wherever V stands (not only where a beaten boss left a rest spot, B-448) and refused outside one.
// The run's end itself (the half share, the travel home) is UTrialRunSubsystem::LeaveRunEarly's, checked in PIE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Party/Party.h"
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
	constexpr int32 TRIAL_NPC_TEST_COUNT = 3;
	constexpr int32 TRIAL_NPC_TEST_RUNS = 60;
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

	It("has one of the placed NPCs sell pillar points for the whole run; only that one sells (B-447, Q-447)", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		ATrialNPC *V = World->SpawnActor<ATrialNPC>();
		ATrialNPC *Second = World->SpawnActor<ATrialNPC>();
		ATrialNPC *Third = World->SpawnActor<ATrialNPC>();
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestTrue(TEXT("setup"), V && Second && Third && Run)) return;
		Second->SetIdentity(FText::FromString(TEXT("Second")), /*bInExtractsParty=*/false);
		Third->SetIdentity(FText::FromString(TEXT("Third")), /*bInExtractsParty=*/false);
		TestNull(TEXT("no run: nobody sells"), WoRTrialNPC::FindPointSeller(World));

		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), TRIAL_NPC_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		const ATrialNPC *Seller = WoRTrialNPC::FindPointSeller(World);
		TestTrue(TEXT("one of the three"), Seller == V || Seller == Second || Seller == Third);
		TestTrue(TEXT("the same one when asked again"), WoRTrialNPC::FindPointSeller(World) == Seller);
		const ATrialNPC *Other = Seller == V ? Second : V;
		TestTrue(TEXT("another NPC has none to sell"),
				 WoRTrialNPC::TryBuyPointAt(World, *Other, FPartyMember()).ToString().Contains(TEXT("no points to sell")));
		TestTrue(TEXT("the debug string names the seller"), WoRTrialNPC::GetTrialNPCsString(World).Contains(TEXT("sells points this run")));
	});

	It("picks the seller from the run's seed, inside the NPCs placed; a lone NPC always sells", [this]()
	{
		TSet<int32> Picked;
		for (int32 Seed = 0; Seed < TRIAL_NPC_TEST_RUNS; ++Seed)
		{
			const int32 Index = WoRTrialNPC::PickSellerIndex(Seed, TRIAL_NPC_TEST_COUNT);
			TestTrue(TEXT("inside the placed NPCs"), Index >= 0 && Index < TRIAL_NPC_TEST_COUNT);
			TestEqual(TEXT("the same seed, the same pick"), WoRTrialNPC::PickSellerIndex(Seed, TRIAL_NPC_TEST_COUNT), Index);
			Picked.Add(Index);
		}
		TestEqual(TEXT("over many runs every NPC gets a turn"), Picked.Num(), TRIAL_NPC_TEST_COUNT);
		TestEqual(TEXT("one NPC: it sells"), WoRTrialNPC::PickSellerIndex(TRIAL_NPC_TEST_SEED, 1), 0);
		TestEqual(TEXT("none placed: no index past the end"), WoRTrialNPC::PickSellerIndex(TRIAL_NPC_TEST_SEED, 0), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
