// RunEndLoadoutSpec.cpp
// B-508 (D-219, D-221 stage 1b): what each ending does to the worn-gear record. A quit, or a game closed mid-run, puts
// back the record the run began with, wheel pick included; an extract, a boss win and a run out of wipes keep it as
// the run left it. The run-start copy is saved with the run.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RUN_END_LOADOUT_TEST_SEED = 508;
	constexpr int32 RUN_END_LOADOUT_PICKED_INDEX = 2;
	const FName RUN_END_LOADOUT_KEY(TEXT("RunEndLoadoutTester"));

	/** A record wearing one weapon, ACTIVE. */
	FWornGear MakeRunEndWorn(const FGuid &Weapon)
	{
		FWornGear Worn;
		Worn.Equipment.AddDefaulted_GetRef().GearID = Weapon;
		Worn.ActiveGearID = Weapon;
		return Worn;
	}
}

BEGIN_DEFINE_SPEC(FRunEndLoadoutSpec, "WoR.Run.EndLoadout", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	FWornGear HubWorn;
	FWornGear RunWorn;
	/** Opens a run (unless one is open) with the hub record kept, then changes the gear and the wheel pick as a run
	 *  would. False without a pool. */
	bool StartRunAndChangeGear();
END_DEFINE_SPEC(FRunEndLoadoutSpec)

bool FRunEndLoadoutSpec::StartRunAndChangeGear()
{
	if (!Pool)
	{
		return false;
	}
	HubWorn = MakeRunEndWorn(FGuid::NewGuid());
	RunWorn = MakeRunEndWorn(FGuid::NewGuid());
	Pool->SetWornGear(RUN_END_LOADOUT_KEY, HubWorn);
	if (!Pool->IsRunLayerActive())
	{
		Pool->BeginRunLayer();
	}
	Pool->SnapshotRunStartGear({RUN_END_LOADOUT_KEY});
	Pool->SetWornGear(RUN_END_LOADOUT_KEY, RunWorn);
	FWheelPick Pick;
	Pick.FieldAttackIndex = RUN_END_LOADOUT_PICKED_INDEX;
	Pick.bPicked = true;
	Pool->SetWheelPick(RUN_END_LOADOUT_KEY, Pick);
	RunWorn.WheelPick = Pick;
	return true;
}

void FRunEndLoadoutSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		TestWorld.Reset();
	});

	It("puts the run-start record back on a quit, the wheel pick included (D-219)", [this]()
	{
		if (!TestTrue(TEXT("run started"), StartRunAndChangeGear())) return;
		TestEqual(TEXT("one record kept at the start"), Pool->GetRunStartWornGear().Num(), 1);
		Pool->CloseRun(EPoolRunEnding::Quit);
		const FWornGear *After = Pool->GetWornGear(RUN_END_LOADOUT_KEY);
		if (!TestNotNull(TEXT("record"), After)) return;
		TestTrue(TEXT("the hub gear is back"), After->SameAs(HubWorn));
		TestFalse(TEXT("the run's pick is gone"), After->WheelPick.bPicked);
		TestFalse(TEXT("the layer is closed"), Pool->IsRunLayerActive());
		TestEqual(TEXT("the copy is spent"), Pool->GetRunStartWornGear().Num(), 0);
	});

	It("keeps the record as the run left it on an extract or a boss win", [this]()
	{
		if (!TestTrue(TEXT("run started"), StartRunAndChangeGear())) return;
		Pool->CloseRun(EPoolRunEnding::Kept);
		const FWornGear *After = Pool->GetWornGear(RUN_END_LOADOUT_KEY);
		TestTrue(TEXT("the run's gear and pick stay"), After && After->SameAs(RunWorn));
	});

	It("keeps the record as the run left it when the party runs out of wipes", [this]()
	{
		if (!TestTrue(TEXT("run started"), StartRunAndChangeGear())) return;
		Pool->CloseRun(EPoolRunEnding::Lost);
		const FWornGear *After = Pool->GetWornGear(RUN_END_LOADOUT_KEY);
		TestTrue(TEXT("the run's gear and pick stay"), After && After->SameAs(RunWorn));
	});

	It("settles a game closed mid-run as a quit: the run-start record comes back", [this]()
	{
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Run"), Run)) return;
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_END_LOADOUT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		if (!TestTrue(TEXT("run started"), StartRunAndChangeGear())) return;
		Run->SettleRunAsQuit(/*bTravelIfAway=*/false);
		const FWornGear *After = Pool->GetWornGear(RUN_END_LOADOUT_KEY);
		TestTrue(TEXT("the hub gear is back"), After && After->SameAs(HubWorn));
	});

	It("saves the run-start record with the run and brings it back on a resume", [this]()
	{
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Run"), Run)) return;
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_END_LOADOUT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		if (!TestTrue(TEXT("run started"), StartRunAndChangeGear())) return;
		const FSavedRun Saved = USaveSubsystem::CaptureRun(*Run, 0);
		TestEqual(TEXT("saved with the run"), Saved.RunStartWornGear.Num(), 1);
		USaveSubsystem::RestoreSavedRunLayer(Saved, *Pool);
		const FWornGear *Restored = Pool->GetRunStartWornGear().Find(RUN_END_LOADOUT_KEY);
		TestTrue(TEXT("the same hub record"), Restored && Restored->SameAs(HubWorn));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
