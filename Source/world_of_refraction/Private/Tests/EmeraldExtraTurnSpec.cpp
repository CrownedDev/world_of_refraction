// EmeraldExtraTurnSpec.cpp
// B-295 (D-052, Crown A): an Emerald bonus turn is a real extra turn. It isn't counted as a turn taken, so the
// holder loses no later normal turn. Before this, equal-speed Hero / Foe went Hero, Hero-bonus, Foe, Foe, Hero (the
// bonus was borrowed from Hero's next turn); now they go Hero, Hero-bonus, Foe, Hero, Foe.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/TurnManager.h"

namespace
{
	constexpr int32 EMERALD_EXTRA_TEST_READY_NOW = 0;
}

BEGIN_DEFINE_SPEC(FEmeraldExtraTurnSpec, "WoR.Turns.EmeraldExtraTurn",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
END_DEFINE_SPEC(FEmeraldExtraTurnSpec)

void FEmeraldExtraTurnSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
	});

	AfterEach([this]()
	{
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	It("gives the bonus without taking the holder's next normal turn (D-052)", [this]()
	{
		if (!TestNotNull(TEXT("TurnManager"), TurnManager)) return;
		// Equal speed: the local Hero takes turn 1, then they alternate.
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("combatants"), Hero && Foe)) return;
		TurnManager->InitializeCombat({Hero}, {Foe});
		if (!TestTrue(TEXT("Hero takes turn 1"), TurnManager->GetCurrentActor() == Hero)) return;

		TurnManager->ScheduleBonusTurn(Hero, EMERALD_EXTRA_TEST_READY_NOW);
		TurnManager->AdvanceToNextTurn();
		TestTrue(TEXT("turn 2 is Hero's bonus"),
				 TurnManager->GetCurrentActor() == Hero && TurnManager->GetCurrentTurnType() == ETurnType::Bonus);
		TurnManager->AdvanceToNextTurn();
		TestTrue(TEXT("turn 3 is Foe's"), TurnManager->GetCurrentActor() == Foe);
		TurnManager->AdvanceToNextTurn();
		TestTrue(TEXT("turn 4 is Hero's normal turn: the bonus cost nothing"),
				 TurnManager->GetCurrentActor() == Hero && TurnManager->GetCurrentTurnType() == ETurnType::Normal);
		TurnManager->AdvanceToNextTurn();
		TestTrue(TEXT("turn 5 is Foe's"), TurnManager->GetCurrentActor() == Foe);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
