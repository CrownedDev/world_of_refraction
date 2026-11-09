// ActionEffectsSpec.cpp
// B-170d (ActionPips.md §1, §3.4, Q-170f, Q-170g): effects that give or take banked actions. Gain / Lose Action land
// now or at a coming holder turn start and are never stored; a gain may pass Max and stays banked; a loss never goes
// below 0; Max Actions buffs and debuffs move Max (never below the floor); waiting changes land gains first, then
// losses (Q-170d2). The turn-start order with the switch on: the regain, then the waiting changes, then the check, so
// a turn left at 0 is skipped without a prompt. Emerald's ally ladder and its enemy skip.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "TurnStartOrderRecorder.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	const TCHAR *const ACTION_EFFECTS_SWITCH = TEXT("wor.Turns.ActionPips");
	constexpr int32 ACTION_EFFECTS_ON = 1;
	constexpr int32 ACTION_EFFECTS_OFF = 0;
	/** A default character: no Reflex (regain 1), no action points (Max 2). */
	constexpr int32 ACTION_EFFECTS_REGAIN = 1;
	constexpr int32 ACTION_EFFECTS_MAX = 2;
	constexpr int32 ACTION_EFFECTS_ONE = 1;
	constexpr int32 ACTION_EFFECTS_TWO = 2;
	constexpr int32 ACTION_EFFECTS_BIG_LOSS = 5;
	constexpr int32 ACTION_EFFECTS_BUFF_TURNS = 2;
	constexpr int32 ACTION_EFFECTS_ID = 917001;

	// Crown's Emerald ladder (Q-170f), F..S, written out so a changed constant fails here.
	constexpr int32 EMERALD_LADDER_GAINED[7] = {1, 1, 1, 2, 2, 2, 3};
	constexpr int32 EMERALD_LADDER_DELAY[7] = {2, 1, 0, 2, 1, 0, 0};
	const EItemTier EMERALD_LADDER_TIERS[7] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
											   EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
}

BEGIN_DEFINE_SPEC(FActionEffectsSpec, "WoR.Combat.ActionEffects",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UTurnManager *TurnManager = nullptr;
	ACombatOrchestrator *Orchestrator = nullptr;
	UTurnStartOrderRecorder *StepRecorder = nullptr;
	IConsoleVariable *PipsSwitch = nullptr;
	int32 SwitchBefore = ACTION_EFFECTS_OFF;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	void SetSwitch(int32 Value)
	{
		if (PipsSwitch)
		{
			PipsSwitch->Set(Value, ECVF_SetByConsole);
		}
	}

	UCharacterDataComponent *DataOf(AActor *Actor) const
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	/** An effect of Type moving Amount, landing now (Instant) or, with TurnsFromNow > 0, at that holder turn start. */
	FActiveSkillEffect ActionEffect(ESkillEffectType Type, int32 Amount, int32 TurnsFromNow = 0)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Action effect"), ACTION_EFFECTS_ID, Type, Amount, TurnsFromNow);
		Effect.ProcessTiming = TurnsFromNow > 0 ? ESkillEffectTiming::StartOfOwnTurn : ESkillEffectTiming::Instant;
		return Effect;
	}

	/** Hero with an empty bank (no fight: the bank starts at 0). */
	UCharacterDataComponent *SpawnHero()
	{
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UCharacterDataComponent *Data = DataOf(Hero);
		TestNotNull(TEXT("Hero data"), Data);
		return Data;
	}

	/** The duel through a real orchestrator with the switch on, Hero's turn first, the recorder on Hero. */
	bool StartDuel()
	{
		SetSwitch(ACTION_EFFECTS_ON);
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator) || !TestTrue(TEXT("Combatants"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		StepRecorder = NewObject<UTurnStartOrderRecorder>();
		StepRecorder->AddToRoot();
		StepRecorder->Listen(Hero, INDEX_NONE, INDEX_NONE, false, Orchestrator, EffectManager, TestWorld->GetSubsystem<UStatusBuildupManager>(), nullptr);
		return TestTrue(TEXT("Hero opens"), Orchestrator->GetCurrentActor() == Hero) &&
			   TestTrue(TEXT("An action-pips fight"), Orchestrator->IsActionPipsFight()) &&
			   TestEqual(TEXT("A fight starts with the regain"), DataOf(Hero)->CurrentActions, ACTION_EFFECTS_REGAIN);
	}

	/** Hero ends its turn, then Foe ends its own, so Hero's next turn starts. */
	void PassToHerosNextTurn()
	{
		FAction EndTurn;
		EndTurn.ActionType = EActionType::EndTurn;
		Orchestrator->SubmitActionAsync(EndTurn);
		TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
		Orchestrator->SubmitActionAsync(EndTurn);
	}

	int32 CountSteps(ETurnStartStep Step) const
	{
		return StepRecorder->Steps.FilterByPredicate([Step](ETurnStartStep Each) { return Each == Step; }).Num();
	}
END_DEFINE_SPEC(FActionEffectsSpec)

void FActionEffectsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("TurnManager"), TurnManager);
		PipsSwitch = IConsoleManager::Get().FindConsoleVariable(ACTION_EFFECTS_SWITCH);
		TestNotNull(TEXT("The switch exists"), PipsSwitch);
		SwitchBefore = PipsSwitch ? PipsSwitch->GetInt() : ACTION_EFFECTS_OFF;
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		if (StepRecorder)
		{
			StepRecorder->RemoveFromRoot();
		}
		StepRecorder = nullptr;
		SetSwitch(SwitchBefore);
		PipsSwitch = nullptr;
		Orchestrator = nullptr;
		Hero = Foe = nullptr;
		EffectManager = nullptr;
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	Describe("Gain / Lose Action", [this]()
	{
		It("gains now past Max, and the regain then adds nothing", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data || !EffectManager) return;
			Data->GainActions(ACTION_EFFECTS_MAX, /*bMayPassMax=*/false);
			TestTrue(TEXT("Applied"), EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::GainActions, ACTION_EFFECTS_ONE), Hero) ==
										  EEffectApplicationResult::Applied);
			TestEqual(TEXT("Past Max"), Data->CurrentActions, ACTION_EFFECTS_MAX + ACTION_EFFECTS_ONE);
			TestFalse(TEXT("Never stored"), EffectManager->HasEffectOfType(Hero, ESkillEffectType::GainActions));
			Data->RegainActions();
			TestEqual(TEXT("The regain adds nothing above Max; the extra stays banked"), Data->CurrentActions, ACTION_EFFECTS_MAX + ACTION_EFFECTS_ONE);
		});

		It("loses now, never below 0", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data || !EffectManager) return;
			Data->GainActions(ACTION_EFFECTS_ONE, false);
			EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::LoseActions, ACTION_EFFECTS_BIG_LOSS), Hero);
			TestEqual(TEXT("At 0"), Data->CurrentActions, 0);
			TestFalse(TEXT("Never stored"), EffectManager->HasEffectOfType(Hero, ESkillEffectType::LoseActions));
		});

		It("waits for the holder's next turn start, or the one after", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data || !EffectManager) return;
			EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::GainActions, ACTION_EFFECTS_ONE, 1), Hero);
			EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::GainActions, ACTION_EFFECTS_TWO, 2), Hero);
			TestEqual(TEXT("Nothing yet"), Data->CurrentActions, 0);
			TestEqual(TEXT("Two waiting"), Data->GetWaitingActionChanges().Num(), 2);
			TestEqual(TEXT("One lands at the next turn start"), Data->ApplyDueActionChanges(), 1);
			TestEqual(TEXT("+1"), Data->CurrentActions, ACTION_EFFECTS_ONE);
			TestEqual(TEXT("The other at the one after"), Data->ApplyDueActionChanges(), 1);
			TestEqual(TEXT("+2 more, past Max"), Data->CurrentActions, ACTION_EFFECTS_ONE + ACTION_EFFECTS_TWO);
			TestEqual(TEXT("Nothing left waiting"), Data->GetWaitingActionChanges().Num(), 0);
		});

		It("plays +1 now, -1 next turn as one skill's two rows (Adrenaline)", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data || !EffectManager) return;
			Data->GainActions(ACTION_EFFECTS_ONE, false);
			EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::GainActions, ACTION_EFFECTS_ONE), Hero);
			EffectManager->ApplyEffect(Hero, ActionEffect(ESkillEffectType::LoseActions, ACTION_EFFECTS_ONE, 1), Hero);
			TestEqual(TEXT("+1 now"), Data->CurrentActions, ACTION_EFFECTS_TWO);
			Data->ApplyDueActionChanges();
			TestEqual(TEXT("-1 at the next turn start"), Data->CurrentActions, ACTION_EFFECTS_ONE);
		});

		It("lands waiting gains before waiting losses (Q-170d2)", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data) return;
			Data->QueueActionChange(-ACTION_EFFECTS_ONE, 1, false, TEXT("loss"));
			Data->QueueActionChange(ACTION_EFFECTS_ONE, 1, true, TEXT("gain"));
			Data->ApplyDueActionChanges();
			TestEqual(TEXT("The loss takes the gain: 0 + 1 - 1"), Data->CurrentActions, 0);
			TestTrue(TEXT("wor.Actions.Print names nothing waiting"), Data->GetWaitingActionChangesString() == TEXT("none"));
		});
	});

	Describe("Max Actions buff / debuff", [this]()
	{
		It("raises and lowers Max, never below the floor", [this]()
		{
			UCharacterDataComponent *Data = SpawnHero();
			if (!Data || !EffectManager) return;
			TestEqual(TEXT("Max 2"), Data->GetMaxActions(), ACTION_EFFECTS_MAX);
			FActiveSkillEffect Up = FActiveSkillEffect::CreateBuff(TEXT("Max up"), ACTION_EFFECTS_ID, ESkillEffectType::MaxActionsBuff, ACTION_EFFECTS_ONE,
																   ACTION_EFFECTS_BUFF_TURNS);
			Up.ProcessTiming = ESkillEffectTiming::Persistent;
			EffectManager->ApplyEffect(Hero, Up, Hero);
			TestEqual(TEXT("Max 3"), Data->GetMaxActions(), ACTION_EFFECTS_MAX + ACTION_EFFECTS_ONE);

			FActiveSkillEffect Down = FActiveSkillEffect::CreateBuff(TEXT("Max down"), ACTION_EFFECTS_ID + 1, ESkillEffectType::MaxActionsDebuff,
																	 ACTION_EFFECTS_BIG_LOSS, ACTION_EFFECTS_BUFF_TURNS);
			Down.ProcessTiming = ESkillEffectTiming::Persistent;
			EffectManager->ApplyEffect(Hero, Down, Hero);
			TestEqual(TEXT("Never below the floor"), Data->GetMaxActions(), ActionPipsConstants::MAX_ACTIONS_FLOOR);
		});
	});

	Describe("The turn-start order with the switch on (ActionPips.md §1)", [this]()
	{
		It("skips a turn a waiting loss leaves at 0: no prompt, and the turn-end step still runs", [this]()
		{
			if (!StartDuel()) return;
			DataOf(Hero)->QueueActionChange(-ACTION_EFFECTS_BIG_LOSS, 1, false, TEXT("test loss"));
			const int32 TurnsBefore = TurnManager->GetTurnCount();
			PassToHerosNextTurn();
			TestEqual(TEXT("The regain came first, then the loss took all"), DataOf(Hero)->CurrentActions, 0);
			TestTrue(TEXT("Skipped: Foe's turn again"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("Hero was never asked"), CountSteps(ETurnStartStep::ActionRequested), 0);
			TestEqual(TEXT("Nor shown a turn start"), CountSteps(ETurnStartStep::TurnStartedBroadcast), 0);
			TestEqual(TEXT("Hero's skipped turn ended like any (three turns ended)"), TurnManager->GetTurnCount(), TurnsBefore + 3);
		});

		It("plays a turn a waiting loss leaves at 1, with 1", [this]()
		{
			if (!StartDuel()) return;
			DataOf(Hero)->QueueActionChange(-ACTION_EFFECTS_ONE, 1, false, TEXT("test loss"));
			PassToHerosNextTurn();
			TestTrue(TEXT("Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("Regain to 2, then -1"), DataOf(Hero)->CurrentActions, ACTION_EFFECTS_REGAIN + ACTION_EFFECTS_REGAIN - ACTION_EFFECTS_ONE);
			TestEqual(TEXT("Asked once"), CountSteps(ETurnStartStep::ActionRequested), 1);
		});

		It("counts a delayed gain landing with a loss before the check", [this]()
		{
			if (!StartDuel()) return;
			UCharacterDataComponent *Data = DataOf(Hero);
			Data->LoseActions(Data->CurrentActions); // Hero's next turn regains to 1
			Data->QueueActionChange(ACTION_EFFECTS_ONE, 1, true, TEXT("test gain"));
			Data->QueueActionChange(-ACTION_EFFECTS_TWO, 1, false, TEXT("test loss"));
			PassToHerosNextTurn();
			TestEqual(TEXT("1 regained + 1 gained - 2 lost"), Data->CurrentActions, 0);
			TestTrue(TEXT("So the turn is skipped"), Orchestrator->GetCurrentActor() == Foe);
		});
	});

	Describe("Emerald with the switch on (Q-170f)", [this]()
	{
		BeforeEach([this]()
		{
			SetSwitch(ACTION_EFFECTS_ON);
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (TurnManager && Hero && Foe)
			{
				TurnManager->InitializeCombat({Hero}, {Foe});
			}
		});

		It("grants yourself actions by tier: now, next turn or the one after, past Max", [this]()
		{
			UItemExecutor *Items = TestWorld->GetSubsystem<UItemExecutor>();
			UCharacterDataComponent *Data = DataOf(Hero);
			if (!TestNotNull(TEXT("ItemExecutor"), Items) || !TestNotNull(TEXT("Hero data"), Data) ||
				!TestTrue(TEXT("Rotation fight"), TurnManager->IsRotationMode()))
			{
				return;
			}
			for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(EMERALD_LADDER_TIERS)); ++Index)
			{
				const FCrystalId Emerald(ECrystalType::Emerald, EMERALD_LADDER_TIERS[Index]);
				const FString Tier = FString::Printf(TEXT(" (tier %d)"), Index);
				TestEqual(TEXT("Gained") + Tier, CrystalEffectTable::GetEmeraldAllyActionsGained(Emerald), EMERALD_LADDER_GAINED[Index]);
				TestEqual(TEXT("Delay") + Tier, CrystalEffectTable::GetEmeraldAllyGainDelay(Emerald), EMERALD_LADDER_DELAY[Index]);

				Data->ResetActionsForFight(); // the regain, nothing waiting
				const int32 Before = Data->CurrentActions;
				TestTrue(TEXT("Used") + Tier, Items->UseItem(Hero, Emerald, Hero).bSuccess);
				for (int32 Turn = 0; Turn < EMERALD_LADDER_DELAY[Index]; ++Turn)
				{
					TestEqual(TEXT("Not landed yet") + Tier, Data->CurrentActions, Before);
					Data->ApplyDueActionChanges();
				}
				TestEqual(TEXT("Landed, past Max where it goes past") + Tier, Data->CurrentActions, Before + EMERALD_LADDER_GAINED[Index]);
			}
			TestFalse(TEXT("No bonus turn granted"), TurnManager->GetCurrentTurnType() == ETurnType::Bonus);
		});

		It("takes an enemy's next turn", [this]()
		{
			UItemExecutor *Items = TestWorld->GetSubsystem<UItemExecutor>();
			if (!TestNotNull(TEXT("ItemExecutor"), Items) || !TestTrue(TEXT("Hero's turn"), TurnManager->GetCurrentActor() == Hero))
			{
				return;
			}
			TestTrue(TEXT("Used on Foe"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::F_Tier), Foe).bSuccess);
			TurnManager->AdvanceToNextTurn();
			TestTrue(TEXT("Foe's turn is gone: Hero again"), TurnManager->GetCurrentActor() == Hero);
			TestEqual(TEXT("Foe gained nothing"), DataOf(Foe)->CurrentActions, 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
