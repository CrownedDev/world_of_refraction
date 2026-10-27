// CombatHangFixesSpec.cpp
// Regression tests for the Arc A combat-hang fixes (B-001), driven through a real
// ACombatOrchestrator on bare 1v1 combatants: a stunned actor's turn (D-001 rule), full
// release on a normal win, and no stranded turn after a failed validation.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	constexpr int32 TEST_STUN_EFFECT_ID = 9001;
	constexpr int32 TEST_BUFF_EFFECT_ID = 9002;
	constexpr int32 ONE_TURN = 1;
	constexpr int32 TEST_BUFF_TURNS = 3;
	constexpr float TEST_BUFF_PERCENT = 10.0f;
	/** Fraction of the bar to fill — well short of a cap, so nothing triggers. */
	constexpr float PARTIAL_BAR_FRACTION = 0.25f;
	/** Enough frames for a next-tick timer to fire. */
	constexpr int32 SETTLE_FRAMES = 3;
	constexpr float TEST_AUTO_ADVANCE_DELAY = 0.05f;
	/** Test-world ticks are 0.01s, so this spans the auto-advance delay with margin. */
	constexpr int32 AUTO_ADVANCE_FRAMES = 10;
}

BEGIN_DEFINE_SPEC(FCombatHangFixesSpec, "WoR.Combat.HangFixes",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	USkillEffectManager *EffectManager = nullptr;
	UStatusBuildupManager *BuildupManager = nullptr;
	UTurnManager *TurnManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Equal stats: the tie-break gives the local party (Hero) turn 1. False if setup failed —
	 *  the failure is already recorded, so the caller just returns instead of crashing. */
	bool StartDuel(bool bHeroIsAI = false)
	{
		if (!Orchestrator || !EffectManager || !BuildupManager || !TurnManager)
		{
			return false;
		}
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), bHeroIsAI);
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		return true;
	}

	static FAction MakeInvalidAction(AActor *Target)
	{
		// An ability with no skill asset — what a mis-picked AI or menu slot submits.
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.Targets.Add(Target);
		return Action;
	}
END_DEFINE_SPEC(FCombatHangFixesSpec)

void FCombatHangFixesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		BuildupManager = TestWorld->GetSubsystem<UStatusBuildupManager>();
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("StatusBuildupManager"), BuildupManager);
		TestNotNull(TEXT("TurnManager"), TurnManager);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	// D-001 (B): stunned = may still Attack or Defend, so the turn is NOT auto-passed (B-061).
	It("asks a stunned actor to act and only lets it Attack or Defend", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		TestTrue(TEXT("Turn 1 is Hero's"), Orchestrator->GetCurrentActor() == Hero);

		// Same shape as the bar-cap / on-crit stun: one turn, ticked at end of own turn.
		EffectManager->ApplyEffect(Foe, FActiveSkillEffect::CreateBuff(TEXT("Stun"), TEST_STUN_EFFECT_ID, ESkillEffectType::Stun, 0.0f, ONE_TURN), Hero);
		TestTrue(TEXT("Foe stunned"), EffectManager->IsStunned(Foe));

		Orchestrator->OnActionCompleted();
		TestWorld->Tick(SETTLE_FRAMES);
		TestTrue(TEXT("Turn 2 is Foe's and stays Foe's (no auto-pass)"), Orchestrator->GetCurrentActor() == Foe);
		TestEqual(TEXT("Turn number"), Orchestrator->GetCurrentTurnNumber(), 2);

		FAction Defend;
		Defend.ActionType = EActionType::Defend;
		FAction Ability;
		Ability.ActionType = EActionType::Ability;
		Ability.SkillData = NewObject<UAbilityData>(GetTransientPackage());
		Ability.Targets.Add(Hero);
		TestTrue(TEXT("Defend allowed while stunned"), Orchestrator->ValidateAction(Defend).bIsValid);
		TestFalse(TEXT("Ability refused while stunned"), Orchestrator->ValidateAction(Ability).bIsValid);

		Orchestrator->SubmitActionAsync(Ability);
		TestTrue(TEXT("Refused pick re-asks Foe"), Orchestrator->GetCurrentActor() == Foe);

		Orchestrator->OnActionCompleted();
		TestTrue(TEXT("Turn 3 is back to Hero"), Orchestrator->GetCurrentActor() == Hero);
		// D-051 (B-225): Foe's turn counted the stun's one turn; it ends as Foe's next turn starts.
		Orchestrator->OnActionCompleted();
		TestTrue(TEXT("Turn 4 is Foe's"), Orchestrator->GetCurrentActor() == Foe);
		TestFalse(TEXT("Stun ended as Foe's next turn started"), EffectManager->IsStunned(Foe));
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
	});

	It("resets status bars and releases combat state on a normal win", [this]()
	{
		if (!StartDuel())
		{
			return;
		}

		BuildupManager->AddStatusBuildup(Foe, Hero, BuildupManager->GetBuildupToTrigger(Hero) * PARTIAL_BAR_FRACTION,
										 ESpellElement::Fire, EPhysicalDamageType::None);
		BuildupManager->AddStatusBuildup(Hero, Foe, BuildupManager->GetBuildupToTrigger(Foe) * PARTIAL_BAR_FRACTION,
										 ESpellElement::Fire, EPhysicalDamageType::None);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Test Buff"), TEST_BUFF_EFFECT_ID, ESkillEffectType::DamageBuff, TEST_BUFF_PERCENT, TEST_BUFF_TURNS), Hero);
		TestTrue(TEXT("Hero bar filled"), BuildupManager->GetStatusBarBuildup(Hero) > 0.0f);
		TestTrue(TEXT("Foe bar filled"), BuildupManager->GetStatusBarBuildup(Foe) > 0.0f);
		TestTrue(TEXT("Hero has an effect"), EffectManager->GetEffectCount(Hero) > 0);

		UCharacterDataComponent *FoeComp = Foe->FindComponentByClass<UCharacterDataComponent>();
		FoeComp->ServerTakeDamage(FoeComp->MaxHP);
		TestFalse(TEXT("Foe dead"), FoeComp->bIsAlive);

		Orchestrator->OnActionCompleted();

		TestTrue(TEXT("Orchestrator back to Idle"), Orchestrator->GetCombatState() == ECombatState::Idle);
		TestFalse(TEXT("TurnManager combat ended"), TurnManager->IsCombatActive());
		TestEqual(TEXT("Hero bar reset"), BuildupManager->GetStatusBarBuildup(Hero), 0.0f);
		TestEqual(TEXT("Foe bar reset"), BuildupManager->GetStatusBarBuildup(Foe), 0.0f);
		TestEqual(TEXT("Hero effects cleared"), EffectManager->GetEffectCount(Hero), 0);
		TestFalse(TEXT("Hero OnDied unbound"), Hero->FindComponentByClass<UCharacterDataComponent>()->OnDied.IsBound());
		TestFalse(TEXT("Foe OnDied unbound"), FoeComp->OnDied.IsBound());

		FCombatGridPosition Position;
		UCombatGridSubsystem *Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		TestFalse(TEXT("Grid cleared"), Grid && Grid->GetActorPosition(Hero, Position));
	});

	It("passes the turn when an AI actor submits an invalid action", [this]()
	{
		if (!StartDuel(/*bHeroIsAI=*/true))
		{
			return;
		}
		TestTrue(TEXT("Hero is AI-routed"), Orchestrator->IsActorAIControlled(Hero));
		TestTrue(TEXT("Turn 1 is Hero's"), Orchestrator->GetCurrentActor() == Hero);

		Orchestrator->SubmitActionAsync(MakeInvalidAction(Foe));

		TestTrue(TEXT("Turn passed to Foe"), Orchestrator->GetCurrentActor() == Foe);
		TestEqual(TEXT("Turn number"), Orchestrator->GetCurrentTurnNumber(), 2);
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
	});

	It("re-requests, without stranding the turn, when a player submits an invalid action", [this]()
	{
		// The debug auto-advance timer is armed only by RequestActionFromActor, so it is the
		// observable proof of a re-request: the invalid submit clears the turn-1 timer, and
		// only the re-request arms a new one.
		if (!Orchestrator)
		{
			return;
		}
		Orchestrator->bAutoAdvanceTurns = true;
		Orchestrator->AutoAdvanceDelay = TEST_AUTO_ADVANCE_DELAY;
		if (!StartDuel())
		{
			return;
		}
		TestTrue(TEXT("Turn 1 is Hero's"), Orchestrator->GetCurrentActor() == Hero);

		Orchestrator->SubmitActionAsync(MakeInvalidAction(Foe));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		TestEqual(TEXT("Turn number"), Orchestrator->GetCurrentTurnNumber(), 1);

		TestWorld->Tick(AUTO_ADVANCE_FRAMES);
		TestTrue(TEXT("Re-requested turn auto-advanced past turn 1"), Orchestrator->GetCurrentTurnNumber() > 1);
		TestTrue(TEXT("Combat still running"), Orchestrator->GetCombatState() == ECombatState::InProgress);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
