// TurnRotationSpec.cpp
// B-170b (ActionPips.md §3.2, Q-170d, Q-170f): with wor.Turns.ActionPips 1 the turns go round in a fixed
// rotation: sides alternate, the party (or the first-striking side) first; a smaller side fills its
// extra seats in turn, carried across rounds (the deckbuilder's §8 items 53-56). Bonus turns, skips,
// death purge, revive and Execution turns keep the turn belt's rules (D-133). The switch-off behaviour
// is covered by the existing turn specs, unchanged.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"

namespace
{
	const TCHAR *const TURN_ROTATION_TEST_SWITCH = TEXT("wor.Turns.ActionPips");
	constexpr int32 TURN_ROTATION_TEST_ON = 1;
	constexpr int32 TURN_ROTATION_TEST_OFF = 0;
	constexpr int32 TURN_ROTATION_TEST_REVIVE_HP = 1;
	constexpr int32 TURN_ROTATION_TEST_EXECUTION_DELAY = 2;
	constexpr int32 TURN_ROTATION_TEST_PAYLOAD = 7;
	constexpr float TURN_ROTATION_TEST_SPELL_SPEED = 0.0f;
	constexpr int32 TURN_ROTATION_TEST_PREVIEW = 10;
	/** Far past max investment: the speed timeline would give this enemy two turns in a row. */
	constexpr int32 TURN_ROTATION_TEST_FAST_SPEED = 500;
}

BEGIN_DEFINE_SPEC(FTurnRotationSpec, "WoR.Turns.Rotation",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
	IConsoleVariable *PipsSwitch = nullptr;
	int32 SwitchBefore = TURN_ROTATION_TEST_OFF;

	AActor *Spawn(bool bEnemy)
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), bEnemy);
	}

	TArray<AActor *> SpawnSide(int32 Count, bool bEnemy)
	{
		TArray<AActor *> Side;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Side.Add(Spawn(bEnemy));
		}
		return Side;
	}

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
			Entry.PayloadId = TurnManager->GetCurrentTurnPayloadId();
			Turns.Add(Entry);
		}
		return Turns;
	}

	/** Checks the recorded turns' actors (all Normal unless the caller checks types itself). */
	void ExpectActors(const TCHAR *What, const TArray<FPreviewTurnEntry> &Turns, const TArray<AActor *> &Expected)
	{
		if (!TestEqual(FString::Printf(TEXT("%s: turn count"), What), Turns.Num(), Expected.Num()))
		{
			return;
		}
		for (int32 Turn = 0; Turn < Expected.Num(); ++Turn)
		{
			TestTrue(FString::Printf(TEXT("%s: turn %d is %s (got %s)"), What, Turn + 2, *GetNameSafe(Expected[Turn]), *GetNameSafe(Turns[Turn].Actor)),
					 Turns[Turn].Actor == Expected[Turn]);
		}
	}

	/** Starts the fight; false (recorded) when setup failed or the rotation is off. */
	bool StartFight(const TArray<AActor *> &Party, const TArray<AActor *> &Enemies, int32 OpeningTeam = INDEX_NONE)
	{
		if (!TestNotNull(TEXT("TurnManager"), TurnManager) || !TestNotNull(TEXT("the switch exists"), PipsSwitch))
		{
			return false;
		}
		TurnManager->InitializeCombat(Party, Enemies, OpeningTeam);
		return TestTrue(TEXT("the fight runs the fixed rotation"), TurnManager->IsRotationMode());
	}

	static void Kill(AActor *Actor)
	{
		UCharacterDataComponent *Data = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (Data)
		{
			Data->ServerTakeDamage(Data->MaxHP);
		}
	}
END_DEFINE_SPEC(FTurnRotationSpec)

void FTurnRotationSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		// Console priority, so a value typed earlier in the session can't override it; put back after.
		PipsSwitch = IConsoleManager::Get().FindConsoleVariable(TURN_ROTATION_TEST_SWITCH);
		if (PipsSwitch)
		{
			SwitchBefore = PipsSwitch->GetInt();
			PipsSwitch->Set(TURN_ROTATION_TEST_ON, ECVF_SetByConsole);
		}
	});

	AfterEach([this]()
	{
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		if (PipsSwitch)
		{
			PipsSwitch->Set(SwitchBefore, ECVF_SetByConsole);
		}
		PipsSwitch = nullptr;
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	Describe("The order", [this]()
	{
		It("alternates the sides, party first, in slot order, and wraps into the next round", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			TestTrue(TEXT("P1 takes turn 1"), TurnManager->GetCurrentActor() == P[0]);
			ExpectActors(TEXT("2v2"), RecordNextTurns(5), {E[0], P[1], E[1], P[0], E[0]});
		});

		It("puts a lone boss after every hero (Q-170d)", [this]()
		{
			const TArray<AActor *> P = SpawnSide(4, false);
			AActor *Boss = Spawn(true);
			if (!StartFight(P, {Boss})) return;
			ExpectActors(TEXT("4v1"), RecordNextTurns(8), {Boss, P[1], Boss, P[2], Boss, P[3], Boss, P[0]});
		});

		It("carries the smaller side's rotation on across rounds (deckbuilder item 53)", [this]()
		{
			const TArray<AActor *> A = SpawnSide(2, false);
			const TArray<AActor *> B = SpawnSide(3, true);
			if (!StartFight(A, B)) return;
			// Round 1: A1 B1 A2 B2 A1 B3; round 2: A2 B1 A1 B2 A2 B3.
			ExpectActors(TEXT("2v3"), RecordNextTurns(11), {B[0], A[1], B[1], A[0], B[2], A[1], B[0], A[0], B[1], A[1], B[2]});
		});

		It("ignores Turn Speed", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			UCharacterData *FastData = FWoRTestWorld::NewCharacterData();
			FastData->TurnSpeed = TURN_ROTATION_TEST_FAST_SPEED;
			AActor *Fast = TestWorld->SpawnCombatant(FastData, true);
			if (!StartFight(P, {Fast})) return;
			TestTrue(TEXT("the party still opens"), TurnManager->GetCurrentActor() == P[0]);
			ExpectActors(TEXT("fast foe"), RecordNextTurns(3), {Fast, P[1], Fast});
		});

		It("lets the first-striking side take its opening round, then lead the alternation (Q-170f)", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E, TurnManagerTeams::OPPOSING_TEAM_INDEX)) return;
			TestTrue(TEXT("E1 opens"), TurnManager->GetCurrentActor() == E[0]);
			ExpectActors(TEXT("enemy first strike"), RecordNextTurns(5), {E[1], E[0], P[0], E[1], P[1]});
		});

		It("shows the same order in the preview before the turn starts (the strip)", [this]()
		{
			const TArray<AActor *> A = SpawnSide(2, false);
			const TArray<AActor *> B = SpawnSide(3, true);
			if (!StartFight(A, B)) return;
			const TArray<FPreviewTurnEntry> Preview = TurnManager->PreviewTurnOrder(TURN_ROTATION_TEST_PREVIEW);
			TArray<AActor *> PreviewActors;
			for (const FPreviewTurnEntry &Entry : Preview)
			{
				PreviewActors.Add(Entry.Actor);
			}
			ExpectActors(TEXT("preview = live"), RecordNextTurns(TURN_ROTATION_TEST_PREVIEW), PreviewActors);
		});
	});

	Describe("The dead", [this]()
	{
		It("are skipped: their own turn goes, and the next round counts the living", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			Kill(E[0]);
			ExpectActors(TEXT("E1 down"), RecordNextTurns(6), {P[1], E[1], P[0], E[1], P[1], E[1]});
		});

		It("pass a smaller side's extra seat to the next living teammate (deckbuilder item 56)", [this]()
		{
			const TArray<AActor *> A = SpawnSide(2, false);
			const TArray<AActor *> B = SpawnSide(3, true);
			if (!StartFight(A, B)) return;
			Kill(A[0]); // after its own turn 1; its seat-5 turn passes to A2
			ExpectActors(TEXT("A1 down"), RecordNextTurns(11), {B[0], A[1], B[1], A[1], B[2], A[1], B[0], A[1], B[1], A[1], B[2]});
		});

		It("re-enter from the next round when revived", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			UCharacterDataComponent *E1Data = E[0]->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("E1 character data"), E1Data)) return;
			Kill(E[0]);
			E1Data->ServerResurrect(TURN_ROTATION_TEST_REVIVE_HP);
			if (!TestTrue(TEXT("E1 is back"), E1Data->bIsAlive)) return;
			ExpectActors(TEXT("E1 revived"), RecordNextTurns(4), {P[1], E[1], P[0], E[0]});
		});
	});

	Describe("The turn belt's rules (D-133)", [this]()
	{
		It("land a bonus turn right after the current one, without taking a seat", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			TurnManager->GrantBonusTurn(E[1]);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(4);
			TestTrue(TEXT("E2's bonus first"), Turns.Num() == 4 && Turns[0].Actor == E[1] && Turns[0].Type == ETurnType::Bonus);
			ExpectActors(TEXT("bonus"), Turns, {E[1], E[0], P[1], E[1]});
		});

		It("remove the holder's next turn on a skip", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			TurnManager->SkipNextTurn(E[0]);
			ExpectActors(TEXT("skip"), RecordNextTurns(4), {P[1], E[1], P[0], E[0]});
			TestEqual(TEXT("the skip is used up"), TurnManager->GetTurnsToSkip(E[0]), 0);
		});

		It("fire an Execution turn after its delay, counting every turn, without moving the rotation", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			TurnManager->ScheduleExecutionTurn(P[0], TURN_ROTATION_TEST_EXECUTION_DELAY, TURN_ROTATION_TEST_PAYLOAD, TURN_ROTATION_TEST_SPELL_SPEED);
			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(5);
			ExpectActors(TEXT("execution"), Turns, {E[0], P[1], P[0], E[1], P[0]});
			if (Turns.Num() == 5)
			{
				TestTrue(TEXT("turn 4 is P1's Execution turn"), Turns[2].Type == ETurnType::Execution && Turns[2].PayloadId == TURN_ROTATION_TEST_PAYLOAD);
				TestTrue(TEXT("turn 6 is P1's own rotation turn"), Turns[4].Type == ETurnType::Normal);
			}
		});

		It("ignore a Wind place delay (B-170d gives Wind its action loss)", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			TurnManager->DelayNextTurn(E[0], 1);
			ExpectActors(TEXT("wind"), RecordNextTurns(2), {E[0], P[1]});
		});
	});

	Describe("The switch", [this]()
	{
		It("is read when the fight starts: flipping it midway changes nothing", [this]()
		{
			const TArray<AActor *> P = SpawnSide(2, false);
			const TArray<AActor *> E = SpawnSide(2, true);
			if (!StartFight(P, E)) return;
			PipsSwitch->Set(TURN_ROTATION_TEST_OFF, ECVF_SetByConsole);
			TestTrue(TEXT("still the rotation"), TurnManager->IsRotationMode());
			ExpectActors(TEXT("after the flip"), RecordNextTurns(3), {E[0], P[1], E[1]});
			TurnManager->EndCombat();
			TurnManager->InitializeCombat(P, E);
			TestFalse(TEXT("the next fight runs the speed timeline"), TurnManager->IsRotationMode());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
