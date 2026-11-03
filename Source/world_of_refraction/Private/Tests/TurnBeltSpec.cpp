// TurnBeltSpec.cpp
// B-341 (D-133): the one bonus-turn rule on the deckbuilder's turn belt (WoR_TCG_Pivot_Design §7c).
// A bonus turn lands immediately after the current turn; a skip removes the holder's next turn (a
// bonus one included); death purges the holder's bonus turns, and a revive re-enters it from the
// next round. Equal speed throughout: the local Hero takes turn 1, then they alternate (ET-08: the
// party goes first on a tie).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"

namespace
{
	constexpr int32 TURN_BELT_TEST_READY_NOW = 0;
	constexpr int32 TURN_BELT_TEST_ONE = 1;
	constexpr int32 TURN_BELT_TEST_WINDOW = 3;
	constexpr int32 TURN_BELT_TEST_REVIVE_HP = 1;
}

BEGIN_DEFINE_SPEC(FTurnBeltSpec, "WoR.Turns.Belt",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** The actor and kind of each of the next NumTurns turns. */
	TArray<FPreviewTurnEntry> RecordNextTurns(int32 NumTurns)
	{
		TArray<FPreviewTurnEntry> Turns;
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			TurnManager->AdvanceToNextTurn();
			FPreviewTurnEntry Entry;
			Entry.Actor = TurnManager->GetCurrentActor();
			Entry.Type = TurnManager->GetCurrentTurnType();
			Turns.Add(Entry);
		}
		return Turns;
	}

	static bool Is(const FPreviewTurnEntry &Turn, const AActor *Actor, ETurnType Type)
	{
		return Turn.Actor == Actor && Turn.Type == Type;
	}

	/** Starts the 1v1 with the Hero on turn 1; false (recorded) when setup failed. */
	bool StartFight()
	{
		if (!TestNotNull(TEXT("TurnManager"), TurnManager) || !TestTrue(TEXT("combatants"), Hero && Foe))
		{
			return false;
		}
		TurnManager->InitializeCombat({Hero}, {Foe});
		return TestTrue(TEXT("Hero takes turn 1"), TurnManager->GetCurrentActor() == Hero);
	}
END_DEFINE_SPEC(FTurnBeltSpec)

void FTurnBeltSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TurnManager = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("A bonus turn", [this]()
	{
		It("lands right after the current turn, and costs no normal turn", [this]()
		{
			if (!StartFight()) return;
			TurnManager->GrantBonusTurn(Foe);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(TURN_BELT_TEST_WINDOW);
			TestTrue(TEXT("turn 2 is Foe's bonus"), Is(Turns[0], Foe, ETurnType::Bonus));
			TestTrue(TEXT("turn 3 is Foe's own normal turn"), Is(Turns[1], Foe, ETurnType::Normal));
			TestTrue(TEXT("turn 4 is Hero's"), Is(Turns[2], Hero, ETurnType::Normal));
		});

		It("lands ahead of a pinned turn that is already ready", [this]()
		{
			if (!StartFight()) return;
			TurnManager->ScheduleBonusTurn(Hero, TURN_BELT_TEST_READY_NOW); // the old delayed path, ready now
			TurnManager->GrantBonusTurn(Foe);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(2);
			TestTrue(TEXT("Foe's granted bonus first"), Is(Turns[0], Foe, ETurnType::Bonus));
			TestTrue(TEXT("then Hero's ready pinned turn"), Is(Turns[1], Hero, ETurnType::Bonus));
		});

		It("keeps the order two grants were made in", [this]()
		{
			if (!StartFight()) return;
			TurnManager->GrantBonusTurn(Foe);
			TurnManager->GrantBonusTurn(Hero);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(2);
			TestTrue(TEXT("first grant first"), Is(Turns[0], Foe, ETurnType::Bonus));
			TestTrue(TEXT("second grant second"), Is(Turns[1], Hero, ETurnType::Bonus));
		});
	});

	Describe("A skip", [this]()
	{
		It("removes one turn: the holder's next", [this]()
		{
			if (!StartFight()) return;
			TurnManager->SkipNextTurn(Foe);
			TestEqual(TEXT("one skip waiting"), TurnManager->GetTurnsToSkip(Foe), TURN_BELT_TEST_ONE);
			// Alternation would give Foe, Hero, Foe. Foe's turn 2 is removed, so the next round starts with Hero.
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(TURN_BELT_TEST_WINDOW);
			TestTrue(TEXT("turn 2 is Hero's"), Is(Turns[0], Hero, ETurnType::Normal));
			TestTrue(TEXT("turn 3 is Foe's: only one turn removed"), Is(Turns[1], Foe, ETurnType::Normal));
			TestTrue(TEXT("turn 4 is Hero's"), Is(Turns[2], Hero, ETurnType::Normal));
			TestEqual(TEXT("the skip is used up"), TurnManager->GetTurnsToSkip(Foe), 0);
		});

		It("removes a bonus turn when that is the holder's next", [this]()
		{
			if (!StartFight()) return;
			TurnManager->GrantBonusTurn(Foe);
			TurnManager->SkipNextTurn(Foe);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(1);
			TestTrue(TEXT("the bonus is gone; Foe's normal turn follows"), Is(Turns[0], Foe, ETurnType::Normal));
		});
	});

	Describe("Death and revive", [this]()
	{
		It("death purges the bonus turn; a revive re-enters from the next round", [this]()
		{
			if (!StartFight()) return;
			UCharacterDataComponent *FoeData = Foe->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("Foe character data"), FoeData)) return;
			TurnManager->GrantBonusTurn(Foe);
			FoeData->ServerTakeDamage(FoeData->MaxHP);
			if (!TestFalse(TEXT("Foe is dead"), FoeData->bIsAlive)) return;
			FoeData->ServerResurrect(TURN_BELT_TEST_REVIVE_HP);
			if (!TestTrue(TEXT("Foe is back"), FoeData->bIsAlive)) return;

			// Before B-341 the revived Foe kept its owed turn (and its bonus): it acted next.
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(2);
			TestTrue(TEXT("no bonus survived death; the next round starts with Hero"), Is(Turns[0], Hero, ETurnType::Normal));
			TestTrue(TEXT("then the revived Foe"), Is(Turns[1], Foe, ETurnType::Normal));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
