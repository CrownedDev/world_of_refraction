// FirstStrikeSpec.cpp
// B-185 (Enemies §2, Q-185): touching a team from outside its notice cone gives the party first
// strike; a team reaching the player from behind gives it to the enemy. The side with first strike
// takes its whole opening round before the other side moves.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/TurnManager.h"

namespace
{
	constexpr float FIRST_STRIKE_TEST_NOTICE_HALF_ANGLE = 60.0f;
	constexpr float FIRST_STRIKE_TEST_DISTANCE = 100.0f;
	/** Far past max investment — turn speed lands on its stat cap. */
	constexpr int32 FIRST_STRIKE_TEST_FAST_SPEED = 500;
	constexpr int32 FIRST_STRIKE_TEST_LOCAL_TEAM = 0;
	constexpr int32 FIRST_STRIKE_TEST_OPPOSING_TEAM = 1;
}

BEGIN_DEFINE_SPEC(FFirstStrikeSpec, "WoR.Combat.FirstStrike",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;

	/** Actor of each of the first NumTurns turns. */
	TArray<AActor *> RecordTurns(const TArray<AActor *> &Local, const TArray<AActor *> &Opposing, int32 OpeningTeam, int32 NumTurns)
	{
		TArray<AActor *> Order;
		TurnManager->InitializeCombat(Local, Opposing, OpeningTeam);
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			if (Turn > 0)
			{
				TurnManager->AdvanceToNextTurn();
			}
			Order.Add(TurnManager->GetCurrentActor());
		}
		TurnManager->EndCombat();
		return Order;
	}
END_DEFINE_SPEC(FFirstStrikeSpec)

void FFirstStrikeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("TurnManager subsystem"), TurnManager);
	});

	AfterEach([this]()
	{
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	It("gives the party first strike from outside the notice cone, the enemy from behind the player", [this]()
	{
		const FVector Team = FVector::ZeroVector;
		const FVector TeamFacing = FVector::ForwardVector;
		const FVector Behind = -FVector::ForwardVector * FIRST_STRIKE_TEST_DISTANCE;
		const FVector InFront = FVector::ForwardVector * FIRST_STRIKE_TEST_DISTANCE;

		TestEqual(TEXT("Behind the team: party"),
				  static_cast<int32>(UEncounterComponent::JudgeFirstStrike(Team, TeamFacing, Behind, FVector::ForwardVector, FIRST_STRIKE_TEST_NOTICE_HALF_ANGLE)),
				  static_cast<int32>(EFirstStrike::Party));
		TestEqual(TEXT("Seen, back turned: enemy"),
				  static_cast<int32>(UEncounterComponent::JudgeFirstStrike(Team, TeamFacing, InFront, FVector::ForwardVector, FIRST_STRIKE_TEST_NOTICE_HALF_ANGLE)),
				  static_cast<int32>(EFirstStrike::Enemy));
		TestEqual(TEXT("Face to face: nobody"),
				  static_cast<int32>(UEncounterComponent::JudgeFirstStrike(Team, TeamFacing, InFront, -FVector::ForwardVector, FIRST_STRIKE_TEST_NOTICE_HALF_ANGLE)),
				  static_cast<int32>(EFirstStrike::None));
	});

	It("lets the enemy side take its whole opening round first", [this]()
	{
		if (!TurnManager) return;
		AActor *Local = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *EnemyA = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		AActor *EnemyB = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);

		const TArray<AActor *> Plain = RecordTurns({Local}, {EnemyA, EnemyB}, INDEX_NONE, 1);
		TestTrue(TEXT("Without it the party wins the tie"), Plain.Num() == 1 && Plain[0] == Local);

		const TArray<AActor *> Order = RecordTurns({Local}, {EnemyA, EnemyB}, FIRST_STRIKE_TEST_OPPOSING_TEAM, 3);
		TestTrue(TEXT("Both enemies first, then the party"), Order.Num() == 3 && Order[0] != Local && Order[1] != Local && Order[2] == Local);
	});

	It("lets the party go first even against a faster enemy, and the preview shows it", [this]()
	{
		if (!TurnManager) return;
		UCharacterData *FastData = FWoRTestWorld::NewCharacterData();
		FastData->TurnSpeed = FIRST_STRIKE_TEST_FAST_SPEED;
		AActor *LocalA = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *LocalB = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Fast = TestWorld->SpawnCombatant(FastData, true);

		const TArray<AActor *> Plain = RecordTurns({LocalA, LocalB}, {Fast}, INDEX_NONE, 1);
		TestTrue(TEXT("Without it the fast enemy goes first"), Plain.Num() == 1 && Plain[0] == Fast);

		TurnManager->InitializeCombat({LocalA, LocalB}, {Fast}, FIRST_STRIKE_TEST_LOCAL_TEAM);
		const TArray<FPreviewTurnEntry> Preview = TurnManager->PreviewTurnOrder(1);
		TestTrue(TEXT("A party member opens"), TurnManager->GetCurrentActor() != Fast);
		TestTrue(TEXT("The preview shows the other member next"), Preview.Num() == 1 && Preview[0].Actor != Fast);
		TurnManager->EndCombat();
	});
}

#endif
