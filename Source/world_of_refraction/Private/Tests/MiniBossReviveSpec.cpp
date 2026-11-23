// MiniBossReviveSpec.cpp
// B-187b part 1 (RunStructure §4, Q-187b): a beaten mini-boss's place is a rest spot whose menu can revive it, one
// tier stronger and with one tier better drops per revival, up to MiniBossRevival::MAX_REVIVALS; other beaten
// encounters can't be revived. B-187c: the marks and revivals survive a save; a revive brings the party back where it
// rested (the pending return stand, consumed once). B-589 (Q-450 = B): the revive row and vote lines name the opponent.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "Save/WoRSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Trial/ReviveVote.h"
#include "UI/Hub/RestSpotWidget.h"

namespace
{
	constexpr int32 REVIVE_TEST_SEED = 11;
	const FName REVIVE_TEST_MINI_BOSS(TEXT("MiniBossTeam"));
	const FName REVIVE_TEST_ELITE(TEXT("EliteTeam"));
	constexpr const TCHAR *REVIVE_TEST_OPPONENT = TEXT("Ash Warden");
	constexpr const TCHAR *REVIVE_TEST_STARTER = TEXT("Seras");
}

BEGIN_DEFINE_SPEC(FMiniBossReviveSpec, "WoR.Trial.MiniBossRevive", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
END_DEFINE_SPEC(FMiniBossReviveSpec)

void FMiniBossReviveSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REVIVE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		}
	});

	AfterEach([this]()
	{
		Run = nullptr;
		TestWorld.Reset();
	});

	It("a beaten mini-boss can be revived up to the cap; an elite can't", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->MarkEncounterBeaten(REVIVE_TEST_ELITE, EEncounterRank::Elite);
		TestTrue(TEXT("a mini-boss's rest spot"), Run->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
		TestFalse(TEXT("not an elite's"), Run->IsBeatenMiniBoss(REVIVE_TEST_ELITE));
		TestFalse(TEXT("an elite can't be revived"), Run->CanReviveEncounter(REVIVE_TEST_ELITE));
		for (int32 Revival = 1; Revival <= MiniBossRevival::MAX_REVIVALS; ++Revival)
		{
			TestTrue(FString::Printf(TEXT("revival %d"), Revival), Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS));
			TestFalse(TEXT("back on the map"), Run->IsEncounterBeaten(REVIVE_TEST_MINI_BOSS));
			TestFalse(TEXT("not beaten: nothing to revive"), Run->CanReviveEncounter(REVIVE_TEST_MINI_BOSS));
			Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss); // beaten again
			TestEqual(TEXT("counted"), Run->GetEncounterRevivals(REVIVE_TEST_MINI_BOSS), Revival);
		}
		TestFalse(TEXT("at the cap"), Run->CanReviveEncounter(REVIVE_TEST_MINI_BOSS));
		TestFalse(TEXT("refused"), Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS));
		TestTrue(TEXT("debug lists it"), Run->GetRevivalsString().Contains(REVIVE_TEST_MINI_BOSS.ToString()));
	});

	It("a won fight marks only the mini-boss teams it caught (review)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->SetPendingEncounterSources({REVIVE_TEST_MINI_BOSS, REVIVE_TEST_ELITE});
		Run->SetPendingMiniBossSources({REVIVE_TEST_MINI_BOSS});
		Run->MarkPendingEncounterBeaten();
		TestTrue(TEXT("both beaten"), Run->IsEncounterBeaten(REVIVE_TEST_MINI_BOSS) && Run->IsEncounterBeaten(REVIVE_TEST_ELITE));
		TestTrue(TEXT("the mini-boss leaves a rest spot"), Run->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
		TestFalse(TEXT("the elite caught with it doesn't"), Run->IsBeatenMiniBoss(REVIVE_TEST_ELITE));
	});

	It("the rest menu offers it only at that mini-boss's rest spot", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		WoRRestSpot::Rest(TestWorld->GetWorld(), TEXT("SomeOtherSpot"));
		TestFalse(TEXT("elsewhere: no revive"), WoRRestSpot::CanReviveHere(TestWorld->GetWorld()));
		WoRRestSpot::Rest(TestWorld->GetWorld(), REVIVE_TEST_MINI_BOSS);
		TestTrue(TEXT("at its spot"), WoRRestSpot::CanReviveHere(TestWorld->GetWorld()));
	});

	It("names the opponent, never its rank, on the revive row and every vote line (B-589, Q-450)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		UWorld *World = TestWorld->GetWorld();
		const FText Starter = FText::FromString(REVIVE_TEST_STARTER);
		const ATrialRestSpot *Spot = ATrialRestSpot::SpawnForMiniBoss(World, FTransform::Identity, REVIVE_TEST_MINI_BOSS,
																	  FText::FromString(REVIVE_TEST_OPPONENT));
		if (!TestNotNull(TEXT("its rest spot"), Spot)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		WoRRestSpot::Rest(World, REVIVE_TEST_MINI_BOSS);
		const FText Opponent = WoRRestSpot::GetReviveOpponentName(World);
		TestEqual(TEXT("the spot's opponent"), Opponent.ToString(), FString(REVIVE_TEST_OPPONENT));
		TestEqual(TEXT("the row"), WoRRestSpot::MakeReviveRowText(Opponent, 1).ToString(), FString(TEXT("Revive the Ash Warden (1 of 3)")));

		URestSpotWidget *Menu = CreateWidget<URestSpotWidget>(World, URestSpotWidget::StaticClass());
		if (!TestNotNull(TEXT("rest menu"), Menu)) return;
		const TSharedRef<SWidget> Built = Menu->TakeWidget(); // builds it (NativeConstruct, Refresh); held, or it destructs
		TestEqual(TEXT("the menu's row"), Menu->GetReviveRowText().ToString(), FString(TEXT("Revive the Ash Warden (1 of 3)")));

		using ReviveVote::ELine;
		for (const ELine Line : {ELine::AlreadyWaits, ELine::Ask, ELine::Failed, ELine::Waits, ELine::Passed, ELine::RefusedLate, ELine::Refused})
		{
			const FString Said = ReviveVote::MakeLine(Line, Opponent, Starter).ToString();
			TestTrue(FString::Printf(TEXT("names it: %s"), *Said), Said.Contains(TEXT("the Ash Warden"), ESearchCase::IgnoreCase));
			TestFalse(FString::Printf(TEXT("no rank: %s"), *Said), Said.Contains(TEXT("boss"), ESearchCase::IgnoreCase));
		}
		TestTrue(TEXT("the ask names who started it"), ReviveVote::MakeLine(ELine::Ask, Opponent, Starter).ToString().StartsWith(TEXT("Seras wants")));
	});

	It("says opponent for a rest spot with no name (B-589)", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		WoRRestSpot::Rest(TestWorld->GetWorld(), TEXT("SomeOtherSpot"));
		TestEqual(TEXT("no spot's name"), WoRRestSpot::GetReviveOpponentName(TestWorld->GetWorld()).ToString(), FString(TEXT("opponent")));
	});

	It("a revival steps the fight's tier, at most S", [this]()
	{
		TestEqual(TEXT("one step"), TrialGate::StepTier(EItemTier::C_Tier, MiniBossRevival::TIER_STEPS_PER_REVIVAL), EItemTier::B_Tier);
		TestEqual(TEXT("no steps"), TrialGate::StepTier(EItemTier::C_Tier, 0), EItemTier::C_Tier);
		TestEqual(TEXT("capped at S"), TrialGate::StepTier(EItemTier::A_Tier, MiniBossRevival::MAX_REVIVALS), EItemTier::S_Tier);
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->SetPendingTierSteps(2);
		TestEqual(TEXT("pending steps"), Run->GetPendingTierSteps(), 2);
		Run->SetPendingTierSteps(-1);
		TestEqual(TEXT("never negative"), Run->GetPendingTierSteps(), 0);
		Run->SetPendingEnemyTierSteps({0, 2, 1});
		TestEqual(TEXT("the most of the members' steps"), Run->GetPendingTierSteps(), 2);
		TestEqual(TEXT("no opposing party pending: none per member"), Run->GetPendingEnemyTierSteps(1), 0);
	});

	It("B-187c: the marks and revivals survive a save", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS);
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss); // beaten again after one revival
		// Review: through the save's bytes and back, as a real save is written.
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Run = USaveSubsystem::CaptureRun(*Run, 0);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("written"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return;
		const UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("read back"), Loaded)) return;
		const FSavedRun Saved = Loaded->Data.Run;
		TestEqual(TEXT("the revivals were written"), Saved.EncounterRevivals.FindRef(REVIVE_TEST_MINI_BOSS), 1);
		FWoRTestWorld After;
		if (!TestTrue(TEXT("second world"), After.Create())) return;
		UTrialRunSubsystem *Resumed = After.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("resumed run"), Resumed) || !TestTrue(TEXT("restored"), USaveSubsystem::RestoreRun(Saved, *Resumed))) return;
		TestTrue(TEXT("still a mini-boss rest spot"), Resumed->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
		TestEqual(TEXT("its revival count"), Resumed->GetEncounterRevivals(REVIVE_TEST_MINI_BOSS), 1);
		TestTrue(TEXT("can be revived again"), Resumed->CanReviveEncounter(REVIVE_TEST_MINI_BOSS));
	});

	It("B-187c: a revive's return stand is used once", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		FTransform Stand;
		TestFalse(TEXT("none before a rest"), Run->GetRestStand(Stand));
		const FTransform Rested(FVector(100.0f, 200.0f, 0.0f));
		Run->NoteRestStand(Rested);
		TestTrue(TEXT("rested here"), Run->GetRestStand(Stand) && Stand.GetLocation().Equals(Rested.GetLocation()));
		Run->SetPendingReturnStand(Rested);
		TestTrue(TEXT("the next load uses it"), Run->ConsumePendingReturnStand(Stand));
		TestFalse(TEXT("once"), Run->ConsumePendingReturnStand(Stand));
	});

	It("a new run forgets the mini-bosses and revivals", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run)) return;
		Run->MarkEncounterBeaten(REVIVE_TEST_MINI_BOSS, EEncounterRank::MiniBoss);
		Run->ReviveEncounter(REVIVE_TEST_MINI_BOSS);
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), REVIVE_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestEqual(TEXT("no revivals"), Run->GetEncounterRevivals(REVIVE_TEST_MINI_BOSS), 0);
		TestFalse(TEXT("no mini-boss spot"), Run->IsBeatenMiniBoss(REVIVE_TEST_MINI_BOSS));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
