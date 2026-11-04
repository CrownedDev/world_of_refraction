// DefeatSpec.cpp
// B-362 (D-155, Q-155): losing a fight. A wipe takes up to 1 run pillar point from each member for a normal enemy,
// 2 for a mini-boss, 0 for the main boss, from the pillar with the most run points (a tie goes Mind, Body, Spirit);
// the run ends after 3 main-boss losses or 5 wipes in all; the counters are saved with the run.
// B-362b (D-160): a wipe takes no points; the run's Prisms drop at the fight, beating that team recovers them, a
// second wipe first loses the earlier drop for good, a boss loss drops too, and the drop is saved with the run.
// The rank-point cases below pin the OBSOLETE rules until they are deleted after PIE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 DEFEAT_TEST_SEED = 11;
	constexpr int32 DEFEAT_RICH_MEMBER_MIND = 3;
	constexpr int32 DEFEAT_POOR_MEMBER_BODY = 1;
	constexpr int32 DEFEAT_BELOW_LIMIT_WIPES = DefeatRules::MAX_WIPES_PER_RUN - 1;
	constexpr int32 DEFEAT_BELOW_LIMIT_BOSS_LOSSES = DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN - 1;
	const FName DEFEAT_TEST_REST_SPOT(TEXT("RestSpotA"));
	const FName DEFEAT_RICH_KEY(TEXT("DefeatRich"));
	const FName DEFEAT_POOR_KEY(TEXT("DefeatPoor"));
	constexpr int32 DEFEAT_RUN_PRISMS = 40;
	constexpr int32 DEFEAT_LATER_PRISMS = 15;
	const FName DEFEAT_TEAM_A(TEXT("DefeatTeamA"));
	const FName DEFEAT_TEAM_B(TEXT("DefeatTeamB"));
	const FName DEFEAT_BOSS_TEAM(TEXT("DefeatBossTeam"));
}

BEGIN_DEFINE_SPEC(FDefeatSpec, "WoR.Trial.Defeat", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;

	/** A wallet holding RunPrisms of the run currency (ECurrencyType::Gold, shown as Prisms). */
	UCurrencyComponent *MakeWallet(int32 RunPrisms)
	{
		AActor *Holder = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		if (!Holder)
		{
			return nullptr;
		}
		UCurrencyComponent *Wallet = NewObject<UCurrencyComponent>(Holder);
		Holder->AddInstanceComponent(Wallet);
		Wallet->RegisterComponent();
		Wallet->Add(ECurrencyType::Gold, RunPrisms);
		return Wallet;
	}
END_DEFINE_SPEC(FDefeatSpec)

void FDefeatSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), DEFEAT_TEST_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
			Run->AddRunEarnedLevels(DEFEAT_RICH_KEY, EWorldPillar::Mind, DEFEAT_RICH_MEMBER_MIND);
			Run->AddRunEarnedLevels(DEFEAT_POOR_KEY, EWorldPillar::Body, DEFEAT_POOR_MEMBER_BODY);
		}
		if (UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>())
		{
			PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>());
		}
	});

	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("sets the points a wipe costs by the enemy's rank: 1 normal, 2 mini-boss, 0 main boss", [this]()
	{
		// The literals pin D-155's ruling: changing a DefeatRules constant must fail here until Crown rules it.
		TestEqual(TEXT("normal"), DefeatRules::GetPointsLost(EEncounterRank::Normal), 1);
		TestEqual(TEXT("elite counts as normal"), DefeatRules::GetPointsLost(EEncounterRank::Elite), 1);
		TestEqual(TEXT("mini-boss"), DefeatRules::GetPointsLost(EEncounterRank::MiniBoss), 2);
		TestEqual(TEXT("main boss"), DefeatRules::GetPointsLost(EEncounterRank::Boss), 0);
	});

	It("ends the run at the 5th wipe or the 3rd main-boss loss, not before", [this]()
	{
		TestFalse(TEXT("4 wipes"), DefeatRules::RunEnds(DEFEAT_BELOW_LIMIT_WIPES, 0));
		TestTrue(TEXT("5 wipes"), DefeatRules::RunEnds(DefeatRules::MAX_WIPES_PER_RUN, 0));
		TestFalse(TEXT("2 boss losses"), DefeatRules::RunEnds(DEFEAT_BELOW_LIMIT_BOSS_LOSSES, DEFEAT_BELOW_LIMIT_BOSS_LOSSES));
		TestTrue(TEXT("3 boss losses"), DefeatRules::RunEnds(DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN, DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN));
	});

	It("takes the point from the pillar with the most run points, a tie going Mind, Body, Spirit", [this]()
	{
		EWorldPillar Pillar = EWorldPillar::Spirit;
		TestFalse(TEXT("no points, none taken"), DefeatRules::PickPillarToLose(0, 0, 0, Pillar));
		TestTrue(TEXT("Body is most"), DefeatRules::PickPillarToLose(1, 3, 2, Pillar) && Pillar == EWorldPillar::Body);
		TestTrue(TEXT("Spirit is most"), DefeatRules::PickPillarToLose(1, 1, 2, Pillar) && Pillar == EWorldPillar::Spirit);
		TestTrue(TEXT("a three-way tie: Mind"), DefeatRules::PickPillarToLose(2, 2, 2, Pillar) && Pillar == EWorldPillar::Mind);
		TestTrue(TEXT("Body and Spirit tie: Body"), DefeatRules::PickPillarToLose(0, 2, 2, Pillar) && Pillar == EWorldPillar::Body);
	});

	It("a wipe at any rank leaves pillar points alone and counts the wipe (D-160)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		for (const EEncounterRank Rank : {EEncounterRank::Normal, EEncounterRank::MiniBoss, EEncounterRank::Boss})
		{
			const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(TestWorld->GetWorld(), Rank, {DEFEAT_RICH_KEY, DEFEAT_POOR_KEY});
			TestEqual(TEXT("no points taken"), Result.PointsTaken, 0);
		}
		TestEqual(TEXT("the rich member kept all"), Run->GetRunEarnedLevels(DEFEAT_RICH_KEY, EWorldPillar::Mind), DEFEAT_RICH_MEMBER_MIND);
		TestEqual(TEXT("the poor member kept all"), Run->GetRunEarnedLevels(DEFEAT_POOR_KEY, EWorldPillar::Body), DEFEAT_POOR_MEMBER_BODY);
		TestEqual(TEXT("three wipes"), Run->GetRunWipes(), 3);
		TestEqual(TEXT("one boss loss"), Run->GetRunMainBossLosses(), 1);
	});

	It("drops the whole run balance at the fight and gives it back for beating that team (B-362b)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		UCurrencyComponent *Wallet = MakeWallet(DEFEAT_RUN_PRISMS);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Run->SetPendingEncounterSources({DEFEAT_TEAM_A});
		int32 LostForGood = INDEX_NONE;
		TestEqual(TEXT("all of it dropped"), WoRDefeat::DropPrisms(*Run, Wallet, LostForGood), DEFEAT_RUN_PRISMS);
		TestEqual(TEXT("nothing earlier lost"), LostForGood, 0);
		TestEqual(TEXT("the wallet is empty"), Wallet->GetBalance(ECurrencyType::Gold), 0);
		TestEqual(TEXT("the run holds the drop"), Run->GetDroppedPrisms(), DEFEAT_RUN_PRISMS);

		Run->SetPendingEncounterSources({DEFEAT_TEAM_B});
		TestEqual(TEXT("beating another team recovers nothing"), WoRDefeat::RecoverPrisms(*Run, Wallet), 0);
		TestEqual(TEXT("the drop waits"), Run->GetDroppedPrisms(), DEFEAT_RUN_PRISMS);

		Run->SetPendingEncounterSources({DEFEAT_TEAM_B, DEFEAT_TEAM_A});
		TestEqual(TEXT("beating that team recovers all of it"), WoRDefeat::RecoverPrisms(*Run, Wallet), DEFEAT_RUN_PRISMS);
		TestEqual(TEXT("back in the wallet"), Wallet->GetBalance(ECurrencyType::Gold), DEFEAT_RUN_PRISMS);
		TestEqual(TEXT("the drop is gone"), Run->GetDroppedPrisms(), 0);
		TestEqual(TEXT("only once"), WoRDefeat::RecoverPrisms(*Run, Wallet), 0);
	});

	It("loses the first drop for good on a second wipe, and a boss loss drops the same way (B-362b)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		UCurrencyComponent *Wallet = MakeWallet(DEFEAT_RUN_PRISMS);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Run->SetPendingEncounterSources({DEFEAT_TEAM_A});
		int32 LostForGood = 0;
		WoRDefeat::DropPrisms(*Run, Wallet, LostForGood);
		Wallet->Add(ECurrencyType::Gold, DEFEAT_LATER_PRISMS);
		Run->SetPendingEncounterSources({DEFEAT_BOSS_TEAM});
		Run->RecordWipe(EEncounterRank::Boss);
		TestEqual(TEXT("the boss fight takes the new balance"), WoRDefeat::DropPrisms(*Run, Wallet, LostForGood), DEFEAT_LATER_PRISMS);
		TestEqual(TEXT("the first drop is lost for good"), LostForGood, DEFEAT_RUN_PRISMS);

		Run->SetPendingEncounterSources({DEFEAT_TEAM_A});
		TestEqual(TEXT("the first team gives nothing back now"), WoRDefeat::RecoverPrisms(*Run, Wallet), 0);
		Run->SetPendingEncounterSources({DEFEAT_BOSS_TEAM});
		TestEqual(TEXT("beating the boss recovers the newest drop"), WoRDefeat::RecoverPrisms(*Run, Wallet), DEFEAT_LATER_PRISMS);
	});

	It("ends the run on the 5th wipe", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		for (int32 Wipe = 1; Wipe < DefeatRules::MAX_WIPES_PER_RUN; ++Wipe)
		{
			TestFalse(TEXT("the run goes on"), WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Normal, {}).bRunEnds);
		}
		TestTrue(TEXT("the 5th wipe ends it"), WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Normal, {}).bRunEnds);
	});

	It("ends the run on the 3rd main-boss loss", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		for (int32 Loss = 1; Loss < DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN; ++Loss)
		{
			TestFalse(TEXT("the run goes on"), WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Boss, {}).bRunEnds);
		}
		TestTrue(TEXT("the 3rd loss ends it"), WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Boss, {}).bRunEnds);
	});

	It("costs nothing outside a run", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->RestoreRun(nullptr, 0, 0, ETrialDifficulty::Medium, {}, {}, {});
		const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::MiniBoss, {DEFEAT_RICH_KEY});
		TestEqual(TEXT("nothing taken"), Result.PointsTaken, 0);
		TestEqual(TEXT("not counted"), Run->GetRunWipes(), 0);
	});

	It("keeps the counters, the last rest spot and the Prism drop through a save", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Boss, {});
		WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Normal, {});
		Run->NoteRestSpotUsed(DEFEAT_TEST_REST_SPOT);
		Run->SetPendingEncounterSources({DEFEAT_TEAM_A});
		Run->DropPrisms(DEFEAT_RUN_PRISMS);
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Run = USaveSubsystem::CaptureRun(*Run, 0);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("saved to memory"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return;
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;
		TestEqual(TEXT("wipes saved"), Loaded->Data.Run.Wipes, 2);
		TestEqual(TEXT("boss losses saved"), Loaded->Data.Run.MainBossLosses, 1);
		TestTrue(TEXT("rest spot saved"), Loaded->Data.Run.LastRestSpot == DEFEAT_TEST_REST_SPOT);
		TestEqual(TEXT("drop saved"), Loaded->Data.Run.DroppedPrisms, DEFEAT_RUN_PRISMS);

		FWoRTestWorld After;
		if (!TestTrue(TEXT("second world"), After.Create())) return;
		UTrialRunSubsystem *RunAfter = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("RunAfter"), RunAfter)) return;
		RunAfter->RestoreRunLosses(Loaded->Data.Run.Wipes, Loaded->Data.Run.MainBossLosses, Loaded->Data.Run.LastRestSpot);
		TestEqual(TEXT("wipes back"), RunAfter->GetRunWipes(), 2);
		TestEqual(TEXT("boss losses back"), RunAfter->GetRunMainBossLosses(), 1);
		TestTrue(TEXT("rest spot back"), RunAfter->GetLastRestSpot() == DEFEAT_TEST_REST_SPOT);
		RunAfter->RestoreDrop(Loaded->Data.Run.DroppedPrisms, Loaded->Data.Run.DropEncounters);
		TestEqual(TEXT("drop back"), RunAfter->GetDroppedPrisms(), DEFEAT_RUN_PRISMS);
		TestTrue(TEXT("its team back"), RunAfter->GetDropEncounters().Contains(DEFEAT_TEAM_A));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
