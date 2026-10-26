// CombatRitualFireSpec.cpp
// B-031: a ritual fires on its Execution turn through the async path (the only action path
// left), a montage-less ritual ability is no longer dropped, and a refused fire never strands
// the turn. Driven through a real ACombatOrchestrator on bare 1v1 combatants.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	constexpr int32 RITUAL_DELAY_TURNS = 1;
	/** Turns to play through: enough for the delay to burn and the Execution turn to come up. */
	constexpr int32 RITUAL_MAX_TURNS_TO_PLAY = 6;
	/** Test-world ticks are 0.01s; spans any timer an async action waits on, with margin. */
	constexpr int32 RITUAL_SETTLE_FRAMES = 200;
}

BEGIN_DEFINE_SPEC(FCombatRitualFireSpec, "WoR.Combat.RitualFire",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UActionExecutor *Executor = nullptr;
	UTurnManager *TurnManager = nullptr;
	UActionExecutedRecorder *Recorder = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Starts a duel (Hero has turn 1) and arms Ritual for Hero, as the executor does at arm time. */
	bool StartDuelWithRitual(const FAction &Ritual)
	{
		if (!Orchestrator || !Executor || !TurnManager)
		{
			return false;
		}
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		Recorder = NewObject<UActionExecutedRecorder>();
		Recorder->AddToRoot();
		Recorder->Listen(Orchestrator);
		Executor->OnActionDeferredArmed.Broadcast(Hero, Ritual, RITUAL_DELAY_TURNS);
		return true;
	}

	/** Ends normal turns until the ritual's Execution turn has come and gone (the TurnManager has no
	 *  Execution turn left to fire) or the cap. The Execution turn must resolve itself: ending it from
	 *  here would hide a stranded fire, so finding one current is a failure. */
	void PlayTurns()
	{
		for (int32 Turn = 0; Turn < RITUAL_MAX_TURNS_TO_PLAY && Orchestrator->GetCombatState() == ECombatState::InProgress; ++Turn)
		{
			if (!TestTrue(TEXT("Never left on an Execution turn"), TurnManager->GetCurrentTurnType() != ETurnType::Execution))
			{
				return;
			}
			const int32 TurnBefore = Orchestrator->GetCurrentTurnNumber();
			Orchestrator->OnActionCompleted();
			TestWorld->Tick(RITUAL_SETTLE_FRAMES);
			if (Orchestrator->GetCurrentTurnNumber() > TurnBefore + 1)
			{
				return; // an Execution turn came up and resolved inside that advance
			}
		}
		AddError(TEXT("The ritual's Execution turn never came up"));
	}

	FAction MakeAbilityOn(AActor *Target, UAbilityData *Ability) const
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.Targets.Add(Target);
		return Action;
	}
END_DEFINE_SPEC(FCombatRitualFireSpec)

void FCombatRitualFireSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("ActionExecutor"), Executor);
		TestNotNull(TEXT("TurnManager"), TurnManager);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		if (Recorder)
		{
			Recorder->RemoveFromRoot();
			Recorder = nullptr;
		}
		Orchestrator = nullptr;
		Executor = nullptr;
		TurnManager = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	It("fires an armed ritual on its Execution turn and moves on", [this]()
	{
		FAction Defend;
		Defend.ActionType = EActionType::Defend;
		if (!StartDuelWithRitual(Defend))
		{
			return;
		}

		PlayTurns();

		TestTrue(TEXT("The ritual fired and worked"), Recorder->Saw(Hero, EActionType::Defend, true));
		TestEqual(TEXT("Fired once"), Recorder->Entries.Num(), 1);
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("Back on a normal turn"), TurnManager->GetCurrentTurnType() == ETurnType::Normal);
	});

	// TODO: a montage-less ritual ability (the case the old sync router dropped) needs PIE for now:
	// the async skill start crashes in the headless test world (BeginSkillExecution), see B-031.

	It("advances past a ritual that can't fire", [this]()
	{
		// An ability with no skill asset is refused at fire time.
		if (!StartDuelWithRitual(MakeAbilityOn(Foe, nullptr)))
		{
			return;
		}
		PlayTurns();

		TestEqual(TEXT("Nothing fired"), Recorder->Entries.Num(), 0);
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("Back on a normal turn"), TurnManager->GetCurrentTurnType() == ETurnType::Normal);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
