// MultiActionTurnSpec.cpp
// B-170c (ActionPips.md §1, §3.3, Q-170b, Q-170c): with wor.Turns.ActionPips 1 a turn spends banked actions.
// Each skill, spell or item costs its ActionCost; the turn goes on until the bank is 0, End Turn or Defend;
// a cost above Max Actions is refused, one above the bank waits; arming a ritual spends its cost and the turn
// goes on; switching is free; bonus actions above Max are lost at turn end (Q-170f2). The turn-start and turn-end
// steps run once per turn. At 0 one action ends the turn, as built. Driven through a real ACombatOrchestrator on bare 1v1 combatants (both player-driven, so
// nothing acts on its own). The "0 actions at turn start skips the turn" cases need B-170d's waiting losses
// (nothing can empty a bank at turn start before them); they land with ActionEffectsSpec. B-430: a kill shot with
// actions still banked wins the fight at once and asks for no second action.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "TurnStartOrderRecorder.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	const TCHAR *const MULTI_ACTION_TEST_SWITCH = TEXT("wor.Turns.ActionPips");
	constexpr int32 MULTI_ACTION_TEST_ON = 1;
	constexpr int32 MULTI_ACTION_TEST_OFF = 0;
	/** Test-world ticks are 0.01s; spans the executor's 10s timeout, which covers the defense window's 8s
	 *  failsafe a montage-less ability waits on (as CombatRitualFireSpec). */
	constexpr int32 MULTI_ACTION_SETTLE_FRAMES = 1100;
	/** A default character: no Reflex (regain 1), no action points (Max 2). */
	constexpr int32 MULTI_ACTION_REGAIN = 1;
	constexpr int32 MULTI_ACTION_MAX = 2;
	constexpr int32 MULTI_ACTION_COST_ONE = 1;
	constexpr int32 MULTI_ACTION_COST_TWO = 2;
	constexpr int32 MULTI_ACTION_COST_ABOVE_MAX = 3;
	/** Q-170f2: bonus actions given past Max (an Emerald, a Gain Action effect). */
	constexpr int32 MULTI_ACTION_BONUS = 2;
	/** Q-170f3: a Max Actions Down of 1 on Foe, lasting past its next turn end (Max 2 -> 1). */
	constexpr int32 MULTI_ACTION_MAX_DOWN = 1;
	constexpr int32 MULTI_ACTION_MAX_DOWN_TURNS = 3;
	constexpr int32 MULTI_ACTION_MAX_DOWN_ID = 9170;
	constexpr int32 MULTI_ACTION_RITUAL_DELAY = 1;
	/** B-430: a hit far above the foe's 1 HP, so the first action kills it. */
	constexpr int32 MULTI_ACTION_KILL_DAMAGE = 100000;
	constexpr int32 MULTI_ACTION_FOE_LAST_HP = 1;
	/** Turns of preview to search for the armed ritual's Execution turn. */
	constexpr int32 MULTI_ACTION_PREVIEW_TURNS = 6;
}

BEGIN_DEFINE_SPEC(FMultiActionTurnSpec, "WoR.Turns.MultiAction",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UTurnManager *TurnManager = nullptr;
	UActionExecutedRecorder *Recorder = nullptr;
	UTurnStartOrderRecorder *StepRecorder = nullptr;
	IConsoleVariable *PipsSwitch = nullptr;
	int32 SwitchBefore = MULTI_ACTION_TEST_OFF;
	TArray<UAbilityData *> Abilities;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	void SetSwitch(int32 Value)
	{
		if (PipsSwitch)
		{
			PipsSwitch->Set(Value, ECVF_SetByConsole); // console priority, so an earlier typed value can't win
		}
	}

	UCharacterDataComponent *DataOf(AActor *Actor) const
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	int32 ActionsOf(AActor *Actor) const
	{
		const UCharacterDataComponent *Data = DataOf(Actor);
		return Data ? Data->CurrentActions : INDEX_NONE;
	}

	/** A montage-less, zero-damage, zero-Energy ability costing Cost actions (armed for Delay turns when > 0). */
	UAbilityData *NewAbility(int32 Cost, int32 Delay = 0)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = FString::Printf(TEXT("Cost %d ability"), Cost);
		Ability->ActionCost = Cost;
		Ability->ActivationDelay = Delay;
		Ability->AddToRoot();
		Abilities.Add(Ability);
		return Ability;
	}

	FAction AbilityOn(AActor *Target, UAbilityData *Ability) const
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.Targets.Add(Target);
		return Action;
	}

	static FAction Plain(EActionType Type)
	{
		FAction Action;
		Action.ActionType = Type;
		return Action;
	}

	void SubmitAndSettle(const FAction &Action)
	{
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(MULTI_ACTION_SETTLE_FRAMES);
	}

	/** Spawns the duel (Hero with a loadout when asked), sets the switch and starts the fight on Hero's turn,
	 *  with both recorders listening. False (recorded) when the setup didn't hold. */
	bool StartDuel(int32 SwitchValue, bool bHeroLoadout = false)
	{
		if (!Orchestrator || !TurnManager || !TestNotNull(TEXT("the switch exists"), PipsSwitch))
		{
			AddError(TEXT("Setup incomplete"));
			return false;
		}
		SetSwitch(SwitchValue);
		if (bHeroLoadout)
		{
			const ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
			Hero = Loadout ? Loadout->GetOwner() : nullptr;
		}
		else
		{
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		}
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe});
		Recorder = NewObject<UActionExecutedRecorder>();
		Recorder->AddToRoot();
		Recorder->Listen(Orchestrator);
		StepRecorder = NewObject<UTurnStartOrderRecorder>();
		StepRecorder->AddToRoot();
		StepRecorder->Listen(Hero, INDEX_NONE, INDEX_NONE, false, Orchestrator, TestWorld->GetSubsystem<USkillEffectManager>(),
							 TestWorld->GetSubsystem<UStatusBuildupManager>(), nullptr);
		if (!TestTrue(TEXT("Hero opens the duel"), Orchestrator->GetCurrentActor() == Hero))
		{
			return false;
		}
		return TestEqual(TEXT("the fight runs the mode the switch asked for"), Orchestrator->IsActionPipsFight(),
						 SwitchValue == MULTI_ACTION_TEST_ON);
	}

	/** Gives Hero a second action (fight start = the regain, 1), for the cases that need two. */
	bool GiveHeroTwoActions()
	{
		UCharacterDataComponent *Data = DataOf(Hero);
		if (!TestNotNull(TEXT("Hero data"), Data) || !TestEqual(TEXT("A fight starts with the regain"), Data->CurrentActions, MULTI_ACTION_REGAIN))
		{
			return false;
		}
		Data->GainActions(MULTI_ACTION_MAX - MULTI_ACTION_REGAIN, /*bMayPassMax=*/false);
		return TestEqual(TEXT("Hero holds Max"), Data->CurrentActions, MULTI_ACTION_MAX);
	}

	int32 CountSteps(ETurnStartStep Step) const
	{
		return StepRecorder->Steps.FilterByPredicate([Step](ETurnStartStep Each) { return Each == Step; }).Num();
	}

	int32 CountHeroActions(EActionType Type) const
	{
		return Recorder->Entries.FilterByPredicate([this, Type](const UActionExecutedRecorder::FEntry &Entry)
		{
			return Entry.Actor.Get() == Hero && Entry.ActionType == Type;
		}).Num();
	}
END_DEFINE_SPEC(FMultiActionTurnSpec)

void FMultiActionTurnSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("TurnManager"), TurnManager);
		PipsSwitch = IConsoleManager::Get().FindConsoleVariable(MULTI_ACTION_TEST_SWITCH);
		SwitchBefore = PipsSwitch ? PipsSwitch->GetInt() : MULTI_ACTION_TEST_OFF;
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		for (UObject *Recorded : TArray<UObject *>{Recorder, StepRecorder})
		{
			if (Recorded)
			{
				Recorded->RemoveFromRoot();
			}
		}
		Recorder = nullptr;
		StepRecorder = nullptr;
		for (UAbilityData *Ability : Abilities)
		{
			Ability->RemoveFromRoot();
		}
		Abilities.Reset();
		SetSwitch(SwitchBefore);
		PipsSwitch = nullptr;
		Orchestrator = nullptr;
		TurnManager = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("With the switch off (wor.Turns.ActionPips 0)", [this]()
	{
		It("ends the turn after one action, as built", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_OFF))
			{
				return;
			}
			SubmitAndSettle(AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ONE)));
			TestEqual(TEXT("Hero acted once"), CountHeroActions(EActionType::Ability), 1);
			TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("No bank is kept"), ActionsOf(Hero), 0);
			TestTrue(TEXT("No cost refusal outside an action-pips fight"),
					 Orchestrator->GetActionCostRefusedReason(Hero, AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ABOVE_MAX))).IsEmpty());
		});

		It("refuses End Turn outside an action-pips fight (B-170e review)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_OFF))
			{
				return;
			}
			const int32 TurnCountBefore = TurnManager->GetTurnCount();
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn)); // logs a refusal warning
			TestTrue(TEXT("Still Hero's turn (asked again)"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("No turn ended"), TurnManager->GetTurnCount(), TurnCountBefore);
		});
	});

	Describe("With the switch on (wor.Turns.ActionPips 1)", [this]()
	{
		It("plays two cost-1 actions in one turn, then ends it at 0", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			UAbilityData *Ability = NewAbility(MULTI_ACTION_COST_ONE);
			const int32 TurnCountBefore = TurnManager->GetTurnCount();

			SubmitAndSettle(AbilityOn(Foe, Ability));
			TestTrue(TEXT("Still Hero's turn after the first action"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("One action left"), ActionsOf(Hero), MULTI_ACTION_MAX - MULTI_ACTION_COST_ONE);
			TestEqual(TEXT("No turn-end step yet (no new turn)"), TurnManager->GetTurnCount(), TurnCountBefore);
			TestEqual(TEXT("Asked again"), CountSteps(ETurnStartStep::ActionRequested), 1);
			TestEqual(TEXT("Without a second turn start"), CountSteps(ETurnStartStep::TurnStartedBroadcast), 0);

			SubmitAndSettle(AbilityOn(Foe, Ability));
			TestEqual(TEXT("Hero acted twice"), CountHeroActions(EActionType::Ability), 2);
			TestEqual(TEXT("The bank is empty"), ActionsOf(Hero), 0);
			TestTrue(TEXT("At 0 the turn ended: Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("One turn ended"), TurnManager->GetTurnCount(), TurnCountBefore + 1);
			TestTrue(TEXT("wor.Actions.Print names Foe's turn now"), Orchestrator->GetTurnSpendsString().Contains(Foe->GetName()));
		});

		It("wins at once when the first of two banked actions kills the only enemy (B-430)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			UCharacterDataComponent *FoeData = DataOf(Foe);
			if (!TestNotNull(TEXT("Foe data"), FoeData))
			{
				return;
			}
			FoeData->CurrentHP = MULTI_ACTION_FOE_LAST_HP;
			UAbilityData *Killer = NewAbility(MULTI_ACTION_COST_ONE);
			Killer->BaseDamage = MULTI_ACTION_KILL_DAMAGE;

			SubmitAndSettle(AbilityOn(Foe, Killer));
			TestFalse(TEXT("The foe is down"), FoeData->bIsAlive);
			TestEqual(TEXT("Hero acted once"), CountHeroActions(EActionType::Ability), 1);
			TestEqual(TEXT("A banked action is left"), ActionsOf(Hero), MULTI_ACTION_MAX - MULTI_ACTION_COST_ONE);
			TestTrue(TEXT("Victory (no kill-shot hold headless: no camera director)"),
					 Orchestrator->GetCombatState() == ECombatState::Victory);
			TestEqual(TEXT("No second action was asked for"), CountSteps(ETurnStartStep::ActionRequested), 0);
		});

		It("ends the turn on End Turn and banks the rest", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			SubmitAndSettle(AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ONE)));
			TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			TestTrue(TEXT("End Turn ended it: Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("The action left stays banked"), ActionsOf(Hero), MULTI_ACTION_MAX - MULTI_ACTION_COST_ONE);
		});

		It("loses a bonus above Max at turn end, and banks the rest up to Max (Q-170f2)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			DataOf(Hero)->GainActions(MULTI_ACTION_BONUS, /*bMayPassMax=*/true);
			TestEqual(TEXT("The bonus passes Max for this turn"), ActionsOf(Hero), MULTI_ACTION_MAX + MULTI_ACTION_BONUS);
			SubmitAndSettle(AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ONE)));
			TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("One spent, still above Max"), ActionsOf(Hero), MULTI_ACTION_MAX + MULTI_ACTION_BONUS - MULTI_ACTION_COST_ONE);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			TestTrue(TEXT("End Turn ended it: Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("What was above Max is lost; Max stays banked"), ActionsOf(Hero), MULTI_ACTION_MAX);
		});

		It("keeps a bank above Max through the other side's turn end, and trims it at the owner's own (Q-170f3, Crown)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON))
			{
				return;
			}
			DataOf(Foe)->GainActions(MULTI_ACTION_MAX - MULTI_ACTION_REGAIN + MULTI_ACTION_BONUS, /*bMayPassMax=*/true);
			TestEqual(TEXT("Foe holds bonus actions above Max during Hero's turn"), ActionsOf(Foe), MULTI_ACTION_MAX + MULTI_ACTION_BONUS);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			if (!TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe))
			{
				return;
			}
			TestEqual(TEXT("Hero's turn end leaves Foe's bonus for Foe's own turn"), ActionsOf(Foe), MULTI_ACTION_MAX + MULTI_ACTION_BONUS);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			TestTrue(TEXT("Hero's turn again"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("Foe's own turn end trims the unused bonus to Max"), ActionsOf(Foe), MULTI_ACTION_MAX);
		});

		It("keeps a bank above a lowered Max through the caster's turn end, and trims it at the holder's own (Q-170f3, Crown)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON))
			{
				return;
			}
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("SkillEffectManager"), Effects))
			{
				return;
			}
			DataOf(Foe)->GainActions(MULTI_ACTION_MAX - MULTI_ACTION_REGAIN, /*bMayPassMax=*/false);
			const FActiveSkillEffect MaxDown = FActiveSkillEffect::CreateBuff(TEXT("Max Actions Down"), MULTI_ACTION_MAX_DOWN_ID,
				ESkillEffectType::MaxActionsDebuff, static_cast<float>(MULTI_ACTION_MAX_DOWN), MULTI_ACTION_MAX_DOWN_TURNS);
			Effects->ApplyEffect(Foe, MaxDown, Hero, TEXT("MultiActionTurnSpec"));
			const int32 LoweredMax = MULTI_ACTION_MAX - MULTI_ACTION_MAX_DOWN;
			if (!TestEqual(TEXT("Hero's Max Actions Down lowers Foe's Max"), DataOf(Foe)->GetMaxActions(), LoweredMax))
			{
				return;
			}
			TestEqual(TEXT("The drop trims nothing at once"), ActionsOf(Foe), MULTI_ACTION_MAX);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			if (!TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe))
			{
				return;
			}
			TestEqual(TEXT("The caster's turn end leaves Foe's bank above its lowered Max"), ActionsOf(Foe), MULTI_ACTION_MAX);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			TestTrue(TEXT("Hero's turn again"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("Foe's own turn end trims to the lowered Max"), ActionsOf(Foe), LoweredMax);
		});

		It("ends the turn after three skills with actions left, which stay banked (Q-170j, Crown)", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON))
			{
				return;
			}
			const int32 Bank = ActionPipsConstants::SKILLS_PER_TURN_MAX + 1;
			DataOf(Hero)->GainActions(Bank - MULTI_ACTION_REGAIN, /*bMayPassMax=*/true);
			if (!TestEqual(TEXT("Hero holds one more action than the skill limit"), ActionsOf(Hero), Bank))
			{
				return;
			}
			UAbilityData *Ability = NewAbility(MULTI_ACTION_COST_ONE);
			for (int32 Used = 1; Used < ActionPipsConstants::SKILLS_PER_TURN_MAX; ++Used)
			{
				SubmitAndSettle(AbilityOn(Foe, Ability));
				TestTrue(TEXT("Still Hero's turn below the limit"), Orchestrator->GetCurrentActor() == Hero);
				TestEqual(TEXT("The skill is counted"), DataOf(Hero)->SkillsUsedThisTurn, Used);
			}
			SubmitAndSettle(AbilityOn(Foe, Ability));
			TestTrue(TEXT("The third skill ends the turn: Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("The action left stays banked"), ActionsOf(Hero), Bank - ActionPipsConstants::SKILLS_PER_TURN_MAX);
			TestEqual(TEXT("The count clears at the turn's end"), DataOf(Hero)->SkillsUsedThisTurn, 0);
		});

		It("reads a cost above 3 as 3 (Q-170j, Crown)", [this]()
		{
			TestEqual(TEXT("cost 5 plays as 3"), AbilityOn(nullptr, NewAbility(ActionPipsConstants::ACTION_COST_MAX + 2)).GetActionCost(),
					  ActionPipsConstants::ACTION_COST_MAX);
		});

		It("ends the turn on Defend, which costs nothing", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			SubmitAndSettle(Plain(EActionType::Defend));
			TestEqual(TEXT("Hero defended"), CountHeroActions(EActionType::Defend), 1);
			TestTrue(TEXT("Defend ended the turn: Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
			TestEqual(TEXT("Defend spent nothing"), ActionsOf(Hero), MULTI_ACTION_MAX);
		});

		It("refuses a cost above Max, and makes a cost above the bank wait", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON))
			{
				return;
			}
			const FAction AboveMax = AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ABOVE_MAX));
			const FText AboveMaxReason = Orchestrator->GetActionCostRefusedReason(Hero, AboveMax);
			TestFalse(TEXT("A cost above Max has a reason"), AboveMaxReason.IsEmpty());
			TestFalse(TEXT("The menu's check refuses it"), Orchestrator->ValidateAction(AboveMax).bIsValid);
			SubmitAndSettle(AboveMax);
			TestEqual(TEXT("It never ran"), CountHeroActions(EActionType::Ability), 0);
			TestTrue(TEXT("Still Hero's turn (asked again)"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("Nothing spent"), ActionsOf(Hero), MULTI_ACTION_REGAIN);

			const FAction CostTwo = AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_TWO));
			const FText NeedsReason = Orchestrator->GetActionCostRefusedReason(Hero, CostTwo);
			TestFalse(TEXT("A cost above the bank has a reason"), NeedsReason.IsEmpty());
			TestFalse(TEXT("A different reason from above Max"), NeedsReason.EqualTo(AboveMaxReason));
			SubmitAndSettle(CostTwo);
			TestEqual(TEXT("It waits"), CountHeroActions(EActionType::Ability), 0);

			if (!GiveHeroTwoActions()) // nothing was spent, so the bank is still the regain
			{
				return;
			}
			TestTrue(TEXT("With 2 banked it can be paid"), Orchestrator->GetActionCostRefusedReason(Hero, CostTwo).IsEmpty());
			SubmitAndSettle(CostTwo);
			TestEqual(TEXT("It ran"), CountHeroActions(EActionType::Ability), 1);
			TestEqual(TEXT("It took both actions"), ActionsOf(Hero), 0);
			TestTrue(TEXT("The turn ended at 0"), Orchestrator->GetCurrentActor() == Foe);
		});

		It("spends a ritual's cost when it is armed, and the turn goes on", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON) || !GiveHeroTwoActions())
			{
				return;
			}
			SubmitAndSettle(AbilityOn(Foe, NewAbility(MULTI_ACTION_COST_ONE, MULTI_ACTION_RITUAL_DELAY)));
			const bool bArmed = TurnManager->PreviewTurnOrder(MULTI_ACTION_PREVIEW_TURNS).ContainsByPredicate(
				[this](const FPreviewTurnEntry &Entry) { return Entry.Type == ETurnType::Execution && Entry.Actor == Hero; });
			TestTrue(TEXT("The ritual is armed (an Execution turn is scheduled)"), bArmed);
			TestEqual(TEXT("Arming spent its cost"), ActionsOf(Hero), MULTI_ACTION_MAX - MULTI_ACTION_COST_ONE);
			TestTrue(TEXT("Arming didn't end the turn"), Orchestrator->GetCurrentActor() == Hero);
		});

		It("lets a weapon switch happen again and again, for free", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON, /*bHeroLoadout=*/true))
			{
				return;
			}
			ULoadoutComponent *Loadout = Hero->FindComponentByClass<ULoadoutComponent>();
			if (!TestNotNull(TEXT("Hero's loadout"), Loadout))
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("First switch"), Loadout->TrySwitchWeaponThisTurn(Reason));
			TestTrue(TEXT("Second switch in the same turn"), Loadout->TrySwitchWeaponThisTurn(Reason));
			TestTrue(TEXT("Never refused"), Loadout->GetTurnSwitchRefusedReason().IsEmpty());
			TestEqual(TEXT("No action spent"), ActionsOf(Hero), MULTI_ACTION_REGAIN);
			TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		});

		It("plays the fight-start bank on a first turn and regains on the next", [this]()
		{
			if (!StartDuel(MULTI_ACTION_TEST_ON))
			{
				return;
			}
			TestEqual(TEXT("First turn: the fight-start bank, no extra regain"), ActionsOf(Hero), MULTI_ACTION_REGAIN);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			if (!TestTrue(TEXT("Foe's turn"), Orchestrator->GetCurrentActor() == Foe))
			{
				return;
			}
			TestEqual(TEXT("Foe's first turn plays its fight-start bank too"), ActionsOf(Foe), MULTI_ACTION_REGAIN);
			Orchestrator->SubmitActionAsync(Plain(EActionType::EndTurn));
			TestTrue(TEXT("Hero's turn again"), Orchestrator->GetCurrentActor() == Hero);
			TestEqual(TEXT("The banked action plus the regain, up to Max"), ActionsOf(Hero), MULTI_ACTION_MAX);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
