// VoidRandomActionSpec.cpp
// B-199 (Q-063b, Q-199): a Void break takes the holder's next turn away. The game plays it with
// a random usable action, for a member you play as much as for the AI (Final Fantasy's Confuse).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Combat/CombatControl.h"
#include "Combat/CombatOrchestrator.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	/** Enough frames for the ally AI's thinking delay (AIConstants::HARD_THINK_MAX) to pass. */
	constexpr int32 VOID_TEST_MAX_FRAMES = 600;
}

BEGIN_DEFINE_SPEC(FVoidRandomActionSpec, "WoR.Effects.VoidRandomAction",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *Yours = nullptr;
	AActor *Foe = nullptr;

	/** Starts a fight on your member's turn; false if the setup failed. */
	bool StartOnYourTurn()
	{
		Yours = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!Orchestrator || !Effects || !Yours || !Foe)
		{
			return false;
		}
		CombatControl::SetPlayerDriven(Yours, true);
		Orchestrator->StartCombat({Yours}, {Foe});
		return Orchestrator->GetCurrentActor() == Yours;
	}

	/** Ticks until the turn leaves Yours, up to VOID_TEST_MAX_FRAMES. */
	bool TurnMovesOn()
	{
		for (int32 Frame = 0; Frame < VOID_TEST_MAX_FRAMES && Orchestrator->GetCurrentActor() == Yours; ++Frame)
		{
			TestWorld->Tick();
		}
		return Orchestrator->GetCurrentActor() != Yours;
	}
END_DEFINE_SPEC(FVoidRandomActionSpec)

void FVoidRandomActionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("SkillEffectManager"), Effects);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Effects = nullptr;
		Yours = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("plays a Void-broken member's turn for you with a random usable action", [this]()
	{
		if (!TestTrue(TEXT("Your member's turn"), StartOnYourTurn())) return;
		Effects->ApplyTriggeredSkillEffect(Foe, Yours, ESkillEffectType::RandomSkill, ESpellElement::Void);
		if (!TestTrue(TEXT("Void-broken"), Effects->HasEffectOfType(Yours, ESkillEffectType::RandomSkill))) return;

		Orchestrator->RequestActionFromActor(Yours);
		TestTrue(TEXT("The turn was played without you"), TurnMovesOn());
	});

	It("leaves an unbroken member's turn to you", [this]()
	{
		if (!TestTrue(TEXT("Your member's turn"), StartOnYourTurn())) return;
		Orchestrator->RequestActionFromActor(Yours);
		TestFalse(TEXT("The turn waits for your command"), TurnMovesOn());
	});

	It("picks only an action the rules accept (Defend when nothing else is usable)", [this]()
	{
		if (!TestTrue(TEXT("Your member's turn"), StartOnYourTurn())) return;
		const UAIDecisionManager *AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		if (!TestNotNull(TEXT("AIDecisionManager"), AI)) return;
		const FAction Action = AI->BuildRandomAction(Yours);
		TestTrue(TEXT("Accepted by the rules"), Orchestrator->ValidateAction(Action).bIsValid);
		TestTrue(TEXT("No loadout: Defend"), Action.ActionType == EActionType::Defend);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
