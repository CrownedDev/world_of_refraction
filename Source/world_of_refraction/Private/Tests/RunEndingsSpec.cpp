// RunEndingsSpec.cpp
// B-451 (D-189, Q-451 reading A): what each ending pays. Running out of wipes pays a quarter of the run's currencies
// with no boss bonus and keeps no upgrades (the run's gear upgrade levels are not remembered); an unfinished run still
// in play pays the full tally; quitting pays nothing (D-176, B-439).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/DefeatRules.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RUN_ENDINGS_TEST_SEED = 451;
	constexpr EItemTier RUN_ENDINGS_UPGRADED_TIER = EItemTier::B_Tier;
}

BEGIN_DEFINE_SPEC(FRunEndingsSpec, "WoR.Trial.RunEndings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
END_DEFINE_SPEC(FRunEndingsSpec)

void FRunEndingsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_ENDINGS_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
			Run->RecordFightReward(TestWorld->GetWorld(), EEncounterRank::MiniBoss);
		}
	});

	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("pays a quarter, with no boss bonus, once the party runs out of wipes (D-189)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		TestFalse(TEXT("not wiped out yet"), Run->IsRunWipedOut());
		const FRunPayout Full = Run->GetRunEndPayout();
		for (int32 Wipe = 0; Wipe < DefeatRules::MAX_WIPES_PER_RUN; ++Wipe)
		{
			Run->RecordWipe(EEncounterRank::Normal);
		}
		TestTrue(TEXT("out of wipes"), Run->IsRunWipedOut());
		const FRunPayout Expected = RunRewards::ScalePayout(Run->GetRunTally(), ETrialDifficulty::Medium, false,
															Run->GetRunPayoutMultiplier() * RunRewards::WIPED_OUT_SHARE);
		const FRunPayout WipedOut = Run->GetRunEndPayout();
		TestEqual(TEXT("a quarter of the Gold"), WipedOut.HubGold, Expected.HubGold);
		TestEqual(TEXT("a quarter of the Gear Essence"), WipedOut.GearEssence, Expected.GearEssence);
		TestTrue(TEXT("the run had a tally"), Full.HubGold > 0);
		TestTrue(TEXT("less than the full tally"), WipedOut.HubGold < Full.HubGold);
		TestTrue(TEXT("a lost run"), Run->IsRunLost());
	});

	It("remembers a found piece's upgrade level after a run that keeps its upgrades (the control)", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		UWeaponData *Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Pool->BeginRunLayer();
		FWeaponInventoryEntry Found = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		Found.PersistentID = FGuid::NewGuid();
		Pool->UpdateWeapon(Found);
		Pool->MarkRunOnly(Found.PersistentID);
		Found.Tier = RUN_ENDINGS_UPGRADED_TIER;
		Pool->UpdateWeapon(Found);
		TestEqual(TEXT("remembered"), Pool->EndRunLayer(/*bKeepUpgrades=*/true).Remembered, 1);
		EItemTier Remembered = EItemTier::F_Tier;
		TestTrue(TEXT("at the upgraded tier"), Pool->FindRememberedTier(Dagger, Remembered) && Remembered == RUN_ENDINGS_UPGRADED_TIER);
	});

	It("keeps no upgrade levels from a run out of wipes (Q-451 reading A)", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		UWeaponData *Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Pool->BeginRunLayer();
		FWeaponInventoryEntry Found = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		Found.PersistentID = FGuid::NewGuid();
		Pool->UpdateWeapon(Found);
		TestTrue(TEXT("found in the run"), Pool->MarkRunOnly(Found.PersistentID));
		Found.Tier = RUN_ENDINGS_UPGRADED_TIER; // upgraded during the run
		Pool->UpdateWeapon(Found);
		const FPoolRunEndResult Result = Pool->EndRunLayer(/*bKeepUpgrades=*/false);
		TestEqual(TEXT("the found piece still leaves"), Result.Dropped, 1);
		TestEqual(TEXT("nothing remembered"), Result.Remembered, 0);
		EItemTier Remembered = EItemTier::F_Tier;
		TestFalse(TEXT("its upgrade level is not kept"), Pool->FindRememberedTier(Dagger, Remembered));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
