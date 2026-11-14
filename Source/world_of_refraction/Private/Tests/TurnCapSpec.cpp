// TurnCapSpec.cpp
// B-297 (D-049): a combatant gets at most 2 turns per turn of the slowest. A far faster combatant never
// takes more than 2 turns in a row while its opponent lives, and its speed ratio stops at 2. B-297b (Q-297 A):
// Emerald bonus turns go past the cap.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 TURN_CAP_SAMPLE_TURNS = 30;
	/** Far past max investment: turn speed lands on its stat cap. */
	constexpr int32 TURN_CAP_FAST_POINTS = 500;
	/** A self-inflicted slow (percent) that puts the fast one well past twice the slow one's speed. */
	constexpr float TURN_CAP_SLOW_PERCENT = -70.0f;
	constexpr int32 TURN_CAP_SLOW_TURNS = 99;
	constexpr int32 TURN_CAP_SLOW_EFFECT_ID = 9297;
	/** The rule's number, as a literal so a changed constant fails here. */
	constexpr int32 TURN_CAP_RULE_RATIO = 2;
}

BEGIN_DEFINE_SPEC(FTurnCapSpec, "WoR.Turns.TurnCap",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
	AActor *Slow = nullptr;
	AActor *Fast = nullptr;

	/** Starts the fight with Slow slowed far below half of Fast's speed. */
	void StartSlowedFight()
	{
		TurnManager->InitializeCombat({Slow}, {Fast});
		if (USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>())
		{
			Effects->ApplyEffect(Slow, FActiveSkillEffect::CreateBuff(TEXT("Turn cap test"), TURN_CAP_SLOW_EFFECT_ID, ESkillEffectType::ModifyTurnSpeed,
																	  TURN_CAP_SLOW_PERCENT, TURN_CAP_SLOW_TURNS),
								 Slow, TEXT("TurnCapTest"));
		}
		TurnManager->OnActorSpeedChanged(Slow);
	}

	/** The actor of each turn, the first included. */
	TArray<AActor *> RecordTurns(int32 NumTurns)
	{
		TArray<AActor *> Order;
		StartSlowedFight();
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			if (Turn > 0)
			{
				TurnManager->AdvanceToNextTurn();
			}
			Order.Add(TurnManager->GetCurrentActor());
		}
		return Order;
	}

	static int32 LongestRun(const TArray<AActor *> &Order, const AActor *Actor)
	{
		int32 Longest = 0;
		int32 Run = 0;
		for (const AActor *Turn : Order)
		{
			Run = Turn == Actor ? Run + 1 : 0;
			Longest = FMath::Max(Longest, Run);
		}
		return Longest;
	}
	TOptional<FWoRActionPipsSwitchScope> SpeedTimeline; // B-170h: this spec checks the old speed timeline
END_DEFINE_SPEC(FTurnCapSpec)

void FTurnCapSpec::Define()
{
	BeforeEach([this]()
	{
		SpeedTimeline.Emplace(WOR_TEST_SPEED_TIMELINE);
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("TurnManager"), TurnManager);
		UCharacterData *FastData = FWoRTestWorld::NewCharacterData();
		FastData->TurnSpeed = TURN_CAP_FAST_POINTS;
		Slow = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Fast = TestWorld->SpawnCombatant(FastData);
	});

	AfterEach([this]()
	{
		SpeedTimeline.Reset();
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TurnManager = nullptr;
		Slow = nullptr;
		Fast = nullptr;
		TestWorld.Reset();
	});

	It("keeps the cap at 2 turns per turn of the slowest (D-049)", [this]()
	{
		TestEqual(TEXT("MAX_TURN_RATIO"), CombatConstants::MAX_TURN_RATIO, TURN_CAP_RULE_RATIO);
	});

	It("never gives the fast combatant more than 2 turns in a row", [this]()
	{
		if (!TurnManager) return;
		TArray<AActor *> Order = RecordTurns(TURN_CAP_SAMPLE_TURNS);
		const int32 SlowSpeed = TurnManager->GetEffectiveTurnSpeed(Slow);
		const int32 FastSpeed = TurnManager->GetEffectiveTurnSpeed(Fast);
		TestTrue(FString::Printf(TEXT("fast is over twice as fast (%d vs %d)"), FastSpeed, SlowSpeed),
				 FastSpeed > TURN_CAP_RULE_RATIO * SlowSpeed);
		TestTrue(FString::Printf(TEXT("longest run %d"), LongestRun(Order, Fast)), LongestRun(Order, Fast) <= TURN_CAP_RULE_RATIO);
		for (const FPreviewTurnEntry &Entry : TurnManager->PreviewTurnOrder(TURN_CAP_SAMPLE_TURNS))
		{
			Order.Add(Entry.Actor); // the belt simulation obeys the cap too
		}
		TestTrue(TEXT("belt keeps the cap"), LongestRun(Order, Fast) <= TURN_CAP_RULE_RATIO);
		const int32 FastTurns = Order.FilterByPredicate([this](AActor *Actor) { return Actor == Fast; }).Num();
		const int32 SlowTurns = Order.Num() - FastTurns;
		TestTrue(FString::Printf(TEXT("fast %d <= 2 x slow %d + 2"), FastTurns, SlowTurns), FastTurns <= TURN_CAP_RULE_RATIO * SlowTurns + TURN_CAP_RULE_RATIO);
		TestTrue(TEXT("fast still acts more"), FastTurns > SlowTurns);
	});

	It("spends a Wind delay the cap blocks instead of breaking the cap (Q-297b)", [this]()
	{
		if (!TurnManager) return;
		StartSlowedFight();
		TurnManager->DelayNextTurn(Slow, TURN_CAP_RULE_RATIO + 1);
		TArray<AActor *> Order = {TurnManager->GetCurrentActor()};
		for (int32 Turn = 1; Turn < TURN_CAP_SAMPLE_TURNS; ++Turn)
		{
			TurnManager->AdvanceToNextTurn();
			Order.Add(TurnManager->GetCurrentActor());
		}
		TestTrue(FString::Printf(TEXT("longest fast run %d"), LongestRun(Order, Fast)), LongestRun(Order, Fast) <= TURN_CAP_RULE_RATIO);
		TestTrue(TEXT("slow still acts"), Order.Contains(Slow));
	});

	It("lets Emerald bonus turns go past the cap (Q-297 A)", [this]()
	{
		if (!TurnManager) return;
		StartSlowedFight();
		AActor *Previous = nullptr;
		for (int32 Turn = 0; Turn < TURN_CAP_SAMPLE_TURNS && !(Previous == Fast && TurnManager->GetCurrentActor() == Fast); ++Turn)
		{
			Previous = TurnManager->GetCurrentActor();
			TurnManager->AdvanceToNextTurn();
		}
		if (!TestTrue(TEXT("fast reached the cap"), Previous == Fast && TurnManager->GetCurrentActor() == Fast)) return;

		TurnManager->ScheduleBonusTurn(Fast, 0, TURN_CAP_RULE_RATIO); // an S Emerald: two turns back to back
		for (int32 Bonus = 1; Bonus <= TURN_CAP_RULE_RATIO; ++Bonus)
		{
			TurnManager->AdvanceToNextTurn();
			TestTrue(FString::Printf(TEXT("bonus %d is fast's, past the cap"), Bonus),
					 TurnManager->GetCurrentActor() == Fast && TurnManager->GetCurrentTurnType() == ETurnType::Bonus);
		}
		TurnManager->AdvanceToNextTurn();
		TestTrue(TEXT("then the opponent acts"), TurnManager->GetCurrentActor() == Slow);
	});
}

#endif
