// RestServiceSpec.cpp
// B-502 (D-214, Q-502): rest points are the services. A placed rest spot's service names it in the prompt; one of
// the placed shrines sells pillar points for the whole run and only there can a point be bought; V's rest point
// extracts where a plain one can't; a mini-boss's rest spot has no service. The menus themselves (the Blacksmith
// button, Buy on the Level up screen) are checked in PIE (docs/PIE/B-502.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Merchant/MerchantData.h"
#include "Party/Party.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"

namespace
{
	constexpr int32 REST_SERVICE_TEST_SEED = 502;
	constexpr int32 REST_SERVICE_TEST_SHRINES = 3;
	constexpr int32 REST_SERVICE_TEST_RUNS = 60;
	const FName REST_SERVICE_TEST_MINI_BOSS(TEXT("TestMiniBoss"));

	ATrialRestSpot *SpawnRestSpot(UWorld *World, ERestService Service)
	{
		ATrialRestSpot *Spot = World->SpawnActor<ATrialRestSpot>();
		if (Spot)
		{
			Spot->SetService(Service);
		}
		return Spot;
	}
}

BEGIN_DEFINE_SPEC(FRestServiceSpec, "WoR.Trial.RestService", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;

	void StartRun()
	{
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REST_SERVICE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
	}

	/** What a host's press does to the run (WoRRestSpot::Rest), without a party to heal. */
	void RestAt(const ATrialRestSpot *Spot)
	{
		Run->NoteRestSpotUsed(Spot->GetRestSpotId());
		Run->SetAtRestSpot(true);
	}
END_DEFINE_SPEC(FRestServiceSpec)

void FRestServiceSpec::Define()
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

	It("names the service in the prompt; a plain rest spot only says Rest", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		const ATrialRestSpot *Plain = SpawnRestSpot(World, ERestService::None);
		const ATrialRestSpot *Anvil = SpawnRestSpot(World, ERestService::Blacksmith);
		if (!TestTrue(TEXT("spawned"), Plain && Anvil)) return;
		TestTrue(TEXT("rest only by default"), Plain->GetService() == ERestService::None);
		TestTrue(TEXT("plain: Rest"), WoRHubHUD::FormatPromptLine(Plain->GetPrompt()).ToString().EndsWith(TEXT("Rest")));
		TestTrue(TEXT("anvil: Rest · Anvil"), WoRHubHUD::FormatPromptLine(Anvil->GetPrompt()).ToString().EndsWith(TEXT("Rest · Anvil")));
		TestTrue(TEXT("listed by the debug string"), WoRRestSpot::GetRestSpotsString(World).Contains(TEXT("Blacksmith")));

		const ATrialRestSpot *Tree = SpawnRestSpot(World, ERestService::Spiritualist);
		if (!TestNotNull(TEXT("tree"), Tree)) return;
		TestTrue(TEXT("tree: Rest · Ritual tree"), WoRHubHUD::FormatPromptLine(Tree->GetPrompt()).ToString().EndsWith(TEXT("Rest · Ritual tree")));
	});

	It("makes an anvil the Blacksmith and a ritual tree the Spiritualist; a shrine and V have no shop", [this]()
	{
		EMerchantType Vendor = EMerchantType::Jeweler;
		TestTrue(TEXT("anvil"), WoRRestSpot::GetServiceVendor(ERestService::Blacksmith, Vendor) && Vendor == EMerchantType::Blacksmith);
		TestTrue(TEXT("tree"), WoRRestSpot::GetServiceVendor(ERestService::Spiritualist, Vendor) && Vendor == EMerchantType::Spiritualist);
		for (const ERestService NoShop : {ERestService::None, ERestService::PillarShrine, ERestService::Extract})
		{
			TestFalse(TEXT("no shop"), WoRRestSpot::GetServiceVendor(NoShop, Vendor));
		}
	});

	It("gives a mini-boss's rest spot no service, whatever is set", [this]()
	{
		ATrialRestSpot *BossSpot = ATrialRestSpot::SpawnForMiniBoss(TestWorld->GetWorld(), FTransform::Identity, REST_SERVICE_TEST_MINI_BOSS);
		if (!TestNotNull(TEXT("spawned"), BossSpot)) return;
		BossSpot->SetService(ERestService::Blacksmith);
		TestTrue(TEXT("unchanged: rest only"), BossSpot->GetService() == ERestService::None);
	});

	It("has one of the placed shrines sell pillar points for the whole run, and only there", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		const ATrialRestSpot *First = SpawnRestSpot(World, ERestService::PillarShrine);
		const ATrialRestSpot *Second = SpawnRestSpot(World, ERestService::PillarShrine);
		const ATrialRestSpot *Third = SpawnRestSpot(World, ERestService::PillarShrine);
		const ATrialRestSpot *Anvil = SpawnRestSpot(World, ERestService::Blacksmith);
		if (!TestTrue(TEXT("setup"), First && Second && Third && Anvil && Run)) return;
		TestNull(TEXT("no run: no shrine sells"), WoRRestSpot::FindAwakeShrine(World));

		StartRun();
		const ATrialRestSpot *Awake = WoRRestSpot::FindAwakeShrine(World);
		TestTrue(TEXT("one of the three shrines, never the anvil"), Awake == First || Awake == Second || Awake == Third);
		TestTrue(TEXT("the same one when asked again"), WoRRestSpot::FindAwakeShrine(World) == Awake);
		if (!Awake) return;

		TestFalse(TEXT("not rested anywhere yet: no points here"), WoRRestSpot::SellsPointsHere(World));
		const ATrialRestSpot *Quiet = Awake == First ? Second : First;
		RestAt(Quiet);
		TestFalse(TEXT("a quiet shrine sells none"), WoRRestSpot::SellsPointsHere(World));
		TestTrue(TEXT("and refuses the buy"), WoRRestSpot::TryBuyPointHere(World, FPartyMember()).ToString().Contains(TEXT("No pillar points")));
		RestAt(Anvil);
		TestFalse(TEXT("the anvil sells none"), WoRRestSpot::SellsPointsHere(World));
		RestAt(Awake);
		TestTrue(TEXT("this run's shrine sells them"), WoRRestSpot::SellsPointsHere(World));
		TestTrue(TEXT("the debug string names it"), WoRRestSpot::GetRestSpotsString(World).Contains(TEXT("sells points this run")));

		// Review: the last rest spot stays the shrine after the party walks on; the points don't.
		WoRRestSpot::StepAway(World);
		TestTrue(TEXT("still the last rest spot"), WoRRestSpot::FindLastRestSpot(World) == Awake);
		TestFalse(TEXT("stepped away: no points"), WoRRestSpot::SellsPointsHere(World));
		TestTrue(TEXT("and the buy is refused"), WoRRestSpot::TryBuyPointHere(World, FPartyMember()).ToString().Contains(TEXT("No pillar points")));
	});

	It("picks the shrine from the run's seed, inside the shrines placed; a lone shrine always sells", [this]()
	{
		TSet<int32> Picked;
		for (int32 Seed = 0; Seed < REST_SERVICE_TEST_RUNS; ++Seed)
		{
			const int32 Index = WoRRestSpot::PickShrineIndex(Seed, REST_SERVICE_TEST_SHRINES);
			TestTrue(TEXT("inside the placed shrines"), Index >= 0 && Index < REST_SERVICE_TEST_SHRINES);
			TestEqual(TEXT("the same seed, the same pick"), WoRRestSpot::PickShrineIndex(Seed, REST_SERVICE_TEST_SHRINES), Index);
			Picked.Add(Index);
		}
		TestEqual(TEXT("over many runs every shrine gets a turn"), Picked.Num(), REST_SERVICE_TEST_SHRINES);
		TestEqual(TEXT("one shrine: it sells"), WoRRestSpot::PickShrineIndex(REST_SERVICE_TEST_SEED, 1), 0);
		TestEqual(TEXT("none placed: no index past the end"), WoRRestSpot::PickShrineIndex(REST_SERVICE_TEST_SEED, 0), 0);
	});

	It("extracts at V's rest point, where a plain rest spot in the wild can't", [this]()
	{
		UWorld *World = TestWorld->GetWorld();
		const ATrialRestSpot *Plain = SpawnRestSpot(World, ERestService::None);
		const ATrialRestSpot *V = SpawnRestSpot(World, ERestService::Extract);
		if (!TestTrue(TEXT("setup"), Plain && V && Run)) return;
		TestFalse(TEXT("no run: no Extract"), WoRRestSpot::CanExtractHere(World));

		StartRun();
		RestAt(Plain);
		TestFalse(TEXT("a plain rest spot: no Extract (B-448)"), WoRRestSpot::CanExtractHere(World));
		RestAt(V);
		TestTrue(TEXT("V's rest point: Extract"), WoRRestSpot::CanExtractHere(World));
		WoRRestSpot::StepAway(World);
		TestFalse(TEXT("stepped away from V: no Extract"), WoRRestSpot::CanExtractHere(World));
		TestTrue(TEXT("the prompt names V"), WoRHubHUD::FormatPromptLine(V->GetPrompt()).ToString().EndsWith(TEXT("Rest · V")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
