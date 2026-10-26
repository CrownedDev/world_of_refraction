// TurnManagerSpec.cpp
// UTurnManager turn order on bare combatants: deterministic tie-break, alternation
// at equal speed, and a faster combatant acting first and more often.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/TurnManager.h"

namespace
{
	constexpr int32 ALTERNATION_TURNS = 6;
	constexpr int32 SPEED_SAMPLE_TURNS = 20;
	/** Far past max investment — turn speed lands on its stat cap (TURN_SPEED_CAP). */
	constexpr int32 OVERCAP_TURN_SPEED_POINTS = 500;
}

BEGIN_DEFINE_SPEC(FTurnManagerSpec, "WoR.TurnManager",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;

	/** Actor of each turn, the first turn (dispatched inside InitializeCombat) included. */
	TArray<AActor *> RecordTurns(AActor *Local, AActor *Opposing, int32 NumTurns)
	{
		TArray<AActor *> Order;
		if (!TurnManager || !Local || !Opposing)
		{
			return Order; // setup failure already recorded; the empty order fails the caller's checks
		}
		TurnManager->InitializeCombat({Local}, {Opposing});
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
END_DEFINE_SPEC(FTurnManagerSpec)

void FTurnManagerSpec::Define()
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

	It("gives the first turn to the local party on a full tie", [this]()
	{
		AActor *Local = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Opposing = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());

		const TArray<AActor *> Order = RecordTurns(Local, Opposing, 1);
		TestTrue(TEXT("Local acts first"), Order.Num() == 1 && Order[0] == Local);
	});

	It("alternates turns at equal speed", [this]()
	{
		AActor *Local = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Opposing = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());

		const TArray<AActor *> Order = RecordTurns(Local, Opposing, ALTERNATION_TURNS);
		for (int32 Turn = 1; Turn < Order.Num(); ++Turn)
		{
			TestTrue(FString::Printf(TEXT("Turn %d differs from turn %d"), Turn + 1, Turn), Order[Turn] != Order[Turn - 1]);
		}
	});

	It("lets a faster combatant act first and more often", [this]()
	{
		UCharacterData *FastData = FWoRTestWorld::NewCharacterData();
		FastData->TurnSpeed = OVERCAP_TURN_SPEED_POINTS;
		AActor *Slow = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Fast = TestWorld->SpawnCombatant(FastData);

		// Fast sits on the opposing side so the team tie-break can't explain its first turn.
		const TArray<AActor *> Order = RecordTurns(Slow, Fast, SPEED_SAMPLE_TURNS);
		const int32 FastTurns = Order.FilterByPredicate([Fast](AActor *Actor) { return Actor == Fast; }).Num();
		const int32 SlowTurns = Order.FilterByPredicate([Slow](AActor *Actor) { return Actor == Slow; }).Num();

		TestTrue(TEXT("Fast acts first"), Order.Num() > 0 && Order[0] == Fast);
		TestTrue(FString::Printf(TEXT("Fast takes more turns (%d vs %d)"), FastTurns, SlowTurns), FastTurns > SlowTurns);
		TestTrue(TEXT("Slow still gets turns"), SlowTurns > 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
