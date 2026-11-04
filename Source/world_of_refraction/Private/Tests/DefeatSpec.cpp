// DefeatSpec.cpp
// B-362 (D-155, Q-155): losing a fight. A wipe takes up to 1 run pillar point from each member for a normal enemy,
// 2 for a mini-boss, 0 for the main boss, from the pillar with the most run points (a tie goes Mind, Body, Spirit);
// the run ends after 3 main-boss losses or 5 wipes in all; the counters are saved with the run.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
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
}

BEGIN_DEFINE_SPEC(FDefeatSpec, "WoR.Trial.Defeat", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
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

	It("a mini-boss wipe takes 2 from a member with 3 and 1 from a member with 1", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::MiniBoss, {DEFEAT_RICH_KEY, DEFEAT_POOR_KEY});
		TestEqual(TEXT("3 points in all"), Result.PointsTaken, 2 + DEFEAT_POOR_MEMBER_BODY);
		TestEqual(TEXT("the rich member kept 1"), Run->GetRunEarnedLevels(DEFEAT_RICH_KEY, EWorldPillar::Mind), DEFEAT_RICH_MEMBER_MIND - 2);
		TestEqual(TEXT("the poor member has none"), Run->GetRunEarnedLevels(DEFEAT_POOR_KEY, EWorldPillar::Body), 0);
		TestEqual(TEXT("one wipe"), Run->GetRunWipes(), 1);
		TestEqual(TEXT("no boss loss"), Run->GetRunMainBossLosses(), 0);
		TestFalse(TEXT("the run goes on"), Result.bRunEnds);
	});

	It("a normal wipe takes 1 and a member with no run points loses nothing", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Normal, {DEFEAT_RICH_KEY, FName(TEXT("DefeatNone"))});
		TestEqual(TEXT("1 point in all"), Result.PointsTaken, 1);
		TestEqual(TEXT("the rich member kept 2"), Run->GetRunEarnedLevels(DEFEAT_RICH_KEY, EWorldPillar::Mind), DEFEAT_RICH_MEMBER_MIND - 1);
	});

	It("a main-boss wipe takes nothing but counts as a wipe and a boss loss", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		const WoRDefeat::FWipeResult Result = WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Boss, {DEFEAT_RICH_KEY, DEFEAT_POOR_KEY});
		TestEqual(TEXT("nothing taken"), Result.PointsTaken, 0);
		TestEqual(TEXT("the rich member kept all"), Run->GetRunEarnedLevels(DEFEAT_RICH_KEY, EWorldPillar::Mind), DEFEAT_RICH_MEMBER_MIND);
		TestEqual(TEXT("one wipe"), Run->GetRunWipes(), 1);
		TestEqual(TEXT("one boss loss"), Run->GetRunMainBossLosses(), 1);
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

	It("keeps the counters and the last rest spot through a save", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Boss, {});
		WoRDefeat::ApplyWipe(TestWorld->GetWorld(), EEncounterRank::Normal, {});
		Run->NoteRestSpotUsed(DEFEAT_TEST_REST_SPOT);
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

		FWoRTestWorld After;
		if (!TestTrue(TEXT("second world"), After.Create())) return;
		UTrialRunSubsystem *RunAfter = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("RunAfter"), RunAfter)) return;
		RunAfter->RestoreRunLosses(Loaded->Data.Run.Wipes, Loaded->Data.Run.MainBossLosses, Loaded->Data.Run.LastRestSpot);
		TestEqual(TEXT("wipes back"), RunAfter->GetRunWipes(), 2);
		TestEqual(TEXT("boss losses back"), RunAfter->GetRunMainBossLosses(), 1);
		TestTrue(TEXT("rest spot back"), RunAfter->GetLastRestSpot() == DEFEAT_TEST_REST_SPOT);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
