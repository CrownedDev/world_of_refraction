// FieldEnemyTeamSpec.cpp
// B-184 (Enemies §2-3): enemy teams on the map. Several teams caught at once send their strongest
// member each, at most three; a win beats every caught team for the run; a beaten team is gone
// when the trial level loads again.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Combat/Encounter/FieldEnemyTeam.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	const FName FIELD_TEAM_TEST_BEATEN(TEXT("FieldTeamTest_Beaten"));
	const FName FIELD_TEAM_TEST_STANDING(TEXT("FieldTeamTest_Standing"));
}

BEGIN_DEFINE_SPEC(FFieldEnemyTeamSpec, "WoR.Trial.FieldEnemyTeam",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	/** An enemy whose stat pool grows with WorldLevel (GetTotalPool). */
	static UCharacterData *NewEnemy(int32 WorldLevel)
	{
		UCharacterData *Enemy = FWoRTestWorld::NewCharacterData();
		Enemy->WorldMindLevel = WorldLevel;
		return Enemy;
	}

	AFieldEnemyTeam *SpawnTeam(FName Name)
	{
		FActorSpawnParameters Params;
		Params.Name = Name;
		return TestWorld->GetWorld()->SpawnActor<AFieldEnemyTeam>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
END_DEFINE_SPEC(FFieldEnemyTeamSpec)

void FFieldEnemyTeamSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("sends one team whole, and the strongest of each when several are caught", [this]()
	{
		UCharacterData *Weak = NewEnemy(0);
		UCharacterData *Strong = NewEnemy(2);
		UCharacterData *Other = NewEnemy(1);
		UCharacterData *Fourth = NewEnemy(3);

		TestEqual(TEXT("One team fights whole"), UEncounterData::BuildCaughtRoster({{Weak, Strong, Other}}).Num(), 3);
		TestEqual(TEXT("An empty team doesn't count"), UEncounterData::BuildCaughtRoster({{}, {Weak, Strong}}).Num(), 2);

		const TArray<UCharacterData *> TwoTeams = UEncounterData::BuildCaughtRoster({{Weak, Strong}, {Other}});
		TestEqual(TEXT("Two teams: one each"), TwoTeams.Num(), 2);
		TestTrue(TEXT("The strongest of the first team"), TwoTeams.Num() == 2 && TwoTeams[0] == Strong && TwoTeams[1] == Other);

		TestEqual(TEXT("At most three"), UEncounterData::BuildCaughtRoster({{Weak}, {Strong}, {Other}, {Fourth}}).Num(),
				  EncounterConstants::ENCOUNTER_MAX_ENEMIES);
	});

	It("marks every team of a won fight beaten for the run", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("TrialRun"), TrialRun)) return;
		TrialRun->SetPendingEncounterSources({FIELD_TEAM_TEST_BEATEN, FIELD_TEAM_TEST_STANDING});
		TestFalse(TEXT("Not beaten before the win"), TrialRun->IsEncounterBeaten(FIELD_TEAM_TEST_BEATEN));
		TrialRun->MarkPendingEncounterBeaten();
		TestTrue(TEXT("First team beaten"), TrialRun->IsEncounterBeaten(FIELD_TEAM_TEST_BEATEN));
		TestTrue(TEXT("Second caught team beaten"), TrialRun->IsEncounterBeaten(FIELD_TEAM_TEST_STANDING));
		TestFalse(TEXT("Others untouched"), TrialRun->IsEncounterBeaten(TEXT("FieldTeamTest_Other")));
	});

	It("is gone when the level loads again after it was beaten", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("TrialRun"), TrialRun)) return;
		TrialRun->SetPendingEncounterSources({FIELD_TEAM_TEST_BEATEN});
		TrialRun->MarkPendingEncounterBeaten();

		AFieldEnemyTeam *Beaten = SpawnTeam(FIELD_TEAM_TEST_BEATEN);
		AFieldEnemyTeam *Standing = SpawnTeam(FIELD_TEAM_TEST_STANDING);
		TestTrue(TEXT("The beaten team is removed"), !IsValid(Beaten) || Beaten->IsActorBeingDestroyed());
		TestTrue(TEXT("Another team stays"), IsValid(Standing) && !Standing->IsActorBeingDestroyed());
		TestEqual(TEXT("Its id is its placed name"), Standing ? Standing->Encounter->GetEncounterId() : NAME_None,
				  FIELD_TEAM_TEST_STANDING);
	});
}

#endif
