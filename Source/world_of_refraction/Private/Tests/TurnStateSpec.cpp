// TurnStateSpec.cpp
// B-228: the GameState's turn state mirrors UTurnManager on the server: the current actor, the upcoming order, each
// combatant's team; it follows a turn advance and clears at combat end. (Replication to a client is the 2-player
// PIE check, docs/PIE/B-228.md.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/TurnManager.h"
#include "Player/TurnStateComponent.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"

namespace
{
	/** UTurnManager's sides: the local party and the opposing party. */
	constexpr int32 TURN_STATE_TEST_LOCAL_TEAM = 0;
	constexpr int32 TURN_STATE_TEST_OPPOSING_TEAM = 1;
}

BEGIN_DEFINE_SPEC(FTurnStateSpec, "WoR.Coop.TurnState",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
	UTurnStateComponent *TurnState = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
END_DEFINE_SPEC(FTurnStateSpec)

void FTurnStateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		AActor *GameStateStandIn = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<AActor>() : nullptr;
		TurnState = GameStateStandIn ? FWoRTestWorld::AddComponent<UTurnStateComponent>(GameStateStandIn) : nullptr;
	});

	AfterEach([this]()
	{
		if (TurnState)
		{
			TurnState->UnbindFromTurnManager();
		}
		TestWorld.Reset();
	});

	It("copies the current actor, the upcoming order and the teams, and follows a turn", [this]()
	{
		if (!TestTrue(TEXT("setup"), TurnManager && TurnState && Hero && Foe))
		{
			return;
		}
		// The component bound at BeginPlay (AddComponent on a live actor); unbind so this binds after the fight starts.
		TurnState->UnbindFromTurnManager();
		TurnManager->InitializeCombat({Hero}, {Foe});
		TurnState->BindToTurnManager();

		TestTrue(TEXT("combat on"), TurnState->IsCombatActive());
		TestEqual(TEXT("turn number"), TurnState->GetTurnNumber(), TurnManager->GetTurnCount());
		TestEqual(TEXT("current actor"), TurnState->GetCurrentActor(), TurnManager->GetCurrentActor());
		const TArray<FPreviewTurnEntry> Preview = TurnManager->PreviewTurnOrder(TurnState->GetUpcoming().Num());
		if (TestEqual(TEXT("upcoming count"), TurnState->GetUpcoming().Num(), Preview.Num()))
		{
			for (int32 Index = 0; Index < Preview.Num(); ++Index)
			{
				TestEqual(FString::Printf(TEXT("upcoming %d"), Index), TurnState->GetUpcoming()[Index].Actor.Get(), Preview[Index].Actor);
			}
		}
		TestEqual(TEXT("hero's team"), TurnState->GetActorTeam(Hero), TURN_STATE_TEST_LOCAL_TEAM);
		TestEqual(TEXT("foe's team"), TurnState->GetActorTeam(Foe), TURN_STATE_TEST_OPPOSING_TEAM);
		TestEqual(TEXT("team members"), TurnState->GetTeamMembers(TURN_STATE_TEST_OPPOSING_TEAM).Num(), 1);

		TurnManager->AdvanceToNextTurn();
		TestEqual(TEXT("after a turn"), TurnState->GetCurrentActor(), TurnManager->GetCurrentActor());
		TestEqual(TEXT("turn number after a turn"), TurnState->GetTurnNumber(), TurnManager->GetTurnCount());
	});

	It("clears at combat end", [this]()
	{
		if (!TestTrue(TEXT("setup"), TurnManager && TurnState && Hero && Foe))
		{
			return;
		}
		TurnManager->InitializeCombat({Hero}, {Foe});
		TurnState->BindToTurnManager();
		TurnManager->EndCombat();
		TestFalse(TEXT("combat off"), TurnState->IsCombatActive());
		TestNull(TEXT("no current actor"), TurnState->GetCurrentActor());
		TestEqual(TEXT("no teams"), TurnState->GetActorTeam(Hero), static_cast<int32>(INDEX_NONE));
	});

	It("builds a client's combat HUD once when combat turns on and tears it down once at the end (B-228b)", [this]()
	{
		TestTrue(TEXT("client, combat on, nothing built: build"), ClientCombatHUD::Next(true, true, false) == EClientHUDStep::Build);
		TestTrue(TEXT("client, combat on, built: nothing"), ClientCombatHUD::Next(true, true, true) == EClientHUDStep::None);
		TestTrue(TEXT("client, combat off, built: tear down"), ClientCombatHUD::Next(true, false, true) == EClientHUDStep::Teardown);
		TestTrue(TEXT("client, combat off, nothing built: nothing"), ClientCombatHUD::Next(true, false, false) == EClientHUDStep::None);
		TestTrue(TEXT("the server builds its own elsewhere"), ClientCombatHUD::Next(false, true, false) == EClientHUDStep::None);
	});

	It("opens the command menu only where the member's player is (B-232b)", [this]()
	{
		TestTrue(TEXT("solo / no run state: opens"), CommandMenuRules::OpensHere(false, false, false));
		TestTrue(TEXT("commanded here: opens"), CommandMenuRules::OpensHere(false, true, true));
		TestFalse(TEXT("another player's member: not here"), CommandMenuRules::OpensHere(false, true, false));
		TestFalse(TEXT("an AI seat: never"), CommandMenuRules::OpensHere(true, true, true));
		TestFalse(TEXT("an AI seat solo: never"), CommandMenuRules::OpensHere(true, false, false));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
