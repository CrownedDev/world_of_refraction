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
#include "Character/CharacterDataComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 RITUAL_DELAY_TURNS = 1;
	/** Turns to play through: enough for the delay to burn and the Execution turn to come up. */
	constexpr int32 RITUAL_MAX_TURNS_TO_PLAY = 6;
	/** Test-world ticks are 0.01s; spans the executor's 10s timeout, which covers the defense
	 *  window's 8s failsafe a montage-less ability waits on (no impacts land to close it). */
	constexpr int32 RITUAL_SETTLE_FRAMES = 1100;
	constexpr int32 RITUAL_BURN_ID = 22501;
	constexpr int32 RITUAL_BURN_TURNS = 1;
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

	/** Starts a duel (Hero has turn 1) and arms the ritual MakeRitual builds for Hero, as the executor
	 *  does at arm time. The ritual is built after the spawn so it can target Foe. */
	bool StartDuelWithRitual(TFunctionRef<FAction()> MakeRitual)
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
		Executor->OnActionDeferredArmed.Broadcast(Hero, MakeRitual(), RITUAL_DELAY_TURNS);
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
		const auto MakeDefend = []()
		{
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			return Defend;
		};
		if (!StartDuelWithRitual(MakeDefend))
		{
			return;
		}

		PlayTurns();

		TestTrue(TEXT("The ritual fired and worked"), Recorder->Saw(Hero, EActionType::Defend, true));
		TestEqual(TEXT("Fired once"), Recorder->Entries.Num(), 1);
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("Back on a normal turn"), TurnManager->GetCurrentTurnType() == ETurnType::Normal);
	});

	It("fires a montage-less ritual ability (the case the old sync router dropped)", [this]()
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Montage-less Ritual");
		Ability->AddToRoot();
		if (!StartDuelWithRitual([this, Ability]() { return MakeAbilityOn(Foe, Ability); }))
		{
			Ability->RemoveFromRoot();
			return;
		}
		PlayTurns();
		Ability->RemoveFromRoot();

		TestTrue(TEXT("The ritual ability fired"), Recorder->Saw(Hero, EActionType::Ability, true));
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("Back on a normal turn"), TurnManager->GetCurrentTurnType() == ETurnType::Normal);
	});

	It("advances past a ritual ability whose target is gone when it fires", [this]()
	{
		// A deferred fire skips the dead-target gate, so the skill finalizes inside its own start
		// (no valid targets) - the B-166 crash path. The dead bystander isn't in the fight.
		AActor *DeadTarget = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UCharacterDataComponent *DeadData = DeadTarget ? DeadTarget->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Dead target data"), DeadData))
		{
			return;
		}
		DeadData->bIsAlive = false;
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Targetless Ritual");
		Ability->AddToRoot();
		if (!StartDuelWithRitual([this, DeadTarget, Ability]() { return MakeAbilityOn(DeadTarget, Ability); }))
		{
			Ability->RemoveFromRoot();
			return;
		}
		PlayTurns();
		Ability->RemoveFromRoot();

		TestTrue(TEXT("The ritual fired and fizzled"), Recorder->Saw(Hero, EActionType::Ability, false));
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestTrue(TEXT("Back on a normal turn"), TurnManager->GetCurrentTurnType() == ETurnType::Normal);
	});

	It("runs the caster's turn-start step on its Execution turn (Q-044d)", [this]()
	{
		const auto MakeDefend = []()
		{
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			return Defend;
		};
		USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("EffectManager"), EffectManager) || !StartDuelWithRitual(MakeDefend))
		{
			return;
		}
		UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Hero data"), HeroData))
		{
			return;
		}
		// Ends turns until the next one is the Execution turn, then lands a lethal burn on the
		// caster: only the Execution turn's own turn-start step can pay it before the fire.
		for (int32 Turn = 0; Turn < RITUAL_MAX_TURNS_TO_PLAY; ++Turn)
		{
			const TArray<FPreviewTurnEntry> Next = TurnManager->PreviewTurnOrder(1);
			if (Next.Num() > 0 && Next[0].Type == ETurnType::Execution)
			{
				break;
			}
			Orchestrator->OnActionCompleted();
			TestWorld->Tick(RITUAL_SETTLE_FRAMES);
		}
		const TArray<FPreviewTurnEntry> Next = TurnManager->PreviewTurnOrder(1);
		if (!TestTrue(TEXT("The Execution turn is next"), Next.Num() > 0 && Next[0].Type == ETurnType::Execution))
		{
			return;
		}
		// Landing on the caster's own turn would pay at once (Q-063), before the Execution turn.
		if (!TestTrue(TEXT("Foe's turn before the Execution turn"), Orchestrator->GetCurrentActor() == Foe))
		{
			return;
		}
		FActiveSkillEffect Burn = FActiveSkillEffect::CreateDOT(TEXT("Lethal burn"), RITUAL_BURN_ID, HeroData->CurrentHP,
																 RITUAL_BURN_TURNS, ESpellElement::None);
		Burn.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		EffectManager->ApplyEffect(Hero, Burn, Foe);
		TestTrue(TEXT("Alive until the turn starts"), HeroData->bIsAlive);

		Orchestrator->OnActionCompleted();
		TestWorld->Tick(RITUAL_SETTLE_FRAMES);

		TestFalse(TEXT("The burn paid at the Execution turn start"), HeroData->bIsAlive);
		TestEqual(TEXT("The dead caster's ritual never fired"), Recorder->Entries.Num(), 0);
		TestTrue(TEXT("The fight is over"), Orchestrator->GetCombatState() != ECombatState::InProgress);
	});

	It("advances past a ritual that can't fire", [this]()
	{
		// An ability with no skill asset is refused at fire time.
		if (!StartDuelWithRitual([this]() { return MakeAbilityOn(Foe, nullptr); }))
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
