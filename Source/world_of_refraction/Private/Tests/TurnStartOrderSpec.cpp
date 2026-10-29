// TurnStartOrderSpec.cpp
// B-284: the turn-start order of operations (ET-17) and the order rituals fire in (ET-16), driven
// through a real ACombatOrchestrator on bare 1v1 combatants.
// ET-17: per-turn flags reset -> start-of-own-turn effects -> always-on effects -> "on turn start"
// triggers -> status-bar decay -> Broken Darkness overload tick; a death there ends the turn with
// no action. Each step is seen by its own signal (TurnStartOrderRecorder.h); a case fails if two
// steps swap. Steps 2-5 are pinned by the plain case, 2, 3, 5 and 6 by the Broken Darkness case
// (an overloaded holder's energy moves at turn start, so the trigger's energy payout can't be
// told apart there), so together they pin the whole chain.
// ET-16: rituals ready together fire faster frozen spell speed first, ties in arming order.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "TurnStartOrderRecorder.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 TURN_START_ORDER_HOT_ID = 28401;
	constexpr int32 TURN_START_ORDER_ALWAYS_ON_ID = 28402;
	constexpr int32 TURN_START_ORDER_TRIGGER_ID = 28403;
	/** The lethal burn deals this many times max HP, so a future flat mitigation can't make it survivable. */
	constexpr int32 TURN_START_ORDER_OVERKILL_FACTOR = 2;
	constexpr int32 TURN_START_ORDER_LETHAL_ID = 28404;
	/** Long enough that nothing the setup applies ends at the watched turn start. */
	constexpr int32 TURN_START_ORDER_EFFECT_TURNS = 5;
	constexpr float TURN_START_ORDER_HOT_HEAL = 1.0f;
	constexpr float TURN_START_ORDER_ALWAYS_ON_PERCENT = 10.0f;
	constexpr float TURN_START_ORDER_TRIGGER_EP = 1.0f;
	/** Status buildup well under the bar's threshold, so it decays rather than breaks. */
	constexpr float TURN_START_ORDER_BUILDUP = 20.0f;
	/** Energy pushed past MaxEP to overload a Broken Darkness holder. */
	constexpr int32 TURN_START_ORDER_OVERLOAD_EP = 5;

	constexpr int32 RITUAL_ORDER_DELAY_TURNS = 1;
	/** Spell speed points that lift the frozen spell speed above a 0-point caster's. */
	constexpr int32 RITUAL_ORDER_FAST_SPELL_SPEED = 50;
	constexpr int32 RITUAL_ORDER_FASTER_STILL_SPELL_SPEED = 90;
	constexpr int32 RITUAL_ORDER_FIRES_EXPECTED = 2;
	/** Turn ends to play for both Execution turns to come up and resolve. */
	constexpr int32 RITUAL_ORDER_MAX_STEPS = 6;
	/** Test-world ticks are 0.01s; spans the executor's 10s timeout (as CombatRitualFireSpec). */
	constexpr int32 RITUAL_ORDER_SETTLE_FRAMES = 1100;
}

BEGIN_DEFINE_SPEC(FTurnStartOrderSpec, "WoR.Combat.TurnStartOrder",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UTurnManager *TurnManager = nullptr;
	USkillEffectManager *EffectManager = nullptr;
	UStatusBuildupManager *BuildupManager = nullptr;
	UActionExecutor *Executor = nullptr;
	UTurnStartOrderRecorder *StepRecorder = nullptr;
	UActionExecutedRecorder *FireRecorder = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Starts the duel and ends Hero's opening turn, so Foe is current and Hero's next turn start is
	 *  the one watched. False (with the reason recorded) when the setup didn't hold. */
	bool StartDuelToFoesTurn()
	{
		if (!Orchestrator || !TurnManager || !EffectManager || !BuildupManager || !Hero || !Foe)
		{
			AddError(TEXT("Setup incomplete"));
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		if (!TestTrue(TEXT("Hero opens the duel"), Orchestrator->GetCurrentActor() == Hero))
		{
			return false;
		}
		Orchestrator->OnActionCompleted();
		return TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
	}

	/** On Foe's turn: a heal over time (start-of-own-turn), an always-on buff and status buildup on Hero,
	 *  each landing off Hero's turn so it first acts at Hero's next turn start. */
	void ApplyTurnStartEffects()
	{
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Turn start order HoT"), TURN_START_ORDER_HOT_ID,
			ESkillEffectType::HealthRestore, TURN_START_ORDER_HOT_HEAL, TURN_START_ORDER_EFFECT_TURNS), Foe);
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreatePersistent(TEXT("Turn start order always-on"), TURN_START_ORDER_ALWAYS_ON_ID,
			ESkillEffectType::DefenseBuff, TURN_START_ORDER_ALWAYS_ON_PERCENT), Foe);
		BuildupManager->AddStatusBuildup(Foe, Hero, TURN_START_ORDER_BUILDUP, ESpellElement::Fire, EPhysicalDamageType::None);
		TestTrue(TEXT("Hero's status bar has buildup to decay"), BuildupManager->GetStatusBarBuildup(Hero) > 0.0f);
	}

	/** Each step's first signal comes after the one before it. */
	void ExpectInOrder(const TArray<ETurnStartStep> &Expected)
	{
		for (int32 Index = 1; Index < Expected.Num(); ++Index)
		{
			const int32 Before = StepRecorder->FirstIndexOf(Expected[Index - 1]);
			const int32 After = StepRecorder->FirstIndexOf(Expected[Index]);
			TestTrue(FString::Printf(TEXT("The %s came"), GetTurnStartStepName(Expected[Index - 1])), Before != INDEX_NONE);
			TestTrue(FString::Printf(TEXT("The %s comes after the %s"), GetTurnStartStepName(Expected[Index]),
									 GetTurnStartStepName(Expected[Index - 1])),
					 Before != INDEX_NONE && After > Before);
		}
	}

	/** Arms a Defend ritual for Caster as the executor does at arm time. */
	void ArmRitual(AActor *Caster)
	{
		FAction Defend;
		Defend.ActionType = EActionType::Defend;
		Executor->OnActionDeferredArmed.Broadcast(Caster, Defend, RITUAL_ORDER_DELAY_TURNS);
	}

	/** Ends normal turns until both rituals have fired (or the cap). Execution turns resolve themselves. */
	void PlayUntilBothRitualsFired()
	{
		for (int32 Step = 0; Step < RITUAL_ORDER_MAX_STEPS && FireRecorder->Entries.Num() < RITUAL_ORDER_FIRES_EXPECTED &&
							 Orchestrator->GetCombatState() == ECombatState::InProgress;
			 ++Step)
		{
			if (TurnManager->GetCurrentTurnType() != ETurnType::Execution)
			{
				Orchestrator->OnActionCompleted();
			}
			TestWorld->Tick(RITUAL_ORDER_SETTLE_FRAMES);
		}
	}

	void ExpectFireOrder(AActor *First, AActor *Second)
	{
		if (!TestEqual(TEXT("Both rituals fired"), FireRecorder->Entries.Num(), RITUAL_ORDER_FIRES_EXPECTED))
		{
			return;
		}
		TestTrue(TEXT("The expected caster fired first"), FireRecorder->Entries[0].Actor.Get() == First);
		TestTrue(TEXT("The other fired second"), FireRecorder->Entries[1].Actor.Get() == Second);
	}

	/** Spawns Hero and Foe and starts the duel on Hero's turn, with the fire recorder listening. */
	bool StartRitualDuel(int32 HeroSpellSpeed, int32 FoeSpellSpeed)
	{
		UCharacterData *HeroData = FWoRTestWorld::NewCharacterData();
		HeroData->SpellSpeed = HeroSpellSpeed;
		UCharacterData *FoeData = FWoRTestWorld::NewCharacterData();
		FoeData->SpellSpeed = FoeSpellSpeed;
		Hero = TestWorld->SpawnCombatant(HeroData);
		Foe = TestWorld->SpawnCombatant(FoeData);
		if (!Orchestrator || !TurnManager || !Executor || !TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		FireRecorder = NewObject<UActionExecutedRecorder>();
		FireRecorder->AddToRoot();
		FireRecorder->Listen(Orchestrator);
		return TestTrue(TEXT("Hero opens the duel"), Orchestrator->GetCurrentActor() == Hero);
	}
END_DEFINE_SPEC(FTurnStartOrderSpec)

void FTurnStartOrderSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		BuildupManager = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("TurnManager"), TurnManager);
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("StatusBuildupManager"), BuildupManager);
		TestNotNull(TEXT("ActionExecutor"), Executor);
		StepRecorder = NewObject<UTurnStartOrderRecorder>();
		StepRecorder->AddToRoot();
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		StepRecorder->RemoveFromRoot();
		StepRecorder = nullptr;
		if (FireRecorder)
		{
			FireRecorder->RemoveFromRoot();
			FireRecorder = nullptr;
		}
		Orchestrator = nullptr;
		TurnManager = nullptr;
		EffectManager = nullptr;
		BuildupManager = nullptr;
		Executor = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("Turn start (ET-17)", [this]()
	{
		It("resets the turn's flags, then runs own-turn effects, always-on effects, triggers and bar decay, then asks for the action", [this]()
		{
			const ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
			Hero = Loadout ? Loadout->GetOwner() : nullptr;
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!StartDuelToFoesTurn())
			{
				return;
			}
			ApplyTurnStartEffects();
			// The trigger is applied before anything else can move Hero's energy at its turn start.
			FActiveSkillEffect Trigger = FActiveSkillEffect::CreatePersistent(TEXT("Turn start order trigger"), TURN_START_ORDER_TRIGGER_ID,
				ESkillEffectType::EnergyRestore, TURN_START_ORDER_TRIGGER_EP);
			Trigger.ProcessTiming = ESkillEffectTiming::OnTrigger;
			Trigger.TriggerCondition = ESkillTrigger::OnTurnStart;
			EffectManager->ApplyEffect(Hero, Trigger, Hero);
			// Use Hero's free switch now, so only the turn-start flag reset can give it back.
			ULoadoutComponent *HeroLoadout = Hero->FindComponentByClass<ULoadoutComponent>();
			if (!TestNotNull(TEXT("Hero's loadout"), HeroLoadout))
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("Hero switches weapon"), HeroLoadout->TrySwitchWeaponThisTurn(Reason));
			if (!TestFalse(TEXT("The free switch is used"), HeroLoadout->GetTurnSwitchRefusedReason().IsEmpty()))
			{
				return;
			}

			StepRecorder->Listen(Hero, TURN_START_ORDER_HOT_ID, TURN_START_ORDER_ALWAYS_ON_ID, true, Orchestrator, EffectManager, BuildupManager, nullptr);
			Orchestrator->OnActionCompleted();

			TestTrue(TEXT("Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
			TestTrue(TEXT("Step 1: the free switch was back before the own-turn effects ran"), StepRecorder->bSwitchFreeAtOwnTurnStartEffect);
			ExpectInOrder({ETurnStartStep::OwnTurnStartEffect, ETurnStartStep::AlwaysOnEffect, ETurnStartStep::TurnStartTrigger,
						   ETurnStartStep::BarDecay, ETurnStartStep::TurnStartedBroadcast, ETurnStartStep::ActionRequested});
		});

		It("ticks the Broken Darkness overload after the bar decay and before the action", [this]()
		{
			UCharacterData *HeroData = FWoRTestWorld::NewCharacterData();
			HeroData->bBrokenDarknessInnate = true;
			Hero = TestWorld->SpawnCombatant(HeroData);
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			UCharacterDataComponent *HeroComp = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestTrue(TEXT("Born Broken Darkness"), HeroComp && HeroComp->IsBrokenDarkness() && HeroComp->MaxEP > 0))
			{
				return;
			}
			UBrokenDarknessManager *BrokenDarkness = FWoRTestWorld::AddComponent<UBrokenDarknessManager>(Hero);
			BrokenDarkness->InitializeBornBrokenDarkness();
			if (!StartDuelToFoesTurn())
			{
				return;
			}
			ApplyTurnStartEffects();
			HeroComp->CurrentEP = HeroComp->MaxEP;
			HeroComp->ServerGainBrokenDarknessEnergy(TURN_START_ORDER_OVERLOAD_EP, HeroComp->MaxEP + TURN_START_ORDER_OVERLOAD_EP);
			if (!TestTrue(TEXT("Hero is overloaded"), BrokenDarkness->IsOverloaded()))
			{
				return;
			}

			StepRecorder->Listen(Hero, TURN_START_ORDER_HOT_ID, TURN_START_ORDER_ALWAYS_ON_ID, false, Orchestrator, EffectManager, BuildupManager,
						  BrokenDarkness);
			Orchestrator->OnActionCompleted();

			TestTrue(TEXT("Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
			ExpectInOrder({ETurnStartStep::OwnTurnStartEffect, ETurnStartStep::AlwaysOnEffect, ETurnStartStep::BarDecay,
						   ETurnStartStep::OverloadTick, ETurnStartStep::TurnStartedBroadcast, ETurnStartStep::ActionRequested});
		});

		It("ends the turn with no action when a turn-start effect kills the actor", [this]()
		{
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			UCharacterDataComponent *HeroComp = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("Hero data"), HeroComp) || !StartDuelToFoesTurn())
			{
				return;
			}
			FActiveSkillEffect Burn = FActiveSkillEffect::CreateDOT(TEXT("Turn start order lethal burn"), TURN_START_ORDER_LETHAL_ID,
																	 HeroComp->MaxHP * TURN_START_ORDER_OVERKILL_FACTOR,
																	 TURN_START_ORDER_EFFECT_TURNS, ESpellElement::None);
			Burn.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
			EffectManager->ApplyEffect(Hero, Burn, Foe);
			TestTrue(TEXT("Alive until the turn starts"), HeroComp->bIsAlive);

			StepRecorder->Listen(Hero, TURN_START_ORDER_LETHAL_ID, INDEX_NONE, false, Orchestrator, EffectManager, BuildupManager, nullptr);
			Orchestrator->OnActionCompleted();

			TestTrue(TEXT("The burn ran at the turn start"), StepRecorder->FirstIndexOf(ETurnStartStep::OwnTurnStartEffect) != INDEX_NONE);
			TestFalse(TEXT("Hero died at the turn start"), HeroComp->bIsAlive);
			TestEqual(TEXT("No turn-started broadcast for the dead"), StepRecorder->FirstIndexOf(ETurnStartStep::TurnStartedBroadcast), INDEX_NONE);
			TestEqual(TEXT("No action asked of the dead"), StepRecorder->FirstIndexOf(ETurnStartStep::ActionRequested), INDEX_NONE);
			TestTrue(TEXT("The fight is over"), Orchestrator->GetCombatState() != ECombatState::InProgress);
		});
	});

	Describe("Ritual order (ET-16)", [this]()
	{
		It("fires the faster ritual first when two are ready together", [this]()
		{
			if (!StartRitualDuel(0, RITUAL_ORDER_FAST_SPELL_SPEED))
			{
				return;
			}
			ArmRitual(Hero); // armed first, so arming order alone would fire it first
			ArmRitual(Foe);
			PlayUntilBothRitualsFired();
			ExpectFireOrder(Foe, Hero);
		});

		It("uses the spell speed frozen at arming", [this]()
		{
			if (!StartRitualDuel(0, RITUAL_ORDER_FAST_SPELL_SPEED))
			{
				return;
			}
			ArmRitual(Hero);
			ArmRitual(Foe);
			// After arming, Hero becomes the faster caster; a speed read at fire time would flip the order.
			UCharacterDataComponent *HeroComp = Hero->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("Hero data"), HeroComp))
			{
				return;
			}
			HeroComp->CharacterData->SpellSpeed = RITUAL_ORDER_FASTER_STILL_SPELL_SPEED;
			PlayUntilBothRitualsFired();
			ExpectFireOrder(Foe, Hero);
		});

		It("fires equal spell speeds in arming order", [this]()
		{
			if (!StartRitualDuel(0, 0))
			{
				return;
			}
			ArmRitual(Foe); // the enemy arms first: neither side nor slot may put Hero ahead
			ArmRitual(Hero);
			PlayUntilBothRitualsFired();
			ExpectFireOrder(Foe, Hero);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
